/*
 * MIT License
 *
 * Copyright (c) Tatyana Borisova <tanusshhka@mail.ru>
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

#include "globalenumssourcegenerator.h"

#include <google/protobuf/io/zero_copy_stream.h>

using namespace ::qtprotobuf::generator;
using namespace ::google::protobuf;
using namespace ::google::protobuf::io;
using namespace ::google::protobuf::compiler;

GlobalEnumsSourceGenerator::GlobalEnumsSourceGenerator(const PackagesList &packageList, std::unique_ptr<io::ZeroCopyOutputStream> out) :
    ClassGeneratorBase(Templates::GlobalEnumClassNameTemplate, std::move(out))
  , mPackageList(packageList) {}

void GlobalEnumsSourceGenerator::run() {
    mPrinter.Print("#include \"globalenums.h\"\n"
                   "#include <QQmlEngine>");

    std::vector<std::string> namespaces;
    for (auto package : mPackageList) {
        utils::split(package.first, namespaces, '.');

        printNamespaces(namespaces);
        printRegisterBody(package.second, namespaces);
        encloseNamespaces(namespaces.size());
    }
}

void GlobalEnumsSourceGenerator::printRegisterBody(const std::list<const FileDescriptor *> &list,
                                                   const std::vector<std::string> &namespaces)
{
    assert(list.size() > 0);

    std::string fullNamespace;
    for (auto name : namespaces) {
        if (fullNamespace.empty()) {
            fullNamespace.append(name);
            continue;
        }
        fullNamespace.append("::").append(name);
    }

    const std::map<std::string, std::string> registrationProperties = {{"classname", mClassName},
                                                                       {"type", mClassName},
                                                                       {"namespaces", fullNamespace},
                                                                       {"package", list.front()->package()}};

    mPrinter.Print(registrationProperties, Templates::ComplexGlobalEnumRegistrationTemplate);
    Indent();
    Indent();
    mPrinter.Print(registrationProperties, Templates::QmlRegisterTypeUncreatableTemplate);

    for (auto file : list) {
        for (int i = 0; i < file->enum_type_count(); i++) {
            const auto enumDescr = file->enum_type(i);
            const std::map<std::string, std::string> properties = {{"classname", mClassName},
                                                                   {"type", mClassName + "::" + enumDescr->name()},
                                                                   {"enum", enumDescr->name() + "List"},
                                                                   {"namespaces", fullNamespace}};
            mPrinter.Print(properties, Templates::ComplexGlobalEnumFieldRegistrationTemplate);
            mPrinter.Print(properties, Templates::RegisterMetaTypeTemplate);
        }
    }
    Outdent();
    mPrinter.Print(Templates::SimpleBlockEnclosureTemplate);
    Outdent();
    mPrinter.Print(Templates::SimpleBlockEnclosureTemplate);
}