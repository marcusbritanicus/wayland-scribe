/**
 * This file contains the code for WaylandScribe.
 * WaylandScribe reads wayland protocols in xml format and generates
 * C++ code.
 *
 * This code was originally taken from qtwaylandscanner. Several
 * modifications have been made to suit the needs of this project.
 * All bug reports are to be filed against this project and not the
 * original authors.
 *
 * Original license:
 * Copyright (C) 2016 The Qt Company Ltd.
 * SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only
 * OR GPL-2.0-only OR GPL-3.0-only
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, visit the FSF website:
 * https://www.gnu.org/licenses/gpl-3.0.html#license-text
 **/


#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QXmlStreamReader>

#include <vector>
#include <iostream>
#include <filesystem>

#include "wayland-scribe.hpp"

namespace fs = std::filesystem;

static inline bool hasSuffix( QString fName, char type ) {
    switch ( type ) {
        case 'h': {
            return fName.endsWith( ".h" ) || fName.endsWith( ".hh" ) || fName.endsWith( ".hpp" );
        }

        case 'c': {
            return fName.endsWith( ".cc" ) || fName.endsWith( ".cpp" );
        }

        default: {
            return false;
        }
    }

    return false;
}


QByteArray snakeCaseToCamelCase( QByteArray snakeCaseName, bool capitalize ) {
    QString name = QString::fromUtf8( snakeCaseName );
    QString camelCaseName;
    bool    nextToUpper = (capitalize ? true : false);

    for (const QChar& ch : name) {
        if ( ch == '_' ) {
            nextToUpper = true;
        }
        else {
            camelCaseName += nextToUpper ? ch.toUpper() : ch;
            nextToUpper    = false;
        }
    }

    return camelCaseName.toUtf8();
}


Wayland::Scribe::Scribe() {
    mScannerName = "wayland-scribe";
}


void Wayland::Scribe::setRunMode( QString specFile, bool server, uint file, QString output ) {
    mProtocolFilePath = specFile.toUtf8();

    if ( server ) {
        mServer = true;
    }

    if ( output.isEmpty() ) {
        output = specFile.replace( ".xml", (mServer ? "-server" : "-client") );
    }

    mFile = file;

    switch ( mFile ) {
        case 0: {
            mOutputSrcPath = (output + "%1.cpp");
            mOutputHdrPath = (output + "%1.hpp");

            break;
        }

        case 1: {
            /** No source suffix: add it! */
            if ( hasSuffix( output, 'c' ) == false ) {
                mOutputSrcPath = output + ".cpp";
            }

            /** Has source suffix: do nothing */
            else {
                mOutputSrcPath = output;
            }

            break;
        }

        case 2: {
            /** No header suffix: add it! */
            if ( hasSuffix( output, 'h' ) == false ) {
                mOutputHdrPath = output + ".hpp";
            }

            /** Has header suffix: do nothing */
            else {
                mOutputHdrPath = output;
            }

            break;
        }

        default: {
            break;
        }
    }
}


void Wayland::Scribe::setArgs( QString headerPath, QString prefix, QStringList includes ) {
    mHeaderPath = headerPath.toUtf8();
    mPrefix     = prefix.toUtf8();

    for ( QString inc : includes ) {
        mIncludes << QString( "<%1>" ).arg( inc ).toUtf8();
    }
}


QByteArray Wayland::Scribe::byteArrayValue( const QXmlStreamReader& xml, const char *name ) {
    if ( xml.attributes().hasAttribute( name ) ) {
        return xml.attributes().value( name ).toUtf8();
    }

    return QByteArray();
}


int Wayland::Scribe::intValue( const QXmlStreamReader& xml, const char *name, int defaultValue ) {
    bool ok;
    int  result = byteArrayValue( xml, name ).toInt( &ok );

    return ok ? result : defaultValue;
}


bool Wayland::Scribe::boolValue( const QXmlStreamReader& xml, const char *name ) {
    return byteArrayValue( xml, name ) == "true";
}


Wayland::Scribe::WaylandEvent Wayland::Scribe::readEvent( QXmlStreamReader& xml, bool request ) {
    WaylandEvent event = {
        .request   = request,
        .name      = byteArrayValue( xml, "name" ),
        .type      = byteArrayValue( xml, "type" ),
        .arguments = {},
    };

    while ( xml.readNextStartElement() ) {
        if ( xml.name() == u"arg" ) {
            WaylandArgument argument = {
                .name      = byteArrayValue( xml, "name" ),
                .type      = byteArrayValue( xml, "type" ),
                .interface = byteArrayValue( xml, "interface" ),
                .summary   = byteArrayValue( xml, "summary" ),
                .allowNull = boolValue( xml,      "allowNull" ),
            };
            event.arguments.push_back( std::move( argument ) );
        }

        xml.skipCurrentElement();
    }
    return event;
}


Wayland::Scribe::WaylandEnum Wayland::Scribe::readEnum( QXmlStreamReader& xml ) {
    WaylandEnum result = {
        .name    = byteArrayValue( xml, "name" ),
        .entries = {},
    };

    while ( xml.readNextStartElement() ) {
        if ( xml.name() == u"entry" ) {
            WaylandEnumEntry entry = {
                .name    = byteArrayValue( xml, "name" ),
                .value   = byteArrayValue( xml, "value" ),
                .summary = byteArrayValue( xml, "summary" ),
            };
            result.entries.push_back( std::move( entry ) );
        }

        xml.skipCurrentElement();
    }

    return result;
}


Wayland::Scribe::WaylandInterface Wayland::Scribe::readInterface( QXmlStreamReader& xml ) {
    WaylandInterface interface = {
        .name     = byteArrayValue( xml, "name" ),
        .version  = intValue( xml,       "version",1 ),
        .enums    = {},
        .events   = {},
        .requests = {},
    };

    while ( xml.readNextStartElement() ) {
        if ( xml.name() == u"event" ) {
            interface.events.push_back( readEvent( xml, false ) );
        }
        else if ( xml.name() == u"request" ) {
            interface.requests.push_back( readEvent( xml, true ) );
        }
        else if ( xml.name() == u"enum" ) {
            interface.enums.push_back( readEnum( xml ) );
        }
        else {
            xml.skipCurrentElement();
        }
    }

    return interface;
}


QByteArray Wayland::Scribe::waylandToCType( const QByteArray& waylandType, const QByteArray& interface ) {
    if ( waylandType == "string" ) {
        return "const char *";
    }
    else if ( waylandType == "int" ) {
        return "int32_t";
    }
    else if ( waylandType == "uint" ) {
        return "uint32_t";
    }
    else if ( waylandType == "fixed" ) {
        return "wl_fixed_t";
    }
    else if ( waylandType == "fd" ) {
        return "int32_t";
    }
    else if ( waylandType == "array" ) {
        return "wl_array *";
    }
    else if ( (waylandType == "object") || (waylandType == "new_id") ) {
        if ( mServer ) {
            return "struct ::wl_resource *";
        }

        if ( interface.isEmpty() ) {
            return "struct ::wl_object *";
        }

        return "struct ::" + interface + " *";
    }

    return waylandType;
}


