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
#include "argparser.hpp"

void printHelpText( bool err ) {
    ( err ? std::cerr : std::cout ) << "Wayland::Scribe " << PROJECT_VERSION << std::endl;
    ( err ? std::cerr : std::cout ) << "A simple program to generate C++ code from Wayland protocol XML spec." << std::endl << std::endl;

    ( err ? std::cerr : std::cout ) << "Usage:" << std::endl;
    ( err ? std::cerr : std::cout ) << "  wayland-scribe --[server|client] --[source|header] [options] specfile output" << std::endl << std::endl;

    ( err ? std::cerr : std::cout ) << "Options:" << std::endl;
    ( err ? std::cerr : std::cout ) << "  -s|--server               Generate the server-side wrapper code for the protocol given in <spec>." << std::endl;
    ( err ? std::cerr : std::cout ) << "  -c|--client               Generate the client-side wrapper code for the protocol given in <spec>." << std::endl;

    ( err ? std::cerr : std::cout ) << "  --source                  Generate the source code for the given protocol, store it in output." << std::endl;
    ( err ? std::cerr : std::cout ) << "  --header                  Generate the header code for the given protocol, store it in output." << std::endl;

    ( err ? std::cerr : std::cout ) << "  --header-path <path>      Path to the c header of this protocol (optional)." << std::endl;
    ( err ? std::cerr : std::cout ) << "  --prefix <prefix>         Prefix of interfaces (to be stripped; optional)." << std::endl;
    ( err ? std::cerr : std::cout ) << "  --include <include>       Add extra includes (can speficy multiple times; optional)." << std::endl << std::endl;

    ( err ? std::cerr : std::cout ) << "Arguments:" << std::endl;
    ( err ? std::cerr : std::cout ) << "  specFile                  Path to the protocol xml specification file." << std::endl;
    ( err ? std::cerr : std::cout ) << "  output                    Optional path in which generated code is stored. Auto-generated filename will be used if unspecified" << std::endl << std::endl;

    ( err ? std::cerr : std::cout ) << "Other options:" << std::endl;
    ( err ? std::cerr : std::cout ) << "  -h|--help                 Print this help text and exit." << std::endl;
    ( err ? std::cerr : std::cout ) << "  -v|--version              Print version information and exit." << std::endl;
}


void printVersion() {
    std::cout << "Wayland::Scribe " << PROJECT_VERSION << std::endl;
    std::cout << "A simple program to generate C++ code from Wayland protocol XML spec." << std::endl << std::endl;
}


int main( int argc, char **argv ) {
    ArgParser parser( "Wayland Scribe", PROJECT_VERSION, "A simple program to generate C++ code from Wayland protocol XML spec." );

    parser.addHelpOption( printHelpText );
    parser.addVersionOption( printVersion );

    parser.addOption( "server",      's',  ArgParser::NoArgument,                                  "Generate the server-side wrapper code for the protocol given in <spec>." );
    parser.addOption( "client",      'c',  ArgParser::NoArgument,                                  "Generate the client-side wrapper code for the protocol given in <spec>." );

    parser.addOption( "source",      '\0', ArgParser::NoArgument,                                  "Generate the source code for the given protocol, store it in output." );
    parser.addOption( "header",      '\0', ArgParser::NoArgument,                                  "Generate the header code for the given protocol, store it in output." );

    parser.addOption( "header-path", '\0', ArgParser::RequiredArgument,                            "Path to the c header of this protocol (optional)." );
    parser.addOption( "prefix",      '\0', ArgParser::RequiredArgument,                            "Prefix of interfaces (to be stripped; optional)." );
    parser.addOption( "include",     '\0', ArgParser::RequiredArgument | ArgParser::AllowMultiple, "Add extra includes (can speficy multiple times; optional)." );

    parser.addPositional( "specFile", ArgParser::PositionalRequired, "Path to the protocol xml specification file." );
    parser.addPositional( "output",   ArgParser::PositionalOptional, "Optional path in which generated code is stored. Auto-generated filename will be used if unspecified." );

    parser.parse( argc, argv );

    /** == Server and Client: Both set or both not set == **/
    if ( parser.isSet( "server" ) == parser.isSet( "client" ) ) {
        std::cerr << "[Error]: Please specify one of --server or --client" << std::endl << std::endl;
        printHelpText( true );

        return EXIT_FAILURE;
    }

    /*
     * * == Protocol XML Spec File == *
     ** Get the spec file
     */
    std::string specFile = parser.positionalValue( "specFile" );

    if ( specFile.empty() ) {
        std::cerr << "[Error]: Please specify protocol xml path" << std::endl << std::endl;
        printHelpText( true );

        return EXIT_FAILURE;
    }

    /*** ------- End of error checking ------- ***/

    /** Init our worker */
    Wayland::Scribe scribe;

    // /** Ensure that that file exists */
    if ( fs::exists( specFile ) == false ) {
        std::cerr << "[Error]: Unable to locate the file: " << specFile.c_str();
        return EXIT_FAILURE;
    }

    /** Get the files to be generated */
    uint file = ( parser.isSet( "--source" ) ? ( parser.isSet( "--header" ) ? 0 : 1 ) : ( parser.isSet( "--header" ) ? 2 : 0 ) );

    /** Set the output file name, if specified */
    std::string output = parser.positionalValue( 1 );

    /** Set the main running mode */
    scribe.setRunMode( specFile, parser.isSet( "--server" ), file, output );

    /** Update other arguments */
    // Get the header-path, prefix, and add-include values, with defaults if not provided
    std::string              headerPath  = parser.value( "header-path" );
    std::string              prefix      = parser.value( "prefix" );
    std::vector<std::string> addIncludes = parser.values( "include" );

    // Update the scribe with the arguments
    scribe.setArgs( headerPath, prefix, addIncludes );

    if ( !scribe.process() ) {
        // scribe.printErrors();
        std::cerr << "Errors encountered while parsing the xml file" << std::endl << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
