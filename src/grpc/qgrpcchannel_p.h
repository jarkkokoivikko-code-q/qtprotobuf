/*
 * MIT License
 *
 * Copyright (c) 2019 Giulio Girardi <giulio.girardi@protechgroup.it>
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

#include <QEventLoop>
#include <QThread>

#include <QQueue>
#include <grpcpp/channel.h>
#include <grpcpp/impl/codegen/byte_buffer.h>
#include <grpcpp/impl/codegen/client_context.h>
#include <grpcpp/impl/codegen/async_stream.h>

#include <grpcpp/impl/codegen/completion_queue.h>
#include <grpcpp/security/credentials.h>

#include "qgrpccallreply.h"
#include "qgrpcstream.h"
#include "qgrpcstreambidirect.h"
#include "qabstractgrpcclient.h"

namespace QtProtobuf {

struct FunctionCall {
    QObject* m_parent;
    std::function<void(bool)> m_method;

    void callMethod(bool arg) {
        QMetaObject::invokeMethod(m_parent, [this, arg](){m_method(arg);}, Qt::QueuedConnection);
    }
};

//! \private
class QGrpcChannelBaseCall : public QObject {
    Q_OBJECT
public:
    //! \private
    typedef enum ReaderState {
        FIRST_CALL = 0,
        PROCESSING,
        ENDED
    } ReaderState;

    QGrpcStatus m_status;
    QGrpcChannelBaseCall(grpc::Channel *channel, grpc::CompletionQueue *queue,
                         const QString &method, QObject *parent = nullptr) :
        QObject(parent),
        m_readerState(FIRST_CALL),
        m_channel(channel),
        m_queue(queue),
        m_method(method.toLatin1())
    {}

protected:
    ReaderState m_readerState;
    grpc::Status m_grpc_status;

    grpc::ClientContext m_context;
    grpc::ByteBuffer response;

    grpc::Channel *m_channel;
    grpc::CompletionQueue* m_queue;
    QByteArray m_method;
};

//! \private
class QGrpcChannelStream : public QGrpcChannelBaseCall {
    //! \private
    Q_OBJECT;
public:
    QGrpcChannelStream(grpc::Channel *channel, grpc::CompletionQueue* queue, const QString &method,
                       const QByteArray &argument, QObject *parent = nullptr);
    ~QGrpcChannelStream();

    void startReader();
    void cancel();
    void newData(bool ok);
    void finishRead(bool ok);

signals:
    void dataReady(const QByteArray &data);
    void finished();

private:
    QByteArray m_argument;
    grpc::ClientAsyncReader<grpc::ByteBuffer> *m_reader;
    FunctionCall m_finishRead;
    FunctionCall m_newData;
};

struct QGrpcChannelWriteData {
    QByteArray data;
    QGrpcWriteReplayShared replay;
    bool done;
};

class QGrpcChannelStreamBidirect : public QGrpcChannelBaseCall {
    //! \private
    Q_OBJECT
public:
    QGrpcChannelStreamBidirect(grpc::Channel *channel, grpc::CompletionQueue* queue, const QString &method,
                               QObject *parent = nullptr);
    ~QGrpcChannelStreamBidirect();

    void startReader();
    void cancel();
    void writeDone(const QGrpcWriteReplayShared& replay);
    void appendToSend(const QByteArray& data, const QGrpcWriteReplayShared& replay);
    void newData(bool ok);
    void finishRead(bool ok);
    void finishWrite(bool ok);
    void dequeueWrite();

signals:
    void dataReady(const QByteArray &data);
    void finished();

private:
    grpc::ClientAsyncReaderWriter<grpc::ByteBuffer,grpc::ByteBuffer> *m_reader;
    QQueue<QGrpcChannelWriteData> m_sendQueue;
    bool m_inProcess;
    QGrpcWriteReplayShared m_currentWriteReplay;

    FunctionCall m_finishWrite;
    FunctionCall m_finishRead;
    FunctionCall m_newData;
};

//! \private
class QGrpcChannelCall : public QGrpcChannelBaseCall {
    //! \private
    Q_OBJECT
public:
    QByteArray responseParsed;
    QGrpcChannelCall(grpc::Channel *channel, grpc::CompletionQueue* queue, const QString &method,
                     const QByteArray &data, QObject *parent = nullptr);
    ~QGrpcChannelCall();

    void startReader();
    void cancel();
    void newData(bool ok);
    void finishRead(bool ok);

signals:
    void finished();

private:
    QByteArray m_argument;
    grpc::ClientAsyncReader<grpc::ByteBuffer> *m_reader;
    FunctionCall m_newData;
    FunctionCall m_finishRead;
};

//! \private
class QGrpcChannelPrivate: public QObject {
    Q_OBJECT
    //! \private
public:
    QGrpcChannelPrivate(const QUrl &url, std::shared_ptr<grpc::ChannelCredentials> credentials);
    ~QGrpcChannelPrivate();

    void call(const QString &method, const QString &service, const QByteArray &args, QGrpcCallReply *reply);
    QGrpcStatus call(const QString &method, const QString &service, const QByteArray &args, QByteArray &ret);
    void stream(QGrpcStream *stream, const QString &service, QAbstractGrpcClient *client);
    void stream(QGrpcStreamBidirect *stream, const QString &service, QAbstractGrpcClient *client);

signals:
    void finished();

private:
    QThread* m_workThread;
    std::shared_ptr<grpc::Channel> m_channel;
    grpc::CompletionQueue m_queue;
};

};
