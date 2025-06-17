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


#include <string>
#include <vector>
#include <cstring>
#include <iostream>
#include <algorithm>
#include <filesystem>

#include "wayland-scribe.hpp"

namespace fs = std::filesystem;

static inline bool hasSuffix( const std::string& fName, char type ) {
    switch ( type ) {
        case 'h': {
            return ( fName.size() >= 2 ) &&
                   ( std::equal( fName.end() - 2, fName.end(), ".h" ) ||
                     ( fName.size() >= 3 && std::equal( fName.end() - 3, fName.end(), ".hh" ) ) ||
                     ( fName.size() >= 4 && std::equal( fName.end() - 4, fName.end(), ".hpp" ) )
                   );
        }

        case 'c': {
            return ( fName.size() >= 3 ) &&
                   ( std::equal( fName.end() - 3, fName.end(), ".cc" ) ||
                     ( fName.size() >= 4 && std::equal( fName.end() - 4, fName.end(), ".cpp" ) )
                   );
        }

        default: {
            return false;
        }
    }
}


static inline std::string replace( const std::string& source, const std::string& what, const std::string& with ) {
    std::string temp = source;
    size_t      pos  = temp.find( what );

    if ( pos != std::string::npos ) {  // Only replace if found
        temp.replace( pos, what.size(), with );
    }

    return temp;
}


static inline bool startsWith( const std::string& source, const std::string& what ) {
    // Find the last occurrence of the suffix in 'str'
    size_t pos = source.find( what );

    return pos == 0;
}


static inline bool endsWith( const std::string& str, const std::string& suffix ) {
    // Find the last occurrence of the suffix in 'str'
    size_t pos = str.rfind( suffix );

    // Check if the suffix is at the end of 'str'
    if ( ( pos != std::string::npos ) && ( pos + suffix.length() == str.length() ) ) {
        return true;
    }

    return false;
}


std::string snakeCaseToCamelCase( const std::string& snakeCaseName, bool capitalize ) {
    std::string camelCaseName;
    bool        nextToUpper = capitalize;

    for (const char& ch : snakeCaseName) {
        if ( ch == '_' ) {
            nextToUpper = true;
        }

        else {
            camelCaseName += nextToUpper ? std::toupper( ch ) : ch;
            nextToUpper    = false;
        }
    }

    return camelCaseName;
}


Wayland::Scribe::Scribe() {
    mScannerName = "wayland-scribe";
}


void Wayland::Scribe::setRunMode( const std::string& specFile, bool server, uint file, const std::string& output ) {
    mProtocolFilePath = specFile;
    std::string tempOutput = output;

    if ( server ) {
        mServer = true;
    }

    if ( tempOutput.empty() ) {
        tempOutput = replace( specFile, ".xml", mServer ? "-server" : "-client" );
        tempOutput = std::filesystem::path( tempOutput ).filename().string();
    }

    mFile = file;

    switch ( mFile ) {
        case 0: {
            mOutputSrcPath = ( tempOutput + "%1.cpp" );
            mOutputHdrPath = ( tempOutput + "%1.hpp" );

            break;
        }

        case 1: {
            /** No source suffix: add it! */
            if ( hasSuffix( tempOutput, 'c' ) == false ) {
                mOutputSrcPath = tempOutput + ".cpp";
            }

            /** Has source suffix: do nothing */
            else {
                mOutputSrcPath = tempOutput;
            }

            break;
        }

        case 2: {
            /** No header suffix: add it! */
            if ( hasSuffix( tempOutput, 'h' ) == false ) {
                mOutputHdrPath = tempOutput + ".hpp";
            }

            /** Has header suffix: do nothing */
            else {
                mOutputHdrPath = tempOutput;
            }

            break;
        }

        default: {
            break;
        }
    }
}


void Wayland::Scribe::setArgs( const std::string& headerPath, const std::string& prefix, const std::vector<std::string>& includes ) {
    mHeaderPath = headerPath;
    mPrefix     = prefix;

    for (const auto& inc : includes) {
        mIncludes.push_back( "<" + inc + ">" );
    }
}


