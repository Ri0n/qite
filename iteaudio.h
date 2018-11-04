#ifndef ITEAUDIO_H
#define ITEAUDIO_H

#include <QObject>
#include <QCursor>

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
    QRectF scaleRect;
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
    void positionChanged(qint64);
};

#endif // ITEAUDIO_H
