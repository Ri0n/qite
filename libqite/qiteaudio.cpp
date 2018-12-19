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

#include "qiteaudio.h"

#include <QTextEdit>
#include <QPainter>
#include <QVector2D>
#include <QEvent>
#include <QHoverEvent>
#include <QMediaPlayer>
#include <QTimer>
#include <QMediaMetaData>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class AudioMessageFormat : public InteractiveTextFormat
{
public:
    enum Property {
        Url =           InteractiveTextFormat::UserProperty,
        PlayPosition, /* in pixels */
        State,
        MetadataState,
        Metadata
    };

    enum MDState {
        NotRequested,
        RequestInProgress,
        Finished
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

    QVariant metaData() const;
    void setMetaData(const QVariant &v);

    MDState metaDataState() const;
    void setMetaDataState(MDState state);

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

QVariant AudioMessageFormat::metaData() const
{
    return property(AudioMessageFormat::Metadata);
}

void AudioMessageFormat::setMetaData(const QVariant &v)
{
    setProperty(AudioMessageFormat::Metadata, v);
    setProperty(AudioMessageFormat::MetadataState, int(Finished));
}

AudioMessageFormat::MDState AudioMessageFormat::metaDataState() const
{
    return AudioMessageFormat::MDState(property(AudioMessageFormat::MetadataState).value<int>());
}

void AudioMessageFormat::setMetaDataState(MDState state)
{
    setProperty(AudioMessageFormat::MetadataState, int(state));
}


//----------------------------------------------------------------------------
// ITEAudioController
//----------------------------------------------------------------------------
QSizeF ITEAudioController::intrinsicSize(QTextDocument *doc, int posInDocument, const QTextFormat &format)
{
    Q_UNUSED(doc);
    Q_UNUSED(posInDocument)
    const QTextCharFormat charFormat = format.toCharFormat();
    auto psize = QFontMetrics(charFormat.font()).height();
    if (lastFontSize != psize) {
        lastFontSize = psize;
        updateGeomtry();
    }
    return elementSize;
}

void ITEAudioController::updateGeomtry()
{
    // compute geomtry of player
    baseSize = lastFontSize / 12.0;
    int elementPadding = int(baseSize * 4);

    bgOutlineWidth = baseSize < 2? 2 : int(baseSize);

    btnRadius = int(baseSize * 10);
    int elementHeight = btnRadius * 2 + int(elementPadding * 2);

    int histogramColumnWidth = qRound(baseSize);
    if (!histogramColumnWidth) {
        histogramColumnWidth = 1;
    }

    auto rightPadding = int(baseSize * 5);
    // elementHeight already includes 2 paddings: to the lest and to the right of button
    elementSize = QSize(elementHeight + histogramColumnWidth * HistogramCompressedSize + rightPadding,
                        elementHeight);

    bgRect = QRect(QPoint(0,0), elementSize);
    bgRect.adjust(bgOutlineWidth / 2, bgOutlineWidth / 2, -bgOutlineWidth / 2, -bgOutlineWidth / 2); // outline should fit the format rect.
    bgRectRadius = bgRect.height() / 5;

    btnCenter = QPoint(elementSize.height() / 2, elementSize.height() / 2);

    signSize = btnRadius / 2;

    // next to the button we need histgram/title and scale.
    int left = elementHeight;
    int right = elementSize.width() - rightPadding;
    
    metaRect = QRect(QPoint(left, bgRect.top() + int(baseSize * 3)), QPoint(right, bgRect.top() + int(bgRect.height() * 0.5)));

    // draw scale
    scaleOutlineWidth = bgOutlineWidth;
    QPointF scaleTopLeft(left, metaRect.bottom() + baseSize * 4); // = bgRect.topLeft() + QPointF(left, bgRect.height() * 0.7);
    QPointF scaleBottomRight(right, scaleTopLeft.y() + baseSize * 4);
    scaleRect = QRectF(scaleTopLeft, scaleBottomRight);
    scaleFillRect = scaleRect.adjusted(scaleOutlineWidth / 2, scaleOutlineWidth / 2, -scaleOutlineWidth / 2, -scaleOutlineWidth / 2);
}

void ITEAudioController::drawITE(QPainter *painter, const QRectF &rect, int posInDocument, const QTextFormat &format)
{
    Q_UNUSED(posInDocument);
    const AudioMessageFormat audioFormat = AudioMessageFormat::fromCharFormat(format.toCharFormat());
    //qDebug() << audioFormat.id();

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

    // draw played part
    auto playPos = audioFormat.playPosition();
    if (playPos) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(170,255,170));
        QRectF playedRect(scaleFillRect.translated(rect.topLeft())); // to the width of the scale border
        playedRect.setWidth(playPos);
        painter->drawRoundedRect(playedRect, playedRect.height() / 2, playedRect.height() / 2);
    }

    // check metadata. maybe it's ready or we need to query it
    auto mdState = audioFormat.metaDataState();
    if (mdState != AudioMessageFormat::Finished) {
        if (!autoFetchMetadata || mdState == AudioMessageFormat::RequestInProgress) {
            return;
        }

        // we need t query histogram. Let's check if it makes sense first.
        if (audioFormat.url().path().endsWith(".mka")) { // we use mka for audio messages. so it may have histogram
            auto id = audioFormat.id();
            // use deleayed call since it's not that good to chage docs from drawing func.
            QTimer::singleShot(0, this, [this, id, posInDocument](){
                QTextCursor cursor = itc->findElement(id, posInDocument);
                if (cursor.isNull()) {
                    return; // was deleted so quickly?
                }
                auto audioFormat = AudioMessageFormat::fromCharFormat(cursor.charFormat());
                if (audioFormat.metaDataState() != AudioMessageFormat::NotRequested) {
                    return; // likely duplicate query, while previous one wasn't finished it.
                }

                // time to query histogram file
                if (!nam) {
                    nam = new QNetworkAccessManager(this);
                }
                QUrl metaUrl(audioFormat.url());
                metaUrl.setPath(metaUrl.path() + ".histogram");
                auto reply = nam->get(QNetworkRequest(metaUrl));
                audioFormat.setMetaDataState(AudioMessageFormat::RequestInProgress);
                cursor.setCharFormat(audioFormat);
                auto pos = cursor.anchor();
                connect(reply, &QNetworkReply::finished, this, [this, id, pos, reply](){
                    QTextCursor cursor = itc->findElement(id, pos);
                    if (!cursor.isNull()) {
                        Histogram hm;
                        hm.reserve(HistogramCompressedSize);
                        for (auto v : QString::fromLatin1(reply->readAll()).split(',')) {
                            auto fv = v.toFloat() / 255.0f;
                            hm.push_back(fv > 1.0f? 1.0f : fv);
                        }
                        auto afmt = AudioMessageFormat::fromCharFormat(cursor.charFormat());
                        afmt.setMetaData(QVariant::fromValue<Histogram>(hm));
                        cursor.setCharFormat(afmt);
                    }
                    reply->close();
                    reply->deleteLater();
                });
            });
        }
    }

    auto hg = audioFormat.metaData();
    if (hg.canConvert<QList<float>>()) {
        // histogram
        auto hglist = hg.value<QList<float>>();
        auto step = metaRect.width() / float(hglist.size());
        auto tmetaRect = metaRect.translated(rect.topLeft().toPoint());
        painter->setPen(QColor(70,150,70));
        painter->setBrush(QColor(120,220,120));
        for (int i = 0; i < hglist.size(); i++) { // values from 0 to 1.0 (including)
            int left = int(i * step);
            int right = int((i+1) * step);
            int height = int(metaRect.height() * hglist[i]);
            if (height) {
                //int top = int(metaRect.height() * (1.0f - hglist[i]));
                QRect hcolRect(QPoint(left, metaRect.height() - height), QSize(right-left, height));
                hcolRect.translate(tmetaRect.topLeft());
                painter->drawRect(hcolRect);
            }
        }
    } else if (hg.type() == QVariant::String) {
        painter->setPen(QColor(70,150,70));
        painter->drawText(metaRect.translated(rect.topLeft().toPoint()), hg.toString());
    }

    // runner
    //QRectF runnerRect(playedRect);
    //runnerRect.set
}

