/****************************************************************************
** Meta object code from reading C++ file 'ack_manager.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../include/ack_manager.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'ack_manager.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.9.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN10AckManagerE_t {};
} // unnamed namespace

template <> constexpr inline auto AckManager::qt_create_metaobjectdata<qt_meta_tag_ZN10AckManagerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "AckManager",
        "resend",
        "",
        "Pending",
        "p",
        "failed",
        "msg_id",
        "receiverId",
        "deadLetter",
        "messageId",
        "attempts",
        "reason",
        "onTick"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'resend'
        QtMocHelpers::SignalData<void(const Pending &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Signal 'failed'
        QtMocHelpers::SignalData<void(qint64, const QString &)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::LongLong, 6 }, { QMetaType::QString, 7 },
        }}),
        // Signal 'deadLetter'
        QtMocHelpers::SignalData<void(qint64, QString, int, QString)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::LongLong, 9 }, { QMetaType::QString, 7 }, { QMetaType::Int, 10 }, { QMetaType::QString, 11 },
        }}),
        // Slot 'onTick'
        QtMocHelpers::SlotData<void()>(12, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<AckManager, qt_meta_tag_ZN10AckManagerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject AckManager::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10AckManagerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10AckManagerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN10AckManagerE_t>.metaTypes,
    nullptr
} };

void AckManager::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<AckManager *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->resend((*reinterpret_cast< std::add_pointer_t<Pending>>(_a[1]))); break;
        case 1: _t->failed((*reinterpret_cast< std::add_pointer_t<qint64>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 2: _t->deadLetter((*reinterpret_cast< std::add_pointer_t<qint64>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[4]))); break;
        case 3: _t->onTick(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (AckManager::*)(const Pending & )>(_a, &AckManager::resend, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (AckManager::*)(qint64 , const QString & )>(_a, &AckManager::failed, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (AckManager::*)(qint64 , QString , int , QString )>(_a, &AckManager::deadLetter, 2))
            return;
    }
}

const QMetaObject *AckManager::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *AckManager::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10AckManagerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int AckManager::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 4)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 4)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 4;
    }
    return _id;
}

// SIGNAL 0
void AckManager::resend(const Pending & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void AckManager::failed(qint64 _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1, _t2);
}

// SIGNAL 2
void AckManager::deadLetter(qint64 _t1, QString _t2, int _t3, QString _t4)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1, _t2, _t3, _t4);
}
QT_WARNING_POP