Wayland::Scribe::WaylandEvent Wayland::Scribe::readEvent( pugi::xml_node& xml, bool request ) {
    WaylandEvent event = {
        .request   = request,
        .name      = xml.attribute( "name" ).value(),
        .type      = xml.attribute( "type" ).value(),
        .arguments = {},
    };

    for (pugi::xml_node argNode : xml.children( "arg" ) ) {
        WaylandArgument argument = {
            .name      = argNode.attribute( "name" ).value(),
            .type      = argNode.attribute( "type" ).value(),
            .interface = argNode.attribute( "interface" ).value(),
            .summary   = argNode.attribute( "summary" ).value(),
            .allowNull = strcmp( argNode.attribute( "allowNull" ).value(),"true" ) == 0,
        };
        event.arguments.push_back( std::move( argument ) );
    }

    return event;
}


Wayland::Scribe::WaylandEnum Wayland::Scribe::readEnum( pugi::xml_node& xml ) {
    WaylandEnum result = {
        .name    = xml.attribute( "name" ).value(),
        .entries = {},
    };

    for (pugi::xml_node entryNode : xml.children( "entry" ) ) {
        WaylandEnumEntry entry = {
            .name    = entryNode.attribute( "name" ).value(),
            .value   = entryNode.attribute( "value" ).value(),
            .summary = entryNode.attribute( "summary" ).value(),
        };
        result.entries.push_back( std::move( entry ) );
    }

    return result;
}


Wayland::Scribe::WaylandInterface Wayland::Scribe::readInterface( pugi::xml_node& xml ) {
    WaylandInterface interface = {
        .name     = xml.attribute( "name" ).value(),
        .version  = xml.attribute( "version" ).as_int( 1 ),
        .enums    = {},
        .events   = {},
        .requests = {},
    };

    for (pugi::xml_node childNode : xml.children() ) {
        if ( strcmp( childNode.name(), "event" ) == 0 ) {
            interface.events.push_back( readEvent( childNode, false ) );
        }
        else if ( strcmp( childNode.name(), "request" ) == 0 ) {
            interface.requests.push_back( readEvent( childNode, true ) );
        }
        else if ( strcmp( childNode.name(), "enum" ) == 0 ) {
            interface.enums.push_back( readEnum( childNode ) );
        }
    }

    return interface;
}


std::string Wayland::Scribe::waylandToCType( const std::string& waylandType, const std::string& interface ) {
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

    else if ( waylandType == "string" ) {
        return "const std::string &";
    }

    else if ( ( waylandType == "object" ) || ( waylandType == "new_id" ) ) {
        if ( mServer ) {
            return "struct ::wl_resource *";
        }

        if ( interface.empty() ) {
            return "struct ::wl_object *";
        }

        return "struct ::" + interface + " *";
    }

    return waylandType;
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
    fprintf( f, "%s( ", snakeCaseToCamelCase( e.name, capitalize ).c_str() );
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

        if ( isNewId && !mServer && ( a.interface.empty() != e.request ) ) {
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
                        fprintf( f, " %s", a.name.c_str() );
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

        std::string cType = waylandToCType( a.type, a.interface );
        fprintf( f, "%s%s%s", cType.c_str(), endsWith( cType, "&" ) || endsWith( cType, "*" ) ? "" : " ", omitNames ? "" : a.name.c_str() );
    }
    fprintf( f, " )" );
}


void Wayland::Scribe::printEventHandlerSignature( FILE *f, const WaylandEvent& e, const char *interfaceName ) {
    fprintf( f, "handle%s( ", snakeCaseToCamelCase( e.name, true ).c_str() );

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

        std::string argBA = snakeCaseToCamelCase( a.name, false );

        if ( mServer && isNewId ) {
            fprintf( f, "uint32_t %s", argBA.c_str() );
        }

        else {
            std::string cType = waylandToCType( a.type, a.interface );
            fprintf( f, "%s%s%s", cType.c_str(), endsWith( cType, "*" ) ? "" : " ", argBA.c_str() );
        }
    }
    fprintf( f, " )" );
}


