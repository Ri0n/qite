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

#ifndef ITEAUDIO_H
#define ITEAUDIO_H

#include <QObject>
#include <QCursor>
#include <QMediaPlayer>

#include "qite.h"

class QMediaPlayer;

class ITEAudioController : public InteractiveTextElementController
{
    Q_OBJECT

    QCursor _cursor;
    QMap<quint32,QMediaPlayer*> activePlayers;

    // geometry
    QSize elementSize;
    QRect bgRect;
    int bgOutlineWidth;
    double bgRectRadius;
    QPointF btnCenter;
    int btnRadius;
    int signSize;
    int scaleOutlineWidth;
    QRectF scaleRect, scaleFillRect;
    int lastFontSize = 0;


    bool isOnButton(const QPoint &pos, const QRect &rect);
    void updateGeomtry();
public:
    ITEAudioController(InteractiveText *itc);
    //using InteractiveTextElementController::InteractiveTextElementController;

    QSizeF intrinsicSize(QTextDocument *doc, int posInDocument, const QTextFormat &format);
    void drawObject(QPainter *painter, const QRectF &rect, QTextDocument *doc, int posInDocument, const QTextFormat &format);


    void insert(const QUrl &audioSrc);
    QCursor cursor();
protected:
    bool mouseEvent(const InteractiveTextElementController::Event &event,
                    const QRect &rect, QTextCursor &selected);
private slots:
    void playerPositionChanged(qint64);
    void playerStateChanged(QMediaPlayer::State);
};

#endif // ITEAUDIO_H
