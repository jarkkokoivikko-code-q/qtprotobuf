qt_protobuf_internal_add_library(Grpc
    SOURCES
        qgrpcasyncoperationbase.cpp
        qgrpcasyncoperationwritebase.cpp
        qgrpccallreply.cpp
        qgrpcstream.cpp
        qgrpcstreambidirect.cpp
        qgrpcstatus.cpp
        qabstractgrpcchannel.cpp
        qgrpchttp2channel.cpp
        qabstractgrpcclient.cpp
        qgrpccredentials.cpp
        qgrpcsslcredentials.cpp
        qgrpcinsecurecredentials.cpp
        qgrpcuserpasswordcredentials.cpp
        qgrpcwritereplay.cpp
    PUBLIC_HEADER
        qgrpcasyncoperationbase_p.h
        qgrpcasyncoperationwritebase_p.h
        qgrpccallreply.h
        qgrpcstream.h
        qgrpcstreambidirect.h
        qgrpcstatus.h
        qabstractgrpcchannel.h
        qgrpchttp2channel.h
        qabstractgrpcclient.h
        qabstractgrpccredentials.h
        qgrpccredentials.h
        qgrpcsslcredentials.h
        qgrpcinsecurecredentials.h
        qgrpcuserpasswordcredentials.h
        qtgrpcglobal.h
        qgrpcwritereplay.h
    PUBLIC_LIBRARIES
        ${QT_PROTOBUF_NAMESPACE}::Protobuf
        ${QT_VERSIONED_PREFIX}::Core
        ${QT_VERSIONED_PREFIX}::Network
)

if(QT_PROTOBUF_NATIVE_GRPC_CHANNEL)
    qt_protobuf_internal_extend_target(Grpc
        SOURCES
            qgrpcchannel.cpp qgrpcchannel_p.h
        PUBLIC_HEADER
            qgrpcchannel.h
        PUBLIC_DEFINES
            QT_PROTOBUF_NATIVE_GRPC_CHANNEL
        PUBLIC_LIBRARIES
            gRPC::grpc++
            ${CMAKE_THREAD_LIBS_INIT}
    )
endif()

if(NOT BUILD_SHARED_LIBS)
    set(QT_PROTOBUF_EXTRA_CONFIG "staticlib") #extra config for .pri file in case if static build enabled
endif()

qt_protobuf_internal_generate_pri(Grpc)

if(TARGET ${QT_VERSIONED_PREFIX}::Quick)
    add_subdirectory("quick")
endif()
