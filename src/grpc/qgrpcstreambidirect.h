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

#pragma once //QGrpcStreamBidirect

#include <functional>
#include <QMutex>
#include <memory>

#include "qabstractgrpcclient.h"
#include "qgrpcasyncoperationwritebase_p.h"

#include "qtgrpcglobal.h"

namespace QtProtobuf {

class QAbstractGrpcClient;

/*!
 * \ingroup QtGrpc
 * \brief The QGrpcStreamBidirect class
 */
class Q_GRPC_EXPORT QGrpcStreamBidirect final : public QGrpcAsyncOperationWriteBase
{
    Q_OBJECT
public:
    /*!
     * \brief Cancels this stream and try to abort call in channel
     */
    void abort() override;

    /*!
     * \brief Returns method for this stream
     */
    QString method() const {
        return m_method;
    }

    /*!
     * \brief Invokes handler method assigned to this stream.
     * \param data updated stream data buffer
     * \details Should be used by QAbstractGrpcChannel implementations,
     *          to update data in stream and notify clients about stream updates.
     */
    void handler(const QByteArray& data) {
        setData(data);
        for (auto handler : m_handlers) {
            handler(data);
        }
        messageReceived();
    }

signals:
    /*!
     * \brief The signal is emitted when stream received updated value from server
     */
    void messageReceived();

protected:
    //! \private
    QGrpcStreamBidirect(const QString &method, const QByteArray &arg,
                        const StreamHandler &handler, QAbstractGrpcClient *parent);
    //! \private
    virtual ~QGrpcStreamBidirect() = default;

    //! \private
    void addHandler(const StreamHandler &handler);

    bool operator ==(const QGrpcStreamBidirect &other) const {
        return other.method() == this->method();
    }

private:
    friend class QAbstractGrpcClient;
    QString m_method;
    std::vector<StreamHandler> m_handlers;
};

}