QByteArray Wayland::Scribe::waylandToQtType( const QByteArray& waylandType, const QByteArray& interface, bool ) {
    if ( waylandType == "string" ) {
        return "const std::string &";
    }
    else {
        return waylandToCType( waylandType, interface );
    }
}


const Wayland::Scribe::WaylandArgument *Wayland::Scribe::newIdArgument( const std::vector<WaylandArgument>& arguments ) {
    for (const WaylandArgument& a : arguments) {
        if ( a.type == "new_id" ) {
            return &a;
        }
    }
    return nullptr;
}


void Wayland::Scribe::printEvent( FILE *f, const WaylandEvent& e, bool omitNames, bool withResource, bool capitalize ) {
    fprintf( f, "%s( ", snakeCaseToCamelCase( e.name, capitalize ).constData() );
    bool needsComma = false;

    if ( mServer ) {
        if ( e.request ) {
            fprintf( f, "Resource *%s", omitNames ? "" : "resource" );
            needsComma = true;
        }

        else if ( withResource ) {
            fprintf( f, "struct ::wl_resource *%s", omitNames ? "" : "resource" );
            needsComma = true;
        }
    }

    for (const WaylandArgument& a : e.arguments) {
        bool isNewId = a.type == "new_id";

        if ( isNewId && !mServer && (a.interface.isEmpty() != e.request) ) {
            continue;
        }

        if ( needsComma ) {
            fprintf( f, ", " );
        }

        needsComma = true;

        if ( isNewId ) {
            if ( mServer ) {
                if ( e.request ) {
                    fprintf( f, "uint32_t" );

                    if ( !omitNames ) {
                        fprintf( f, " %s", a.name.constData() );
                    }

                    continue;
                }
            }
            else {
                if ( e.request ) {
                    fprintf( f, "const struct ::wl_interface *%s, uint32_t%s", omitNames ? "" : "interface", omitNames ? "" : " version" );
                    continue;
                }
            }
        }

        QByteArray qtType = waylandToQtType( a.type, a.interface, e.request == mServer );
        fprintf( f, "%s%s%s", qtType.constData(), qtType.endsWith( "&" ) || qtType.endsWith( "*" ) ? "" : " ", omitNames ? "" : a.name.constData() );
    }
    fprintf( f, " )" );
}


void Wayland::Scribe::printEventHandlerSignature( FILE *f, const WaylandEvent& e, const char *interfaceName ) {
    fprintf( f, "handle%s( ", snakeCaseToCamelCase( e.name, true ).constData() );

    if ( mServer ) {
        fprintf( f, "::wl_client *, " );
        fprintf( f, "struct wl_resource *resource" );
    }

    else {
        fprintf( f, "void *data, " );
        fprintf( f, "struct ::%s *", interfaceName );
    }

    for (const WaylandArgument& a : e.arguments) {
        fprintf( f, ", " );
        bool isNewId = a.type == "new_id";

        QByteArray argBA = snakeCaseToCamelCase( a.name, false );

        if ( mServer && isNewId ) {
            fprintf( f, "uint32_t %s", argBA.constData() );
        }

        else {
            QByteArray cType = waylandToCType( a.type, a.interface );
            fprintf( f, "%s%s%s", cType.constData(), cType.endsWith( "*" ) ? "" : " ", argBA.constData() );
        }
    }
    fprintf( f, " )" );
}


void Wayland::Scribe::printEnums( FILE *f, const std::vector<WaylandEnum>& enums ) {
    for (const WaylandEnum& e : enums) {
        fprintf( f, "\n" );
        fprintf( f, "        enum class %s {\n", e.name.constData() );
        for (const WaylandEnumEntry& entry : e.entries) {
            fprintf( f, "            %s_%s = %s,", e.name.constData(), entry.name.constData(), entry.value.constData() );

            if ( !entry.summary.isNull() ) {
                fprintf( f, " // %s", entry.summary.constData() );
            }

            fprintf( f, "\n" );
        }
        fprintf( f, "        };\n" );
    }
}


QByteArray Wayland::Scribe::stripInterfaceName( const QByteArray& name, bool capitalize ) {
    if ( !mPrefix.isEmpty() && name.startsWith( mPrefix ) ) {
        return snakeCaseToCamelCase( name.mid( mPrefix.size() ), capitalize );
    }

    if ( name.startsWith( "qt_" ) || name.startsWith( "wl_" ) ) {
        return snakeCaseToCamelCase( name.mid( 3 ), capitalize );
    }

    return snakeCaseToCamelCase( name, capitalize );
}


bool Wayland::Scribe::ignoreInterface( const QByteArray& name ) {
    return name == "wl_display" ||
           (mServer && name == "wl_registry");
}


bool Wayland::Scribe::process() {
    QFile file( mProtocolFilePath );

    if ( !file.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
        fprintf( stderr, "Unable to open file %s\n", mProtocolFilePath.constData() );
        return false;
    }

    mXml = new QXmlStreamReader( &file );

    if ( !mXml->readNextStartElement() ) {
        return false;
    }

    if ( mXml->name() != u"protocol" ) {
        mXml->raiseError( QStringLiteral( "The file is not a wayland protocol file." ) );
        return false;
    }

    mProtocolName = byteArrayValue( *mXml, "name" );

    if ( mProtocolName.isEmpty() ) {
        mXml->raiseError( QStringLiteral( "Missing protocol name." ) );
        return false;
    }

    // We should convert - to _ so that the preprocessor won't
    // generate code which will lead to unexpected behavior
    QByteArray preProcessorProtocolName = QByteArray( mProtocolName ).replace( '-', '_' ).toUpper();
    // QByteArray preProcessorProtocolName = QByteArray( mProtocolName ).toUpper();

    std::vector<WaylandInterface> interfaces;

    while ( mXml->readNextStartElement() ) {
        if ( mXml->name() == u"interface" ) {
            interfaces.push_back( readInterface( *mXml ) );
        }
        else {
            mXml->skipCurrentElement();
        }
    }

    if ( mXml->hasError() ) {
        return false;
    }

    auto writeHeader =
        [ = ] ( FILE *f, QByteArray scanner, QByteArray protoPath, QList<QByteArray> includes, bool isHeader ) {
            fprintf( f, "// This file was generated by %s " PROJECT_VERSION "\n", scanner.constData() );
            fprintf( f, "// Source: %s\n\n",                                      protoPath.constData() );

            /** Header guard */
            if ( isHeader ) {
                fprintf( f, "#pragma once\n" );
                fprintf( f, "\n" );
            }

            for (auto b : std::as_const( includes ) ) {
                fprintf( f, "#include %s\n", b.constData() );
            }
            fprintf( f, "#include <string>\n" );
        };

    std::string headerPath;
    std::string codePath;

    if ( mFile == 0 ) {
        headerPath = QFileInfo( mOutputHdrPath.arg( mServer ? "-server" : "-client" ) ).absoluteFilePath().toStdString();
        codePath   = QFileInfo( mOutputSrcPath.arg( mServer ? "-server" : "-client" ) ).absoluteFilePath().toStdString();
    }

    else {
        headerPath = QFileInfo( mOutputHdrPath ).absoluteFilePath().toStdString();
        codePath   = QFileInfo( mOutputSrcPath ).absoluteFilePath().toStdString();
    }

    if ( mServer ) {
        if ( (mFile == 0) || (mFile == 2) ) {
            FILE *head = fopen( headerPath.c_str(), "w" );

            writeHeader( head, mScannerName, mProtocolFilePath, mIncludes, true );

            generateServerHeader( head, interfaces );
            fclose( head );
        }

        if ( (mFile == 0) || (mFile == 1) ) {
            FILE *code = fopen( codePath.c_str(), "w" );

            writeHeader( code, mScannerName, mProtocolFilePath, mIncludes, false );
            generateServerCode( code, interfaces );
            fclose( code );
        }
    }

    else {
        if ( (mFile == 0) || (mFile == 2) ) {
            FILE *head = fopen( headerPath.c_str(), "w" );

            writeHeader( head, mScannerName, mProtocolFilePath, mIncludes, true );
            generateClientHeader( head, interfaces );
            fclose( head );
        }

        if ( (mFile == 0) || (mFile == 1) ) {
            FILE *code = fopen( codePath.c_str(), "w" );

            writeHeader( code, mScannerName, mProtocolFilePath, mIncludes, false );
            generateClientCode( code, interfaces );
            fclose( code );
        }
    }

    return true;
}


