#ifndef INTERACTIVETEXTOVERLORD_H
#define INTERACTIVETEXTOVERLORD_H

#include <QObject>
#include <QTextObjectInterface>

class QTextEdit;
class InteractiveTextController;

//-------
// Interactive handlers
//-----------
class InteractiveTextElementController : public QObject, public QTextObjectInterface
{
    Q_OBJECT
    Q_INTERFACES(QTextObjectInterface)
protected:
    friend class InteractiveTextController;
    InteractiveTextController *itc;
    int objectType;

    virtual bool mouseEvent(QEvent *event, const QTextCharFormat &charFormat, const QRect &rect);
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

    inline quint32 uniqueElementId() { return _uniqueElementId++; }
protected:
    bool eventFilter(QObject *obj, QEvent *event);
private:
    QTextEdit *_textEdit = nullptr;
    int _objectType = 0;
    quint32 _uniqueElementId = 0;
    QMap<int,InteractiveTextElementController*> _controllers;
    bool _lastHandled = false;
};



#endif // INTERACTIVETEXTOVERLORD_H
