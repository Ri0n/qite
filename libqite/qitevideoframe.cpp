/* SPDX-License-Identifier: Apache-2.0 */
#include "qitevideoframe_p.h"

#include <QMediaPlayer>
#include <QLoggingCategory>
#include <QMutex>
#include <QMutexLocker>
#include <QPainter>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QVideoSink>
#else
#include <QVideoSurfaceFormat>

Q_LOGGING_CATEGORY(qiteVideoSurfaceLog, "qite.video.surface")

QList<QVideoFrame::PixelFormat> ITEVideoSurface::supportedPixelFormats(QAbstractVideoBuffer::HandleType type) const
{
    if (type != QAbstractVideoBuffer::NoHandle)
        return {};
    // Prefer directly paintable RGB frames, but allow the software formats
    // that Qt 5.15's QVideoFrame::image() can convert. Some GStreamer sinks
    // expose only NV12/YUV420P or BGR, even when AAC audio decodes correctly.
    QList<QVideoFrame::PixelFormat> formats {
        QVideoFrame::Format_RGB32, QVideoFrame::Format_ARGB32, QVideoFrame::Format_ARGB32_Premultiplied,
        QVideoFrame::Format_RGB24, QVideoFrame::Format_RGB565, QVideoFrame::Format_RGB555
    };
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    formats << QVideoFrame::Format_BGRA32 << QVideoFrame::Format_BGRA32_Premultiplied
            << QVideoFrame::Format_BGR32 << QVideoFrame::Format_BGR24
            << QVideoFrame::Format_BGR565 << QVideoFrame::Format_BGR555
            << QVideoFrame::Format_YUV420P << QVideoFrame::Format_YV12
            << QVideoFrame::Format_NV12 << QVideoFrame::Format_NV21
            << QVideoFrame::Format_UYVY << QVideoFrame::Format_YUYV
            << QVideoFrame::Format_YUV444 << QVideoFrame::Format_AYUV444;
#endif
    return formats;
}

bool ITEVideoSurface::start(const QVideoSurfaceFormat &format)
{
    const bool success = QAbstractVideoSurface::start(format);
    reportedFirstFrame_ = false;
    qCDebug(qiteVideoSurfaceLog) << "Qt5 surface start:" << (success ? "accepted" : "rejected")
                                << "format" << format.pixelFormat()
                                << "size" << format.frameSize()
                                << "handle" << format.handleType();
    return success;
}

bool ITEVideoSurface::present(const QVideoFrame &frame)
{
    if (!reportedFirstFrame_ && frame.isValid()) {
        reportedFirstFrame_ = true;
        qCDebug(qiteVideoSurfaceLog) << "Qt5 first video frame:" << frame.pixelFormat()
                                    << frame.size() << "handle" << frame.handleType();
    }
    emit frameReady(frame, surfaceFormat().sizeHint(),
                    surfaceFormat().scanLineDirection() == QVideoSurfaceFormat::BottomToTop,
                    surfaceFormat().viewport());
    return true;
}
#endif

struct ITEVideoFrameWidget::Mailbox {
    QMutex               mutex;
    ITEVideoFrameWidget *target = nullptr;
    QVideoFrame          frame;
    QSize                displaySize;
    QRect                sourceRect;
    bool                 bottomUp = false;
    bool                 queued   = false;
};

