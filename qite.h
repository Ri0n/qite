#ifndef QITE_H
#define QITE_H

#include <QObject>
#include <QTextObjectInterface>

class QTextEdit;
class InteractiveTextController;

class InteractiveTextFormat : public QTextCharFormat
{
public:
    using QTextCharFormat::QTextCharFormat;

    enum Property {
        Id =              QTextFormat::UserProperty + 10,
        UserProperty
    };

    inline InteractiveTextFormat(int objectType)
    { setObjectType(objectType); }

    inline quint32 id() const
    {
        return property(Id).toUInt();
    }
};

class InteractiveTextElementController : public QObject, public QTextObjectInterface
{
    Q_OBJECT
    Q_INTERFACES(QTextObjectInterface)
protected:
    friend class InteractiveTextController;
    InteractiveTextController *itc;
    int objectType;

    virtual bool mouseEvent(QEvent *event, const QTextCharFormat &charFormat, const QRect &rect, QTextCursor &selected);
public:
    InteractiveTextElementController(InteractiveTextController *itc);
    virtual ~InteractiveTextElementController();
    virtual QCursor cursor();
};

class InteractiveTextController : public QObject
{
    Q_OBJECT
public:
    InteractiveTextController(QTextEdit *_textEdit, int baseObjectType = QTextFormat::UserObject);
    inline QTextEdit* textEdit() const { return _textEdit; }
    inline int newObjectType() { return _objectType++; }

    int registerController(InteractiveTextElementController *elementController);
    void unregisterController(InteractiveTextElementController *elementController);
    quint32 insert(InteractiveTextFormat &fmt);
protected:
    bool eventFilter(QObject *obj, QEvent *event);
private:
    QTextEdit *_textEdit = nullptr;
    int _objectType = 0;
    quint32 _uniqueElementId = 0;
    QMap<int,InteractiveTextElementController*> _controllers;
    bool _lastHandled = false;
};



#endif // QITE_H
