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


#include <iostream>

#include "wayland-scribe.hpp"
#include "cxxopts.hpp"

void printHelpText( bool err ) {
    ( err ? std::cerr : std::cout ) << "Wayland::Scribe " << PROJECT_VERSION << std::endl << std::endl;

    ( err ? std::cerr : std::cout ) << "Usage:" << std::endl;
    ( err ? std::cerr : std::cout ) << "  wayland-scribe --[server|client] specfile [options] --[source|header] output" << std::endl << std::endl;

    ( err ? std::cerr : std::cout ) << "Options:" << std::endl;
    ( err ? std::cerr : std::cout ) << "  --header-path <path>      Path to the c header of this protocol (optional)." << std::endl;
    ( err ? std::cerr : std::cout ) << "  --prefix <prefix>         Prefix of interfaces (to be stripped; optional)." << std::endl;
    ( err ? std::cerr : std::cout ) << "  --add-include <include>   Add extra include path (can speficy multiple times; optional)." << std::endl << std::endl;

    ( err ? std::cerr : std::cout ) << "Other options:" << std::endl;
    ( err ? std::cerr : std::cout ) << "  -h|--help                 Print this help text and exit." << std::endl;
    ( err ? std::cerr : std::cout ) << "  -v|--version              Print version information and exit." << std::endl;
}


void printVersion() {
    std::cout << "Wayland::Scribe " << PROJECT_VERSION << std::endl;
}


int main( int argc, char **argv ) {
    cxxopts::Options options( "Wayland::Scribe", "A simple program to generate C++ code from Wayland protocol XML spec." );

    options.add_options()
    ( "h,help", "Print this help" )
    ( "v,version", "Print application version and exit" )
    ( "s,server", "Generate the server-side wrapper code for the given protocol.", cxxopts::value<std::string> () )
    ( "c,client", "Generate the client-side wrapper code for the given protocol.", cxxopts::value<std::string> () )
    ( "source", "Generate the header code for the given protocol." )
    ( "header", "Generate the source code for the given protocol." )
    ( "header-path", "Path to the c header of this protocol (optional).", cxxopts::value<std::string> () )
    ( "prefix", "Prefix of interfaces (to be stripped; optional).", cxxopts::value<std::string> () )
    ( "add-include", "Additional include paths", cxxopts::value<std::vector<std::string> > () )
    ( "output", "N", cxxopts::value<std::vector<std::string> > () );

    options.parse_positional( { "output" } );

    auto result = options.parse( argc, argv );

    /** == Help and Version == **/
    if ( result.count( "help" ) ) {
        printHelpText( false );
        return 0;
    }

    if ( result.count( "version" ) ) {
        printVersion();
        return 0;
    }

    /** == Server and Client == **/
    if ( result.count( "server" ) == result.count( "client" ) ) {
        std::cerr << "[Error]: Please specify one of --server or --client" << std::endl << std::endl;
        printHelpText( true );

        return EXIT_FAILURE;
    }

    // if ( ( strcmp( argv[ 1 ], "--server" ) != 0 ) && ( strcmp( argv[ 1 ], "--client" ) != 0 ) ) {
    //     std::cerr << "[Error]: The first argument should be --server or --client" << std::endl <<
    // std::endl;
    //     printHelpText();

    //     return EXIT_FAILURE;
    // }

    if ( result.count( "output" ) != 1 ) {
        std::vector<std::string> posArgs = result[ "output" ].as<std::vector<std::string> >();

        std::cerr << "[Warning]: Ignoring the extra argument" << ( posArgs.size() == 1 ? ": (" : "s: (" );

        for ( size_t i = 1; i < posArgs.size(); i++ ) {
            std::cerr << posArgs.at( i ) << ( i == posArgs.size() - 1 ? ")" : " " );
        }

        std::cerr << std::endl << std::endl;
    }

    /*** ------- End of error checking ------- ***/

    /** Init our worker */
    Wayland::Scribe scribe;

    /** Get the spec file */
    std::string specFile;

    if ( result.count( "server" ) ) {
        specFile = result[ "server" ].as<std::string>();
    }

    else if ( result.count( "client" ) ) {
        specFile = result[ "client" ].as<std::string>();
    }

    // /** Ensure that that file exists */
    if ( fs::exists( specFile ) == false ) {
        std::cerr << "[Error]: Unable to locate the file" << specFile.c_str();
        return EXIT_FAILURE;
    }

    /** Get the files to be generated */
    uint file = ( result.count( "source" ) ? ( result.count( "header" ) ? 0 : 1 ) : ( result.count( "header" ) ? 2 : 0 ) );

    /** Set the output file name, if specified */
    std::vector<std::string> posArgs = result[ "output" ].as<std::vector<std::string> >();
    std::string              output  = ( posArgs.size() ? posArgs.at( 0 ) : "" );

    /** Set the main running mode */
    scribe.setRunMode( specFile, result.count( "server" ), file, output );

    /** Update other arguments */
    scribe.setArgs( result[ "header-path" ].as<std::string>(), result[ "prefix" ].as<std::string>(), result[ "add-include" ].as<std::vector<std::string> >() );

    if ( !scribe.process() ) {
        // scribe.printErrors();
        std::cerr << "Errors encountered while parsing the xml file" << std::endl << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
