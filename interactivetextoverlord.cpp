#include "interactivetextoverlord.h"

#include <QTextEdit>
#include <QPainter>
#include <QTextDocument>
#include <QTextObjectInterface>
#include <QDebug>
#include <QHoverEvent>



//----------------------------------//
// InteractiveTextElementController //
//----------------------------------//
InteractiveTextElementController::InteractiveTextElementController(InteractiveTextController *it) :
    itc(it)
{
    objectType = itc->registerController(this);
}

InteractiveTextElementController::~InteractiveTextElementController()
{
    itc->unregisterController(this);
}

bool InteractiveTextElementController::mouseEvent(QEvent *event, const QTextCharFormat &charFormat, const QRect &rect)
{
    Q_UNUSED(event)
    Q_UNUSED(charFormat)
    Q_UNUSED(rect)
    return false;
}

QCursor InteractiveTextElementController::cursor()
{
    return QCursor(Qt::IBeamCursor);
}

//---------------------------//
// InteractiveTextController //
//---------------------------//
InteractiveTextController::InteractiveTextController(QTextEdit *textEdit, int baseObjectType) :
    QObject(textEdit),
    _textEdit(textEdit),
    _objectType(baseObjectType)
{
    textEdit->installEventFilter(this);
    //ui->textEdit->setMouseTracking(true);
}

int InteractiveTextController::registerController(InteractiveTextElementController *elementController)
{
    auto objectType = newObjectType();
    QTextDocument *doc = textEdit()->document();
    doc->documentLayout()->registerHandler(objectType, elementController);
    _controllers.insert(objectType, elementController);
    return objectType;
}

void InteractiveTextController::unregisterController(InteractiveTextElementController *elementController)
{
    _controllers.remove(elementController->objectType);
}

bool InteractiveTextController::eventFilter(QObject *obj, QEvent *event)
{
    bool ret = false;
    if (event->type() == QEvent::HoverEnter || event->type() == QEvent::HoverMove) {
        auto e = static_cast<QHoverEvent*>(event);
        int docLPos = _textEdit->document()->documentLayout()->hitTest( e->posF(), Qt::ExactHit );
        if (docLPos != -1) {

            QTextCursor newCursor(_textEdit->document());
            newCursor.setPosition(docLPos);
            auto ch = _textEdit->document()->characterAt(newCursor.position());

            if (ch == QChar::ObjectReplacementCharacter) {
                int left = _textEdit->cursorRect(newCursor).left();
                newCursor.movePosition(QTextCursor::Right);
                auto format = newCursor.charFormat();
                auto ot = format.objectType();
                auto it = _controllers.constFind(ot);
                if (_controllers.constEnd() != it) {
                    QRect cr = _textEdit->cursorRect(newCursor);
                    QRect rect(QPoint(left, cr.top()), QPoint(cr.left() - 1, cr.bottom()));
                    ret = (*it)->mouseEvent(event, format, rect);
                    if (ret) {
                        _textEdit->viewport()->setCursor((*it)->cursor());
                    }
                }
            }
        }
        if (!ret && _lastHandled) { // previousy we changed cursor shape but nothing handled now. need default
            _textEdit->viewport()->setCursor(Qt::IBeamCursor);
        }
        _lastHandled = ret;

    } else if (event->type() == QEvent::HoverLeave) {
        qDebug() << "exit all";
        _textEdit->viewport()->setCursor(Qt::IBeamCursor);
        _lastHandled = false;
    } else {
        //qDebug() << obj->objectName() << event->type();
        _lastHandled = false;
    }
    if (!ret) {
        ret = QObject::eventFilter(obj, event);
        //_textEdit->setCursor(QCursor(Qt::IBeamCursor));
    }
    return ret;
}

