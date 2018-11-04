#include "iteaudio.h"

#include <QTextEdit>
#include <QPainter>
#include <QDebug>
#include <QVector2D>
#include <QEvent>
#include <QHoverEvent>
#include <QMediaPlayer>

class AudioMessageFormat : public InteractiveTextFormat
{
public:
    enum Property {
        Url =           InteractiveTextFormat::UserProperty,
        PlayPosition, /* in pixels */
        State
    };

    enum Flag {
        Playing = 0x1,
        MouseOnButton = 0x2,
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    using InteractiveTextFormat::InteractiveTextFormat;
    AudioMessageFormat(int objectType, const QUrl &url, quint32 position = 0, const Flags &state = Flags());

    Flags state() const;
    void setState(const Flags &state);

    quint32 playPosition() const;
    void setPlayPosition(quint32 position);

    QUrl url() const;

    static AudioMessageFormat fromCharFormat(const QTextCharFormat &fmt) { return AudioMessageFormat(fmt); }
};
Q_DECLARE_OPERATORS_FOR_FLAGS(AudioMessageFormat::Flags)

AudioMessageFormat::AudioMessageFormat(int objectType, const QUrl &url, quint32 position, const Flags &state)
    : InteractiveTextFormat(objectType)
{
    setProperty(Url, url);
    setProperty(PlayPosition, position);
    setState(state);
}

AudioMessageFormat::Flags AudioMessageFormat::state() const
{
    return Flags(property(AudioMessageFormat::State).toUInt());
}

void AudioMessageFormat::setState(const AudioMessageFormat::Flags &state)
{
    setProperty(State, unsigned(state));
}

quint32 AudioMessageFormat::playPosition() const
{
    return property(AudioMessageFormat::PlayPosition).toUInt();
}

void AudioMessageFormat::setPlayPosition(quint32 position)
{
    setProperty(AudioMessageFormat::PlayPosition, position);
}

QUrl AudioMessageFormat::url() const
{
    return property(AudioMessageFormat::Url).value<QUrl>();
}

//----------------------------------------------------------------------------
// ITEAudioController
//----------------------------------------------------------------------------
QSizeF ITEAudioController::intrinsicSize(QTextDocument *doc, int posInDocument, const QTextFormat &format)
{
    Q_UNUSED(doc);
    Q_UNUSED(posInDocument)
    const QTextCharFormat charFormat = format.toCharFormat();
    if (lastFontSize != charFormat.font().pixelSize()) {
        lastFontSize = charFormat.font().pixelSize();
        updateGeomtry();
    }
    return elementSize;
}

void ITEAudioController::updateGeomtry()
{
    // compute geomtry of player
    bgOutlineWidth = lastFontSize / 12;
    if (bgOutlineWidth < 2) {
        bgOutlineWidth = 2;
    }

    elementSize = QSize(bgOutlineWidth * 100, bgOutlineWidth * 25);
    bgRect = QRect(QPoint(0,0), elementSize);
    bgRect.adjust(bgOutlineWidth / 2, bgOutlineWidth / 2, -bgOutlineWidth / 2, -bgOutlineWidth / 2);
    bgRectRadius = bgRect.height() / 5;

    btnRadius = int(bgRect.height()) / 2;
    btnCenter = bgRect.topLeft() + QPoint(btnRadius, btnRadius);
    btnRadius -= 4;

    signSize = btnRadius / 2;

    // draw scale
    scaleOutlineWidth = bgOutlineWidth;
    QPointF scaleTopLeft = bgRect.topLeft() + QPointF(bgRect.height() + bgRect.height() / 10, bgRect.height() * 0.7);
    QPointF scaleBottomRight(bgRect.right() - bgRect.height() / 5, scaleTopLeft.y() + bgRect.height() / 10);
    scaleRect = QRectF(scaleTopLeft, scaleBottomRight);
}

void ITEAudioController::drawObject(QPainter *painter, const QRectF &rect, QTextDocument *doc, int posInDocument, const QTextFormat &format)
{
    Q_UNUSED(doc);
    Q_UNUSED(posInDocument);
    const AudioMessageFormat audioFormat = AudioMessageFormat::fromCharFormat(format.toCharFormat());

    painter->setRenderHints( QPainter::HighQualityAntialiasing );

    QPen bgPen(QColor(100,200,100)); // TODO name all the magic colors
    bgPen.setWidth(bgOutlineWidth);
    painter->setPen(bgPen);
    painter->setBrush(QColor(150,250,150));
    painter->drawRoundedRect(bgRect.translated(int(rect.left()), int(rect.top())), bgRectRadius, bgRectRadius);

    // draw button
    if (audioFormat.state() & AudioMessageFormat::MouseOnButton) {
        painter->setBrush(QColor(130,230,130));
    } else {
        painter->setBrush(QColor(120,220,120));
    }
    auto xBtnCenter = btnCenter + rect.topLeft();
    painter->drawEllipse(xBtnCenter, btnRadius, btnRadius);

    // draw pause/play
    QPen signPen((QColor(Qt::white)));
    signPen.setWidth(bgOutlineWidth);
    painter->setPen(signPen);
    painter->setBrush(QColor(Qt::white));
    bool isPlaying = audioFormat.state() & AudioMessageFormat::Playing;
    if (isPlaying) {
        QRectF bar(0,0,signSize / 3, signSize * 2);
        bar.moveCenter(xBtnCenter - QPointF(signSize / 2, 0));
        painter->drawRect(bar);
        bar.moveCenter(xBtnCenter + QPointF(signSize / 2, 0));
        painter->drawRect(bar);
    } else {
        QPointF play[3] = {xBtnCenter - QPoint(signSize / 2, signSize), xBtnCenter - QPoint(signSize / 2, -signSize), xBtnCenter + QPoint(signSize, 0)};
        painter->drawConvexPolygon(play, 3);
    }

    // draw scale
    QPen scalePen(QColor(100,200,100));
    scalePen.setWidth(scaleOutlineWidth);
    painter->setPen(scalePen);
    painter->setBrush(QColor(120,220,120));
    QRectF xScaleRect(scaleRect.translated(rect.topLeft()));
    painter->drawRoundedRect(xScaleRect, scaleRect.height() / 2, scaleRect.height() / 2);

    // fill before runner
    double position = 0; // in 0..1
    if (isPlaying) {
        auto player = activePlayers.value(audioFormat.id());
        if (!player) {
            qWarning("Player is not found for id=%u", audioFormat.id());
        } else {
            position = double(player->position()) / double(player->duration());
        }
    }

    if (isPlaying) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(170,255,170));
        QRectF playedRect(xScaleRect.adjusted(scaleOutlineWidth / 2, scaleOutlineWidth / 2, -scaleOutlineWidth / 2, -scaleOutlineWidth / 2)); // to the width of the scale border
        playedRect.setWidth(playedRect.width() * position);
        painter->drawRoundedRect(playedRect, playedRect.height() / 2, playedRect.height() / 2);
    }

    // runner
    //QRectF runnerRect(playedRect);
    //runnerRect.set
}

