#ifndef ITEAUDIO_H
#define ITEAUDIO_H

#include <QObject>
#include <QCursor>

#include "qite.h"

class ITEAudioController : public InteractiveTextElementController
{
    Q_OBJECT

    QCursor _cursor;

    bool isOnButton(const QPoint &pos, const QRect &rect);
public:
    using InteractiveTextElementController::InteractiveTextElementController;

    QSizeF intrinsicSize(QTextDocument *doc, int posInDocument, const QTextFormat &format);
    void drawObject(QPainter *painter, const QRectF &rect, QTextDocument *doc, int posInDocument, const QTextFormat &format);


    void insert(const QUrl &audioSrc);
    QCursor cursor();
protected:
    bool mouseEvent(QEvent *event, const QTextCharFormat &charFormat, const QRect &rect, QTextCursor &selected);
};

#endif // ITEAUDIO_H
