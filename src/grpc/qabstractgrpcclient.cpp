/*
 * MIT License
 *
 * Copyright (c) 2019 Alexey Edelev <semlanik@gmail.com>
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

#include "qabstractgrpcclient.h"

#include "qgrpccallreply.h"
#include "qgrpcstream.h"
#include "qgrpcstreambidirect.h"

#include <QTimer>
#include <QThread>

namespace QtProtobuf {

//! \private
class QAbstractGrpcClientPrivate final {
public:
    QAbstractGrpcClientPrivate(const QString &service) : service(service) {}

    std::shared_ptr<QAbstractGrpcChannel> channel;
    const QString service;
    std::vector<QGrpcStreamShared> activeStreams;
    std::vector<QGrpcStreamBidirectShared> activeStreamsBidirect;
};
}

using namespace QtProtobuf;

QAbstractGrpcClient::QAbstractGrpcClient(const QString &service, QObject *parent) : QObject(parent)
  , dPtr(std::make_unique<QAbstractGrpcClientPrivate>(service))
{
}

QAbstractGrpcClient::~QAbstractGrpcClient()
{}

void QAbstractGrpcClient::attachChannel(const std::shared_ptr<QAbstractGrpcChannel> &channel)
{
    if (channel->thread() != QThread::currentThread()) {
        qProtoCritical() << "QAbstractGrpcClient::attachChannel is called from different thread.\n"
                           "QtGrpc doesn't guarantie thread safety on channel level.\n"
                           "You have to be confident that channel routines are working in the same thread as QAbstractGrpcClient";
        throw std::runtime_error("Call from another thread");
    }
    for (auto stream : dPtr->activeStreams) {
        stream->abort();
    }
    dPtr->channel = channel;
    for (auto stream : dPtr->activeStreams) {
        stream->abort();
    }
}

QGrpcStatus QAbstractGrpcClient::call(const QString &method, const QByteArray &arg, QByteArray &ret)
{
    QGrpcStatus callStatus{QGrpcStatus::Unknown};
    if (thread() != QThread::currentThread()) {
        QMetaObject::invokeMethod(this, [&]()->QGrpcStatus {
                                                qProtoDebug() << "Method: " << dPtr->service << method << " called from different thread";
                                                return call(method, arg, ret);
                                            }, Qt::BlockingQueuedConnection, &callStatus);
        return callStatus;
    }

    if (dPtr->channel) {
        callStatus = dPtr->channel->call(method, dPtr->service, arg, ret);
    } else {
        callStatus = QGrpcStatus{QGrpcStatus::Unknown, u"No channel(s) attached."_qs};
    }

    if (callStatus != QGrpcStatus::Ok) {
        emit error(callStatus);
    }

    return callStatus;
}

QGrpcCallReplyShared QAbstractGrpcClient::call(const QString &method, const QByteArray &arg)
{
    QGrpcCallReplyShared reply;
    if (thread() != QThread::currentThread()) {
        QMetaObject::invokeMethod(this, [&]()->QGrpcCallReplyShared {
                                      qProtoDebug() << "Method: " << dPtr->service << method << " called from different thread";
                                      return call(method, arg);
                                  }, Qt::BlockingQueuedConnection, &reply);
    } else if (dPtr->channel) {
        reply.reset(new QGrpcCallReply(this), [](QGrpcCallReply *reply) { delete reply; });

        auto errorConnection = std::make_shared<QMetaObject::Connection>();
        auto finishedConnection = std::make_shared<QMetaObject::Connection>();
        *errorConnection = connect(reply.get(), &QGrpcCallReply::error, this, [this, reply, errorConnection, finishedConnection](const QGrpcStatus &status) mutable {
            emit error(status);
            QObject::disconnect(*finishedConnection);
            QObject::disconnect(*errorConnection);
            reply.reset();
        });

        *finishedConnection = connect(reply.get(), &QGrpcCallReply::finished, [this, reply, errorConnection, finishedConnection]() mutable {
            QObject::disconnect(*finishedConnection);
            QObject::disconnect(*errorConnection);
            reply.reset();
        });

        dPtr->channel->call(method, dPtr->service, arg, reply.get());
    } else {
        emit error({QGrpcStatus::Unknown, u"No channel(s) attached."_qs});
    }

    return reply;
}

QGrpcStreamBidirectShared QAbstractGrpcClient::streamBidirect(const QString &method, const QByteArray &arg, const QtProtobuf::StreamHandler &handler)
{
    QGrpcStreamBidirectShared grpcStream;

    if (thread() != QThread::currentThread()) {
        QMetaObject::invokeMethod(this, [&]()->QGrpcStreamBidirectShared {
                                      qProtoDebug() << "Stream: " << dPtr->service << method << " called from different thread";
                                      return streamBidirect(method, arg, handler);
                                  }, Qt::BlockingQueuedConnection, &grpcStream);
    } else if (dPtr->channel) {
        grpcStream.reset(new QGrpcStreamBidirect(method, arg, handler, this), [](QGrpcStreamBidirect *stream) { delete stream; });

        auto it = std::find_if(std::begin(dPtr->activeStreamsBidirect), std::end(dPtr->activeStreamsBidirect), [grpcStream](const QGrpcStreamBidirectShared &activeStreamBidirect) {
           return *activeStreamBidirect == *grpcStream;
        });

        if (it != std::end(dPtr->activeStreamsBidirect)) {
            (*it)->addHandler(handler);
            return *it; //If stream already exists return it for handling
        }

        auto errorConnection = std::make_shared<QMetaObject::Connection>();
        *errorConnection = connect(grpcStream.get(), &QGrpcStreamBidirect::error, this, [this, grpcStream](const QGrpcStatus &status) {
            qProtoWarning() << grpcStream->method() << "call" << dPtr->service << "stream error: " << status.message();
            emit error(status);
            std::weak_ptr<QGrpcStreamBidirect> weakStream = grpcStream;
            //TODO: Make timeout configurable from channel settings
            QTimer::singleShot(1000, this, [this, weakStream, method = grpcStream->method()] {
                auto stream = weakStream.lock();
                if (stream) {
                    dPtr->channel->stream(stream.get(), dPtr->service, this);
                } else {
                    qProtoDebug() << "Stream for " << dPtr->service << "method" << method << " will not be restored by timeout.";
                }
            });
        });

        auto finishedConnection = std::make_shared<QMetaObject::Connection>();
        *finishedConnection = connect(grpcStream.get(), &QGrpcStreamBidirect::finished, this, [this, grpcStream, errorConnection, finishedConnection]() mutable {
            qProtoWarning() << grpcStream->method() << "call" << dPtr->service << "stream finished";
            auto it = std::find_if(std::begin(dPtr->activeStreamsBidirect), std::end(dPtr->activeStreamsBidirect), [grpcStream](QGrpcStreamBidirectShared activeStream) {
               return *activeStream == *grpcStream;
            });

            if (it != std::end(dPtr->activeStreamsBidirect)) {
                dPtr->activeStreamsBidirect.erase(it);
            }
            QObject::disconnect(*errorConnection);
            QObject::disconnect(*finishedConnection);
            grpcStream.reset();
        });

        dPtr->channel->stream(grpcStream.get(), dPtr->service, this);
        dPtr->activeStreamsBidirect.push_back(grpcStream);
    } else {
        emit error({QGrpcStatus::Unknown, u"No channel(s) attached."_qs});
    }
    return grpcStream;
}

QGrpcStreamShared QAbstractGrpcClient::stream(const QString &method, const QByteArray &arg, const QtProtobuf::StreamHandler &handler)
{
    QGrpcStreamShared grpcStream;

    if (thread() != QThread::currentThread()) {
        QMetaObject::invokeMethod(this, [&]()->QGrpcStreamShared {
                                      qProtoDebug() << "Stream: " << dPtr->service << method << " called from different thread";
                                      return stream(method, arg, handler);
                                  }, Qt::BlockingQueuedConnection, &grpcStream);
    } else if (dPtr->channel) {
        grpcStream.reset(new QGrpcStream(method, arg, handler, this), [](QGrpcStream *stream) { delete stream; });

        auto it = std::find_if(std::begin(dPtr->activeStreams), std::end(dPtr->activeStreams), [grpcStream](const QGrpcStreamShared &activeStream) {
           return *activeStream == *grpcStream;
        });

        if (it != std::end(dPtr->activeStreams)) {
            (*it)->addHandler(handler);
            return *it; //If stream already exists return it for handling
        }

        auto errorConnection = std::make_shared<QMetaObject::Connection>();
        *errorConnection = connect(grpcStream.get(), &QGrpcStream::error, this, [this, grpcStream](const QGrpcStatus &status) {
            qProtoWarning() << grpcStream->method() << "call" << dPtr->service << "stream error: " << status.message();
            emit error(status);
            std::weak_ptr<QGrpcStream> weakStream = grpcStream;
            //TODO: Make timeout configurable from channel settings
            QTimer::singleShot(1000, this, [this, weakStream, method = grpcStream->method()] {
                auto stream = weakStream.lock();
                if (stream) {
                    dPtr->channel->stream(stream.get(), dPtr->service, this);
                } else {
                    qProtoDebug() << "Stream for " << dPtr->service << "method" << method << " will not be restored by timeout.";
                }
            });
        });

        auto finishedConnection = std::make_shared<QMetaObject::Connection>();
        *finishedConnection = connect(grpcStream.get(), &QGrpcStream::finished, this, [this, grpcStream, errorConnection, finishedConnection]() mutable {
            qProtoWarning() << grpcStream->method() << "call" << dPtr->service << "stream finished";
            auto it = std::find_if(std::begin(dPtr->activeStreams), std::end(dPtr->activeStreams), [grpcStream](QGrpcStreamShared activeStream) {
               return *activeStream == *grpcStream;
            });

            if (it != std::end(dPtr->activeStreams)) {
                dPtr->activeStreams.erase(it);
            }
            QObject::disconnect(*errorConnection);
            QObject::disconnect(*finishedConnection);
            grpcStream.reset();
        });

        dPtr->channel->stream(grpcStream.get(), dPtr->service, this);
        dPtr->activeStreams.push_back(grpcStream);
    } else {
        emit error({QGrpcStatus::Unknown, u"No channel(s) attached."_qs});
    }
    return grpcStream;
}

std::shared_ptr<QAbstractProtobufSerializer> QAbstractGrpcClient::serializer() const
{
    if (dPtr->channel == nullptr || dPtr->channel->serializer() == nullptr) {
        return nullptr;
    }
    return dPtr->channel->serializer();
}
