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
#include <QCommandLineParser>
#include <iostream>

#include "wayland-scribe.hpp"

void printHelpText() {
    std::cout << "Wayland::Scribe " << PROJECT_VERSION << std::endl << std::endl;

    std::cout << "Usage:" << std::endl;
    std::cout << "  wayland-scribe --[server|client] specfile [options] --[source|header] output" << std::endl << std::endl;

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
    parser.addOption( { "source", "Generate the header code for the given protocol." } );
    parser.addOption( { "header", "Generate the source code for the given protocol." } );
    parser.addOption( { "header-path", "Path to the c header of this protocol (optional).", "path" } );
    parser.addOption( { "prefix", "Prefix of interfaces (to be stripped; optional).", "prefix" } );
    parser.addOption( { "add-include", "Add extra include path (can speficy multiple times; optional).", "include" } );

    /** Help and version */
    parser.addOption( { { "h", "help" }, "Print this help" } );
    parser.addOption( { { "v", "version" }, "Print application version and exit" } );

    parser.addPositionalArgument( "output", "Name of the output file to be generated." );

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
        std::cerr << "[Error]: Please specify either --server or --client" << std::endl << std::endl;
        printHelpText();

        return EXIT_FAILURE;
    }

    if ( (strcmp( argv[ 1 ], "--server" ) != 0) && (strcmp( argv[ 1 ], "--client" ) != 0) ) {
        std::cerr << "[Error]: The first argument should be --server or --client" << std::endl << std::endl;
        printHelpText();

        return EXIT_FAILURE;
    }

    if ( parser.positionalArguments().length() && (parser.positionalArguments().length() != 1) ) {
        QStringList posArgs = parser.positionalArguments();

        std::cerr << "[Warning]: Ignoring the extra argument" << (posArgs.length() == 1 ? ": (" : "s: (");

        for ( int i = 1; i < posArgs.length(); i++ ) {
            std::cerr << posArgs.at( i ).toStdString() << (i == posArgs.length() - 1 ? ")" : " ");
        }

        std::cerr << std::endl << std::endl;
    }

    /*** ------- End of error checking ------- ***/

    /** Init our worker */
    Wayland::Scribe scribe;

    /** Get the spec file */
    QString specFile;

    if ( parser.isSet( "server" ) ) {
        specFile = parser.value( "server" );
    }

    else if ( parser.isSet( "client" ) ) {
        specFile = parser.value( "client" );
    }

    // /** Ensure that that file exists */
    if ( QFile::exists( specFile ) == false ) {
        std::cerr << "[Error]: Unable to locate the file" << specFile.toUtf8().constData();
        return EXIT_FAILURE;
    }

    /** Get the files to be generated */
    uint file = (parser.isSet( "source" ) ? (parser.isSet( "header" ) ? 0 : 1) : (parser.isSet( "header" ) ? 2 : 0) );

    /** Set the output file name, if specified */
    QString output = (parser.positionalArguments().count() ? parser.positionalArguments().at( 0 ) : "");

    /** Set the main running mode */
    scribe.setRunMode( specFile, parser.isSet( "server" ), file, output );

    /** Update other arguments */
    scribe.setArgs( parser.value( "header-path" ), parser.value( "prefix" ), parser.values( "add-include" ) );

    if ( !scribe.process() ) {
        scribe.printErrors();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