ITEVideoFrameWidget::ITEVideoFrameWidget(QWidget *parent) : QWidget(parent), mailbox(new Mailbox)
{
    setObjectName(QStringLiteral("qite-video-frame"));
    setAttribute(Qt::WA_OpaquePaintEvent);
    mailbox->target = this;
    // Producer threads touch only the mailbox. Enqueue under its lock, so the
    // destructor cannot invalidate the receiver between checking and posting.
    auto enqueue
        = [box = mailbox](const QVideoFrame &frame, const QSize &size, bool bottomUp, const QRect &sourceRect) {
              QMutexLocker lock(&box->mutex);
              auto         target = box->target;
              if (!target)
                  return;
              box->frame       = frame;
              box->displaySize = size;
              box->sourceRect  = sourceRect;
              box->bottomUp    = bottomUp;
              if (box->queued)
                  return;
              box->queued = true;
              QMetaObject::invokeMethod(
                  target,
                  [box, target] {
                      bool sizeChanged = false;
                      QSize newSize;
                      {
                          QMutexLocker lock(&box->mutex);
                          if (box->target != target)
                              return;
                          target->frame       = std::move(box->frame);
                          target->displaySize = box->displaySize;
                          target->sourceRect  = box->sourceRect;
                          target->bottomUp    = box->bottomUp;
                          box->queued         = false;
                          newSize = target->displaySize.isValid()
                              ? target->displaySize : target->frame.size();
                          if (newSize.isValid() && target->currentVideoSize != newSize) {
                              target->currentVideoSize = newSize;
                              sizeChanged = true;
                          }
                      }
                      if (sizeChanged)
                          emit target->videoSizeChanged(newSize);
                      target->update(); // Only this opaque widget, never the text viewport.
                  },
                  Qt::QueuedConnection);
          };
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    sink = new QVideoSink(this);
    connect(
        sink, &QVideoSink::videoFrameChanged, sink,
        [enqueue](const QVideoFrame &frame) { enqueue(frame, {}, false, {}); }, Qt::DirectConnection);
#else
    surface = new ITEVideoSurface(this);
    connect(surface, &ITEVideoSurface::frameReady, surface, enqueue, Qt::DirectConnection);
#endif
}

ITEVideoFrameWidget::~ITEVideoFrameWidget()
{
    QMutexLocker lock(&mailbox->mutex);
    mailbox->target = nullptr;
    mailbox->frame  = {};
}

void ITEVideoFrameWidget::attach(QMediaPlayer *player)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    player->setVideoOutput(sink);
#else
    player->setVideoOutput(surface);
#endif
}

void ITEVideoFrameWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.fillRect(rect(), Qt::black);
    if (!frame.isValid())
        return;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QVideoFrame::PaintOptions options;
    options.backgroundColor = Qt::black;
    frame.paint(&painter, rect(), options);
#else
    const auto format = QVideoFrame::imageFormatFromPixelFormat(frame.pixelFormat());
    QImage converted;
    QImage image;
    bool mapped = false;
    if (format != QImage::Format_Invalid) {
        // Fast path: direct RGB scan lines, borrowed only for this paint event.
        mapped = frame.map(QAbstractVideoBuffer::ReadOnly);
        if (mapped)
            image = QImage(frame.bits(), frame.width(), frame.height(), frame.bytesPerLine(), format);
    }
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    else {
        // Qt already has tested native converters for planar YUV and BGR.
        // Do not interpret those bytes as packed RGB or add a second decoder.
        converted = frame.image();
        image = converted;
    }
#endif
    if (image.isNull()) {
        if (!warnedUnmappableFrame) {
            warnedUnmappableFrame = true;
            qWarning() << "Qite Qt 5: cannot paint video frame, pixel format"
                       << frame.pixelFormat() << "size" << frame.size()
                       << "handle" << frame.handleType();
        }
        if (mapped)
            frame.unmap();
        return;
    }
    warnedUnmappableFrame = false;
    QSizeF size = displaySize.isValid() ? QSizeF(displaySize) : QSizeF(image.size());
    size.scale(this->size(), Qt::KeepAspectRatio);
    const QRectF target((width() - size.width()) / 2, (height() - size.height()) / 2, size.width(), size.height());
    if (bottomUp) {
        painter.translate(0, height());
        painter.scale(1, -1);
    }
    painter.drawImage(target, image, sourceRect.isValid() ? sourceRect : image.rect());
    if (mapped)
        frame.unmap();
#endif
}
