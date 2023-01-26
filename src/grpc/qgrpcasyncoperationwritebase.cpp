/*
 * MIT License
 *
 * Copyright (c) 2019 Alexey Edelev <semlanik@gmail.com>
 * Copyright (c) 2021 Nikolai Lubiagov <lubagov@gmail.com>
 *
 * This file is part of QtProtobuf project https://git.semlanik.org/semlanik/qtprotobuf
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

#include "qgrpcasyncoperationwritebase_p.h"

#include <qtprotobuflogging.h>

namespace QtProtobuf {

QGrpcAsyncOperationWriteBase::QGrpcAsyncOperationWriteBase(QAbstractGrpcClient *parent) :
    QGrpcAsyncOperationBase(parent)
{

}

QGrpcAsyncOperationWriteBase::~QGrpcAsyncOperationWriteBase()
{
    qProtoDebug() << "Trying ~QGrpcAsyncOperationWriteBase" << this;
    QMutexLocker locker(&m_asyncLock);
    qProtoDebug() << "~QGrpcAsyncOperationWriteBase" << this;
    (void)locker;
}

const QByteArray &QGrpcAsyncOperationWriteBase::getWriteData() const
{
    return m_writeData;
}

QGrpcWriteReplayShared &QGrpcAsyncOperationWriteBase::getReplay()
{
    return m_writeReplay;
}

QGrpcWriteReplayShared QGrpcAsyncOperationWriteBase::writeDone()
{
    if (thread() != QThread::currentThread()) {
        QGrpcWriteReplayShared writeShared;
        QMetaObject::invokeMethod(this, [&]() -> QGrpcWriteReplayShared {
            qProtoDebug() << "Write: called from different thread";
            return writeDone();
        }, Qt::BlockingQueuedConnection, &writeShared);
        return writeShared;
    } else {
        QMutexLocker locker(&m_asyncLock);
        m_writeReplay = QSharedPointer<QGrpcWriteReplay>(new QGrpcWriteReplay(), &QObject::deleteLater);
        emit writeDoneReady();
        return m_writeReplay;
    }
}

QGrpcWriteReplay::WriteStatus QGrpcAsyncOperationWriteBase::writeDoneBlocked()
{
    QGrpcWriteReplayShared reply = writeDone();
    QEventLoop loop;
    QObject::connect(reply.get(), &QGrpcWriteReplay::finished, &loop,
                     &QEventLoop::quit, Qt::QueuedConnection);
    if (reply->status() == QGrpcWriteReplay::WriteStatus::InProcess) {
        loop.exec();
    }
    return reply->status();
}

} // namespace QtProtobuf
