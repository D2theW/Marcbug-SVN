/****************************************************************************
** Meta object code from reading C++ file 'messageHandler.h'
**
** Created: Tue Aug 24 18:05:28 2010
**      by: The Qt Meta Object Compiler version 62 (Qt 4.6.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../sccCommon/include/messageHandler.h"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'messageHandler.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 62
#error "This file was generated using the moc from 4.6.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
static const uint qt_meta_data_messageHandler[] = {

 // content:
       4,       // revision
       0,       // classname
       0,    0, // classinfo
       2,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: signature, parameters, type, tag, flags
      16,   15,   15,   15, 0x0a,
      32,   24,   15,   15, 0x0a,

       0        // eod
};

static const char qt_meta_stringdata_messageHandler[] = {
    "messageHandler\0\0clear()\0message\0"
    "statusSlot(QString)\0"
};

const QMetaObject messageHandler::staticMetaObject = {
    { &QObject::staticMetaObject, qt_meta_stringdata_messageHandler,
      qt_meta_data_messageHandler, 0 }
};

#ifdef Q_NO_DATA_RELOCATION
const QMetaObject &messageHandler::getStaticMetaObject() { return staticMetaObject; }
#endif //Q_NO_DATA_RELOCATION

const QMetaObject *messageHandler::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->metaObject : &staticMetaObject;
}

void *messageHandler::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_messageHandler))
        return static_cast<void*>(const_cast< messageHandler*>(this));
    return QObject::qt_metacast(_clname);
}

int messageHandler::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: clear(); break;
        case 1: statusSlot((*reinterpret_cast< QString(*)>(_a[1]))); break;
        default: ;
        }
        _id -= 2;
    }
    return _id;
}
QT_END_MOC_NAMESPACE