void ITEAudioController::insert(const QUrl &audioSrc)
{
    AudioMessageFormat fmt(objectType, audioSrc);
    fmt.setFontPointSize(itc->textEdit()->currentFont().pointSize());
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
                        player->setNotifyInterval(int(player->duration() / double(metaRect.width()) * 3.0)); // 3 px
                        connect(player, SIGNAL(positionChanged(qint64)), this, SLOT(playerPositionChanged(qint64)));
                    } else {
                        connect(player, &QMediaPlayer::durationChanged, [player,part,this](qint64 duration) {
                            // the timer is a workaround for some Qt bug
                            QTimer::singleShot(0, [player,part,this,duration](){
                                if (part > 0) { // don't jump back if event came quite late
                                    player->setPosition(qint64(duration * part));
                                }
                                //qDebug() << int(duration / double(metaRect.width()));
                                player->setNotifyInterval(int(duration / 1000.0 / double(metaRect.width()) * 3.0));
                                connect(player, SIGNAL(positionChanged(qint64)), this, SLOT(playerPositionChanged(qint64)));
                            });
                        });
                        player->setNotifyInterval(50); // while we don't know duration, lets use quite small value
                    }

                    // check for title in metadata
                    connect(player, &QMediaPlayer::metaDataAvailableChanged, this, [this, player](bool available){
                        if (available) {
                            auto title = player->metaData(QMediaMetaData::Title).toString();
                            if (title.isEmpty()) {
                                return;
                            }
                            quint32 playerId = player->property("playerId").toUInt();
                            int textCursorPos = player->property("cursorPos").toInt();
                            QTextCursor cursor = itc->findElement(playerId, textCursorPos);
                            if (cursor.isNull()) {
                                return;
                            }
                            auto format = AudioMessageFormat::fromCharFormat(cursor.charFormat().toCharFormat());
                            if (format.metaData().type() == QVariant::List) {
                                return; // seems we have histogram already
                            }
                            format.setMetaData(title);
                            cursor.setCharFormat(format);
                        }
                    });

                    // try to extract from metadata and store histogram
                    connect(player, QOverload<const QString &, const QVariant &>::of(&QMediaPlayer::metaDataChanged),
                          [=](const QString &key, const QVariant &value){
                        QString comment;
                        int index = 0;
                        if (key != QMediaMetaData::Comment || (comment = value.toString()).isEmpty() ||
                                !comment.startsWith(QLatin1String("AMPLDIAGSTART")) || (index = comment.indexOf("AMPLDIAGEND")) == -1)
                        {
                            return; // In comment we keep histogram. We don't expect anything else
                        }
                        auto sl = comment.mid(int(sizeof("AMPLDIAGSTART")), index - int(sizeof("AMPLDIAGSTART")) - 1).split(",");
                        QList<float> histogram;
                        histogram.reserve(sl.size());
                        std::transform(sl.constBegin(), sl.constEnd(), std::back_inserter(histogram), [](const QString &v){
                            auto fv = v.toFloat() / float(255.0);
                            if (fv > 1) {
                                return 1.0f;
                            }
                            return fv;
                        });

                        quint32 playerId = player->property("playerId").toUInt();
                        int textCursorPos = player->property("cursorPos").toInt();
                        QTextCursor cursor = itc->findElement(playerId, textCursorPos);
                        if (cursor.isNull()) {
                            return;
                        }

                        auto format = AudioMessageFormat::fromCharFormat(cursor.charFormat().toCharFormat());
                        format.setMetaData(QVariant::fromValue<decltype (histogram)>(histogram));
                        cursor.setCharFormat(format);
                    });

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
    //qDebug() << "hiding player" << fmt.id();
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
