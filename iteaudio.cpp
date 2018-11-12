/*
Licensed to the Apache Software Foundation (ASF) under one
or more contributor license agreements.  See the NOTICE file
distributed with this work for additional information
regarding copyright ownership.  The ASF licenses this file
to you under the Apache License, Version 2.0 (the
"License"); you may not use this file except in compliance
with the License.  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing,
software distributed under the License is distributed on an
"AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
KIND, either express or implied.  See the License for the
specific language governing permissions and limitations
under the License.
*/

#include "iteaudio.h"

#include <QTextEdit>
#include <QPainter>
#include <QVector2D>
#include <QEvent>
#include <QHoverEvent>
#include <QMediaPlayer>
#include <QTimer>

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
    scaleFillRect = scaleRect.adjusted(scaleOutlineWidth / 2, scaleOutlineWidth / 2, -scaleOutlineWidth / 2, -scaleOutlineWidth / 2);
}

void ITEAudioController::drawITE(QPainter *painter, const QRectF &rect, int posInDocument, const QTextFormat &format)
{
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

    auto playPos = audioFormat.playPosition();
    if (playPos) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(170,255,170));
        QRectF playedRect(scaleFillRect.translated(rect.topLeft())); // to the width of the scale border
        playedRect.setWidth(playPos);
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
    Q_UNUSED(rect);
    quint32 onButton = false;
    if (event.type != EventType::Leave) {
        onButton = isOnButton(event.pos, bgRect)? AudioMessageFormat::MouseOnButton : 0;
    }
    if (onButton) {
        _cursor = QCursor(Qt::PointingHandCursor);
    } else {
        _cursor = QCursor(Qt::ArrowCursor);
    }

    AudioMessageFormat format = AudioMessageFormat::fromCharFormat(selected.charFormat());
    AudioMessageFormat::Flags state = format.state();
    bool onButtonChanged = (state & AudioMessageFormat::MouseOnButton) != onButton;
    bool playStateChanged = false;
    bool positionSet = false;

    if (onButtonChanged) {
        state ^= AudioMessageFormat::MouseOnButton;
    }

    auto playerId = format.id();
    if (event.type == EventType::Click) {
        if (onButton) {
            playStateChanged = true;
            state ^= AudioMessageFormat::Playing;
            auto player = activePlayers.value(playerId);
            if (state & AudioMessageFormat::Playing) {
                if (!player) {
                    player = new QMediaPlayer(this);
                    player->setProperty("playerId", playerId);
                    player->setProperty("cursorPos", selected.anchor());
                    activePlayers.insert(playerId, player);
                    player->setMedia(format.url());
                    auto part = double(format.playPosition()) / double(scaleFillRect.width());

                    if (player->duration() > 0) {
                        player->setPosition(qint64(player->duration() * part));
                        connect(player, SIGNAL(positionChanged(qint64)), this, SLOT(playerPositionChanged(qint64)));
                    } else {
                        connect(player, &QMediaPlayer::durationChanged, [player,part,this](qint64 duration) {
                            // the timer is a workaround for some Qt bug
                            QTimer::singleShot(0, [player,part,this,duration](){
                                player->setPosition(qint64(duration * part));
                                connect(player, SIGNAL(positionChanged(qint64)), this, SLOT(playerPositionChanged(qint64)));
                            });
                        });
                    }

                    connect(player, SIGNAL(stateChanged(QMediaPlayer::State)), this, SLOT(playerStateChanged(QMediaPlayer::State)));


                }
                //player->setVolume(0);
                player->play();
            } else {
                if (player) {
                    player->pause();
                    //player->disconnect(this);activePlayers.take(playerId)->deleteLater();
                }
            }
        } else if (scaleRect.contains(event.pos)) {
            // include outline to clickable area but compute only for inner part
            double part;
            if (event.pos.x() < scaleFillRect.left()) {
                part = 0;
            } else if (event.pos.x() >= scaleFillRect.right()) {
                part = 1;
            } else {
                part = double(event.pos.x() - scaleFillRect.left()) / double(scaleFillRect.width());
            }
            auto player = activePlayers.value(playerId);
            if (player) {
                player->setPosition(qint64(player->duration() * part));
            } // else it's not playing likely
            format.setPlayPosition(quint32(double(scaleFillRect.width()) * part));
            positionSet = true;
        }
    }

    if (onButtonChanged || playStateChanged || positionSet) {
        format.setState(state);
        selected.setCharFormat(format);
    }

    return true;
}

void ITEAudioController::hideEvent(QTextCursor &selected)
{
    auto fmt = AudioMessageFormat::fromCharFormat(selected.charFormat());
    auto player = activePlayers.value(fmt.id());
    qDebug() << "hiding player" << fmt.id();
    if (player) {
        player->stop();
    }
}

bool ITEAudioController::isOnButton(const QPoint &pos, const QRect &rect)
{
    QPoint rel = pos - rect.topLeft();
    return QVector2D(btnCenter).distanceToPoint(QVector2D(rel)) <= btnRadius;
}

void ITEAudioController::playerPositionChanged(qint64 newPos)
{
    auto player = static_cast<QMediaPlayer *>(sender());
    quint32 playerId = player->property("playerId").toUInt();
    int textCursorPos = player->property("cursorPos").toInt();
    QTextCursor cursor = itc->findElement(playerId, textCursorPos);
    if (!cursor.isNull()) {
        auto audioFormat = AudioMessageFormat::fromCharFormat(cursor.charFormat());
        auto lastPixelPos = audioFormat.playPosition();
        auto newPixelPos = decltype (lastPixelPos)(scaleFillRect.width() * (double(newPos) / double(player->duration())));
        if (newPixelPos != lastPixelPos) {
            audioFormat.setPlayPosition(newPixelPos);
            cursor.setCharFormat(audioFormat);
        }
    }
}

void ITEAudioController::playerStateChanged(QMediaPlayer::State state)
{
    if (state == QMediaPlayer::StoppedState) {
        auto player = static_cast<QMediaPlayer *>(sender());
        quint32 playerId = player->property("playerId").toUInt();
        int textCursorPos = player->property("cursorPos").toInt();
        QTextCursor cursor = itc->findElement(playerId, textCursorPos);
        if (!cursor.isNull()) {
            auto audioFormat = AudioMessageFormat::fromCharFormat(cursor.charFormat());
            audioFormat.setState(audioFormat.state().setFlag(AudioMessageFormat::Playing, false));
            audioFormat.setPlayPosition(0);
            cursor.setCharFormat(audioFormat);
        }
        activePlayers.take(playerId)->deleteLater();
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
