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

namespace Wayland {
    class Scribe;
}

class Wayland::Scribe {
    public:
        explicit Scribe();
        ~Scribe() { delete mXml; }

        bool process();
        void printErrors();

        void setRunMode( QString specFile, bool isServer );
        void setArgs( QString headerPath, QString prefix, QStringList includes );

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

        void generateServerHeader( FILE *head, std::vector<WaylandInterface> interfaces );
        void generateServerCode( FILE *head, std::vector<WaylandInterface> interfaces );

        void generateClientHeader( FILE *head, std::vector<WaylandInterface> interfaces );
        void generateClientCode( FILE *head, std::vector<WaylandInterface> interfaces );

        QByteArray byteArrayValue( const QXmlStreamReader& xml, const char *name );
        int intValue( const QXmlStreamReader& xml, const char *name, int defaultValue = 0 );
        bool boolValue( const QXmlStreamReader& xml, const char *name );
        WaylandEvent readEvent( QXmlStreamReader& xml, bool request );
        Scribe::WaylandEnum readEnum( QXmlStreamReader& xml );
        Scribe::WaylandInterface readInterface( QXmlStreamReader& xml );
        QByteArray waylandToCType( const QByteArray& waylandType, const QByteArray& interface );
        QByteArray waylandToQtType( const QByteArray& waylandType, const QByteArray& interface, bool cStyleArray );
        const Scribe::WaylandArgument *newIdArgument( const std::vector<WaylandArgument>& arguments );

        void printEvent( FILE *f, const WaylandEvent& e, bool omitNames = false, bool withResource = false, bool capitalize = false );
        void printEventHandlerSignature( FILE *f, const WaylandEvent& e, const char *interfaceName );
        void printEnums( FILE *f, const std::vector<WaylandEnum>& enums );

        QByteArray stripInterfaceName( const QByteArray& name, bool );
        bool ignoreInterface( const QByteArray& name );

        bool mServer = false;
        bool mClient = false;

        QByteArray mProtocolName;
        QByteArray mProtocolFilePath;
        QByteArray mScannerName;
        QByteArray mHeaderPath;
        QByteArray mPrefix;
        QList<QByteArray> mIncludes;
        QXmlStreamReader *mXml = nullptr;
};
