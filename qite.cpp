#include "qite.h"

#include <QTextEdit>
#include <QPainter>
#include <QTextDocument>
#include <QTextObjectInterface>
#include <QDebug>
#include <QHoverEvent>
#include <QScrollBar>



//----------------------------------//
// InteractiveTextElementController //
//----------------------------------//
InteractiveTextElementController::InteractiveTextElementController(InteractiveText *it) :
    itc(it)
{
    objectType = itc->registerController(this);
}

InteractiveTextElementController::~InteractiveTextElementController()
{
    itc->unregisterController(this);
}

bool InteractiveTextElementController::mouseEvent(const Event &event, const QRect &rect, QTextCursor &selected)
{
    Q_UNUSED(event)
    Q_UNUSED(rect)
    Q_UNUSED(selected)
    return false;
}

QCursor InteractiveTextElementController::cursor()
{
    return QCursor(Qt::IBeamCursor);
}

//---------------------------//
// InteractiveTextController //
//---------------------------//
InteractiveText::InteractiveText(QTextEdit *textEdit, int baseObjectType) :
    QObject(textEdit),
    _textEdit(textEdit),
    _baseObjectType(baseObjectType),
    _objectType(baseObjectType)
{
    textEdit->installEventFilter(this);
    textEdit->viewport()->installEventFilter(this);

    //ui->textEdit->setMouseTracking(true);
}

int InteractiveText::registerController(InteractiveTextElementController *elementController)
{
    auto objectType = _objectType++;
    QTextDocument *doc = _textEdit->document();
    doc->documentLayout()->registerHandler(objectType, elementController);
    _controllers.insert(objectType, elementController);
    return objectType;
}

void InteractiveText::unregisterController(InteractiveTextElementController *elementController)
{
    textEdit()->document()->documentLayout()->unregisterHandler(elementController->objectType, elementController);
    _controllers.remove(elementController->objectType);
}

quint32 InteractiveText::insert(InteractiveTextFormat &fmt)
{
    auto id = _uniqueElementId++;
    fmt.setProperty(InteractiveTextFormat::Id, id);
    _textEdit->textCursor().insertText(QString(QChar::ObjectReplacementCharacter), fmt);
    // TODO check if mouse is already on the element
    return id;
}

QTextCursor InteractiveText::findElement(quint32 elementId, int cursorPositionHint)
{
    QTextCursor cursor(_textEdit->document());
    cursor.setPosition(cursorPositionHint);

    cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
    QString selectedText = cursor.selectedText();
    if (selectedText.size() && selectedText[0] == QChar::ObjectReplacementCharacter) {
        QTextCharFormat fmt = cursor.charFormat();
        auto otype = fmt.objectType();
        if (otype >= _baseObjectType && otype < _objectType && fmt.property(InteractiveTextFormat::Id).toUInt() == elementId) {
            return cursor;
        }
    }

    cursor.setPosition(0);
    QString elText(QChar::ObjectReplacementCharacter);
    while (!(cursor = _textEdit->document()->find(elText, cursor)).isNull()) {
        QTextCharFormat fmt = cursor.charFormat();
        auto otype = fmt.objectType();
        if (otype >= _baseObjectType && otype < _objectType && fmt.property(InteractiveTextFormat::Id).toUInt() == elementId) {
            break;
        }
    }
    return cursor;
}

bool InteractiveText::eventFilter(QObject *obj, QEvent *event)
{
    bool ourEvent = (obj == _textEdit && (event->type() == QEvent::HoverEnter ||
                                          event->type() == QEvent::HoverMove ||
                                          event->type() == QEvent::HoverLeave)) ||
            (obj == _textEdit->viewport() && event->type() == QEvent::MouseButtonPress);
    if (!ourEvent) {
        return false;
    }

    bool ret = false;
    bool leaveHandled = false;
    QPoint pos;
    if (event->type() == QEvent::MouseButtonPress) {
        pos = static_cast<QMouseEvent*>(event)->pos();
    } else {
        pos = static_cast<QHoverEvent*>(event)->pos();
    }
    pos += QPoint(_textEdit->horizontalScrollBar()->value(), _textEdit->verticalScrollBar()->value());

    if (event->type() == QEvent::HoverEnter || event->type() == QEvent::HoverMove || event->type() == QEvent::MouseButtonPress)
    {
        int docLPos = _textEdit->document()->documentLayout()->hitTest( pos, Qt::ExactHit );
        if (docLPos != -1) {
            QTextCursor cursor(_textEdit->document());
            cursor.setPosition(docLPos);
            int left = _textEdit->cursorRect(cursor).left();
            cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
            if (cursor.selectedText()[0] == QChar::ObjectReplacementCharacter) {

                auto format = cursor.charFormat();
                auto elementId = format.property(InteractiveTextFormat::Id).toUInt();
                auto ot = format.objectType();
                auto *elementController = _controllers.value(ot);
                if (elementController) {
                    // we are definitely on a known interactive element.
                    // first we have to check what was before to generate proper events.
                    bool isEnter = !_lastMouseHandled || _lastElementId != elementId;
                    if (isEnter && _lastMouseHandled) { // jump from another element
                        checkAndGenerateLeaveEvent(event);
                    }
                    leaveHandled = true;

                    QRect cr = _textEdit->cursorRect(cursor);
                    QRect rect(QPoint(left, cr.top()), QPoint(cr.left() - 1, cr.bottom()));

                    InteractiveTextElementController::Event iteEvent;
                    iteEvent.qevent = event;
                    iteEvent.pos = QPoint(pos.x() - rect.left(), pos.y() - rect.top());
                    qDebug() << pos << iteEvent.pos << rect;
                    if (event->type() == QEvent::MouseButtonPress) {
                        iteEvent.type = InteractiveTextElementController::EventType::Click;
                    } else {
                        iteEvent.type = isEnter? InteractiveTextElementController::EventType::Enter :
                                                 InteractiveTextElementController::EventType::Move;
                    }

                    ret = elementController->mouseEvent(iteEvent, rect, cursor);
                    if (ret) {
                        _lastCursorPositionHint = cursor.position();
                        _lastElementId = elementId;
                        _textEdit->viewport()->setCursor(elementController->cursor());
                    } else {
                        _textEdit->viewport()->setCursor(Qt::IBeamCursor);
                    }
                }
            }
        }
    }

    if (!leaveHandled) {
        // not checked yet if we need leave event.This also means we are not on an element.
        checkAndGenerateLeaveEvent(event);
        if (_lastMouseHandled) {
            _textEdit->viewport()->setCursor(Qt::IBeamCursor);
        }
    }

    _lastMouseHandled = ret;
    return ret;
}

void InteractiveText::checkAndGenerateLeaveEvent(QEvent *event)
{
    if (!_lastMouseHandled) {
        return;
    }
    QTextCursor cursor = findElement(_lastElementId, _lastCursorPositionHint);
    if (!cursor.isNull()) {
        auto fmt = cursor.charFormat();
        InteractiveTextElementController *controller = _controllers.value(fmt.objectType());
        if (!controller) {
            return;
        }

        InteractiveTextElementController::Event iteEvent;
        iteEvent.qevent = event;
        iteEvent.type = InteractiveTextElementController::EventType::Leave;

        controller->mouseEvent(iteEvent, QRect(), cursor);
    }
}