void Wayland::Scribe::printEnums( FILE *f, const std::vector<WaylandEnum>& enums ) {
    for (const WaylandEnum& e : enums) {
        fprintf( f, "\n" );
        fprintf( f, "        enum class %s {\n", e.name.c_str() );
        for (const WaylandEnumEntry& entry : e.entries) {
            fprintf( f, "            %s_%s = %s,", e.name.c_str(), entry.name.c_str(), entry.value.c_str() );

            if ( !entry.summary.empty() ) {
                fprintf( f, " // %s", entry.summary.c_str() );
            }

            fprintf( f, "\n" );
        }
        fprintf( f, "        };\n" );
    }
}


std::string Wayland::Scribe::stripInterfaceName( const std::string& name, bool capitalize ) {
    if ( !mPrefix.empty() && startsWith( name, mPrefix ) ) {
        return snakeCaseToCamelCase( name.substr( mPrefix.size() ), capitalize );
    }

    if ( startsWith( name, "qt_" ) || startsWith( name, "wl_" ) ) {
        return snakeCaseToCamelCase( name.substr( 3 ), capitalize );
    }

    return snakeCaseToCamelCase( name, capitalize );
}


bool Wayland::Scribe::ignoreInterface( const std::string& name ) {
    return name == "wl_display" ||
           ( mServer && name == "wl_registry" );
}


