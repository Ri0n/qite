#ifndef QITE_H
#define QITE_H

#include <QObject>
#include <QTextObjectInterface>

class QTextEdit;
class InteractiveText;

#ifndef QITE_FIRST_USER_PROPERTY
# define QITE_FIRST_USER_PROPERTY 0
#endif

class InteractiveTextFormat : public QTextCharFormat
{
public:
    using QTextCharFormat::QTextCharFormat;

    enum Property {
        Id =              QTextFormat::UserProperty + QITE_FIRST_USER_PROPERTY,
        UserProperty
    };

    inline InteractiveTextFormat(int objectType)
    { setObjectType(objectType); }

    inline quint32 id() const
    { return property(Id).toUInt(); }
};

class InteractiveTextElementController : public QObject, public QTextObjectInterface
{
    Q_OBJECT
    Q_INTERFACES(QTextObjectInterface)

public:
    enum class EventType {
        Enter,
        Leave,
        Move,
        Click
    };
    class Event
    {
    public:
        QEvent *qevent;
        EventType type;
        QPoint pos; // relative to element. last position for "Leave"
    };

    InteractiveTextElementController(InteractiveText *itc);
    virtual ~InteractiveTextElementController();
    virtual QCursor cursor();

protected:
    friend class InteractiveText;
    InteractiveText *itc;
    int objectType;

    virtual bool mouseEvent(const Event &event, const QRect &rect, QTextCursor &selected);
};

class InteractiveText : public QObject
{
    Q_OBJECT
public:
    InteractiveText(QTextEdit *_textEdit, int baseObjectType = QTextFormat::UserObject);
    inline QTextEdit* textEdit() const { return _textEdit; }

    int registerController(InteractiveTextElementController *elementController);
    void unregisterController(InteractiveTextElementController *elementController);
    quint32 insert(InteractiveTextFormat &fmt);
    QTextCursor findElement(quint32 elementId, int cursorPositionHint = 0);
protected:
    bool eventFilter(QObject *obj, QEvent *event);
private:
    QTextEdit *_textEdit = nullptr;
    int _baseObjectType;
    int _objectType;
    quint32 _uniqueElementId = 0; // just a sequence number
    quint32 _lastElementId; // last which had mouse event
    int _lastCursorPositionHint; // wrt mouse event
    QMap<int,InteractiveTextElementController*> _controllers;
    bool _lastMouseHandled = false;
    void checkAndGenerateLeaveEvent(QEvent *event);
};



#endif // QITE_H