void ITEAudioController::insert(const QUrl &audioSrc)
{
    AudioMessageFormat fmt(objectType, audioSrc);
    itc->insert(fmt);
}

bool ITEAudioController::mouseEvent(const Event &event, const QRect &rect, QTextCursor &selected)
{
    quint32 onButton = false;
    if (event.type != EventType::Leave) {
        onButton = isOnButton(event.pos, bgRect)? AudioMessageFormat::MouseOnButton : 0;
    }
    if (onButton) {
        qDebug() << "on button";
        _cursor = QCursor(Qt::PointingHandCursor);
    } else {
        _cursor = QCursor(Qt::ArrowCursor);
    }

    AudioMessageFormat format = AudioMessageFormat::fromCharFormat(selected.charFormat());
    AudioMessageFormat::Flags state = format.state();
    bool onButtonChanged = (state & AudioMessageFormat::MouseOnButton) != onButton;
    bool playStateChanged = false;

    if (onButtonChanged) {
        state ^= AudioMessageFormat::MouseOnButton;
    }

    auto playerId = format.id();
    if (event.type == EventType::Enter || event.type == EventType::Move) {
        qDebug() << "inside of player" << playerId << rect;


    } else if (event.type == EventType::Click) {
        if (onButton) {
            qDebug() << "playing music!";
            playStateChanged = true;
            state ^= AudioMessageFormat::Playing;
            auto player = activePlayers.value(playerId);
            if (state & AudioMessageFormat::Playing) {
                if (!player) {
                    player = new QMediaPlayer;
                    player->setProperty("playerId", playerId);
                    player->setProperty("cursorPos", selected.position());
                    connect(player, SIGNAL(positionChanged(qint64)), this, SLOT(positionChanged(qint64)));
                    activePlayers.insert(playerId, player);
                    player->setMedia(format.url());
                }
                //player->setVolume(0);
                player->play();
            } else {
                if (player) {
                    player->pause();
                }
            }
        }
    }

    if (onButtonChanged || playStateChanged) {
        format.setState(state);
        selected.setCharFormat(format);
    }

    return true;
}

bool ITEAudioController::isOnButton(const QPoint &pos, const QRect &rect)
{
    QPoint rel = pos - rect.topLeft();
    return QVector2D(btnCenter).distanceToPoint(QVector2D(rel)) <= btnRadius;
}

void ITEAudioController::positionChanged(qint64 newPos)
{
    auto player = static_cast<QMediaPlayer *>(sender());
    quint32 playerId = player->property("playerId").toUInt();
    int textCursorPos = player->property("cursorPos").toInt();
    QTextCursor cursor = itc->findElement(playerId, textCursorPos);
    if (!cursor.isNull()) {
        auto audioFormat = AudioMessageFormat::fromCharFormat(cursor.charFormat());
        auto lastPixelPos = audioFormat.playPosition();
        auto newPixelPos = decltype (lastPixelPos)(scaleRect.width() * (double(newPos) / double(player->duration())));
        if (newPixelPos != lastPixelPos) {
            audioFormat.setPlayPosition(newPixelPos);
            cursor.setCharFormat(audioFormat);
        }
    }
}

ITEAudioController::ITEAudioController(InteractiveText *itc)
    : InteractiveTextElementController(itc)
{

}

QCursor ITEAudioController::cursor()
{
    return _cursor;
}
