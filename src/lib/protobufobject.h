/*
 * MIT License
 *
 * Copyright (c) 2019 Alexey Edelev <semlanik@gmail.com>
 *
 * This file is part of qtprotobuf project https://git.semlanik.org/semlanik/qtprotobuf
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and
 * to permit persons to whom the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies
 * or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
 * FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include <QObject>
#include <QDebug>
#include <QMetaObject>
#include <QMetaProperty>
#include <QBitArray>

#include <unordered_map>

namespace qtprotobuf {

enum WireTypes {
    Varint = 0,
    Fixed64 = 1,
    LengthDelimited = 2,
    Fixed32 = 5
};

template <typename T>
class ProtobufObject : public QObject
{
public:
    explicit ProtobufObject(QObject *parent = nullptr) : QObject(parent)
    {}

    QByteArray serialize() {
        QByteArray result;
        T* instance = dynamic_cast<T*>(this);
        for(auto field : T::propertyOrdering) {
            int propertyIndex = field.second;
            int fieldIndex = field.first;
            const char* propertyName = T::staticMetaObject.property(propertyIndex).name();
            switch(T::staticMetaObject.property(propertyIndex).type()) {
            case QVariant::Int:
                result.append(serializeInt(instance->property(propertyName).toInt(), fieldIndex));
                break;
            }
        }

        return result;
    }

    void deserialize(const QByteArray& array) {
        T* instance = dynamic_cast<T*>(this);
        //TODO
    }

    QByteArray serializeInt(int value, int fieldIndex) {
        char typeByte = (fieldIndex << 3) | Varint;
        QByteArray result;
        result.append(typeByte);
        return result;
    }
};

}