bool Wayland::Scribe::process() {
    pugi::xml_document     doc;
    pugi::xml_parse_result result = doc.load_file( mProtocolFilePath.c_str() );

    if ( !result ) {
        fprintf( stderr, "Unable to open or parse file %s\n", mProtocolFilePath.c_str() );
        return false;
    }

    pugi::xml_node protocolNode = doc.child( "protocol" );

    if ( !protocolNode ) {
        fprintf( stderr, "The file is not a Wayland protocol file.\n" );
        return false;
    }

    mProtocolName = protocolNode.attribute( "name" ).value();

    if ( mProtocolName.empty() ) {
        fprintf( stderr, "Missing protocol name.\n" );
        return false;
    }

    // We should convert - to _ so that the preprocessor won't
    // generate code which will lead to unexpected behavior
    std::string tmpProtName = mProtocolName;
    std::string preProcessorProtocolName = replace( mProtocolName, "-", "_" );

    std::vector<WaylandInterface> interfaces;

    for (pugi::xml_node interfaceNode : protocolNode.children( "interface" ) ) {
        interfaces.push_back( readInterface( interfaceNode ) );
    }

    auto writeHeader =
        [ = ] ( FILE *f, std::string scanner, std::string protoPath, std::vector<std::string> includes, bool isHeader ) {
            fprintf( f, "// This file was generated by %s " PROJECT_VERSION "\n", scanner.c_str() );
            fprintf( f, "// Source: %s\n\n",                                      protoPath.c_str() );

            /** Header guard */
            if ( isHeader ) {
                fprintf( f, "#pragma once\n" );
                fprintf( f, "\n" );
            }

            for (auto b : includes ) {
                fprintf( f, "#include %s\n", b.c_str() );
            }
            fprintf( f, "#include <string>\n" );
        };

    std::string headerPath;
    std::string codePath;

    if ( mFile == 0 ) {
        headerPath = fs::absolute( replace( mOutputHdrPath, "%1", mServer ? "-server" : "-client" ) ).string();
        codePath   = fs::absolute( replace( mOutputSrcPath, "%1", mServer ? "-server" : "-client" ) ).string();
    }

    else {
        headerPath = fs::absolute( mOutputHdrPath ).string();
        codePath   = fs::absolute( mOutputSrcPath ).string();
    }

    if ( mServer ) {
        if ( ( mFile == 0 ) || ( mFile == 2 ) ) {
            FILE *head = fopen( headerPath.c_str(), "w" );

            writeHeader( head, mScannerName, mProtocolFilePath, mIncludes, true );

            generateServerHeader( head, interfaces );
            fclose( head );
        }

        if ( ( mFile == 0 ) || ( mFile == 1 ) ) {
            FILE *code = fopen( codePath.c_str(), "w" );

            writeHeader( code, mScannerName, mProtocolFilePath, mIncludes, false );
            generateServerCode( code, interfaces );
            fclose( code );
        }
    }

    else {
        if ( ( mFile == 0 ) || ( mFile == 2 ) ) {
            FILE *head = fopen( headerPath.c_str(), "w" );

            writeHeader( head, mScannerName, mProtocolFilePath, mIncludes, true );
            generateClientHeader( head, interfaces );
            fclose( head );
        }

        if ( ( mFile == 0 ) || ( mFile == 1 ) ) {
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

    if ( mHeaderPath.empty() ) {
        fprintf( head, "#include \"%s-server.h\"\n\n", replace( mProtocolName, "_", "-" ).c_str() );
    }
    else {
        fprintf( head, "#include <%s/%s-server.h>\n\n", mHeaderPath.c_str(), replace( mProtocolName, "_", "-" ).c_str() );
    }

    fprintf( head, "#include <iostream>\n" );
    fprintf( head, "#include <map>\n" );
    fprintf( head, "#include <string>\n" );
    fprintf( head, "#include <utility>\n" );

    fprintf( head, "\n" );
    std::string serverExport;

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

        std::string interfaceNameBA         = snakeCaseToCamelCase( interface.name, true );
        const char  *interfaceName          = interfaceNameBA.data();
        std::string interfaceNameStrippedBA = stripInterfaceName( interface.name, false );
        const char  *interfaceNameStripped  = interfaceNameStrippedBA.data();

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
            fprintf( head, "        static const struct ::%s_interface m_%s_interface;\n", interface.name.c_str(), interface.name.c_str() );

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
    if ( mHeaderPath.empty() ) {
        fprintf( code, "#include \"%s-server.h\"\n",   replace( mProtocolName, "_", "-" ).c_str() );
        fprintf( code, "#include \"%s-server.hpp\"\n", replace( mProtocolName, "_", "-" ).c_str() );
    }
    else {
        fprintf( code, "#include <%s/%s-server.h>\n",   mHeaderPath.c_str(), replace( mProtocolName, "_", "-" ).c_str() );
        fprintf( code, "#include <%s/%s-server.hpp>\n", mHeaderPath.c_str(), replace( mProtocolName, "_", "-" ).c_str() );
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

        std::string interfaceNameBA         = snakeCaseToCamelCase( interface.name, true );
        const char  *interfaceName          = interfaceNameBA.data();
        std::string interfaceNameStrippedBA = stripInterfaceName( interface.name, false );
        const char  *interfaceNameStripped  = interfaceNameStrippedBA.data();

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
        fprintf( code, "    m_global = wl_global_create(display, &::%s_interface, version, this, bind_func);\n", interface.name.c_str() );
        fprintf( code, "    m_displayDestroyedListener.notify = %s::display_destroy_func;\n",                    interfaceName );
        fprintf( code, "    m_displayDestroyedListener.parent = this;\n" );
        fprintf( code, "    wl_display_add_destroy_listener(display, &m_displayDestroyedListener);\n" );
        fprintf( code, "}\n" );
        fprintf( code, "\n" );

        fprintf( code, "const struct wl_interface *Wayland::Server::%s::interface() {\n", interfaceName );
        fprintf( code, "    return &::%s_interface;\n",                                   interface.name.c_str() );
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

        std::string interfaceMember = hasRequests ? "&m_" + interface.name + "_interface" : std::string( "nullptr" );

        //We should consider changing bind so that it doesn't special case id == 0
        //and use function overloading instead. Jan do you have a lot of code dependent on this
        // behavior?
        fprintf( code, "Wayland::Server::%s::Resource *Wayland::Server::%s::bind(struct ::wl_client *client, uint32_t id, int version) {\n", interfaceName, interfaceName );
        fprintf( code, "    struct ::wl_resource *handle = wl_resource_create(client, &::%s_interface, version, id);\n",                     interface.name.c_str() );
        fprintf( code, "    return bind(handle);\n" );
        fprintf( code, "}\n" );
        fprintf( code, "\n" );

        fprintf( code, "Wayland::Server::%s::Resource *Wayland::Server::%s::bind(struct ::wl_resource *handle) {\n", interfaceName, interfaceName );
        fprintf( code, "    Resource *resource = allocate();\n" );
        fprintf( code, "    resource->%sObject = this;\n",                                                           interfaceNameStripped );
        fprintf( code, "\n" );
        fprintf( code, "    wl_resource_set_implementation(handle, %s, resource, destroy_func);",                    interfaceMember.c_str() );
        fprintf( code, "\n" );
        fprintf( code, "    resource->handle = handle;\n" );
        fprintf( code, "    bindResource(resource);\n" );
        fprintf( code, "    return resource;\n" );
        fprintf( code, "}\n" );
        fprintf( code, "\n" );

        fprintf( code, "Wayland::Server::%s::Resource *Wayland::Server::%s::Resource::fromResource(struct ::wl_resource *resource) {\n", interfaceName,          interfaceName );
        fprintf( code, "    if (!resource)\n" );
        fprintf( code, "        return nullptr;\n" );
        fprintf( code, "    if (wl_resource_instance_of(resource, &::%s_interface, %s))\n",                                              interface.name.c_str(), interfaceMember.c_str() );
        fprintf( code, "        return static_cast<Resource *>(wl_resource_get_user_data(resource));\n" );
        fprintf( code, "    return nullptr;\n" );
        fprintf( code, "}\n" );

        if ( hasRequests ) {
            fprintf( code, "\n" );
            fprintf( code, "const struct ::%s_interface Wayland::Server::%s::m_%s_interface = {", interface.name.c_str(), interfaceName, interface.name.c_str() );
            bool needsComma = false;
            for (const WaylandEvent& e : interface.requests) {
                if ( needsComma ) {
                    fprintf( code, "," );
                }

                needsComma = true;
                fprintf( code, "\n" );
                fprintf( code, "    Wayland::Server::%s::handle%s", interfaceName, snakeCaseToCamelCase( e.name, true ).c_str() );
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

                std::string eventNameBA = snakeCaseToCamelCase( e.name, false );
                const char  *eventName  = eventNameBA.c_str();
                // std::string eventRequestBA = snakeCaseToCamelCase(e.request, false);
                // const char *eventRequest  = eventRequestBA.c_str();

                fprintf( code, "        return;\n" );
                fprintf( code, "    }\n" );
                fprintf( code, "    static_cast<%s *>(r->%sObject)->%s(r", interfaceName, interfaceNameStripped, eventName );
                for (const WaylandArgument& a : e.arguments) {
                    fprintf( code, ", " );
                    std::string argumentName = snakeCaseToCamelCase( a.name, false );

                    if ( a.type == "string" ) {
                        fprintf( code, "std::string(%s)", argumentName.c_str() );
                    }

                    else {
                        fprintf( code, "%s", argumentName.c_str() );
                    }
                }
                fprintf( code, " );\n" );
                fprintf( code, "}\n" );
            }
        }

        for (const WaylandEvent& e : interface.events) {
            std::string eventNameBA = snakeCaseToCamelCase( e.name, true );
            const char  *eventName  = eventNameBA.c_str();
            // std::string eventRequestBA = snakeCaseToCamelCase(e.request, false);
            // const char *eventRequest  = eventRequestBA.c_str();

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
                fprintf( code, "%s", a.name.c_str() );
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

                std::string array         = a.name + "_data";
                const char  *arrayName    = array.c_str();
                const char  *variableName = a.name.c_str();
                fprintf( code, "    struct wl_array %s;\n",                                            arrayName );
                fprintf( code, "    %s.size = %s.size();\n",                                           arrayName, variableName );
                fprintf( code, "    %s.data = static_cast<void *>(const_cast<char *>(%s.c_str()));\n", arrayName, variableName );
                fprintf( code, "    %s.alloc = 0;\n",                                                  arrayName );
                fprintf( code, "\n" );
            }

            fprintf( code, "    %s_send_%s( ", interface.name.c_str(), e.name.c_str() );
            fprintf( code, "resource" );

            for (const WaylandArgument& a : e.arguments) {
                fprintf( code, ", " );

                if ( a.type == "string" ) {
                    fprintf( code, "%s.c_str()", a.name.c_str() );
                }

                else if ( a.type == "array" ) {
                    fprintf( code, "&%s_data", a.name.c_str() );
                }

                else {
                    fprintf( code, "%s", a.name.c_str() );
                }
            }
        }

        fprintf( code, " );\n" );
        fprintf( code, "}\n" );
        fprintf( code, "\n" );
    }
}


void Wayland::Scribe::generateClientHeader( FILE *head, std::vector<WaylandInterface> interfaces ) {
    if ( mHeaderPath.empty() ) {
        fprintf( head, "#include \"%s-client.h\"\n", replace( mProtocolName, "_", "-" ).c_str() );
    }

    else {
        fprintf( head, "#include <%s/%s-client.h>\n", mHeaderPath.c_str(), replace( mProtocolName, "_", "-" ).c_str() );
    }

    fprintf( head, "struct wl_registry;\n" );
    fprintf( head, "\n" );

    std::string clientExport;

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

        std::string interfaceNameBA = snakeCaseToCamelCase( interface.name, true );
        const char  *interfaceName  = interfaceNameBA.data();

        fprintf( head, "    class %s %s\n    {\n",
                 clientExport.c_str(), interfaceName );
        fprintf( head, "    public:\n" );
        fprintf( head, "        %s(struct ::wl_registry *registry, uint32_t id, int version);\n", interfaceName );
        fprintf( head, "        %s(struct ::%s *object);\n",                                      interfaceName, interface.name.c_str() );
        fprintf( head, "        %s();\n",                                                         interfaceName );
        fprintf( head, "\n" );
        fprintf( head, "        virtual ~%s();\n",                                                interfaceName );
        fprintf( head, "\n" );
        fprintf( head, "        void init(struct ::wl_registry *registry, uint32_t id, int version);\n" );
        fprintf( head, "        void init(struct ::%s *object);\n",                               interface.name.c_str() );
        fprintf( head, "\n" );
        fprintf( head, "        struct ::%s *object() { return m_%s; }\n",                        interface.name.c_str(), interface.name.c_str() );
        fprintf( head, "        const struct ::%s *object() const { return m_%s; }\n",            interface.name.c_str(), interface.name.c_str() );
        fprintf( head, "        static %s *fromObject(struct ::%s *object);\n",                   interfaceName,          interface.name.c_str() );
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
                std::string           new_id_str = "void ";

                if ( new_id ) {
                    if ( new_id->interface.empty() ) {
                        new_id_str = "void *";
                    }
                    else {
                        new_id_str = "struct ::" + new_id->interface + " *";
                    }
                }

                fprintf( head, "        %s", new_id_str.c_str() );
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
            fprintf( head, "        static const struct %s_listener m_%s_listener;\n", interface.name.c_str(), interface.name.c_str() );
            for (const WaylandEvent& e : interface.events) {
                fprintf( head, "        static void " );

                printEventHandlerSignature( head, e, interface.name.c_str() );
                fprintf( head, ";\n" );
            }
        }

        fprintf( head, "        struct ::%s *m_%s;\n", interface.name.c_str(), interface.name.c_str() );
        fprintf( head, "    };\n" );
    }
    fprintf( head, "}\n" );
    fprintf( head, "}\n" );
    fprintf( head, "\n" );
}