void Wayland::Scribe::generateServerHeader( FILE *head, std::vector<WaylandInterface> interfaces ) {
    fprintf( head, "#include \"wayland-server-core.h\"\n" );

    if ( mHeaderPath.isEmpty() ) {
        fprintf( head, "#include \"%s-server.h\"\n\n", QByteArray( mProtocolName ).replace( '_', '-' ).constData() );
    }
    else {
        fprintf( head, "#include <%s/%s-server.h>\n\n", mHeaderPath.constData(), QByteArray( mProtocolName ).replace( '_', '-' ).constData() );
    }

    fprintf( head, "#include <iostream>\n" );
    fprintf( head, "#include <map>\n" );
    fprintf( head, "#include <string>\n" );
    fprintf( head, "#include <utility>\n" );

    fprintf( head, "\n" );
    QByteArray serverExport;

    fprintf( head, "\n" );
    fprintf( head, "namespace Wayland {\n" );
    fprintf( head, "namespace Server {\n" );

    bool needsNewLine = false;
    for (const WaylandInterface& interface : interfaces) {
        if ( ignoreInterface( interface.name ) ) {
            continue;
        }

        if ( needsNewLine ) {
            fprintf( head, "\n" );
        }

        needsNewLine = true;

        QByteArray interfaceNameBA         = snakeCaseToCamelCase( interface.name, true );
        const char *interfaceName          = interfaceNameBA.data();
        QByteArray interfaceNameStrippedBA = stripInterfaceName( interface.name, false );
        const char *interfaceNameStripped  = interfaceNameStrippedBA.data();

        fprintf( head, "    class %s {\n",                                                    interfaceName );
        fprintf( head, "    public:\n" );
        fprintf( head, "        %s(struct ::wl_client *client, uint32_t id, int version);\n", interfaceName );
        fprintf( head, "        %s(struct ::wl_display *display, int version);\n",            interfaceName );
        fprintf( head, "        %s(struct ::wl_resource *resource);\n",                       interfaceName );
        fprintf( head, "        %s();\n",                                                     interfaceName );
        fprintf( head, "\n" );
        fprintf( head, "        virtual ~%s();\n",                                            interfaceName );
        fprintf( head, "\n" );
        fprintf( head, "        class Resource {\n" );
        fprintf( head, "        public:\n" );
        fprintf( head, "            Resource() : %sObject(nullptr), handle(nullptr) {}\n", interfaceNameStripped );
        fprintf( head, "            virtual ~Resource() {}\n" );
        fprintf( head, "\n" );
        fprintf( head, "            %s *%sObject;\n",                                      interfaceName, interfaceNameStripped );
        fprintf( head, "            %s *object() { return %sObject; } \n",                 interfaceName, interfaceNameStripped );
        fprintf( head, "            struct ::wl_resource *handle;\n" );
        fprintf( head, "\n" );
        fprintf( head, "            struct ::wl_client *client() const { return wl_resource_get_client(handle); }\n" );
        fprintf( head, "            int version() const { return wl_resource_get_version(handle); }\n" );
        fprintf( head, "\n" );
        fprintf( head, "            static Resource *fromResource(struct ::wl_resource *resource);\n" );
        fprintf( head, "        };\n" );
        fprintf( head, "\n" );
        fprintf( head, "        void init(struct ::wl_client *client, uint32_t id, int version);\n" );
        fprintf( head, "        void init(struct ::wl_display *display, int version);\n" );
        fprintf( head, "        void init(struct ::wl_resource *resource);\n" );
        fprintf( head, "\n" );
        fprintf( head, "        Resource *add(struct ::wl_client *client, int version);\n" );
        fprintf( head, "        Resource *add(struct ::wl_client *client, uint32_t id, int version);\n" );
        fprintf( head, "        Resource *add(struct wl_list *resource_list, struct ::wl_client *client, uint32_t id, int version);\n" );
        fprintf( head, "\n" );
        fprintf( head, "        Resource *resource() { return m_resource; }\n" );
        fprintf( head, "        const Resource *resource() const { return m_resource; }\n" );
        fprintf( head, "\n" );
        fprintf( head, "        std::multimap<struct ::wl_client*, Resource*> resourceMap() { return m_resource_map; }\n" );
        fprintf( head, "        const std::multimap<struct ::wl_client*, Resource*> resourceMap() const { return m_resource_map; }\n" );
        fprintf( head, "\n" );
        fprintf( head, "        bool isGlobal() const { return m_global != nullptr; }\n" );
        fprintf( head, "        bool isResource() const { return m_resource != nullptr; }\n" );
        fprintf( head, "\n" );
        fprintf( head, "        static const struct ::wl_interface *interface();\n" );
        fprintf( head, "        static std::string interfaceName() { return interface()->name; }\n" );
        fprintf( head, "        static int interfaceVersion() { return interface()->version; }\n" );
        fprintf( head, "\n" );

        printEnums( head, interface.enums );

        bool hasEvents = !interface.events.empty();

        if ( hasEvents ) {
            fprintf( head, "\n" );
            for (const WaylandEvent& e : interface.events) {
                fprintf( head, "        void send" );
                printEvent( head, e, false, false, true );
                fprintf( head, ";\n" );
                fprintf( head, "        void send" );
                printEvent( head, e, false, true, true );
                fprintf( head, ";\n" );
            }
        }

        fprintf( head, "\n" );
        fprintf( head, "    protected:\n" );
        fprintf( head, "        virtual Resource *allocate();\n" );
        fprintf( head, "\n" );
        fprintf( head, "        virtual void bindResource(Resource *resource);\n" );
        fprintf( head, "        virtual void destroyResource(Resource *resource);\n" );

        bool hasRequests = !interface.requests.empty();

        if ( hasRequests ) {
            fprintf( head, "\n" );
            for (const WaylandEvent& e : interface.requests) {
                fprintf( head, "        virtual void " );
                printEvent( head, e );
                fprintf( head, ";\n" );
            }
        }

        fprintf( head, "\n" );
        fprintf( head, "    private:\n" );
        fprintf( head, "        static void bind_func(struct ::wl_client *client, void *data, uint32_t version, uint32_t id);\n" );
        fprintf( head, "        static void destroy_func(struct ::wl_resource *client_resource);\n" );
        fprintf( head, "        static void display_destroy_func(struct ::wl_listener *listener, void *data);\n" );
        fprintf( head, "\n" );
        fprintf( head, "        Resource *bind(struct ::wl_client *client, uint32_t id, int version);\n" );
        fprintf( head, "        Resource *bind(struct ::wl_resource *handle);\n" );

        if ( hasRequests ) {
            fprintf( head, "\n" );
            fprintf( head, "        static const struct ::%s_interface m_%s_interface;\n", interface.name.constData(), interface.name.constData() );

            fprintf( head, "\n" );
            for (const WaylandEvent& e : interface.requests) {
                fprintf( head, "        static void " );

                printEventHandlerSignature( head, e, interfaceName );
                fprintf( head, ";\n" );
            }
        }

        fprintf( head, "\n" );
        fprintf( head, "        std::multimap<struct ::wl_client*, Resource*> m_resource_map;\n" );
        fprintf( head, "        Resource *m_resource = nullptr;\n" );
        fprintf( head, "        struct ::wl_global *m_global = nullptr;\n" );
        fprintf( head, "        struct DisplayDestroyedListener : ::wl_listener {\n" );
        fprintf( head, "            %s *parent;\n", interfaceName );
        fprintf( head, "        };\n" );
        fprintf( head, "        DisplayDestroyedListener m_displayDestroyedListener;\n" );
        fprintf( head, "    };\n" );
    }

    fprintf( head, "}\n" );
    fprintf( head, "}\n" );
    fprintf( head, "\n" );
}


