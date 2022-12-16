cmake_minimum_required(VERSION 3.16...3.22)

set(QT_PROTOBUF_VERSION 0.6.0)
set(QT_PROTOBUF_NAMESPACE QtProtobuf)
set(QT_PREFIX Qt)
set(QT_VERSIONED_PREFIX Qt6 CACHE STRING "" FORCE)

project(${QT_PROTOBUF_NAMESPACE} VERSION ${QT_PROTOBUF_VERSION} LANGUAGES CXX)

# Conan support, optional
if(EXISTS "${CMAKE_BINARY_DIR}/conanbuildinfo.cmake")
    include("${CMAKE_BINARY_DIR}/conanbuildinfo.cmake")
    conan_basic_setup()
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(QT_PROTOBUF_CMAKE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(QT_PROTOBUF_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(QT_PROTOBUF_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")

list(PREPEND CMAKE_MODULE_PATH ${QT_PROTOBUF_CMAKE_DIR})

if(NOT CMAKE_CROSSCOMPILING)
    set(Protobuf_USE_STATIC_LIBS ON)
    find_package(Protobuf COMPONENTS Protoc OPTIONAL_COMPONENTS Generator Protobuf REQUIRED)
else()
    if(NOT QT_PROTOBUF_HOST_PATH)
        message(FATAL_ERROR "QT_PROTOBUF_HOST_PATH is not specified. You shall specify
QT_PROTOBUF_HOST_PATH to find the host version of QtProtobufGenerator")
    endif()
    find_package(${QT_PROTOBUF_NAMESPACE} COMPONENTS ProtobufGenerator CONFIG REQUIRED
        PATHS ${QT_PROTOBUF_HOST_PATH}
        NO_DEFAULT_PATH
    )
endif()
find_package(gRPC CONFIG REQUIRED)
find_package(Threads)
find_package(${QT_VERSIONED_PREFIX} COMPONENTS Core Qml CONFIG REQUIRED)
find_package(${QT_VERSIONED_PREFIX} OPTIONAL_COMPONENTS Network Quick Gui CONFIG)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(QtProtobufInternalHelpers)
include(QtProtobufCoverage)
include(QtProtobufGen)

qt_protobuf_internal_check_install()

set(PROTOBUF_QUICK_PLUGIN_NAME protobufquickplugin)
set(GRPC_QUICK_PLUGIN_NAME grpcquickplugin)

set(QT_PROTOBUF_MAKE_TESTS OFF CACHE BOOL "Enable QtProtobuf tests build")
set(QT_PROTOBUF_STANDALONE_TESTS OFF CACHE BOOL "Enable QtProtobuf tests build")
set(QT_PROTOBUF_MAKE_EXAMPLES OFF CACHE BOOL "Enable QtProtobuf examples build")
set(QT_PROTOBUF_MAKE_COVERAGE OFF CACHE BOOL "Enable QtProtobuf build for profiler (gcov)")
set(QT_PROTOBUF_FIELD_ENUM OFF CACHE BOOL "Enable generation of enumeration with fields numbers for well-known and Qt types libraries")
set(QT_PROTOBUF_NATIVE_GRPC_CHANNEL OFF CACHE BOOL "Enable native gRPC channel implementation")

if(NOT QT_PROTOBUF_STANDALONE_TESTS)
    if(QT_PROTOBUF_NATIVE_GRPC_CHANNEL)
        if(NOT gRPC_FOUND OR NOT Threads_FOUND)
            message("QT_PROTOBUF_NATIVE_GRPC_CHANNEL is ON, but required dependencies are not found.")
            set(QT_PROTOBUF_NATIVE_GRPC_CHANNEL OFF)
        endif()
    endif()

    if(CMAKE_CROSSCOMPILING)
        set(QT_PROTOBUF_MAKE_TESTS OFF)
        set(QT_PROTOBUF_MAKE_EXAMPLES OFF)
    endif()

    set(QT_PROTOBUF_STATIC ON CACHE INTERNAL "" FORCE)

    set(extra_type_libraries_options "") #Prevent to set from outside
    if(QT_PROTOBUF_FIELD_ENUM)
        set(extra_type_libraries_options "FIELDENUM ${extra_type_libraries_options}")
    endif()

    configure_package_config_file(
         "${CMAKE_CURRENT_SOURCE_DIR}/ProjectConfig.cmake.in" "${QT_PROTOBUF_BINARY_DIR}/${QT_PROTOBUF_NAMESPACE}Config.cmake"
         INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake")

    install(FILES "${QT_PROTOBUF_BINARY_DIR}/${QT_PROTOBUF_NAMESPACE}Config.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${QT_PROTOBUF_NAMESPACE}"
        COMPONENT dev
    )
    export(PACKAGE ${QT_PROTOBUF_NAMESPACE})

    if(NOT CMAKE_CROSSCOMPILING)
        add_subdirectory("src/generator")
    endif()

    add_subdirectory("src/protobuf")
    if(TARGET ${QT_VERSIONED_PREFIX}::Network)
        if(${QT_VERSIONED_PREFIX}Core_VERSION VERSION_LESS "5.12.4")
            # grpc target requires QT version not less than 5.12.4
            # earlier versions Http2DirectAttribute is broken: https://doc.qt.io/qt-5/whatsnew511.html
            #                                                  https://bugreports.qt.io/browse/QTBUG-74765
            message(WARNING "QtGrpc requires the Qt framework version 5.12.4 or higher")
        else()
            add_subdirectory("src/grpc")
        endif()
    endif()

    if(TARGET protobuf::libprotobuf AND TARGET protobuf::protoc)
         add_subdirectory("src/wellknowntypes")
    endif()
    if(TARGET ${QT_VERSIONED_PREFIX}::Gui AND TARGET protobuf::protoc)
        add_subdirectory("src/qttypes")
    endif()
else()
    # Disable any installtion rules if build tests only
    set(QT_PROTOBUF_INSTALL FALSE CACHE INTERNAL "" FORCE)
    set(QT_PROTOBUF_MAKE_TESTS TRUE)
endif()

if(QT_PROTOBUF_MAKE_TESTS)
    find_package(WrapGTest)
    find_package(${QT_VERSIONED_PREFIX} OPTIONAL_COMPONENTS Test QuickTest CONFIG)
    if(WrapGTest_FOUND AND TARGET ${QT_VERSIONED_PREFIX}::Test)
        enable_testing()
        add_subdirectory("tests")
    else()
        message(STATUS "googletest not found. Force disable tests.")
    endif()
endif()

if(QT_PROTOBUF_MAKE_EXAMPLES AND NOT QT_PROTOBUF_STANDALONE_TESTS)
    if(TARGET ${QT_VERSIONED_PREFIX}::Quick)
        add_subdirectory("examples")
    else()
        message(STATUS "${QT_VERSIONED_PREFIX}::Quick is not found. Disable examples")
    endif()
endif()

if(QT_PROTOBUF_INSTALL)
    include(packaging)
endif()
include(doxygen)

qt_protobuf_internal_install_targets()