void Wayland::Scribe::generateClientCode( FILE *code, std::vector<WaylandInterface> interfaces ) {
    if ( mHeaderPath.empty() ) {
        fprintf( code, "#include \"%s-client.h\"\n",   replace( mProtocolName, "_", "-" ).c_str() );
        fprintf( code, "#include \"%s-client.hpp\"\n", replace( mProtocolName, "_", "-" ).c_str() );
    }
    else {
        fprintf( code, "#include <%s/%s-client.h>\n",   mHeaderPath.c_str(), replace( mProtocolName, "_", "-" ).c_str() );
        fprintf( code, "#include <%s/%s-client.hpp>\n", mHeaderPath.c_str(), replace( mProtocolName, "_", "-" ).c_str() );
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

        std::string interfaceNameBA = snakeCaseToCamelCase( interface.name, true );
        const char  *interfaceName  = interfaceNameBA.data();

        bool hasEvents = !interface.events.empty();

        fprintf( code, "Wayland::Client::%s::%s(struct ::wl_registry *registry, uint32_t id, int version) {\n", interfaceName, interfaceName );
        fprintf( code, "    init(registry, id, version);\n" );
        fprintf( code, "}\n" );
        fprintf( code, "\n" );

        fprintf( code, "Wayland::Client::%s::%s(struct ::%s *obj)\n", interfaceName, interfaceName, interface.name.c_str() );
        fprintf( code, "    : m_%s(obj) {\n",                         interface.name.c_str() );

        if ( hasEvents ) {
            fprintf( code, "    init_listener();\n" );
        }

        fprintf( code, "}\n" );
        fprintf( code, "\n" );

        fprintf( code, "Wayland::Client::%s::%s()\n", interfaceName, interfaceName );
        fprintf( code, "    : m_%s(nullptr) {\n",     interface.name.c_str() );
        fprintf( code, "}\n" );
        fprintf( code, "\n" );

        fprintf( code, "Wayland::Client::%s::~%s() {\n", interfaceName, interfaceName );
        fprintf( code, "}\n" );
        fprintf( code, "\n" );

        fprintf( code, "void Wayland::Client::%s::init(struct ::wl_registry *registry, uint32_t id, int version) {\n", interfaceName );
        fprintf(
            code, "    m_%s = static_cast<struct ::%s *>(wlRegistryBind(registry, id, &%s_interface, version));\n",
            interface.name.c_str(), interface.name.c_str(), interface.name.c_str()
        );

        if ( hasEvents ) {
            fprintf( code, "    init_listener();\n" );
        }

        fprintf( code, "}\n" );
        fprintf( code, "\n" );

        fprintf( code, "void Wayland::Client::%s::init(struct ::%s *obj) {\n", interfaceName, interface.name.c_str() );
        fprintf( code, "    m_%s = obj;\n",                                    interface.name.c_str() );

        if ( hasEvents ) {
            fprintf( code, "    init_listener();\n" );
        }

        fprintf( code, "}\n" );
        fprintf( code, "\n" );

        fprintf( code, "Wayland::Client::%s *Wayland::Client::%s::fromObject(struct ::%s *object) {\n", interfaceName, interfaceName, interface.name.c_str() );

        if ( hasEvents ) {
            fprintf( code, "    if (wl_proxy_get_listener((struct ::wl_proxy *)object) != (void *)&m_%s_listener)\n", interface.name.c_str() );
            fprintf( code, "        return nullptr;\n" );
        }

        fprintf( code, "    return static_cast<Wayland::Client::%s *>(%s_get_user_data(object));\n", interfaceName, interface.name.c_str() );
        fprintf( code, "}\n" );
        fprintf( code, "\n" );

        fprintf( code, "bool Wayland::Client::%s::isInitialized() const {\n", interfaceName );
        fprintf( code, "    return m_%s != nullptr;\n",                       interface.name.c_str() );
        fprintf( code, "}\n" );
        fprintf( code, "\n" );

        fprintf( code, "uint32_t Wayland::Client::%s::version() const {\n",                     interfaceName );
        fprintf( code, "    return wl_proxy_get_version(reinterpret_cast<wl_proxy*>(m_%s));\n", interface.name.c_str() );
        fprintf( code, "}\n" );
        fprintf( code, "\n" );

        fprintf( code, "const struct wl_interface *Wayland::Client::%s::interface() {\n", interfaceName );
        fprintf( code, "    return &::%s_interface;\n",                                   interface.name.c_str() );
        fprintf( code, "}\n" );

        for (const WaylandEvent& e : interface.requests) {
            fprintf( code, "\n" );
            const WaylandArgument *new_id    = newIdArgument( e.arguments );
            std::string           new_id_str = "void ";

            if ( new_id ) {
                if ( new_id->interface.empty() ) {
                    new_id_str = "void *";
                }
                else {
                    new_id_str = "struct ::" + new_id->interface + " *";
                }
            }

            fprintf( code, "%s Wayland::Client::%s::", new_id_str.c_str(), interfaceName );
            printEvent( code, e );
            fprintf( code, " {\n" );
            for (const WaylandArgument& a : e.arguments) {
                if ( a.type != "array" ) {
                    continue;
                }

                std::string array         = a.name + "_data";
                const char  *arrayName    = array.c_str();
                const char  *variableName = a.name.c_str();
                fprintf( code, "    struct wl_array %s;\n",                                            arrayName );
                fprintf( code, "    %s.size = %s.size();\n",                                           arrayName, variableName );
                fprintf( code, "    %s.data = static_cast<void *>(const_cast<char *>(%s.c_str()));\n", arrayName, variableName );
                fprintf( code, "    %s.alloc = 0;\n",                                                  arrayName );
                fprintf( code, "\n" );
            }

            int actualArgumentCount = new_id ? int(e.arguments.size() ) - 1 : int(e.arguments.size() );
            fprintf( code, "    %s::%s_%s( ", new_id ? "return " : "", interface.name.c_str(), e.name.c_str() );
            fprintf( code, "m_%s%s",          interface.name.c_str(),  actualArgumentCount > 0 ? ", " : "" );
            bool needsComma = false;
            for (const WaylandArgument& a : e.arguments) {
                bool isNewId = a.type == "new_id";

                if ( isNewId && !a.interface.empty() ) {
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
                    if ( a.type == "string" ) {
                        fprintf( code, "%s", a.name.c_str() );
                    }

                    else if ( a.type == "array" ) {
                        fprintf( code, "&%s_data", a.name.c_str() );
                    }

                    else {
                        fprintf( code, "%s", a.name.c_str() );
                    }
                }
            }
            fprintf( code, " );\n" );

            if ( e.type == "destructor" ) {
                fprintf( code, "    m_%s = nullptr;\n", interface.name.c_str() );
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
                printEventHandlerSignature( code, e, interface.name.c_str() );
                fprintf( code, " {\n" );
                fprintf( code, "    static_cast<Wayland::Client::%s *>(data)->%s( ", interfaceName, snakeCaseToCamelCase( e.name.c_str(), false ).c_str() );
                bool needsComma = false;
                for (const WaylandArgument& a : e.arguments) {
                    if ( needsComma ) {
                        fprintf( code, ", " );
                    }

                    needsComma = true;
                    const char *argumentName = a.name.c_str();

                    if ( a.type == "string" ) {
                        fprintf( code, "%s", snakeCaseToCamelCase( argumentName, false ).c_str() );
                    }
                    else {
                        fprintf( code, "%s", snakeCaseToCamelCase( argumentName, false ).c_str() );
                    }
                }
                fprintf( code, " );\n" );

                fprintf( code, "}\n" );
                fprintf( code, "\n" );
            }
            fprintf( code, "const struct %s_listener Wayland::Client::%s::m_%s_listener = {\n", interface.name.c_str(), interfaceName, interface.name.c_str() );
            for (const WaylandEvent& e : interface.events) {
                fprintf( code, "    Wayland::Client::%s::handle%s,\n", interfaceName, snakeCaseToCamelCase( e.name.c_str(), true ).c_str() );
            }
            fprintf( code, "};\n" );
            fprintf( code, "\n" );

            fprintf( code, "void Wayland::Client::%s::init_listener() {\n",      interfaceName );
            fprintf( code, "    %s_add_listener(m_%s, &m_%s_listener, this);\n", interface.name.c_str(), interface.name.c_str(), interface.name.c_str() );
            fprintf( code, "}\n" );
        }
    }
    fprintf( code, "\n" );
}
