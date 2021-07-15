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

#pragma once

#include <QObject>
#include <QMutex>
#include <QEventLoop>
#include <QMetaObject>
#include <QThread>

#include <functional>
#include <memory>

#include "qabstractgrpcclient.h"
#include "qgrpcasyncoperationbase_p.h"
#include "qgrpcwritereplay.h"

#include "qtgrpcglobal.h"


namespace QtProtobuf {
/*!
 * \ingroup QtGrpc
 * \private
 * \brief The QGrpcAsyncOperationWriteBase class implements stream logic
 */
class Q_GRPC_EXPORT QGrpcAsyncOperationWriteBase : public QGrpcAsyncOperationBase
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(QGrpcAsyncOperationWriteBase)

protected:
    //! \private
    QGrpcAsyncOperationWriteBase(QAbstractGrpcClient *parent);
    //! \private
    virtual ~QGrpcAsyncOperationWriteBase();

    /*!
     * \return Returns the returns the raw buffer to be written to the stream
     */
    const QByteArray &getWriteData() const;

    /*!
     * \return Returns QGrpcWriteReplayShared which will be available for the next
     * asynchronous write call
     */
    QGrpcWriteReplayShared &getReplay();

public:
    /*!
     * \brief Write message from value to channel.
     * \return QGrpcWriteReplayShared with the status of the operation, and signals recording status
     * \details This function writes data to the raw byte array from the value, and emmit the writeReady signal,
     *          in blocking mode, the QAbstractGrpcChannel implementation, must pick up the data by calling getWriteData()
     */
    template <typename T>
    QGrpcWriteReplayShared write(T value) {
        if (thread() != QThread::currentThread()) {
            QGrpcWriteReplayShared writeShared;
            QMetaObject::invokeMethod(this, [&]() -> QGrpcWriteReplayShared {
                qProtoDebug() << "Write: called from different thread";
                return write(value);
            }, Qt::BlockingQueuedConnection, &writeShared);
            return writeShared;
        } else {
            QMutexLocker locker(&m_asyncLock);
            try {
                m_writeReplay = QSharedPointer<QGrpcWriteReplay>(new QGrpcWriteReplay(), &QObject::deleteLater);
                m_writeData = value.serialize(static_cast<QAbstractGrpcClient *>(parent())->serializer().get());
                emit writeReady();
            } catch (std::invalid_argument &) {
                m_writeReplay->setStatus(QGrpcWriteReplay::WriteStatus::Failed);
                emit error({QGrpcStatus::InvalidArgument, u"Response deserialization failed invalid field found"_qs});
            } catch (std::out_of_range &) {
                m_writeReplay->setStatus(QGrpcWriteReplay::WriteStatus::Failed);
                emit error({QGrpcStatus::OutOfRange, u"Invalid size of received buffer"_qs});
            } catch (...) {
                m_writeReplay->setStatus(QGrpcWriteReplay::WriteStatus::Failed);
                emit error({QGrpcStatus::Internal, u"Unknown exception caught during deserialization"_qs});
            }
            if (m_writeReplay->status() == QGrpcWriteReplay::WriteStatus::Failed ||
                    m_writeReplay->status() == QGrpcWriteReplay::WriteStatus::NotConnected) {
                m_writeReplay->emitError();
                m_writeReplay->emitFinished();
            }
            return m_writeReplay;
        }
    }

    /*!
     * \brief Blocking write message from value to channel. Call write method and wait over QEventLoop when async operation finished
     * \return Returns the status of a write operation to a channel.
     */
    template <typename T>
    QGrpcWriteReplay::WriteStatus writeBlocked(T value) {
        QGrpcWriteReplayShared reply = write(value);
        QEventLoop loop;
        QObject::connect(reply.get(), &QGrpcWriteReplay::finished, &loop, &QEventLoop::quit, Qt::QueuedConnection);
        if (reply->status() == QGrpcWriteReplay::WriteStatus::InProcess) {
            loop.exec();
        }
        return reply->status();
    }

    /*!
     * \brief Sends a message about the completion of write to the channel. The function is called after the last write operation.
     * \return QGrpcWriteReplayShared with the status of the operation, and signals handle status.
     * \details This function emmit the writeDoneReady signal, the QAbstractGrpcChannel implementation, should process the signal
     *          blocking mode and complete the stream.
     */
    QGrpcWriteReplayShared writeDone();

    /*!
     * \brief Blocking sends a message about the completion of write to the channel. Call writeDone method and wait over QEventLoop when
     *        async operation finished.
     * \return Returns the status of a operation to a channel.
     */
    QGrpcWriteReplay::WriteStatus writeDoneBlocked();

signals:
    // Connect over blocking queue in QAbstractGrpcClient, to copy data in process thread
    void writeReady();
    void writeDoneReady();

private:
    QGrpcAsyncOperationWriteBase();
    QByteArray m_writeData;
    QGrpcWriteReplayShared m_writeReplay;

    friend class QAbstractGrpcClient;
    friend class QGrpcChannelPrivate;
};

}
