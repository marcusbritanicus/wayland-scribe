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
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License.
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
#include <QXmlStreamReader>

#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

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


class Scanner {
    public:
        explicit Scanner() {}
        ~Scanner() { delete m_xml; }

        bool parseArguments( int argc, char **argv );
        void printUsage();
        bool process();
        void printErrors();

    private:
        struct WaylandEnumEntry {
            QByteArray name;
            QByteArray value;
            QByteArray summary;
        };

        struct WaylandEnum {
            QByteArray                    name;

            std::vector<WaylandEnumEntry> entries;
        };

        struct WaylandArgument {
            QByteArray name;
            QByteArray type;
            QByteArray interface;
            QByteArray summary;
            bool       allowNull;
        };

        struct WaylandEvent {
            bool                         request;
            QByteArray                   name;
            QByteArray                   type;
            std::vector<WaylandArgument> arguments;
        };

        struct WaylandInterface {
            QByteArray                name;
            int                       version;

            std::vector<WaylandEnum>  enums;
            std::vector<WaylandEvent> events;
            std::vector<WaylandEvent> requests;
        };

        bool isServerSide();
        bool parseOption( const QByteArray& str );

        void generateServerHeader( FILE *head, std::vector<WaylandInterface> interfaces );
        void generateServerCode( FILE *head, std::vector<WaylandInterface> interfaces );

        void generateClientHeader( FILE *head, std::vector<WaylandInterface> interfaces );
        void generateClientCode( FILE *head, std::vector<WaylandInterface> interfaces );

        QByteArray byteArrayValue( const QXmlStreamReader& xml, const char *name );
        int intValue( const QXmlStreamReader& xml, const char *name, int defaultValue = 0 );
        bool boolValue( const QXmlStreamReader& xml, const char *name );
        WaylandEvent readEvent( QXmlStreamReader& xml, bool request );
        Scanner::WaylandEnum readEnum( QXmlStreamReader& xml );
        Scanner::WaylandInterface readInterface( QXmlStreamReader& xml );
        QByteArray waylandToCType( const QByteArray& waylandType, const QByteArray& interface );
        QByteArray waylandToQtType( const QByteArray& waylandType, const QByteArray& interface, bool cStyleArray );
        const Scanner::WaylandArgument *newIdArgument( const std::vector<WaylandArgument>& arguments );

        void printEvent( FILE *f, const WaylandEvent& e, bool omitNames = false, bool withResource = false, bool capitalize = false );
        void printEventHandlerSignature( FILE *f, const WaylandEvent& e, const char *interfaceName );
        void printEnums( FILE *f, const std::vector<WaylandEnum>& enums );

        QByteArray stripInterfaceName( const QByteArray& name, bool );
        bool ignoreInterface( const QByteArray& name );

        enum Option {
            ClientHeader,
            ServerHeader,
            ClientCode,
            ServerCode
        }
        m_option;

        bool mServer = false;
        bool mClient = false;

        QByteArray m_protocolName;
        QByteArray m_protocolFilePath;
        QByteArray m_scannerName;
        QByteArray m_headerPath;
        QByteArray m_prefix;
        QByteArray m_buildMacro;
        QList<QByteArray> m_includes;
        QXmlStreamReader *m_xml = nullptr;
};

bool Scanner::parseArguments( int argc, char **argv ) {
    QList<QByteArray> args;

    args.reserve( argc );
    for (int i = 0; i < argc; ++i) {
        args << QByteArray( argv[ i ] );
    }

    m_scannerName = args[ 0 ];

    if ( (argc <= 2) || !parseOption( args[ 1 ] ) ) {
        return false;
    }

    m_protocolFilePath = args[ 2 ];

    if ( (argc > 3) && !args[ 3 ].startsWith( '-' ) ) {
        // legacy positional arguments
        m_headerPath = args[ 3 ];

        if ( argc == 5 ) {
            m_prefix = args[ 4 ];
        }
    }
    else {
        // --header-path=<path> (14 characters)
        // --prefix=<prefix> (9 characters)
        // --add-include=<include> (14 characters)
        for (int pos = 3; pos < argc; pos++) {
            const QByteArray& option = args[ pos ];

            if ( option.startsWith( "--header-path=" ) ) {
                m_headerPath = option.mid( 14 );
            }
            else if ( option.startsWith( "--prefix=" ) ) {
                m_prefix = option.mid( 10 );
            }
            else if ( option.startsWith( "--build-macro=" ) ) {
                m_buildMacro = option.mid( 14 );
            }
            else if ( option.startsWith( "--add-include=" ) ) {
                auto include = option.mid( 14 );

                if ( !include.isEmpty() ) {
                    m_includes << include;
                }
            }
            else {
                return false;
            }
        }
    }

    return true;
}


void Scanner::printUsage() {
    fprintf( stderr, "Usage: %s [server|client] specfile [--header-path=<path>] [--prefix=<prefix>] [--add-include=<include>]\n", m_scannerName.constData() );
}


bool Scanner::isServerSide() {
    return mServer;
}


bool Scanner::parseOption( const QByteArray& str ) {
    if ( str == "server" ) {
        mServer = true;
    }

    else if ( str == "client" ) {
        mClient = true;
    }

    else {
        return false;
    }

    return true;
}


QByteArray Scanner::byteArrayValue( const QXmlStreamReader& xml, const char *name ) {
    if ( xml.attributes().hasAttribute( name ) ) {
        return xml.attributes().value( name ).toUtf8();
    }

    return QByteArray();
}


