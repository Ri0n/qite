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

#ifndef QITE_H
#define QITE_H

#include <QObject>
#include <QPointer>
#include <QTextEdit>
#include <QTextObjectInterface>

class InteractiveText;

#ifndef QITE_FIRST_USER_PROPERTY
#define QITE_FIRST_USER_PROPERTY 0
#endif

class InteractiveTextFormat : public QTextCharFormat {
public:
    using QTextCharFormat::QTextCharFormat;

    enum Property { Id = QTextFormat::UserProperty + QITE_FIRST_USER_PROPERTY, UserProperty };

    typedef quint32 ElementId;

    // just make it public. no reason to keep it protected
    inline explicit InteractiveTextFormat(const QTextCharFormat &fmt) : QTextCharFormat(fmt) { }

    inline InteractiveTextFormat(int objectType, ElementId id)
    {
        setObjectType(objectType);
        setProperty(Id, id);
    }

    inline ElementId id() const { return id(*this); }

    static inline ElementId id(const QTextFormat &format) { return ElementId(format.property(Id).toUInt()); }
};

class InteractiveTextElementController : public QObject, public QTextObjectInterface {
    Q_OBJECT
    Q_INTERFACES(QTextObjectInterface)

public:
    enum class EventType { Enter, Leave, Move, Click };
    class Event {
    public:
        QEvent   *qevent;
        EventType type;
        QPoint    pos; // relative to element. last position for "Leave"
    };

    InteractiveTextElementController(InteractiveText *itc, QObject *parent = nullptr);
    virtual ~InteractiveTextElementController();
    virtual QCursor cursor();

    void drawObject(QPainter *painter, const QRectF &rect, QTextDocument *doc, int posInDocument,
                    const QTextFormat &format);

    // subclasses should implement drawITE instead of drawObject
    virtual void drawITE(QPainter *painter, const QRectF &rect, int posInDocument, const QTextFormat &format) = 0;

protected:
    friend class InteractiveText;
    QPointer<InteractiveText> itc;
    int                       objectType;

    virtual bool mouseEvent(const Event &event, const QRect &rect, QTextCursor &selected);
    virtual void hideEvent(QTextCursor &selected);
};

class InteractiveText : public QObject {
    Q_OBJECT
public:
    InteractiveText(QTextEdit *_textEdit, int baseObjectType = QTextFormat::UserObject);
    ~InteractiveText();
    inline QTextEdit *textEdit() const { return _textEdit.data(); }

    int                              registerController(InteractiveTextElementController *elementController);
    void                             unregisterController(InteractiveTextElementController *elementController);
    void                             insert(const InteractiveTextFormat &fmt);
    QTextCursor                      findElement(quint32 elementId, int cursorPositionHint = 0);
    void                             markVisible(const InteractiveTextFormat::ElementId &id);
    InteractiveTextFormat::ElementId nextId();

protected:
    bool eventFilter(QObject *obj, QEvent *event);

private:
    void  checkAndGenerateLeaveEvent(QEvent *event);
    QRect elementRect(const QTextCursor &selected) const;
private slots:
    void trackVisibility();

private:
    QPointer<QTextEdit>                           _textEdit;
    int                                           _baseObjectType;
    int                                           _objectType;
    quint32                                       _uniqueElementId = 0;    // just a sequence number
    quint32                                       _lastElementId;          // last which had mouse event
    int                                           _lastCursorPositionHint; // wrt mouse event
    QMap<int, InteractiveTextElementController *> _controllers;
    QSet<InteractiveTextFormat::ElementId>        _visibleElements;
    bool                                          _lastMouseHandled = false;
};

class ITEMediaOpener {
public:
    virtual QIODevice *open(QUrl &url)           = 0;
    virtual void       close(QIODevice *dev)     = 0;
    virtual QVariant   metadata(const QUrl &url) = 0;
};

#endif // QITE_H