void Wayland::Scribe::generateServerCode( FILE *code, std::vector<WaylandInterface> interfaces ) {
    if ( mHeaderPath.isEmpty() ) {
        fprintf( code, "#include \"%s-server.h\"\n",   QByteArray( mProtocolName ).replace( '_', '-' ).constData() );
        fprintf( code, "#include \"%s-server.hpp\"\n", QByteArray( mProtocolName ).replace( '_', '-' ).constData() );
    }
    else {
        fprintf( code, "#include <%s/%s-server.h>\n",   mHeaderPath.constData(), QByteArray( mProtocolName ).replace( '_', '-' ).constData() );
        fprintf( code, "#include <%s/%s-server.hpp>\n", mHeaderPath.constData(), QByteArray( mProtocolName ).replace( '_', '-' ).constData() );
    }

    fprintf( code, "\n" );

    bool needsNewLine = false;
    for (const WaylandInterface& interface : interfaces) {
        if ( ignoreInterface( interface.name ) ) {
            continue;
        }

        if ( needsNewLine ) {
            fprintf( code, "\n" );
        }

        needsNewLine = true;

        QByteArray interfaceNameBA         = snakeCaseToCamelCase( interface.name, true );
        const char *interfaceName          = interfaceNameBA.data();
        QByteArray interfaceNameStrippedBA = stripInterfaceName( interface.name, false );
        const char *interfaceNameStripped  = interfaceNameStrippedBA.data();

        fprintf( code, "Wayland::Server::%s::%s(struct ::wl_client *client, uint32_t id, int version) {\n", interfaceName, interfaceName );
        fprintf( code, "    m_resource_map.clear();\n" );
        fprintf( code, "    init(client, id, version);\n" );
        fprintf( code, "}\n" );
        fprintf( code, "\n" );

        fprintf( code, "Wayland::Server::%s::%s(struct ::wl_display *display, int version) {\n", interfaceName, interfaceName );
        fprintf( code, "    m_resource_map.clear();\n" );
        fprintf( code, "    init(display, version);\n" );
        fprintf( code, "}\n" );
        fprintf( code, "\n" );

        fprintf( code, "Wayland::Server::%s::%s(struct ::wl_resource *resource) {\n", interfaceName, interfaceName );
        fprintf( code, "    m_resource_map.clear();\n" );
        fprintf( code, "    init(resource);\n" );
        fprintf( code, "}\n" );
        fprintf( code, "\n" );

        fprintf( code, "Wayland::Server::%s::%s() {\n", interfaceName, interfaceName );
        fprintf( code, "    m_resource_map.clear();\n" );
        fprintf( code, "}\n" );
        fprintf( code, "\n" );

        fprintf( code, "Wayland::Server::%s::~%s() {\n", interfaceName, interfaceName );
        fprintf( code, "    for (auto it = m_resource_map.begin(); it != m_resource_map.end(); ) {\n" );
        fprintf( code, "        Resource *resourcePtr = it->second;\n" );
        fprintf( code, "\n" );
        fprintf( code, "        // Delete the Resource object pointed to by resourcePtr\n" );
        fprintf( code, "        resourcePtr->%sObject = nullptr;\n", interfaceNameStripped );
        fprintf( code, "    }\n" );
        fprintf( code, "\n" );
        fprintf( code, "    if (m_resource)\n" );
        fprintf( code, "        m_resource->%sObject = nullptr;\n", interfaceNameStripped );
        fprintf( code, "\n" );
        fprintf( code, "    if (m_global) {\n" );
        fprintf( code, "        wl_global_destroy(m_global);\n" );
        fprintf( code, "        wl_list_remove(&m_displayDestroyedListener.link);\n" );
        fprintf( code, "    }\n" );
        fprintf( code, "}\n" );
        fprintf( code, "\n" );

        fprintf( code, "void Wayland::Server::%s::init(struct ::wl_client *client, uint32_t id, int version) {\n", interfaceName );
        fprintf( code, "    m_resource = bind(client, id, version);\n" );
        fprintf( code, "}\n" );
        fprintf( code, "\n" );

        fprintf( code, "void Wayland::Server::%s::init(struct ::wl_resource *resource) {\n", interfaceName );
        fprintf( code, "    m_resource = bind(resource);\n" );
        fprintf( code, "}\n" );
        fprintf( code, "\n" );

        fprintf( code, "Wayland::Server::%s::Resource *Wayland::Server::%s::add(struct ::wl_client *client, int version) {\n", interfaceName, interfaceName );
        fprintf( code, "    Resource *resource = bind(client, 0, version);\n" );
        fprintf( code, "    m_resource_map.insert(std::pair{client, resource});\n" );
        fprintf( code, "    return resource;\n" );
        fprintf( code, "}\n" );
        fprintf( code, "\n" );

        fprintf( code, "Wayland::Server::%s::Resource *Wayland::Server::%s::add(struct ::wl_client *client, uint32_t id, int version) {\n", interfaceName, interfaceName );
        fprintf( code, "    Resource *resource = bind(client, id, version);\n" );
        fprintf( code, "    m_resource_map.insert(std::pair{client, resource});\n" );
        fprintf( code, "    return resource;\n" );
        fprintf( code, "}\n" );
        fprintf( code, "\n" );

        fprintf( code, "void Wayland::Server::%s::init(struct ::wl_display *display, int version) {\n",          interfaceName );
        fprintf( code, "    m_global = wl_global_create(display, &::%s_interface, version, this, bind_func);\n", interface.name.constData() );
        fprintf( code, "    m_displayDestroyedListener.notify = %s::display_destroy_func;\n",                    interfaceName );
        fprintf( code, "    m_displayDestroyedListener.parent = this;\n" );
        fprintf( code, "    wl_display_add_destroy_listener(display, &m_displayDestroyedListener);\n" );
        fprintf( code, "}\n" );
        fprintf( code, "\n" );

        fprintf( code, "const struct wl_interface *Wayland::Server::%s::interface() {\n", interfaceName );
        fprintf( code, "    return &::%s_interface;\n",                                   interface.name.constData() );
        fprintf( code, "}\n" );
        fprintf( code, "\n" );

        fprintf( code, "Wayland::Server::%s::Resource *Wayland::Server::%s::allocate() {\n", interfaceName, interfaceName );
        fprintf( code, "    return new Resource;\n" );
        fprintf( code, "}\n" );
        fprintf( code, "\n" );

        fprintf( code, "void Wayland::Server::%s::bindResource(Resource *) {\n", interfaceName );
        fprintf( code, "}\n" );
        fprintf( code, "\n" );

        fprintf( code, "void Wayland::Server::%s::destroyResource(Resource *) {\n", interfaceName );
        fprintf( code, "}\n" );
        fprintf( code, "\n" );

        fprintf( code, "void Wayland::Server::%s::bind_func(struct ::wl_client *client, void *data, uint32_t version, uint32_t id) {\n", interfaceName );
        fprintf( code, "    %s *that = static_cast<%s *>(data);\n",                                                                      interfaceName, interfaceName );
        fprintf( code, "    that->add(client, id, version);\n" );
        fprintf( code, "}\n" );
        fprintf( code, "\n" );

        fprintf( code, "void Wayland::Server::%s::display_destroy_func(struct ::wl_listener *listener, void *) {\n", interfaceName );
        fprintf( code, "    %s *that = static_cast<%s::DisplayDestroyedListener *>(listener)->parent;\n",            interfaceName, interfaceName );
        fprintf( code, "    that->m_global = nullptr;\n" );
        fprintf( code, "}\n" );
        fprintf( code, "\n" );

        fprintf( code, "void Wayland::Server::%s::destroy_func(struct ::wl_resource *client_resource) {\n", interfaceName );
        fprintf( code, "    Resource *resource = Resource::fromResource(client_resource);\n" );
        fprintf( code, "    %s *that = resource->%sObject;\n",                                              interfaceName, interfaceNameStripped );
        fprintf( code, "    if (that) {\n" );
        fprintf( code, "        auto it = that->m_resource_map.begin();\n" );
        fprintf( code, "        while ( it != that->m_resource_map.end() ) {\n" );
        fprintf( code, "            if ( it->first == resource->client() ) {\n" );
        fprintf( code, "                it = that->m_resource_map.erase( it );\n" );
        fprintf( code, "            }\n" );
        fprintf( code, "\n" );
        fprintf( code, "            else {\n" );
        fprintf( code, "                ++it;\n" );
        fprintf( code, "            }\n" );
        fprintf( code, "        }\n" );
        fprintf( code, "        that->destroyResource(resource);\n" );
        fprintf( code, "\n" );
        fprintf( code, "        that = resource->%sObject;\n", interfaceNameStripped );
        fprintf( code, "        if (that && that->m_resource == resource)\n" );
        fprintf( code, "            that->m_resource = nullptr;\n" );
        fprintf( code, "    }\n" );
        fprintf( code, "    delete resource;\n" );
        fprintf( code, "}\n" );
        fprintf( code, "\n" );

        bool hasRequests = !interface.requests.empty();

        QByteArray interfaceMember = hasRequests ? "&m_" + interface.name + "_interface" : QByteArray( "nullptr" );

        //We should consider changing bind so that it doesn't special case id == 0
        //and use function overloading instead. Jan do you have a lot of code dependent on this
        // behavior?
        fprintf( code, "Wayland::Server::%s::Resource *Wayland::Server::%s::bind(struct ::wl_client *client, uint32_t id, int version) {\n", interfaceName, interfaceName );
        fprintf( code, "    struct ::wl_resource *handle = wl_resource_create(client, &::%s_interface, version, id);\n",                     interface.name.constData() );
        fprintf( code, "    return bind(handle);\n" );
        fprintf( code, "}\n" );
        fprintf( code, "\n" );

        fprintf( code, "Wayland::Server::%s::Resource *Wayland::Server::%s::bind(struct ::wl_resource *handle) {\n", interfaceName, interfaceName );
        fprintf( code, "    Resource *resource = allocate();\n" );
        fprintf( code, "    resource->%sObject = this;\n",                                                           interfaceNameStripped );
        fprintf( code, "\n" );
        fprintf( code, "    wl_resource_set_implementation(handle, %s, resource, destroy_func);",                    interfaceMember.constData() );
        fprintf( code, "\n" );
        fprintf( code, "    resource->handle = handle;\n" );
        fprintf( code, "    bindResource(resource);\n" );
        fprintf( code, "    return resource;\n" );
        fprintf( code, "}\n" );
        fprintf( code, "\n" );

        fprintf( code, "Wayland::Server::%s::Resource *Wayland::Server::%s::Resource::fromResource(struct ::wl_resource *resource) {\n", interfaceName,              interfaceName );
        fprintf( code, "    if (!resource)\n" );
        fprintf( code, "        return nullptr;\n" );
        fprintf( code, "    if (wl_resource_instance_of(resource, &::%s_interface, %s))\n",                                              interface.name.constData(), interfaceMember.constData() );
        fprintf( code, "        return static_cast<Resource *>(wl_resource_get_user_data(resource));\n" );
        fprintf( code, "    return nullptr;\n" );
        fprintf( code, "}\n" );

        if ( hasRequests ) {
            fprintf( code, "\n" );
            fprintf( code, "const struct ::%s_interface Wayland::Server::%s::m_%s_interface = {", interface.name.constData(), interfaceName, interface.name.constData() );
            bool needsComma = false;
            for (const WaylandEvent& e : interface.requests) {
                if ( needsComma ) {
                    fprintf( code, "," );
                }

                needsComma = true;
                fprintf( code, "\n" );
                fprintf( code, "    Wayland::Server::%s::handle%s", interfaceName, snakeCaseToCamelCase( e.name, true ).constData() );
            }
            fprintf( code, "\n" );
            fprintf( code, "};\n" );

            for (const WaylandEvent& e : interface.requests) {
                fprintf( code, "\n" );
                fprintf( code, "void Wayland::Server::%s::", interfaceName );
                printEvent( code, e, true );
                fprintf( code, " {\n" );
                fprintf( code, "}\n" );
            }
            fprintf( code, "\n" );

            for (const WaylandEvent& e : interface.requests) {
                fprintf( code, "\n" );
                fprintf( code, "void Wayland::Server::%s::", interfaceName );

                printEventHandlerSignature( code, e, interfaceName );
                fprintf( code, " {\n" );
                fprintf( code, "    Resource *r = Resource::fromResource(resource);\n" );
                fprintf( code, "    if (!r->%sObject) {\n", interfaceNameStripped );

                if ( e.type == "destructor" ) {
                    fprintf( code, "        wl_resource_destroy(resource);\n" );
                }

                QByteArray eventNameBA = snakeCaseToCamelCase( e.name, false );
                const char *eventName  = eventNameBA.constData();
                // QByteArray eventRequestBA = snakeCaseToCamelCase(e.request, false);
                // const char *eventRequest  = eventRequestBA.constData();

                fprintf( code, "        return;\n" );
                fprintf( code, "    }\n" );
                fprintf( code, "    static_cast<%s *>(r->%sObject)->%s(r", interfaceName, interfaceNameStripped, eventName );
                for (const WaylandArgument& a : e.arguments) {
                    fprintf( code, ", " );
                    QByteArray cType        = waylandToCType( a.type, a.interface );
                    QByteArray qtType       = waylandToQtType( a.type, a.interface, e.request );
                    QByteArray argumentName = snakeCaseToCamelCase( a.name, false );

                    if ( cType == qtType ) {
                        fprintf( code, "%s", argumentName.constData() );
                    }
                    else if ( a.type == "string" ) {
                        fprintf( code, "std::string(%s)", argumentName.constData() );
                    }
                }
                fprintf( code, " );\n" );
                fprintf( code, "}\n" );
            }
        }

        for (const WaylandEvent& e : interface.events) {
            QByteArray eventNameBA = snakeCaseToCamelCase( e.name, true );
            const char *eventName  = eventNameBA.constData();
            // QByteArray eventRequestBA = snakeCaseToCamelCase(e.request, false);
            // const char *eventRequest  = eventRequestBA.constData();

            fprintf( code, "\n" );
            fprintf( code, "void Wayland::Server::%s::send", interfaceName );
            printEvent( code, e, false, false, true );
            fprintf( code, " {\n" );
            fprintf( code, "    if ( !m_resource ) {\n" );
            fprintf( code, "        return;\n" );
            fprintf( code, "    }\n" );
            fprintf( code, "    send%s( m_resource->handle", eventName );
            for (const WaylandArgument& a : e.arguments) {
                fprintf( code, ", " );
                fprintf( code, "%s", a.name.constData() );
            }
            fprintf( code, " );\n" );
            fprintf( code, "}\n" );
            fprintf( code, "\n" );

            fprintf( code, "void Wayland::Server::%s::send", interfaceName );
            printEvent( code, e, false, true, true );
            fprintf( code, " {\n" );

            for (const WaylandArgument& a : e.arguments) {
                if ( a.type != "array" ) {
                    continue;
                }

                QByteArray array         = a.name + "_data";
                const char *arrayName    = array.constData();
                const char *variableName = a.name.constData();
                fprintf( code, "    struct wl_array %s;\n",                                                arrayName );
                fprintf( code, "    %s.size = %s.size();\n",                                               arrayName, variableName );
                fprintf( code, "    %s.data = static_cast<void *>(const_cast<char *>(%s.constData()));\n", arrayName, variableName );
                fprintf( code, "    %s.alloc = 0;\n",                                                      arrayName );
                fprintf( code, "\n" );
            }

            fprintf( code, "    %s_send_%s( ", interface.name.constData(), e.name.constData() );
            fprintf( code, "resource" );

            for (const WaylandArgument& a : e.arguments) {
                fprintf( code, ", " );
                QByteArray cType  = waylandToCType( a.type, a.interface );
                QByteArray qtType = waylandToQtType( a.type, a.interface, e.request );

                if ( a.type == "string" ) {
                    fprintf( code, "%s.c_str()", a.name.constData() );
                }
                else if ( a.type == "array" ) {
                    fprintf( code, "&%s_data", a.name.constData() );
                }
                else if ( cType == qtType ) {
                    fprintf( code, "%s", a.name.constData() );
                }
            }

            fprintf( code, " );\n" );
            fprintf( code, "}\n" );
            fprintf( code, "\n" );
        }
    }
    fprintf( code, "\n" );
}


