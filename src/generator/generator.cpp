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

#include "generator.h"
#include "templates.h"
#include "classgeneratorbase.h"
#include "protobufclassgenerator.h"
#include "protobufsourcegenerator.h"
#include "globalenumsgenerator.h"
#include "servergenerator.h"
#include "clientgenerator.h"
#include "clientsourcegenerator.h"
#include "utils.h"

#include <sys/stat.h>
#include <time.h>

#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/descriptor.h>

using namespace ::qtprotobuf::generator;
using namespace ::google::protobuf;
using namespace ::google::protobuf::compiler;

namespace {
#ifdef _WIN32
    const char separator = '\\';
#else
    const char separator = '/';
#endif

bool checkFileModification(struct stat *protoFileStat, std::string filename) {
    struct stat genFileStat;
    return stat(filename.c_str(), &genFileStat) != 0 || difftime(protoFileStat->st_mtim.tv_sec, genFileStat.st_mtim.tv_sec) >= 0;
}
}

bool QtGenerator::Generate(const FileDescriptor *file,
                           const std::string &parameter,
                           GeneratorContext *generatorContext,
                           std::string *error) const
{
    struct stat protoFileStat;
    std::vector<std::pair<std::string, std::string> > parammap;
    ParseGeneratorParameter(parameter, &parammap);
    std::string outDir;

    for(auto param : parammap) {
        if (param.first == "out") {
            outDir = param.second + separator;
        }
    }

    if (file->syntax() != FileDescriptor::SYNTAX_PROTO3) {
        *error = "Invalid proto used. This plugin only supports 'proto3' syntax";
        return false;
    }

    for (int i = 0; i < file->message_type_count(); i++) {
        const Descriptor *message = file->message_type(i);
        std::string baseFilename(message->name());
        utils::tolower(baseFilename);
        stat(message->file()->name().c_str(), &protoFileStat);

        std::string filename = baseFilename + ".h";
        if (checkFileModification(&protoFileStat, outDir + filename)) {
            std::cerr << "Regen" << filename << std::endl;
            ProtobufClassGenerator classGen(message,
                                      std::move(std::unique_ptr<io::ZeroCopyOutputStream>(generatorContext->Open(filename))));
            classGen.run();
        }

        filename = baseFilename + ".cpp";
        if (checkFileModification(&protoFileStat, outDir + filename)) {
            std::cerr << "Regen" << filename << std::endl;
            ProtobufSourceGenerator classSourceGen(message,
                                      std::move(std::unique_ptr<io::ZeroCopyOutputStream>(generatorContext->Open(filename))));
            classSourceGen.run();
        }
    }

    stat(file->name().c_str(), &protoFileStat);

    for (int i = 0; i < file->service_count(); i++) {
        const ServiceDescriptor* service = file->service(i);
        std::string baseFilename(service->name());
        utils::tolower(baseFilename);

        std::string fullFilename = baseFilename + "server.h";
        if (checkFileModification(&protoFileStat, outDir + fullFilename)) {
            ServerGenerator serverGen(service,
                                      std::move(std::unique_ptr<io::ZeroCopyOutputStream>(generatorContext->Open(fullFilename))));
            serverGen.run();
        }

        fullFilename = baseFilename + "client.h";
        if (checkFileModification(&protoFileStat, outDir + fullFilename)) {
            ClientGenerator clientGen(service,
                                      std::move(std::unique_ptr<io::ZeroCopyOutputStream>(generatorContext->Open(fullFilename))));
            clientGen.run();
        }

        fullFilename = baseFilename + "client.cpp";
        if (checkFileModification(&protoFileStat, outDir + fullFilename)) {
            ClientSourceGenerator clientSrcGen(service,
                                               std::move(std::unique_ptr<io::ZeroCopyOutputStream>(generatorContext->Open(fullFilename))));
            clientSrcGen.run();
        }
    }
    return true;
}

bool QtGenerator::GenerateAll(const std::vector<const FileDescriptor *> &files, const string &parameter, GeneratorContext *generatorContext, string *error) const
{
    std::string globalEnumsFilename = "globalenums.h";

    PackagesList packageList;
    for (auto file : files) {
        packageList[file->package()].push_back(file);
    }

    GlobalEnumsGenerator enumGen(packageList,
                                 std::move(std::unique_ptr<io::ZeroCopyOutputStream>(generatorContext->Open(globalEnumsFilename))));
    enumGen.run();

    // FIXME: not sure about the actual protobuf version where GenerateAll was actually implemented
#if GOOGLE_PROTOBUF_VERSION < 3006000
    CodeGenerator::GenerateAll(files, parameter, generatorContext, error);
    *error = "";
    return true;
#else
    return CodeGenerator::GenerateAll(files, parameter, generatorContext, error);
#endif

}

