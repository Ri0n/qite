#ifndef ITEAUDIO_H
#define ITEAUDIO_H

#include <QObject>
#include <QCursor>

#include "interactivetextoverlord.h"

class ITEAudioController : public InteractiveTextElementController
{
    Q_OBJECT

    QCursor _cursor;

public:
    using InteractiveTextElementController::InteractiveTextElementController;

    QSizeF intrinsicSize(QTextDocument *doc, int posInDocument, const QTextFormat &format);
    void drawObject(QPainter *painter, const QRectF &rect, QTextDocument *doc, int posInDocument, const QTextFormat &format);


    void add(const QUrl &audioSrc);
    QCursor cursor();
protected:
    bool mouseEvent(QEvent *event, const QTextCharFormat &charFormat, const QRect &rect);
};

#endif // ITEAUDIO_H
