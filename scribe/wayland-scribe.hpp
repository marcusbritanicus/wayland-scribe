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

#pragma once

#include <vector>
#include <filesystem>

#include <pugixml.hpp>

namespace fs = std::filesystem;

namespace Wayland {
    class Scribe;
}

class Wayland::Scribe {
    public:
        explicit Scribe();
        ~Scribe() = default;

        bool process();

        void setRunMode( const std::string& specFile, bool server, uint file, const std::string& output );
        void setArgs( const std::string& headerPath, const std::string& prefix, const std::vector<std::string>& includes );

    private:
        struct WaylandEnumEntry {
            std::string name;
            std::string value;
            std::string summary;
        };

        struct WaylandEnum {
            std::string                   name;
            std::vector<WaylandEnumEntry> entries;
        };

        struct WaylandArgument {
            std::string name;
            std::string type;
            std::string interface;
            std::string summary;
            bool        allowNull;
        };

        struct WaylandEvent {
            bool                         request;
            std::string                  name;
            std::string                  type;
            std::vector<WaylandArgument> arguments;
        };

        struct WaylandInterface {
            std::string               name;
            int                       version;

            std::vector<WaylandEnum>  enums;
            std::vector<WaylandEvent> events;
            std::vector<WaylandEvent> requests;
        };

        void generateServerHeader( FILE *head, std::vector<WaylandInterface> interfaces );
        void generateServerCode( FILE *head, std::vector<WaylandInterface> interfaces );

        void generateClientHeader( FILE *head, std::vector<WaylandInterface> interfaces );
        void generateClientCode( FILE *head, std::vector<WaylandInterface> interfaces );

        WaylandEvent readEvent( pugi::xml_node& xml, bool request );
        Scribe::WaylandEnum readEnum( pugi::xml_node& xml );
        Scribe::WaylandInterface readInterface( pugi::xml_node& xml );
        std::string waylandToCType( const std::string& waylandType, const std::string& interface );
        const Scribe::WaylandArgument *newIdArgument( const std::vector<WaylandArgument>& arguments );

        void printEvent( FILE *f, const WaylandEvent& e, bool omitNames = false, bool withResource = false, bool capitalize = false );
        void printEventHandlerSignature( FILE *f, const WaylandEvent& e, const char *interfaceName );
        void printEnums( FILE *f, const std::vector<WaylandEnum>& enums );

        std::string stripInterfaceName( const std::string& name, bool );
        bool ignoreInterface( const std::string& name );

        bool mServer = false;

        /**
         * File(s) to be generated
         * 0: both source and header
         * 1: source
         * 2: header
         */
        uint mFile = 0;

        std::string mProtocolName;
        std::string mProtocolFilePath;
        std::string mScannerName;
        std::string mHeaderPath;
        std::string mPrefix;
        std::string mOutputSrcPath;
        std::string mOutputHdrPath;
        std::vector<std::string> mIncludes;
};
