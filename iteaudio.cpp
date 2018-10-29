#include "iteaudio.h"

#include <QTextEdit>
#include <QPainter>
#include <QDebug>
#include <QVector2D>
#include <QEvent>
#include <QHoverEvent>


class AudioMessageFormat : public QTextCharFormat
{
public:
    AudioMessageFormat(int objectType, const QUrl &id);

    enum Property {
        Url =           QTextFormat::UserProperty + 10,
        PlayPosition =  QTextFormat::UserProperty + 11,
        Id =            QTextFormat::UserProperty + 12
    };
};

AudioMessageFormat::AudioMessageFormat(int objectType, const QUrl &url)
    : QTextCharFormat()
{
    setObjectType(objectType);
    QTextFormat::setProperty(Url, url);
    QTextFormat::setProperty(PlayPosition, 0);
}


//----------------------------------------------------------------------------
// ITEAudioController
//----------------------------------------------------------------------------
QSizeF ITEAudioController::intrinsicSize(QTextDocument *doc, int posInDocument, const QTextFormat &format)
{
    Q_UNUSED(doc);
    Q_UNUSED(posInDocument)
    const QTextCharFormat charFormat = format.toCharFormat();
    int skinHeight = 50; // FIXME
    int skinWidth = 200; // FIXME
    int skinScale = 1;
    if (charFormat.font().pixelSize() > 1.5*skinHeight) {
        skinScale <<= 1;
    }
    return QSize(skinWidth, skinHeight);
}

void ITEAudioController::drawObject(QPainter *painter, const QRectF &rect, QTextDocument *doc, int posInDocument, const QTextFormat &format)
{
    Q_UNUSED(doc);
    Q_UNUSED(posInDocument);
    const QTextCharFormat charFormat = format.toCharFormat();

    painter->setRenderHints( QPainter::HighQualityAntialiasing );

    int bgOutlineWidth = 2;
    QRectF bgRect(rect.adjusted(bgOutlineWidth / 2, bgOutlineWidth / 2, -bgOutlineWidth / 2, -bgOutlineWidth / 2));
    QPen bgPen(QColor(100,200,100));
    bgPen.setWidth(bgOutlineWidth);
    painter->setPen(bgPen);

    painter->setBrush(QColor(150,250,150));
    painter->drawRoundedRect(bgRect, 10, 10);

    // draw button
    painter->setBrush(QColor(120,220,120));
    int radius = int(bgRect.height()) / 2;
    QPointF btnCenter = bgRect.topLeft() + QPoint(radius, radius);
    painter->drawEllipse(btnCenter, radius - 4, radius - 4);

    // draw pause/play
    QPen signPen((QColor(Qt::white)));
    signPen.setWidth(2);
    painter->setPen(signPen);
    painter->setBrush(QColor(Qt::white));
    int signSize = ( radius - 4 ) / 2;
    QPointF play[3] = {btnCenter - QPoint(signSize / 2, signSize), btnCenter - QPoint(signSize / 2, -signSize), btnCenter + QPoint(signSize, 0)};
    painter->drawConvexPolygon(play, 3);
    // TODO pause

    // draw scale
    int scaleOutlineWidth = 2;
    QPen scalePen(QColor(100,200,100));
    scalePen.setWidth(scaleOutlineWidth);
    painter->setPen(scalePen);
    painter->setBrush(QColor(120,220,120));
    QPointF scaleTopLeft = bgRect.topLeft() + QPointF(radius * 2 + 4, radius * 1.4);
    QPointF scaleBottomRight(bgRect.right() - 10, scaleTopLeft.y() + 6);
    QRectF scaleRect(scaleTopLeft, scaleBottomRight);
    painter->drawRoundedRect(scaleRect, scaleRect.height() / 2, scaleRect.height() / 2);

    // fill before runner
    auto position = charFormat.property(AudioMessageFormat::PlayPosition).toUInt();
    Q_ASSERT(position <= 100); // percents

    if (position) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(170,255,170));
        QRectF playedRect(scaleRect.adjusted(scaleOutlineWidth / 2, scaleOutlineWidth / 2, -scaleOutlineWidth / 2, -scaleOutlineWidth / 2)); // to the width of the scale border
        playedRect.setWidth(playedRect.width() / 100.0 * position);
        painter->drawRoundedRect(playedRect, playedRect.height() / 2, playedRect.height() / 2);
    }
}

void ITEAudioController::add(const QUrl &audioSrc)
{
    AudioMessageFormat fmt(objectType, audioSrc);
    auto id = itc->uniqueElementId();
    fmt.setProperty(AudioMessageFormat::Id, id);
    itc->textEdit()->textCursor().insertText(QString(QChar::ObjectReplacementCharacter), fmt);
}

bool ITEAudioController::mouseEvent(QEvent *event, const QTextCharFormat &charFormat, const QRect &rect)
{
    if (event->type() == QEvent::HoverEnter || event->type() == QEvent::HoverMove) {
        qDebug() << "inside of player" << charFormat.property(AudioMessageFormat::Id).toUInt() << rect;

        // TODO cache computing
        int bgOutlineWidth = 2;
        QRect bgRect(rect.adjusted(bgOutlineWidth / 2, bgOutlineWidth / 2, -bgOutlineWidth / 2, -bgOutlineWidth / 2));
        int radius = int(bgRect.height()) / 2;
        auto btnCenter = bgRect.topLeft() + QPoint(radius, radius);
        if (QVector2D(btnCenter).distanceToPoint(QVector2D(static_cast<QHoverEvent*>(event)->pos())) <= radius - 4) {
            qDebug() << "on button";
            _cursor = QCursor(Qt::PointingHandCursor);
        } else {
            _cursor = QCursor(Qt::ArrowCursor);
        }
    }
    return true;
}

QCursor ITEAudioController::cursor()
{
    return _cursor;
}
