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
#include <QCommandLineParser>
#include <iostream>

#include "wayland-scribe.hpp"

void printHelpText() {
    std::cout << "Wayland::Scribe " << PROJECT_VERSION << std::endl << std::endl;

    std::cout << "Usage:" << std::endl;
    std::cout << "  wayland-scribe --[server|client] specfile [options]" << std::endl << std::endl;

    std::cout << "Options:" << std::endl;
    std::cout << "  --header-path <path>      Path to the c header of this protocol (optional)." << std::endl;
    std::cout << "  --prefix <prefix>         Prefix of interfaces (to be stripped; optional)." << std::endl;
    std::cout << "  --add-include <include>   Add extra include path (can speficy multiple times; optional)." << std::endl << std::endl;

    std::cout << "Other options:" << std::endl;
    std::cout << "  -h|--help                 Print this help text and exit." << std::endl;
    std::cout << "  -v|--version              Print version information and exit." << std::endl;
}


void printVersion() {
    std::cout << "Wayland::Scribe " << PROJECT_VERSION << std::endl;
}


int main( int argc, char **argv ) {
    QCoreApplication app( argc, argv );

    QCommandLineParser parser;

    parser.addOption( { "server", "Generate the server-side wrapper code for the given protocol.", "spec-file" } );
    parser.addOption( { "client", "Generate the client-side wrapper code for the given protocol.", "spec-file" } );
    parser.addOption( { "header-path", "Path to the c header of this protocol (optional).", "path" } );
    parser.addOption( { "prefix", "Prefix of interfaces (to be stripped; optional).", "prefix" } );
    parser.addOption( { "add-include", "Add extra include path (can speficy multiple times; optional).", "include" } );

    /** Help and version */
    parser.addOption( { { "h", "help" }, "Print this help" } );
    parser.addOption( { { "v", "version" }, "Print application version and exit" } );

    /** Process the CLI arguments */
    parser.process( app );

    /** == Help and Version == **/
    if ( parser.isSet( "help" ) ) {
        printHelpText();

        return 0;
    }

    if ( parser.isSet( "version" ) ) {
        printVersion();

        return 0;
    }

    if ( parser.isSet( "server" ) && parser.isSet( "client" ) ) {
        std::cerr << "[Error]: Please specify only one of --server|--client" << std::endl << std::endl;
        printHelpText();

        return EXIT_FAILURE;
    }

    if ( (parser.isSet( "server" ) == false) && (parser.isSet( "client" ) == false) ) {
        std::cerr << "[Error]: Please specify one of --server|--client" << std::endl << std::endl;
        printHelpText();

        return EXIT_FAILURE;
    }

    if ( (strcmp( argv[ 1 ], "--server" ) != 0) && (strcmp( argv[ 1 ], "--client" ) != 0) ) {
        std::cerr << "[Error]: Please specify one of --server|--client" << std::endl << std::endl;
        printHelpText();

        return EXIT_FAILURE;
    }

    Wayland::Scribe scribe;

    QString specFile = (parser.isSet( "server" ) ? parser.value( "server" ) : parser.value( "client" ) );

    /** Set the protocol file path */
    scribe.setRunMode( specFile, parser.isSet( "server" ) );

    /** Update other arguments */
    scribe.setArgs( parser.value( "header-path" ), parser.value( "prefix" ), parser.values( "add-include" ) );

    if ( !scribe.process() ) {
        scribe.printErrors();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