int Scanner::intValue( const QXmlStreamReader& xml, const char *name, int defaultValue ) {
    bool ok;
    int  result = byteArrayValue( xml, name ).toInt( &ok );

    return ok ? result : defaultValue;
}


bool Scanner::boolValue( const QXmlStreamReader& xml, const char *name ) {
    return byteArrayValue( xml, name ) == "true";
}


Scanner::WaylandEvent Scanner::readEvent( QXmlStreamReader& xml, bool request ) {
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


Scanner::WaylandEnum Scanner::readEnum( QXmlStreamReader& xml ) {
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


Scanner::WaylandInterface Scanner::readInterface( QXmlStreamReader& xml ) {
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


QByteArray Scanner::waylandToCType( const QByteArray& waylandType, const QByteArray& interface ) {
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
        if ( isServerSide() ) {
            return "struct ::wl_resource *";
        }

        if ( interface.isEmpty() ) {
            return "struct ::wl_object *";
        }

        return "struct ::" + interface + " *";
    }

    return waylandType;
}


QByteArray Scanner::waylandToQtType( const QByteArray& waylandType, const QByteArray& interface, bool cStyleArray ) {
    if ( waylandType == "string" ) {
        return "const QString &";
    }
    else if ( waylandType == "array" ) {
        return cStyleArray ? "wl_array *" : "const QByteArray &";
    }
    else {
        return waylandToCType( waylandType, interface );
    }
}


const Scanner::WaylandArgument *Scanner::newIdArgument( const std::vector<WaylandArgument>& arguments ) {
    for (const WaylandArgument& a : arguments) {
        if ( a.type == "new_id" ) {
            return &a;
        }
    }
    return nullptr;
}


void Scanner::printEvent( FILE *f, const WaylandEvent& e, bool omitNames, bool withResource, bool capitalize ) {
    fprintf( f, "%s(", snakeCaseToCamelCase( e.name, capitalize ).constData() );
    bool needsComma = false;

    if ( isServerSide() ) {
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

        if ( isNewId && !isServerSide() && (a.interface.isEmpty() != e.request) ) {
            continue;
        }

        if ( needsComma ) {
            fprintf( f, ", " );
        }

        needsComma = true;

        if ( isNewId ) {
            if ( isServerSide() ) {
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

        QByteArray qtType = waylandToQtType( a.type, a.interface, e.request == isServerSide() );
        fprintf( f, "%s%s%s", qtType.constData(), qtType.endsWith( "&" ) || qtType.endsWith( "*" ) ? "" : " ", omitNames ? "" : a.name.constData() );
    }
    fprintf( f, ")" );
}


void Scanner::printEventHandlerSignature( FILE *f, const WaylandEvent& e, const char *interfaceName ) {
    fprintf( f, "handle%s( ", snakeCaseToCamelCase( e.name, true ).constData() );

    if ( isServerSide() ) {
        fprintf( f, "::wl_client *, " );
        fprintf( f, "struct wl_resource *resource" );
    }

    else {
        fprintf( f, "void *data, " );
        fprintf( f, "struct ::%s *object", interfaceName );
    }

    for (const WaylandArgument& a : e.arguments) {
        fprintf( f, ", " );
        bool isNewId = a.type == "new_id";

        QByteArray argBA = snakeCaseToCamelCase( a.name, false );

        if ( isServerSide() && isNewId ) {
            fprintf( f, "uint32_t %s", argBA.constData() );
        }
        else {
            QByteArray cType = waylandToCType( a.type, a.interface );
            fprintf( f, "%s%s%s", cType.constData(), cType.endsWith( "*" ) ? "" : " ", argBA.constData() );
        }
    }
    fprintf( f, " )" );
}


void Scanner::printEnums( FILE *f, const std::vector<WaylandEnum>& enums ) {
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


QByteArray Scanner::stripInterfaceName( const QByteArray& name, bool capitalize ) {
    if ( !m_prefix.isEmpty() && name.startsWith( m_prefix ) ) {
        return snakeCaseToCamelCase( name, capitalize ).mid( m_prefix.size() );
    }

    if ( name.startsWith( "qt_" ) || name.startsWith( "wl_" ) ) {
        return snakeCaseToCamelCase( name.mid( 3 ), capitalize );
    }

    return snakeCaseToCamelCase( name, capitalize );
}


bool Scanner::ignoreInterface( const QByteArray& name ) {
    return name == "wl_display" ||
           (isServerSide() && name == "wl_registry");
}


bool Scanner::process() {
    QFile file( m_protocolFilePath );

    if ( !file.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
        fprintf( stderr, "Unable to open file %s\n", m_protocolFilePath.constData() );
        return false;
    }

    m_xml = new QXmlStreamReader( &file );

    if ( !m_xml->readNextStartElement() ) {
        return false;
    }

    if ( m_xml->name() != u"protocol" ) {
        m_xml->raiseError( QStringLiteral( "The file is not a wayland protocol file." ) );
        return false;
    }

    m_protocolName = byteArrayValue( *m_xml, "name" );

    if ( m_protocolName.isEmpty() ) {
        m_xml->raiseError( QStringLiteral( "Missing protocol name." ) );
        return false;
    }

    //We should convert - to _ so that the preprocessor wont generate code which will lead to unexpected
    // behavior
    //However, the wayland-scanner doesn't do so we will do the same for now
    //QByteArray preProcessorProtocolName = QByteArray(m_protocolName).replace('-', '_').toUpper();
    QByteArray preProcessorProtocolName = QByteArray( m_protocolName ).toUpper();

    std::vector<WaylandInterface> interfaces;

    while ( m_xml->readNextStartElement() ) {
        if ( m_xml->name() == u"interface" ) {
            interfaces.push_back( readInterface( *m_xml ) );
        }
        else {
            m_xml->skipCurrentElement();
        }
    }

    if ( m_xml->hasError() ) {
        return false;
    }

    auto headerPath = fs::path( QByteArray( m_protocolFilePath ).replace( ".xml", (mServer ? "-server.hpp" : "-client.hpp") ).constData() ).filename();
    auto codePath   = fs::path( QByteArray( m_protocolFilePath ).replace( ".xml", (mServer ? "-server.cpp" : "-client.cpp") ).constData() ).filename();

    FILE *head = fopen( headerPath.string().c_str(), "w" );
    FILE *code = fopen( codePath.string().c_str(), "w" );

    fprintf( head, "// This file was generated by wayland-scribe\n" );
    fprintf( code, "// This file was generated by wayland-scribe\n" );

    fprintf( head, "// source file is %s\n\n", qPrintable( m_protocolFilePath ) );
    fprintf( code, "// source file is %s\n\n", qPrintable( m_protocolFilePath ) );

    for (auto b : std::as_const( m_includes ) ) {
        fprintf( head, "#include %s\n", b.constData() );
        fprintf( code, "#include %s\n", b.constData() );
    }

    if ( mServer ) {
        generateServerHeader( head, interfaces );
        generateServerCode( code, interfaces );
    }

    else {
        generateClientHeader( head, interfaces );
        generateClientCode( code, interfaces );
    }

    fclose( head );
    fclose( code );

    return true;
}


void Scanner::generateServerHeader( FILE *head, std::vector<WaylandInterface> interfaces ) {
    fprintf( head, "#pragma once\n" );
    fprintf( head, "\n" );
    fprintf( head, "#include \"wayland-server-core.h\"\n" );

    if ( m_headerPath.isEmpty() ) {
        fprintf( head, "#include \"%s.h\"\n\n", QByteArray( m_protocolName ).replace( '_', '-' ).constData() );
    }
    else {
        fprintf( head, "#include <%s/%s.h>\n\n", m_headerPath.constData(), QByteArray( m_protocolName ).replace( '_', '-' ).constData() );
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
        fprintf( head, "        Resource *m_resource;\n" );
        fprintf( head, "        struct ::wl_global *m_global;\n" );
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


void Scanner::generateServerCode( FILE *code, std::vector<WaylandInterface> interfaces ) {
    if ( m_headerPath.isEmpty() ) {
        fprintf( code, "#include \"%s.h\"\n",   QByteArray( m_protocolName ).replace( '_', '-' ).constData() );
        fprintf( code, "#include \"%s-server.hpp\"\n", QByteArray( m_protocolName ).replace( '_', '-' ).constData() );
    }
    else {
        fprintf( code, "#include <%s/%s.h>\n",   m_headerPath.constData(), QByteArray( m_protocolName ).replace( '_', '-' ).constData() );
        fprintf( code, "#include <%s/%s-server.hpp>\n", m_headerPath.constData(), QByteArray( m_protocolName ).replace( '_', '-' ).constData() );
    }

    fprintf( code, "\n" );
    fprintf( code, "namespace Wayland {\n" );
    fprintf( code, "namespace Server {\n" );

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

        fprintf( code, "    %s::%s(struct ::wl_client *client, uint32_t id, int version)\n", interfaceName, interfaceName );
        fprintf( code, "        : m_resource_map()\n" );
        fprintf( code, "        , m_resource(nullptr)\n" );
        fprintf( code, "        , m_global(nullptr) {\n" );
        fprintf( code, "        init(client, id, version);\n" );
        fprintf( code, "    }\n" );
        fprintf( code, "\n" );

        fprintf( code, "    %s::%s(struct ::wl_display *display, int version)\n", interfaceName, interfaceName );
        fprintf( code, "        : m_resource_map()\n" );
        fprintf( code, "        , m_resource(nullptr)\n" );
        fprintf( code, "        , m_global(nullptr) {\n" );
        fprintf( code, "        init(display, version);\n" );
        fprintf( code, "    }\n" );
        fprintf( code, "\n" );

        fprintf( code, "    %s::%s(struct ::wl_resource *resource)\n", interfaceName, interfaceName );
        fprintf( code, "        : m_resource_map()\n" );
        fprintf( code, "        , m_resource(nullptr)\n" );
        fprintf( code, "        , m_global(nullptr) {\n" );
        fprintf( code, "        init(resource);\n" );
        fprintf( code, "    }\n" );
        fprintf( code, "\n" );

        fprintf( code, "    %s::%s()\n", interfaceName, interfaceName );
        fprintf( code, "        : m_resource_map()\n" );
        fprintf( code, "        , m_resource(nullptr)\n" );
        fprintf( code, "        , m_global(nullptr) {\n" );
        fprintf( code, "    }\n" );
        fprintf( code, "\n" );

        fprintf( code, "    %s::~%s() {\n", interfaceName, interfaceName );
        fprintf( code, "        for (auto it = m_resource_map.begin(); it != m_resource_map.end(); ) {\n" );
        fprintf( code, "            Resource *resourcePtr = it->second;\n" );
        fprintf( code, "\n" );
        fprintf( code, "            // Delete the Resource object pointed to by resourcePtr\n" );
        fprintf( code, "            resourcePtr->%sObject = nullptr;\n", interfaceNameStripped );
        fprintf( code, "        }\n" );
        fprintf( code, "\n" );
        fprintf( code, "        if (m_resource)\n" );
        fprintf( code, "            m_resource->%sObject = nullptr;\n", interfaceNameStripped );
        fprintf( code, "\n" );
        fprintf( code, "        if (m_global) {\n" );
        fprintf( code, "            wl_global_destroy(m_global);\n" );
        fprintf( code, "            wl_list_remove(&m_displayDestroyedListener.link);\n" );
        fprintf( code, "        }\n" );
        fprintf( code, "    }\n" );
        fprintf( code, "\n" );

        fprintf( code, "    void %s::init(struct ::wl_client *client, uint32_t id, int version) {\n", interfaceName );
        fprintf( code, "        m_resource = bind(client, id, version);\n" );
        fprintf( code, "    }\n" );
        fprintf( code, "\n" );

        fprintf( code, "    void %s::init(struct ::wl_resource *resource) {\n", interfaceName );
        fprintf( code, "        m_resource = bind(resource);\n" );
        fprintf( code, "    }\n" );
        fprintf( code, "\n" );

        fprintf( code, "    %s::Resource *%s::add(struct ::wl_client *client, int version) {\n", interfaceName, interfaceName );
        fprintf( code, "        Resource *resource = bind(client, 0, version);\n" );
        fprintf( code, "        m_resource_map.insert(std::pair{client, resource});\n" );
        fprintf( code, "        return resource;\n" );
        fprintf( code, "    }\n" );
        fprintf( code, "\n" );

        fprintf( code, "    %s::Resource *%s::add(struct ::wl_client *client, uint32_t id, int version) {\n", interfaceName, interfaceName );
        fprintf( code, "        Resource *resource = bind(client, id, version);\n" );
        fprintf( code, "        m_resource_map.insert(std::pair{client, resource});\n" );
        fprintf( code, "        return resource;\n" );
        fprintf( code, "    }\n" );
        fprintf( code, "\n" );

        fprintf( code, "    void %s::init(struct ::wl_display *display, int version) {\n",                           interfaceName );
        fprintf( code, "        m_global = wl_global_create(display, &::%s_interface, version, this, bind_func);\n", interface.name.constData() );
        fprintf( code, "        m_displayDestroyedListener.notify = %s::display_destroy_func;\n",                    interfaceName );
        fprintf( code, "        m_displayDestroyedListener.parent = this;\n" );
        fprintf( code, "        wl_display_add_destroy_listener(display, &m_displayDestroyedListener);\n" );
        fprintf( code, "    }\n" );
        fprintf( code, "\n" );

        fprintf( code, "    const struct wl_interface *%s::interface() {\n", interfaceName );
        fprintf( code, "        return &::%s_interface;\n",                  interface.name.constData() );
        fprintf( code, "    }\n" );
        fprintf( code, "\n" );

        fprintf( code, "    %s::Resource *%s::allocate() {\n", interfaceName, interfaceName );
        fprintf( code, "        return new Resource;\n" );
        fprintf( code, "    }\n" );
        fprintf( code, "\n" );

        fprintf( code, "    void %s::bindResource(Resource *) {\n", interfaceName );
        fprintf( code, "    }\n" );
        fprintf( code, "\n" );

        fprintf( code, "    void %s::destroyResource(Resource *) {\n", interfaceName );
        fprintf( code, "    }\n" );
        fprintf( code, "\n" );

        fprintf( code, "    void %s::bind_func(struct ::wl_client *client, void *data, uint32_t version, uint32_t id)\n", interfaceName );
        fprintf( code, "    {\n" );
        fprintf( code, "        %s *that = static_cast<%s *>(data);\n",                                                   interfaceName, interfaceName );
        fprintf( code, "        that->add(client, id, version);\n" );
        fprintf( code, "    }\n" );
        fprintf( code, "\n" );

        fprintf( code, "    void %s::display_destroy_func(struct ::wl_listener *listener, void *) {\n",       interfaceName );
        fprintf( code, "        %s *that = static_cast<%s::DisplayDestroyedListener *>(listener)->parent;\n", interfaceName, interfaceName );
        fprintf( code, "        that->m_global = nullptr;\n" );
        fprintf( code, "    }\n" );
        fprintf( code, "\n" );

        fprintf( code, "    void %s::destroy_func(struct ::wl_resource *client_resource)\n", interfaceName );
        fprintf( code, "    {\n" );
        fprintf( code, "        Resource *resource = Resource::fromResource(client_resource);\n" );
        fprintf( code, "        %s *that = resource->%sObject;\n",                           interfaceName, interfaceNameStripped );
        fprintf( code, "        if (that) {\n" );
        fprintf( code, "            auto it = that->m_resource_map.begin();\n" );
        fprintf( code, "            while ( it != that->m_resource_map.end() ) {\n" );
        fprintf( code, "                if ( it->first == resource->client() ) {\n" );
        fprintf( code, "                    it = that->m_resource_map.erase( it );\n" );
        fprintf( code, "                }\n" );
        fprintf( code, "\n" );
        fprintf( code, "                else {\n" );
        fprintf( code, "                    ++it;\n" );
        fprintf( code, "                }\n" );
        fprintf( code, "            }\n" );
        fprintf( code, "            that->destroyResource(resource);\n" );
        fprintf( code, "\n" );
        fprintf( code, "            that = resource->%sObject;\n", interfaceNameStripped );
        fprintf( code, "            if (that && that->m_resource == resource)\n" );
        fprintf( code, "                that->m_resource = nullptr;\n" );
        fprintf( code, "        }\n" );
        fprintf( code, "        delete resource;\n" );
        fprintf( code, "    }\n" );
        fprintf( code, "\n" );

        bool hasRequests = !interface.requests.empty();

        QByteArray interfaceMember = hasRequests ? "&m_" + interface.name + "_interface" : QByteArray( "nullptr" );

        //We should consider changing bind so that it doesn't special case id == 0
        //and use function overloading instead. Jan do you have a lot of code dependent on this
        // behavior?
        fprintf( code, "    %s::Resource *%s::bind(struct ::wl_client *client, uint32_t id, int version)\n",                 interfaceName, interfaceName );
        fprintf( code, "    {\n" );
        fprintf( code, "        struct ::wl_resource *handle = wl_resource_create(client, &::%s_interface, version, id);\n", interface.name.constData() );
        fprintf( code, "        return bind(handle);\n" );
        fprintf( code, "    }\n" );
        fprintf( code, "\n" );

        fprintf( code, "    %s::Resource *%s::bind(struct ::wl_resource *handle)\n",                  interfaceName, interfaceName );
        fprintf( code, "    {\n" );
        fprintf( code, "        Resource *resource = allocate();\n" );
        fprintf( code, "        resource->%sObject = this;\n",                                        interfaceNameStripped );
        fprintf( code, "\n" );
        fprintf( code, "        wl_resource_set_implementation(handle, %s, resource, destroy_func);", interfaceMember.constData() );
        fprintf( code, "\n" );
        fprintf( code, "        resource->handle = handle;\n" );
        fprintf( code, "        bindResource(resource);\n" );
        fprintf( code, "        return resource;\n" );
        fprintf( code, "    }\n" );

        fprintf( code, "    %s::Resource *%s::Resource::fromResource(struct ::wl_resource *resource)\n", interfaceName, interfaceName );
        fprintf( code, "    {\n" );
        fprintf( code, "        if (!resource)\n" );
        fprintf( code, "            return nullptr;\n" );
        fprintf( code, "        if (wl_resource_instance_of(resource, &::%s_interface, %s))\n", interface.name.constData(), interfaceMember.constData() );
        fprintf( code, "            return static_cast<Resource *>(wl_resource_get_user_data(resource));\n" );
        fprintf( code, "        return nullptr;\n" );
        fprintf( code, "    }\n" );

        if ( hasRequests ) {
            fprintf( code, "\n" );
            fprintf( code, "    const struct ::%s_interface %s::m_%s_interface = {", interface.name.constData(), interfaceName, interface.name.constData() );
            bool needsComma = false;
            for (const WaylandEvent& e : interface.requests) {
                if ( needsComma ) {
                    fprintf( code, "," );
                }

                needsComma = true;
                fprintf( code, "\n" );
                fprintf( code, "        %s::handle%s", interfaceName, snakeCaseToCamelCase( e.name, true ).constData() );
            }
            fprintf( code, "\n" );
            fprintf( code, "    };\n" );

            for (const WaylandEvent& e : interface.requests) {
                fprintf( code, "\n" );
                fprintf( code, "    void %s::", interfaceName );
                printEvent( code, e, true );
                fprintf( code, "\n" );
                fprintf( code, "    {\n" );
                fprintf( code, "    }\n" );
            }
            fprintf( code, "\n" );

            for (const WaylandEvent& e : interface.requests) {
                fprintf( code, "\n" );
                fprintf( code, "    void %s::", interfaceName );

                printEventHandlerSignature( code, e, interfaceName );
                fprintf( code, " {\n" );
                fprintf( code, "        Resource *r = Resource::fromResource(resource);\n" );
                fprintf( code, "        if (!r->%sObject) {\n", interfaceNameStripped );

                if ( e.type == "destructor" ) {
                    fprintf( code, "            wl_resource_destroy(resource);\n" );
                }

                QByteArray eventNameBA = snakeCaseToCamelCase( e.name, false );
                const char *eventName  = eventNameBA.constData();
                // QByteArray eventRequestBA = snakeCaseToCamelCase(e.request, false);
                // const char *eventRequest  = eventRequestBA.constData();

                fprintf( code, "            return;\n" );
                fprintf( code, "        }\n" );
                fprintf( code, "        static_cast<%s *>(r->%sObject)->%s(\n", interfaceName, interfaceNameStripped, eventName );
                fprintf( code, "            r" );
                for (const WaylandArgument& a : e.arguments) {
                    fprintf( code, ",\n" );
                    QByteArray cType        = waylandToCType( a.type, a.interface );
                    QByteArray qtType       = waylandToQtType( a.type, a.interface, e.request );
                    QByteArray argumentName = snakeCaseToCamelCase( a.name, false );

                    if ( cType == qtType ) {
                        fprintf( code, "            %s", argumentName.constData() );
                    }
                    else if ( a.type == "string" ) {
                        fprintf( code, "            QString::fromUtf8(%s)", argumentName.constData() );
                    }
                }
                fprintf( code, ");\n" );
                fprintf( code, "    }\n" );
            }
        }

        for (const WaylandEvent& e : interface.events) {
            QByteArray eventNameBA = snakeCaseToCamelCase( e.name, true );
            const char *eventName  = eventNameBA.constData();
            // QByteArray eventRequestBA = snakeCaseToCamelCase(e.request, false);
            // const char *eventRequest  = eventRequestBA.constData();

            fprintf( code, "\n" );
            fprintf( code, "    void %s::send", interfaceName );
            printEvent( code, e, false, false, true );
            fprintf( code, "\n" );
            fprintf( code, "    {\n" );
            fprintf( code, "        if (!m_resource) {\n" );
            fprintf( code, "            return;\n" );
            fprintf( code, "        }\n" );
            fprintf( code, "        send%s(\n", eventName );
            fprintf( code, "            m_resource->handle" );
            for (const WaylandArgument& a : e.arguments) {
                fprintf( code, ",\n" );
                fprintf( code, "            %s", a.name.constData() );
            }
            fprintf( code, ");\n" );
            fprintf( code, "    }\n" );
            fprintf( code, "\n" );

            fprintf( code, "    void %s::send", interfaceName );
            printEvent( code, e, false, true, true );
            fprintf( code, " {\n" );

            for (const WaylandArgument& a : e.arguments) {
                if ( a.type != "array" ) {
                    continue;
                }

                QByteArray array         = a.name + "_data";
                const char *arrayName    = array.constData();
                const char *variableName = a.name.constData();
                fprintf( code, "        struct wl_array %s;\n",                                                arrayName );
                fprintf( code, "        %s.size = %s.size();\n",                                               arrayName, variableName );
                fprintf( code, "        %s.data = static_cast<void *>(const_cast<char *>(%s.constData()));\n", arrayName, variableName );
                fprintf( code, "        %s.alloc = 0;\n",                                                      arrayName );
                fprintf( code, "\n" );
            }

            fprintf( code, "        %s_send_%s(\n", interface.name.constData(), e.name.constData() );
            fprintf( code, "            resource" );

            for (const WaylandArgument& a : e.arguments) {
                fprintf( code, ",\n" );
                QByteArray cType  = waylandToCType( a.type, a.interface );
                QByteArray qtType = waylandToQtType( a.type, a.interface, e.request );

                if ( a.type == "string" ) {
                    fprintf( code, "            %s.toUtf8().constData()", a.name.constData() );
                }
                else if ( a.type == "array" ) {
                    fprintf( code, "            &%s_data", a.name.constData() );
                }
                else if ( cType == qtType ) {
                    fprintf( code, "            %s", a.name.constData() );
                }
            }

            fprintf( code, ");\n" );
            fprintf( code, "    }\n" );
            fprintf( code, "\n" );
        }
    }
    fprintf( code, "}\n" );
    fprintf( code, "}\n" );
    fprintf( code, "\n" );
}


void Scanner::generateClientHeader( FILE *head, std::vector<WaylandInterface> interfaces ) {
    fprintf( head, "#pragma once\n" );
    fprintf( head, "\n" );

    if ( m_headerPath.isEmpty() ) {
        fprintf( head, "#include \"wayland-%s-client-protocol.h\"\n", QByteArray( m_protocolName ).replace( '_', '-' ).constData() );
    }
    else {
        fprintf( head, "#include <%s/wayland-%s-client-protocol.h>\n", m_headerPath.constData(), QByteArray( m_protocolName ).replace( '_', '-' ).constData() );
    }

    fprintf( head, "#include <QByteArray>\n" );
    fprintf( head, "#include <QString>\n" );
    fprintf( head, "\n" );
    fprintf( head, "struct wl_registry;\n" );
    fprintf( head, "\n" );

    QByteArray clientExport;

    fprintf( head, "\n" );
    fprintf( head, "namespace WaylandClient {\n" );

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

        fprintf( head, "    class %s %s\n    {\n",
                 clientExport.constData(), interfaceName );
        fprintf( head, "    public:\n" );
        fprintf( head, "        %s(struct ::wl_registry *registry, uint32_t id, int version);\n", interfaceName );
        fprintf( head, "        %s(struct ::%s *object);\n",                                      interfaceName, interfaceName );
        fprintf( head, "        %s();\n",                                                         interfaceName );
        fprintf( head, "\n" );
        fprintf( head, "        virtual ~%s();\n",                                                interfaceName );
        fprintf( head, "\n" );
        fprintf( head, "        void init(struct ::wl_registry *registry, uint32_t id, int version);\n" );
        fprintf( head, "        void init(struct ::%s *object);\n",                               interfaceName );
        fprintf( head, "\n" );
        fprintf( head, "        struct ::%s *object() { return m_%s; }\n",                        interfaceName, interfaceName );
        fprintf( head, "        const struct ::%s *object() const { return m_%s; }\n",            interfaceName, interfaceName );
        fprintf( head, "        static %s *fromObject(struct ::%s *object);\n",                   interfaceName, interfaceName );
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
                fprintf( head, "        virtual void %s_", interfaceNameStripped );
                printEvent( head, e );
                fprintf( head, ";\n" );
            }
        }

        fprintf( head, "\n" );
        fprintf( head, "    private:\n" );

        if ( hasEvents ) {
            fprintf( head, "        void init_listener();\n" );
            fprintf( head, "        static const struct %s_listener m_%s_listener;\n", interfaceName, interfaceName );
            for (const WaylandEvent& e : interface.events) {
                fprintf( head, "        static void " );

                printEventHandlerSignature( head, e, interfaceName );
                fprintf( head, ";\n" );
            }
        }

        fprintf( head, "        struct ::%s *m_%s;\n", interfaceName, interfaceName );
        fprintf( head, "    };\n" );
    }
    fprintf( head, "}\n" );
    fprintf( head, "\n" );
}


void Scanner::generateClientCode( FILE *code, std::vector<WaylandInterface> interfaces ) {
    if ( m_headerPath.isEmpty() ) {
        fprintf( code, "#include \"%s.h\"\n", QByteArray( m_protocolName ).replace( '_', '-' ).constData() );
        fprintf( code, "#include \"%s-client.hpp\"\n", QByteArray( m_protocolName ).replace( '_', '-' ).constData() );
    }
    else {
        fprintf( code, "#include <%s/%s.h>\n", m_headerPath.constData(), QByteArray( m_protocolName ).replace( '_', '-' ).constData() );
        fprintf( code, "#include <%s/%s-client.hpp>\n", m_headerPath.constData(), QByteArray( m_protocolName ).replace( '_', '-' ).constData() );
    }

    fprintf( code, "\n" );
    fprintf( code, "\n" );
    fprintf( code, "namespace WaylandClient {\n" );
    fprintf( code, "\n" );

    // wl_registry_bind is part of the protocol, so we can't use that... instead we use core
    // libwayland API to do the same thing a wayland-scanner generated wl_registry_bind would.
    fprintf( code, "static inline void *wlRegistryBind(struct ::wl_registry *registry, uint32_t name, const struct ::wl_interface *interface, uint32_t version) {\n" );
    fprintf( code, "    const uint32_t bindOpCode = 0;\n" );
    fprintf( code, "    return (void *) wl_proxy_marshal_constructor_versioned((struct wl_proxy *) registry,\n" );
    fprintf( code, "    bindOpCode, interface, version, name, interface->name, version, nullptr);\n" );
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

        QByteArray interfaceNameBA         = snakeCaseToCamelCase( interface.name, true );
        const char *interfaceName          = interfaceNameBA.data();
        QByteArray interfaceNameStrippedBA = stripInterfaceName( interface.name, false );
        const char *interfaceNameStripped  = interfaceNameStrippedBA.data();

        bool hasEvents = !interface.events.empty();

        fprintf( code, "    %s::%s(struct ::wl_registry *registry, uint32_t id, int version) {\n", interfaceName, interfaceName );
        fprintf( code, "    {\n" );
        fprintf( code, "        init(registry, id, version);\n" );
        fprintf( code, "    }\n" );
        fprintf( code, "\n" );

        fprintf( code, "    %s::%s(struct ::%s *obj)\n", interfaceName, interfaceName, interfaceName );
        fprintf( code, "        : m_%s(obj) {\n",        interfaceName );

        if ( hasEvents ) {
            fprintf( code, "        init_listener();\n" );
        }

        fprintf( code, "    }\n" );
        fprintf( code, "\n" );

        fprintf( code, "    %s::%s()\n",              interfaceName, interfaceName );
        fprintf( code, "        : m_%s(nullptr) {\n", interfaceName );
        fprintf( code, "    }\n" );
        fprintf( code, "\n" );

        fprintf( code, "    %s::~%s() {\n", interfaceName, interfaceName );
        fprintf( code, "    }\n" );
        fprintf( code, "\n" );

        fprintf( code, "    void %s::init(struct ::wl_registry *registry, uint32_t id, int version) {\n",                    interfaceName );
        fprintf( code, "        m_%s = static_cast<struct ::%s *>(wlRegistryBind(registry, id, &%s_interface, version));\n", interfaceName, interfaceName, interfaceName );

        if ( hasEvents ) {
            fprintf( code, "        init_listener();\n" );
        }

        fprintf( code, "    }\n" );
        fprintf( code, "\n" );

        fprintf( code, "    void %s::init(struct ::%s *obj) {\n", interfaceName, interfaceName );
        fprintf( code, "        m_%s = obj;\n",                   interfaceName );

        if ( hasEvents ) {
            fprintf( code, "        init_listener();\n" );
        }

        fprintf( code, "    }\n" );
        fprintf( code, "\n" );

        fprintf( code, "    %s *%s::fromObject(struct ::%s *object)\n", interfaceName, interfaceName,
                 interfaceName );
        fprintf( code, "    {\n" );

        if ( hasEvents ) {
            fprintf( code, "        if (wl_proxy_get_listener((struct ::wl_proxy *)object) != (void *)&m_%s_listener)\n", interfaceName );
            fprintf( code, "            return nullptr;\n" );
        }

        fprintf( code, "        return static_cast<%s *>(%s_get_user_data(object));\n", interfaceName,
                 interfaceName );
        fprintf( code, "    }\n" );
        fprintf( code, "\n" );

        fprintf( code, "    bool %s::isInitialized() const\n", interfaceName );
        fprintf( code, "    {\n" );
        fprintf( code, "        return m_%s != nullptr;\n",    interfaceName );
        fprintf( code, "    }\n" );
        fprintf( code, "\n" );

        fprintf( code, "    uint32_t %s::version() const\n",
                 interfaceName );
        fprintf( code, "    {\n" );
        fprintf( code, "        return wl_proxy_get_version(reinterpret_cast<wl_proxy*>(m_%s));\n",
                 interfaceName );
        fprintf( code, "    }\n" );
        fprintf( code, "\n" );

        fprintf( code, "    const struct wl_interface *%s::interface()\n", interfaceName );
        fprintf( code, "    {\n" );
        fprintf( code, "        return &::%s_interface;\n",                interfaceName );
        fprintf( code, "    }\n" );

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

            fprintf( code, "    %s%s::", new_id_str.constData(), interfaceName );
            printEvent( code, e );
            fprintf( code, "\n" );
            fprintf( code, "    {\n" );
            for (const WaylandArgument& a : e.arguments) {
                if ( a.type != "array" ) {
                    continue;
                }

                QByteArray array         = a.name + "_data";
                const char *arrayName    = array.constData();
                const char *variableName = a.name.constData();
                fprintf( code, "        struct wl_array %s;\n",                                                arrayName );
                fprintf( code, "        %s.size = %s.size();\n",                                               arrayName, variableName );
                fprintf( code, "        %s.data = static_cast<void *>(const_cast<char *>(%s.constData()));\n", arrayName, variableName );
                fprintf( code, "        %s.alloc = 0;\n",                                                      arrayName );
                fprintf( code, "\n" );
            }
            int actualArgumentCount = new_id ? int(e.arguments.size() ) - 1 : int(e.arguments.size() );
            fprintf( code, "        %s::%s_%s(\n", new_id ? "return " : "", interfaceName, e.name.constData() );
            fprintf( code, "            m_%s%s",   interfaceName,           actualArgumentCount > 0 ? "," : "" );
            bool needsComma = false;
            for (const WaylandArgument& a : e.arguments) {
                bool isNewId = a.type == "new_id";

                if ( isNewId && !a.interface.isEmpty() ) {
                    continue;
                }

                if ( needsComma ) {
                    fprintf( code, "," );
                }

                needsComma = true;
                fprintf( code, "\n" );

                if ( isNewId ) {
                    fprintf( code, "            interface,\n" );
                    fprintf( code, "            version" );
                }
                else {
                    QByteArray cType  = waylandToCType( a.type, a.interface );
                    QByteArray qtType = waylandToQtType( a.type, a.interface, e.request );

                    if ( a.type == "string" ) {
                        fprintf( code, "            %s.toUtf8().constData()", a.name.constData() );
                    }

                    else if ( a.type == "array" ) {
                        fprintf( code, "            &%s_data", a.name.constData() );
                    }

                    else if ( cType == qtType ) {
                        fprintf( code, "            %s", a.name.constData() );
                    }
                }
            }
            fprintf( code, ");\n" );

            if ( e.type == "destructor" ) {
                fprintf( code, "        m_%s = nullptr;\n", interfaceName );
            }

            fprintf( code, "    }\n" );
        }

        if ( hasEvents ) {
            fprintf( code, "\n" );
            for (const WaylandEvent& e : interface.events) {
                fprintf( code, "    void %s::%s_", interfaceName, interfaceNameStripped );
                printEvent( code, e, true );
                fprintf( code, "\n" );
                fprintf( code, "    {\n" );
                fprintf( code, "    }\n" );
                fprintf( code, "\n" );
                fprintf( code, "    void %s::", interfaceName );
                printEventHandlerSignature( code, e, interfaceName );
                fprintf( code, " {\n" );
                fprintf( code, "        static_cast<%s *>(data)->%s_%s(", interfaceName, interfaceNameStripped, e.name.constData() );
                bool needsComma = false;
                for (const WaylandArgument& a : e.arguments) {
                    if ( needsComma ) {
                        fprintf( code, "," );
                    }

                    needsComma = true;
                    fprintf( code, "\n" );
                    const char *argumentName = a.name.constData();

                    if ( a.type == "string" ) {
                        fprintf( code, "            QString::fromUtf8(%s)", argumentName );
                    }
                    else {
                        fprintf( code, "            %s", argumentName );
                    }
                }
                fprintf( code, ");\n" );

                fprintf( code, "    }\n" );
                fprintf( code, "\n" );
            }
            fprintf( code, "    const struct %s_listener %s::m_%s_listener = {\n", interfaceName,
                     interfaceName, interfaceName );
            for (const WaylandEvent& e : interface.events) {
                fprintf( code, "        %s::%s,\n", interfaceName, e.name.constData() );
            }
            fprintf( code, "    };\n" );
            fprintf( code, "\n" );

            fprintf( code, "    void %s::init_listener()\n",                         interfaceName );
            fprintf( code, "    {\n" );
            fprintf( code, "        %s_add_listener(m_%s, &m_%s_listener, this);\n", interfaceName,
                     interfaceName, interfaceName );
            fprintf( code, "    }\n" );
        }
    }
    fprintf( code, "}\n" );
    fprintf( code, "\n" );
}


void Scanner::printErrors() {
    if ( m_xml->hasError() ) {
        fprintf( stderr, "XML error: %s\nLine %lld, column %lld\n", m_xml->errorString().toLocal8Bit().constData(), m_xml->lineNumber(), m_xml->columnNumber() );
    }
}


int main( int argc, char **argv ) {
    QCoreApplication app( argc, argv );
    Scanner          scanner;

    if ( !scanner.parseArguments( argc, argv ) ) {
        scanner.printUsage();
        return EXIT_FAILURE;
    }

    if ( !scanner.process() ) {
        scanner.printErrors();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