void Wayland::Scribe::generateClientHeader( FILE *head, std::vector<WaylandInterface> interfaces ) {
    if ( mHeaderPath.isEmpty() ) {
        fprintf( head, "#include \"%s-client.h\"\n", QByteArray( mProtocolName ).replace( '_', '-' ).constData() );
    }

    else {
        fprintf( head, "#include <%s/%s-client.h>\n", mHeaderPath.constData(), QByteArray( mProtocolName ).replace( '_', '-' ).constData() );
    }

    fprintf( head, "struct wl_registry;\n" );
    fprintf( head, "\n" );

    QByteArray clientExport;

    fprintf( head, "\n" );
    fprintf( head, "namespace Wayland {\n" );
    fprintf( head, "namespace Client {\n" );

    bool needsNewLine = false;
    for (const WaylandInterface& interface : interfaces) {
        if ( ignoreInterface( interface.name ) ) {
            continue;
        }

        if ( needsNewLine ) {
            fprintf( head, "\n" );
        }

        needsNewLine = true;

        QByteArray interfaceNameBA = snakeCaseToCamelCase( interface.name, true );
        const char *interfaceName  = interfaceNameBA.data();

        fprintf( head, "    class %s %s\n    {\n",
                 clientExport.constData(), interfaceName );
        fprintf( head, "    public:\n" );
        fprintf( head, "        %s(struct ::wl_registry *registry, uint32_t id, int version);\n", interfaceName );
        fprintf( head, "        %s(struct ::%s *object);\n",                                      interfaceName, interface.name.constData() );
        fprintf( head, "        %s();\n",                                                         interfaceName );
        fprintf( head, "\n" );
        fprintf( head, "        virtual ~%s();\n",                                                interfaceName );
        fprintf( head, "\n" );
        fprintf( head, "        void init(struct ::wl_registry *registry, uint32_t id, int version);\n" );
        fprintf( head, "        void init(struct ::%s *object);\n",                               interface.name.constData() );
        fprintf( head, "\n" );
        fprintf( head, "        struct ::%s *object() { return m_%s; }\n",                        interface.name.constData(), interface.name.constData() );
        fprintf( head, "        const struct ::%s *object() const { return m_%s; }\n",            interface.name.constData(), interface.name.constData() );
        fprintf( head, "        static %s *fromObject(struct ::%s *object);\n",                   interfaceName,              interface.name.constData() );
        fprintf( head, "\n" );
        fprintf( head, "        bool isInitialized() const;\n" );
        fprintf( head, "\n" );
        fprintf( head, "        uint32_t version() const;" );
        fprintf( head, "\n" );
        fprintf( head, "        static const struct ::wl_interface *interface();\n" );

        printEnums( head, interface.enums );

        if ( !interface.requests.empty() ) {
            fprintf( head, "\n" );
            for (const WaylandEvent& e : interface.requests) {
                const WaylandArgument *new_id    = newIdArgument( e.arguments );
                QByteArray            new_id_str = "void ";

                if ( new_id ) {
                    if ( new_id->interface.isEmpty() ) {
                        new_id_str = "void *";
                    }
                    else {
                        new_id_str = "struct ::" + new_id->interface + " *";
                    }
                }

                fprintf( head, "        %s", new_id_str.constData() );
                printEvent( head, e );
                fprintf( head, ";\n" );
            }
        }

        bool hasEvents = !interface.events.empty();

        if ( hasEvents ) {
            fprintf( head, "\n" );
            fprintf( head, "    protected:\n" );
            for (const WaylandEvent& e : interface.events) {
                fprintf( head, "        virtual void " );
                printEvent( head, e );
                fprintf( head, ";\n" );
            }
        }

        fprintf( head, "\n" );
        fprintf( head, "    private:\n" );

        if ( hasEvents ) {
            fprintf( head, "        void init_listener();\n" );
            fprintf( head, "        static const struct %s_listener m_%s_listener;\n", interface.name.constData(), interface.name.constData() );
            for (const WaylandEvent& e : interface.events) {
                fprintf( head, "        static void " );

                printEventHandlerSignature( head, e, interface.name.constData() );
                fprintf( head, ";\n" );
            }
        }

        fprintf( head, "        struct ::%s *m_%s;\n", interface.name.constData(), interface.name.constData() );
        fprintf( head, "    };\n" );
    }
    fprintf( head, "}\n" );
    fprintf( head, "}\n" );
    fprintf( head, "\n" );
}


