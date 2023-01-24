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

#include "qgrpcchannel.h"
#include "qgrpcchannel_p.h"

#include <QEventLoop>
#include <QThread>

#include <memory>

#include <grpcpp/channel.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/impl/codegen/byte_buffer.h>
#include <grpcpp/impl/codegen/client_unary_call.h>
#include <grpcpp/impl/codegen/rpc_method.h>
#include <grpcpp/impl/codegen/slice.h>
#include <grpcpp/impl/codegen/status.h>
#include <grpcpp/impl/codegen/sync_stream.h>
#include <grpcpp/security/credentials.h>

#include "qgrpccallreply.h"
#include "qgrpcstatus.h"
#include "qgrpcstream.h"
#include "qgrpcstreambidirect.h"
#include "qabstractgrpcclient.h"
#include "qprotobufserializerregistry_p.h"
#include "qtprotobuflogging.h"

using namespace QtProtobuf;

namespace QtProtobuf {

static inline grpc::Status parseByteBuffer(const grpc::ByteBuffer &buffer, QByteArray &data)
{
    std::vector<grpc::Slice> slices;
    auto status = buffer.Dump(&slices);

    if (!status.ok())
        return status;

    for (const auto& slice : slices) {
        data.append(QByteArray((const char *)slice.begin(), slice.size()));
    }

    return grpc::Status::OK;
}

static inline void parseQByteArray(const QByteArray &bytearray, grpc::ByteBuffer &buffer)
{
    grpc::Slice slice(bytearray.data(), bytearray.size());
    grpc::ByteBuffer tmp(&slice, 1);
    buffer.Swap(&tmp);
}

QGrpcChannelStream::QGrpcChannelStream(grpc::Channel *channel, grpc::CompletionQueue *queue,
                                       const QString &method, const QByteArray &argument, QObject *parent) :
    QGrpcChannelBaseCall(channel, queue, method, parent),
    m_argument(argument)
{
    using std::placeholders::_1;
    m_finishRead = {this, std::bind(&QGrpcChannelStream::finishRead, this, _1)};
    m_newData = {this, std::bind(&QGrpcChannelStream::newData, this, _1)};
}

QGrpcChannelStream::~QGrpcChannelStream()
{
    cancel();
    if (m_reader != nullptr) {
        delete m_reader;
    }
}

void QGrpcChannelStream::startReader()
{
    m_status = QGrpcStatus();
    grpc::ByteBuffer request;
    parseQByteArray(m_argument, request);
    m_reader = grpc::internal::ClientAsyncReaderFactory<grpc::ByteBuffer>::Create(
                m_channel, m_queue,
                grpc::internal::RpcMethod(m_method.data(), grpc::internal::RpcMethod::SERVER_STREAMING),
                &m_context, request, true, &m_newData);
    m_reader->Finish(&m_grpc_status, &m_finishRead);
}

void QGrpcChannelStream::cancel()
{
    // TODO: check thread safety
    qProtoDebug() << "Stream cancelled";
    m_context.TryCancel();
}

void QGrpcChannelStream::newData(bool ok)
{
    if (!ok) {
        qProtoDebug() << "QGrpcChannelStream error tag received!";
        return;
    }
    if (m_readerState == FIRST_CALL) {
        m_reader->Read(&response, &m_newData);
        m_readerState = PROCESSING;
        return;
    }
    if (m_readerState == PROCESSING) {
        QByteArray data;
        m_grpc_status = parseByteBuffer(response, data);
        if (!m_grpc_status.ok()) {
            m_status = QGrpcStatus(static_cast<QGrpcStatus::StatusCode>(m_grpc_status.error_code()),
                                   QString::fromStdString(m_grpc_status.error_message()));
            m_readerState = ENDED;
            // emit finished();
            cancel();
            return;
        }
        emit dataReady(data);
        m_reader->Read(&response, &m_newData);
    }
}

void QGrpcChannelStream::finishRead(bool ok)
{
    qProtoDebug() << "Finish QGrpcChannelStream received status, ok:" << ok
           << "code:" << m_grpc_status.error_code()
           << "message:" << QString::fromStdString(m_grpc_status.error_message())
           << "detail:" << QString::fromStdString(m_grpc_status.error_details());

    // If was parsing error, then grpc_status.ok()==true and status.code()!=OK, we should not fill
    // because this is error on top level
    if (m_status.code() == QGrpcStatus::Ok || !m_grpc_status.ok()) {
        m_status = QGrpcStatus(static_cast<QGrpcStatus::StatusCode>(m_grpc_status.error_code()),
                               QString::fromStdString(m_grpc_status.error_message()));
    }
    emit finished();
}

QGrpcChannelStreamBidirect::QGrpcChannelStreamBidirect(grpc::Channel *channel, grpc::CompletionQueue *queue, const QString &method, QObject *parent) :
    QGrpcChannelBaseCall(channel, queue, method, parent),
    m_inProcess(false)
{
    using std::placeholders::_1;
    m_finishRead = {this, std::bind(&QGrpcChannelStreamBidirect::finishRead, this, _1)};
    m_newData = {this, std::bind(&QGrpcChannelStreamBidirect::newData, this, _1)};
    m_finishWrite = {this, std::bind(&QGrpcChannelStreamBidirect::finishWrite, this, _1)};
}

QGrpcChannelStreamBidirect::~QGrpcChannelStreamBidirect()
{
    cancel();
    if (m_reader != nullptr) {
        delete m_reader;
    }
}

void QGrpcChannelStreamBidirect::startReader()
{
    m_status = QGrpcStatus();
    m_inProcess = true; // Until FIRST_CALL
    m_reader = grpc::internal::ClientAsyncReaderWriterFactory<grpc::ByteBuffer, grpc::ByteBuffer>::Create(
                m_channel, m_queue,
                grpc::internal::RpcMethod(m_method.data(), grpc::internal::RpcMethod::BIDI_STREAMING),
                &m_context, true, &m_newData);
    m_reader->Finish(&m_grpc_status, &m_finishRead);
}

void QGrpcChannelStreamBidirect::cancel()
{
    m_context.TryCancel();
}

void QGrpcChannelStreamBidirect::writeDone(const QGrpcWriteReplayShared& replay)
{
    if (m_inProcess) {
        m_sendQueue.enqueue({QByteArray(), replay, true});
        return;
    }
    m_currentWriteReplay = replay;
    m_reader->WritesDone(&m_finishWrite);
}

void QGrpcChannelStreamBidirect::appendToSend(const QByteArray& data, const QGrpcWriteReplayShared& replay)
{
    if (m_inProcess) {
        m_sendQueue.enqueue({data, replay, false});
        return;
    }
    m_inProcess = true;
    grpc::ByteBuffer dataParsed;
    parseQByteArray(data, dataParsed);
    m_currentWriteReplay = replay;
    m_reader->Write(dataParsed, &m_finishWrite);
}

void QGrpcChannelStreamBidirect::newData(bool ok)
{
    if (!ok) {
        qProtoDebug() << "QGrpcChannelStreamBidirect error tag received!";
        return;
    }
    if (m_readerState == FIRST_CALL) {
        dequeueWrite();
        m_reader->Read(&response, &m_newData);
        m_readerState = PROCESSING;
        return;
    }
    if (m_readerState == PROCESSING) {
        QByteArray data;
        m_grpc_status = parseByteBuffer(response, data);
        if (!m_grpc_status.ok()) {
            m_status = QGrpcStatus(static_cast<QGrpcStatus::StatusCode>(m_grpc_status.error_code()),
                                   QString::fromStdString(m_grpc_status.error_message()));
            m_readerState = ENDED;
            // emit finished();
            cancel();
            return;
        }
        emit dataReady(data);
        m_reader->Read(&response, &m_newData);
    }
}

void QGrpcChannelStreamBidirect::finishRead(bool ok)
{
    qProtoDebug() << "Finish QGrpcChannelStreamBidirect received status, ok:"
                  << ok << "code:" << m_grpc_status.error_code() << "message:"
                  << QString::fromStdString(m_grpc_status.error_message())
                  << "detail:"
                  << QString::fromStdString(m_grpc_status.error_details());

    // If was parsing error, then grpc_status.ok()==true and status.code()!=OK, we
    // should not fill because this is error on top level
    if (m_status.code() == QGrpcStatus::Ok || !m_grpc_status.ok()) {
        m_status = QGrpcStatus(
                    static_cast<QGrpcStatus::StatusCode>(m_grpc_status.error_code()),
                    QString::fromStdString(m_grpc_status.error_message()));
    }

    // Finish all write replay
    if (m_inProcess) {
        m_currentWriteReplay->setStatus(QGrpcWriteReplay::WriteStatus::Failed);
        emit m_currentWriteReplay->finished();
        emit m_currentWriteReplay->error();
    }
    while (!m_sendQueue.empty()) {
        QGrpcChannelWriteData d = m_sendQueue.dequeue();
        d.replay->setStatus(QGrpcWriteReplay::WriteStatus::Failed);
        emit d.replay->finished();
        emit d.replay->error();
    }
    emit finished();
}

void QGrpcChannelStreamBidirect::finishWrite(bool ok)
{
    if (!ok) {
        m_currentWriteReplay->setStatus(QGrpcWriteReplay::WriteStatus::Failed);
        m_currentWriteReplay->emitError();
        m_currentWriteReplay->emitFinished();
    } else {
        m_currentWriteReplay->setStatus(QGrpcWriteReplay::WriteStatus::OK);
        m_currentWriteReplay->emitFinished();
    }
    dequeueWrite();
}

void QGrpcChannelStreamBidirect::dequeueWrite()
{
    m_inProcess = !m_sendQueue.isEmpty();
    if (m_inProcess) {
        QGrpcChannelWriteData d = m_sendQueue.dequeue();
        if (d.done) {
            m_reader->WritesDone(&m_finishWrite);
        } else {
            grpc::ByteBuffer data;
            parseQByteArray(d.data, data);
            m_currentWriteReplay = d.replay;
            m_reader->Write(data, &m_finishWrite);
        }
    }
}

QGrpcChannelCall::QGrpcChannelCall(grpc::Channel *channel, grpc::CompletionQueue* queue, const QString &method,
                                   const QByteArray &argument, QObject *parent) :
    QGrpcChannelBaseCall(channel, queue, method, parent),
    m_argument(argument)
{
    using std::placeholders::_1;
    m_finishRead = {this, std::bind(&QGrpcChannelCall::finishRead, this, _1)};
    m_newData = {this, std::bind(&QGrpcChannelCall::newData, this, _1)};
}

QGrpcChannelCall::~QGrpcChannelCall()
{
    cancel();
    // TODO wait while Finished will called?
    if (m_reader != nullptr) {
        delete m_reader;
    }
}

void QGrpcChannelCall::startReader()
{
    m_status = QGrpcStatus();
    grpc::ByteBuffer request;
    parseQByteArray(m_argument, request);
    grpc::internal::RpcMethod method_(m_method.data(),
                                      grpc::internal::RpcMethod::NORMAL_RPC);
    m_reader = grpc::internal::ClientAsyncReaderFactory<grpc::ByteBuffer>::Create(
                m_channel, m_queue, method_, &m_context, request, true, &m_newData);
    m_reader->Finish(&m_grpc_status, &m_finishRead);
}

void QGrpcChannelCall::cancel()
{
    // TODO: check thread safety
    qProtoDebug() << "Call cancelled";
    m_context.TryCancel();
}

void QGrpcChannelCall::newData(bool ok)
{
    if (!ok) {
        // this->status=QGrpcStatus(QGrpcStatus::Aborted,"Some error happens");
        // emit finished();
        qProtoDebug() << "QGrpcChannelCall error tag received!";
        return;
    }
    if (m_readerState == FIRST_CALL) {
        // read next value
        m_reader->Read(&response, &m_newData);
        m_readerState = PROCESSING;
        return;
    }
    if (m_readerState == PROCESSING) {
        m_grpc_status = parseByteBuffer(response, responseParsed);
        m_status = QGrpcStatus(static_cast<QGrpcStatus::StatusCode>(m_grpc_status.error_code()),
                               QString::fromStdString(m_grpc_status.error_message()));
        m_readerState = ENDED;
        // cancel();
        // emit finished();
    }
}

void QGrpcChannelCall::finishRead(bool ok)
{
    qProtoDebug() << "Finish QGrpcChannelCall received status, ok:" << ok
           << "code:" << m_grpc_status.error_code()
           << "message:" << QString::fromStdString(m_grpc_status.error_message())
           << "detail:" << QString::fromStdString(m_grpc_status.error_details());

    // If was parsing error, then grpc_status.ok()==true and status.code()!=OK, we should not fill
    // because this is error on top level
    if (m_status.code() == QGrpcStatus::Ok || !m_grpc_status.ok()) {
        m_status = QGrpcStatus(static_cast<QGrpcStatus::StatusCode>(m_grpc_status.error_code()),
                               QString::fromStdString(m_grpc_status.error_message()));
    }
    emit finished();
}

QGrpcChannelPrivate::QGrpcChannelPrivate(const QUrl &url, std::shared_ptr<grpc::ChannelCredentials> credentials) :
    QObject(nullptr)
{
    m_channel = grpc::CreateChannel(url.toString().toStdString(), credentials);
    m_workThread = QThread::create([this]() {
        void *tag = nullptr;
        bool ok = true;
        while (m_queue.Next(&tag, &ok)) {
            if (tag == nullptr) {
                qProtoDebug() << "GrpcChannel received null tag!";
                continue;
            }
            FunctionCall *f = reinterpret_cast<FunctionCall *>(tag);
            f->callMethod(ok);
        }
        qProtoDebug() << "Exit form worker thread";
    });
    // Channel finished;
    connect(m_workThread, &QThread::finished, this, &QGrpcChannelPrivate::finished);
    m_workThread->start();
}

QGrpcChannelPrivate::~QGrpcChannelPrivate()
{
    m_queue.Shutdown();
    m_workThread->wait();
    m_workThread->deleteLater();
}

void QGrpcChannelPrivate::call(const QString &method, const QString &service, const QByteArray &args, QGrpcCallReply *reply)
{
    QString rpcName = u"/%1/%2"_qs.arg(service, method);

    std::shared_ptr<QGrpcChannelCall> call;
    std::shared_ptr<QMetaObject::Connection> connection(new QMetaObject::Connection);
    std::shared_ptr<QMetaObject::Connection> abortConnection(new QMetaObject::Connection);
    std::shared_ptr<QMetaObject::Connection> clientConnection(new QMetaObject::Connection);

    call.reset(new QGrpcChannelCall(m_channel.get(), &m_queue, rpcName, args, reply),
               [](QGrpcChannelCall *c) { c->deleteLater(); });

    *clientConnection = QObject::connect(
                this, &QGrpcChannelPrivate::finished, reply,
                [this, clientConnection, call, reply, connection, abortConnection]() {
        qProtoDebug() << "Call gRPC chanel was destroyed";
        reply->setData({});
        reply->emitError(QGrpcStatus(QGrpcStatus::Aborted, "GRPC channel aborted"));
        reply->emitFinished();

        QObject::disconnect(*connection);
        QObject::disconnect(*abortConnection);
        QObject::disconnect(*clientConnection);
    });

    *connection = QObject::connect(
                call.get(), &QGrpcChannelCall::finished, reply,
                [this, clientConnection, call, reply, connection, abortConnection]() {
        if (call->m_status.code() == QGrpcStatus::Ok) {
            reply->setData(call->responseParsed);
            reply->emitFinished();
        } else {
            reply->setData({});
            reply->emitError(call->m_status);
        }
        qProtoDebug() << "Call gRPC was Finished";
        QObject::disconnect(*connection);
        QObject::disconnect(*abortConnection);
        QObject::disconnect(*clientConnection);
    });

    *abortConnection = QObject::connect(
                reply, &QGrpcCallReply::error, call.get(),
                [this, clientConnection, call, connection, abortConnection](const QGrpcStatus &status) {
        if (status.code() == QGrpcStatus::Aborted) {
            qProtoDebug() << "Call gRPC was Aborted";
            QObject::disconnect(*connection);
            QObject::disconnect(*abortConnection);
            QObject::disconnect(*clientConnection);
        }
    });

    call->startReader();
}

QGrpcStatus QGrpcChannelPrivate::call(const QString &method, const QString &service, const QByteArray &args, QByteArray &ret)
{
    QEventLoop loop;

    QString rpcName = u"/%1/%2"_qs.arg(service, method);
    QGrpcChannelCall call(m_channel.get(), &m_queue, rpcName, args);

    // TODO if connection aborted, then data should not filled
    QObject::connect(this, &QGrpcChannelPrivate::finished, &loop, &QEventLoop::quit);
    QObject::connect(&call, &QGrpcChannelCall::finished, &loop, &QEventLoop::quit);

    call.startReader();

    loop.exec();

    // I think not good way, it is should not success if worker thread stopped
    if (m_workThread->isFinished()) {
        call.m_status = QGrpcStatus(QGrpcStatus::Aborted, u"Connection aborted"_qs);
    }

    ret = call.responseParsed;
    return call.m_status;
}

void QGrpcChannelPrivate::stream(QGrpcStream *stream, const QString &service, QAbstractGrpcClient *client)
{
    Q_ASSERT(stream != nullptr);

    QString rpcName = u"/%1/%2"_qs.arg(service,stream->method());

    std::shared_ptr<QGrpcChannelStream> sub;
    std::shared_ptr<QMetaObject::Connection> abortConnection(new QMetaObject::Connection);
    std::shared_ptr<QMetaObject::Connection> readConnection(new QMetaObject::Connection);
    std::shared_ptr<QMetaObject::Connection> clientConnection(new QMetaObject::Connection);
    std::shared_ptr<QMetaObject::Connection> connection(new QMetaObject::Connection);
    std::shared_ptr<QMetaObject::Connection> channelFinished(new QMetaObject::Connection);

    sub.reset(new QGrpcChannelStream(m_channel.get(), &m_queue, rpcName, stream->arg(), stream),
              [](QGrpcChannelStream *sub) { sub->deleteLater(); });

    *readConnection = QObject::connect(sub.get(), &QGrpcChannelStream::dataReady, stream,
                                       [stream](const QByteArray &data) {stream->handler(data);});

    *connection = QObject::connect(
                sub.get(), &QGrpcChannelStream::finished, stream,
                [this, channelFinished, sub, stream, readConnection, abortConnection, service,
                connection, clientConnection]() {
        qProtoDebug() << "QGrpcStream ended with server closing connection";

        QObject::disconnect(*channelFinished);
        QObject::disconnect(*connection);
        QObject::disconnect(*readConnection);
        QObject::disconnect(*abortConnection);
        QObject::disconnect(*clientConnection);

        if (sub->m_status.code() != QGrpcStatus::Ok) {
            stream->emitError(sub->m_status);
        }
    });

    *abortConnection = QObject::connect(
                stream, &QGrpcStream::finished, sub.get(),
                [this, channelFinished, connection, abortConnection, readConnection, sub,
                clientConnection]() {
        qProtoDebug() << "Stream client was finished";

        QObject::disconnect(*channelFinished);
        QObject::disconnect(*connection);
        QObject::disconnect(*readConnection);
        QObject::disconnect(*abortConnection);
        QObject::disconnect(*clientConnection);

        sub->cancel();
    });

    *clientConnection = QObject::connect(
                client, &QAbstractGrpcClient::destroyed, sub.get(),
                [this, channelFinished, readConnection, connection, abortConnection, sub,
                clientConnection]() {
        qProtoDebug() << "QGrpcStream client was destroyed";

        QObject::disconnect(*channelFinished);
        QObject::disconnect(*connection);
        QObject::disconnect(*readConnection);
        QObject::disconnect(*abortConnection);
        QObject::disconnect(*clientConnection);

        sub->cancel();
    });

    *channelFinished = QObject::connect(
                this, &QGrpcChannelPrivate::finished, sub.get(),
                [this, channelFinished, readConnection, connection, abortConnection, sub,
                clientConnection]() {
        qProtoDebug() << "QGrpcStream chanel was destroyed";

        QObject::disconnect(*channelFinished);
        QObject::disconnect(*connection);
        QObject::disconnect(*readConnection);
        QObject::disconnect(*abortConnection);
        QObject::disconnect(*clientConnection);

        sub->cancel();
    });

    sub->startReader();
}

void QGrpcChannelPrivate::stream(QGrpcStreamBidirect *stream, const QString &service, QAbstractGrpcClient *client)
{
    Q_ASSERT(stream != nullptr);

    QString rpcName = u"/%1/%2"_qs.arg(service,stream->method());

    std::shared_ptr<QGrpcChannelStreamBidirect> sub;
    std::shared_ptr<QMetaObject::Connection> abortConnection(new QMetaObject::Connection);
    std::shared_ptr<QMetaObject::Connection> readConnection(new QMetaObject::Connection);
    std::shared_ptr<QMetaObject::Connection> writeConnection(new QMetaObject::Connection);
    std::shared_ptr<QMetaObject::Connection> writeDoneConnection(new QMetaObject::Connection);
    std::shared_ptr<QMetaObject::Connection> clientConnection(new QMetaObject::Connection);
    std::shared_ptr<QMetaObject::Connection> connection(new QMetaObject::Connection);    
    std::shared_ptr<QMetaObject::Connection> channelFinished(new QMetaObject::Connection);

    sub.reset(new QGrpcChannelStreamBidirect(m_channel.get(), &m_queue, rpcName, stream),
              [](QGrpcChannelStreamBidirect *sub) { sub->deleteLater(); });

    *readConnection = QObject::connect(
                sub.get(), &QGrpcChannelStreamBidirect::dataReady, stream,
                [stream](const QByteArray &data) { stream->handler(data); });

    *writeConnection = QObject::connect(
                stream, &QGrpcStreamBidirect::writeReady, this,
                [stream, sub]() {
        QGrpcWriteReplayShared &reply = stream->getReplay();
        reply->setStatus(QGrpcWriteReplay::WriteStatus::InProcess);
        sub->appendToSend(stream->getWriteData(), reply);
    }, Qt::DirectConnection);

    *writeDoneConnection = QObject::connect(
                stream, &QGrpcStreamBidirect::writeDoneReady, this,
                [stream, sub]() {
        QGrpcWriteReplayShared &reply = stream->getReplay();
        reply->setStatus(QGrpcWriteReplay::WriteStatus::InProcess);
        sub->writeDone(reply);
    }, Qt::DirectConnection);

    *connection = QObject::connect(
                sub.get(), &QGrpcChannelStreamBidirect::finished, stream,
                [this, channelFinished, sub, stream, readConnection, writeConnection,
                writeDoneConnection, abortConnection, service, connection,
                clientConnection]() {
        qProtoDebug() << "QGrpcStreamBidirect ended with server closing connection";

        QObject::disconnect(*channelFinished);
        QObject::disconnect(*connection);
        QObject::disconnect(*readConnection);
        QObject::disconnect(*abortConnection);
        QObject::disconnect(*clientConnection);
        QObject::disconnect(*writeConnection);
        QObject::disconnect(*writeDoneConnection);

        if (sub->m_status.code() != QGrpcStatus::Ok) {
            stream->emitError(sub->m_status);
        }
    });

    *abortConnection = QObject::connect(
                stream, &QGrpcStreamBidirect::finished, sub.get(),
                [this, channelFinished, connection, abortConnection, readConnection,
                writeConnection, writeDoneConnection, sub, clientConnection]() {
        qProtoDebug() << "QGrpcStreamBidirect client was finished";

        QObject::disconnect(*channelFinished);
        QObject::disconnect(*connection);
        QObject::disconnect(*readConnection);
        QObject::disconnect(*abortConnection);
        QObject::disconnect(*clientConnection);
        QObject::disconnect(*writeConnection);
        QObject::disconnect(*writeDoneConnection);

        sub->cancel();
    });

    *clientConnection = QObject::connect(
                client, &QAbstractGrpcClient::destroyed, sub.get(),
                [this, channelFinished, readConnection, writeConnection, writeDoneConnection,
                connection, abortConnection, sub, clientConnection]() {
        qProtoDebug() << "QGrpcStreamBidirect client was destroyed";

        QObject::disconnect(*channelFinished);
        QObject::disconnect(*connection);
        QObject::disconnect(*readConnection);
        QObject::disconnect(*abortConnection);
        QObject::disconnect(*clientConnection);
        QObject::disconnect(*writeConnection);
        QObject::disconnect(*writeDoneConnection);

        sub->cancel();
    });

    *channelFinished = QObject::connect(
                this, &QGrpcChannelPrivate::finished, sub.get(),
                [this, channelFinished, readConnection, writeConnection, writeDoneConnection,
                connection, abortConnection, sub, clientConnection]() {
        qProtoDebug() << "QGrpcStreamBidirect chanel was destroyed";

        QObject::disconnect(*channelFinished);
        QObject::disconnect(*connection);
        QObject::disconnect(*readConnection);
        QObject::disconnect(*abortConnection);
        QObject::disconnect(*clientConnection);
        QObject::disconnect(*writeConnection);
        QObject::disconnect(*writeDoneConnection);

        sub->cancel();
    });

    sub->startReader();
}

QGrpcChannel::QGrpcChannel(const QUrl &url, std::shared_ptr<grpc::ChannelCredentials> credentials) :
    QAbstractGrpcChannel(),
    dPtr(std::make_unique<QGrpcChannelPrivate>(url, credentials))
{

}

QGrpcChannel::~QGrpcChannel()
{

}

QGrpcStatus QGrpcChannel::call(const QString &method, const QString &service, const QByteArray &args, QByteArray &ret)
{
    return dPtr->call(method, service, args, ret);
}

void QGrpcChannel::call(const QString &method, const QString &service, const QByteArray &args, QGrpcCallReply *reply)
{
    dPtr->call(method, service, args, reply);
}

void QGrpcChannel::stream(QGrpcStream *stream, const QString &service, QAbstractGrpcClient *client)
{
    dPtr->stream(stream, service, client);
}

void QGrpcChannel::stream(QGrpcStreamBidirect *stream, const QString &service, QAbstractGrpcClient *client)
{
    dPtr->stream(stream, service, client);
}

std::shared_ptr<QAbstractProtobufSerializer> QGrpcChannel::serializer() const
{
    // TODO: make selection based on credentials or channel settings
    return QProtobufSerializerRegistry::instance().getSerializer(u"protobuf"_qs);
}

}