void Wayland::Scribe::generateClientCode( FILE *code, std::vector<WaylandInterface> interfaces ) {
    if ( mHeaderPath.isEmpty() ) {
        fprintf( code, "#include \"%s-client.h\"\n",   QByteArray( mProtocolName ).replace( '_', '-' ).constData() );
        fprintf( code, "#include \"%s-client.hpp\"\n", QByteArray( mProtocolName ).replace( '_', '-' ).constData() );
    }
    else {
        fprintf( code, "#include <%s/%s-client.h>\n",   mHeaderPath.constData(), QByteArray( mProtocolName ).replace( '_', '-' ).constData() );
        fprintf( code, "#include <%s/%s-client.hpp>\n", mHeaderPath.constData(), QByteArray( mProtocolName ).replace( '_', '-' ).constData() );
    }

    fprintf( code, "\n" );

    // wl_registry_bind is part of the protocol, so we can't use that... instead we use core
    // libwayland API to do the same thing a wayland-scanner generated wl_registry_bind would.
    fprintf( code, "static inline void *wlRegistryBind(struct ::wl_registry *registry, uint32_t name, const struct ::wl_interface *interface, uint32_t version) {\n" );
    fprintf( code, "    const uint32_t bindOpCode = 0;\n" );
    fprintf( code, "    return (void *) wl_proxy_marshal_constructor_versioned((struct wl_proxy *) registry, " );
    fprintf( code, " bindOpCode, interface, version, name, interface->name, version, nullptr);\n" );
    fprintf( code, "}\n" );
    fprintf( code, "\n" );

    bool needsNewLine = false;
    for (const WaylandInterface& interface : interfaces) {
        if ( ignoreInterface( interface.name ) ) {
            continue;
        }

        if ( needsNewLine ) {
            fprintf( code, "\n" );
        }

        needsNewLine = true;

        QByteArray interfaceNameBA = snakeCaseToCamelCase( interface.name, true );
        const char *interfaceName  = interfaceNameBA.data();

        bool hasEvents = !interface.events.empty();

        fprintf( code, "Wayland::Client::%s::%s(struct ::wl_registry *registry, uint32_t id, int version) {\n", interfaceName, interfaceName );
        fprintf( code, "    init(registry, id, version);\n" );
        fprintf( code, "}\n" );
        fprintf( code, "\n" );

        fprintf( code, "Wayland::Client::%s::%s(struct ::%s *obj)\n", interfaceName, interfaceName, interface.name.constData() );
        fprintf( code, "    : m_%s(obj) {\n",                         interface.name.constData() );

        if ( hasEvents ) {
            fprintf( code, "    init_listener();\n" );
        }

        fprintf( code, "}\n" );
        fprintf( code, "\n" );

        fprintf( code, "Wayland::Client::%s::%s()\n", interfaceName, interfaceName );
        fprintf( code, "    : m_%s(nullptr) {\n",     interface.name.constData() );
        fprintf( code, "}\n" );
        fprintf( code, "\n" );

        fprintf( code, "Wayland::Client::%s::~%s() {\n", interfaceName, interfaceName );
        fprintf( code, "}\n" );
        fprintf( code, "\n" );

        fprintf( code, "void Wayland::Client::%s::init(struct ::wl_registry *registry, uint32_t id, int version) {\n", interfaceName );
        fprintf(
            code, "    m_%s = static_cast<struct ::%s *>(wlRegistryBind(registry, id, &%s_interface, version));\n",
            interface.name.constData(), interface.name.constData(), interface.name.constData()
        );

        if ( hasEvents ) {
            fprintf( code, "    init_listener();\n" );
        }

        fprintf( code, "}\n" );
        fprintf( code, "\n" );

        fprintf( code, "void Wayland::Client::%s::init(struct ::%s *obj) {\n", interfaceName, interface.name.constData() );
        fprintf( code, "    m_%s = obj;\n",                                    interface.name.constData() );

        if ( hasEvents ) {
            fprintf( code, "    init_listener();\n" );
        }

        fprintf( code, "}\n" );
        fprintf( code, "\n" );

        fprintf( code, "Wayland::Client::%s *Wayland::Client::%s::fromObject(struct ::%s *object) {\n", interfaceName, interfaceName, interface.name.constData() );

        if ( hasEvents ) {
            fprintf( code, "    if (wl_proxy_get_listener((struct ::wl_proxy *)object) != (void *)&m_%s_listener)\n", interface.name.constData() );
            fprintf( code, "        return nullptr;\n" );
        }

        fprintf( code, "    return static_cast<Wayland::Client::%s *>(%s_get_user_data(object));\n", interfaceName, interface.name.constData() );
        fprintf( code, "}\n" );
        fprintf( code, "\n" );

        fprintf( code, "bool Wayland::Client::%s::isInitialized() const {\n", interfaceName );
        fprintf( code, "    return m_%s != nullptr;\n",                       interface.name.constData() );
        fprintf( code, "}\n" );
        fprintf( code, "\n" );

        fprintf( code, "uint32_t Wayland::Client::%s::version() const {\n",                     interfaceName );
        fprintf( code, "    return wl_proxy_get_version(reinterpret_cast<wl_proxy*>(m_%s));\n", interface.name.constData() );
        fprintf( code, "}\n" );
        fprintf( code, "\n" );

        fprintf( code, "const struct wl_interface *Wayland::Client::%s::interface() {\n", interfaceName );
        fprintf( code, "    return &::%s_interface;\n",                                   interface.name.constData() );
        fprintf( code, "}\n" );

        for (const WaylandEvent& e : interface.requests) {
            fprintf( code, "\n" );
            const WaylandArgument *new_id    = newIdArgument( e.arguments );
            QByteArray            new_id_str = "void ";

            if ( new_id ) {
                if ( new_id->interface.isEmpty() ) {
                    new_id_str = "void *";
                }
                else {
                    new_id_str = "struct ::" + new_id->interface + " *";
                }
            }

            fprintf( code, "%s Wayland::Client::%s::", new_id_str.constData(), interfaceName );
            printEvent( code, e );
            fprintf( code, " {\n" );
            for (const WaylandArgument& a : e.arguments) {
                if ( a.type != "array" ) {
                    continue;
                }

                QByteArray array         = a.name + "_data";
                const char *arrayName    = array.constData();
                const char *variableName = a.name.constData();
                fprintf( code, "    struct wl_array %s;\n",                                                arrayName );
                fprintf( code, "    %s.size = %s.size();\n",                                               arrayName, variableName );
                fprintf( code, "    %s.data = static_cast<void *>(const_cast<char *>(%s.constData()));\n", arrayName, variableName );
                fprintf( code, "    %s.alloc = 0;\n",                                                      arrayName );
                fprintf( code, "\n" );
            }

            int actualArgumentCount = new_id ? int(e.arguments.size() ) - 1 : int(e.arguments.size() );
            fprintf( code, "    %s::%s_%s( ", new_id ? "return " : "",    interface.name.constData(), e.name.constData() );
            fprintf( code, "m_%s%s",          interface.name.constData(), actualArgumentCount > 0 ? ", " : "" );
            bool needsComma = false;
            for (const WaylandArgument& a : e.arguments) {
                bool isNewId = a.type == "new_id";

                if ( isNewId && !a.interface.isEmpty() ) {
                    continue;
                }

                if ( needsComma ) {
                    fprintf( code, ", " );
                }

                needsComma = true;

                if ( isNewId ) {
                    fprintf( code, "interface, version" );
                }
                else {
                    QByteArray cType  = waylandToCType( a.type, a.interface );
                    QByteArray qtType = waylandToQtType( a.type, a.interface, e.request );

                    if ( a.type == "string" ) {
                        fprintf( code, "%s.c_str()", a.name.constData() );
                    }

                    else if ( a.type == "array" ) {
                        fprintf( code, "&%s_data", a.name.constData() );
                    }

                    else if ( cType == qtType ) {
                        fprintf( code, "%s", a.name.constData() );
                    }
                }
            }
            fprintf( code, " );\n" );

            if ( e.type == "destructor" ) {
                fprintf( code, "    m_%s = nullptr;\n", interface.name.constData() );
            }

            fprintf( code, "}\n" );
        }

        if ( hasEvents ) {
            fprintf( code, "\n" );
            for (const WaylandEvent& e : interface.events) {
                fprintf( code, "void Wayland::Client::%s::", interfaceName );
                printEvent( code, e, true );
                fprintf( code, " {\n" );
                fprintf( code, "}\n" );
                fprintf( code, "\n" );
                fprintf( code, "void Wayland::Client::%s::", interfaceName );
                printEventHandlerSignature( code, e, interface.name.constData() );
                fprintf( code, " {\n" );
                fprintf( code, "    static_cast<Wayland::Client::%s *>(data)->%s( ", interfaceName, snakeCaseToCamelCase( e.name.constData(), false ).constData() );
                bool needsComma = false;
                for (const WaylandArgument& a : e.arguments) {
                    if ( needsComma ) {
                        fprintf( code, ", " );
                    }

                    needsComma = true;
                    const char *argumentName = a.name.constData();

                    if ( a.type == "string" ) {
                        fprintf( code, "std::string(%s)", snakeCaseToCamelCase( argumentName, false ).constData() );
                    }
                    else {
                        fprintf( code, "%s", snakeCaseToCamelCase( argumentName, false ).constData() );
                    }
                }
                fprintf( code, " );\n" );

                fprintf( code, "}\n" );
                fprintf( code, "\n" );
            }
            fprintf( code, "const struct %s_listener Wayland::Client::%s::m_%s_listener = {\n", interface.name.constData(), interfaceName, interface.name.constData() );
            for (const WaylandEvent& e : interface.events) {
                fprintf( code, "    Wayland::Client::%s::handle%s,\n", interfaceName, snakeCaseToCamelCase( e.name.constData(), true ).constData() );
            }
            fprintf( code, "};\n" );
            fprintf( code, "\n" );

            fprintf( code, "void Wayland::Client::%s::init_listener() {\n",      interfaceName );
            fprintf( code, "    %s_add_listener(m_%s, &m_%s_listener, this);\n", interface.name.constData(), interface.name.constData(), interface.name.constData() );
            fprintf( code, "}\n" );
        }
    }
    fprintf( code, "\n" );
}


void Wayland::Scribe::printErrors() {
    if ( mXml->hasError() ) {
        fprintf( stderr, "XML error: %s\nLine %lld, column %lld\n", mXml->errorString().toLocal8Bit().constData(), mXml->lineNumber(), mXml->columnNumber() );
    }
}
