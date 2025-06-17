#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <utility>
#include <functional>
#include <iostream>
#include <cstdlib>
#include <cctype>

class ArgParser {
    public:
        enum OptionFlags {
            NoArgument       = 0,
            RequiredArgument = 1,
            OptionalArgument = 2,
            AllowMultiple    = 4
        };

        enum PositionalFlags {
            PositionalRequired = 0,
            PositionalOptional = 1
        };

        ArgParser( std::string name, std::string version, std::string descr ) {
            mName    = name;
            mVersion = version;
            mDescr   = descr;
        }

        // Add a named option (automatically generates short option if possible)
        void addOption( const std::string& longName, char shortName = '\0', int flags = NoArgument, const std::string& helpText = "", bool required = false ) {
            if ( longName.empty() ) {
                error( "Option name cannot be empty" );
            }

            Option opt;
            opt.longName   = longName;
            opt.flags      = flags;
            opt.helpText   = helpText;
            opt.isRequired = required;
            opt.isSet      = false;

            // Try to find a suitable short option
            if ( shortName == '\0' ) {
                shortName = findAvailableShortOption( longName );
            }

            if ( shortName != '\0' ) {
                opt.shortName = shortName;
                optionsMap[ "-" + std::string( 1, shortName ) ] = opt;
                shortOptionToName[ shortName ] = longName;
            }

            optionsMap[ "--" + longName ] = opt;
            longOptionNames.push_back( longName );
        }

        void addPositional( const std::string& name, int flags = PositionalOptional, const std::string& helpText = "" ) {
            PositionalArg arg;

            arg.name     = name;
            arg.helpText = helpText;
            arg.flags    = flags;
            arg.value    = "";
            arg.isSet    = false;

            positionalArgs.push_back( arg );
            positionalArgsMap[ name ] = positionalArgs.size() - 1;

            /** This is a required positional: bump the minimum pos args count */
            if ( flags == PositionalRequired ) {
                minPositionalArgs++;
            }

            /** Bump the maximum pos args count */
            maxPositionalArgs++;
        }

        void setPositionalCount( size_t required, size_t optional = 0 ) {
            minPositionalArgs = required;
            maxPositionalArgs = required + optional;
        }

        // Add a Help option
        void addHelpOption( std::function<void(bool)> callback = nullptr ) {
            mHelpCallback = callback;
            addOption( "help", '\0', NoArgument, "Show this help message" );
        }

        // Add a mVersion option
        void addVersionOption( std::function<void()> callback = nullptr ) {
            mmVersionCallback = callback;
            addOption( "version", '\0', NoArgument, "Show mVersion information" );
        }

        void parse( int argc, char *argv[] ) {
            mName = argv[ 0 ];

            // First check for help/mVersion requests
            for (int i = 1; i < argc; i++) {
                std::string arg = argv[ i ];

                if ( ( arg == "--help" ) || ( arg == "-h" ) ) {
                    showHelp();
                    exit( 0 );
                }

                if ( ( arg == "--mVersion" ) || ( arg == "-v" ) ) {
                    showmVersion();
                    exit( 0 );
                }
            }

            size_t      positionalIndex = 0;
            Option      *currentOpt     = nullptr;
            std::string currentOptName;

            for (int i = 1; i < argc; ) {
                std::string arg = argv[ i ];

                // Handle --option=value syntax
                if ( arg.substr( 0, 2 ) == "--" ) {
                    size_t equalPos = arg.find( '=' );

                    if ( equalPos != std::string::npos ) {
                        std::string optName  = arg.substr( 0, equalPos );
                        std::string optValue = arg.substr( equalPos + 1 );

                        auto it = optionsMap.find( optName );

                        if ( it == optionsMap.end() ) {
                            error( "Unknown option: " + optName );
                        }

                        it->second.values.push_back( optValue );
                        it->second.isSet = true;
                        i++;
                        continue;
                    }
                }

                // Handle regular options
                if ( arg[ 0 ] == '-' ) {
                    if ( currentOpt && ( currentOpt->flags & RequiredArgument ) && currentOpt->values.empty() ) {
                        error( "Missing argument for: " + currentOptName );
                    }

                    // Process short (-x) or long (--xyz) options
                    if ( arg.substr( 0, 2 ) == "--" ) {
                        // Long option
                        auto it = optionsMap.find( arg );

                        if ( it == optionsMap.end() ) {
                            error( "Unknown option: " + arg );
                        }

                        currentOpt        = &it->second;
                        currentOptName    = arg;
                        currentOpt->isSet = true;

                        if ( currentOpt->flags & RequiredArgument ) {
                            if ( i + 1 >= argc ) {
                                error( "Option requires argument: " + arg );
                            }

                            currentOpt->values.push_back( argv[ ++i ] );
                        }
                        else if ( currentOpt->flags & OptionalArgument ) {
                            // For OptionalArgument, check if next arg is a value (not another option)
                            if ( ( i + 1 < argc ) && ( argv[ i + 1 ][ 0 ] != '-' ) ) {
                                currentOpt->values.push_back( argv[ ++i ] );
                            }

                            // Else leave values empty but still mark as set
                        }
                        else {
                            // NoArgument case
                            currentOpt->values.push_back( "" );
                        }
                    }
                    else {
                        // Short option(s)
                        for (size_t j = 1; j < arg.size(); j++) {
                            std::string optName = "-" + std::string( 1, arg[ j ] );
                            auto        it      = optionsMap.find( optName );

                            if ( it == optionsMap.end() ) {
                                error( "Unknown option: " + optName );
                            }

                            currentOpt        = &it->second;
                            currentOptName    = optName;
                            currentOpt->isSet = true;

                            if ( currentOpt->flags & RequiredArgument ) {
                                if ( ( j + 1 < arg.size() ) || ( i + 1 >= argc ) ) {
                                    error( "Option requires argument: " + optName );
                                }

                                currentOpt->values.push_back( argv[ ++i ] );
                                break;
                            }
                            else if ( currentOpt->flags & OptionalArgument ) {
                                if ( j + 1 < arg.size() ) {
                                    // Value is attached to short option (-o3)
                                    currentOpt->values.push_back( arg.substr( j + 1 ) );
                                    break;
                                }
                                else if ( ( i + 1 < argc ) && ( argv[ i + 1 ][ 0 ] != '-' ) ) {
                                    // Value is next argument
                                    currentOpt->values.push_back( argv[ ++i ] );
                                    break;
                                }

                                // Else leave values empty but still mark as set
                            }
                            else {
                                // NoArgument case
                                currentOpt->values.push_back( "" );
                            }
                        }
                    }

                    i++;
                    continue;
                }

                // Handle positional arguments
                if ( positionalIndex < maxPositionalArgs ) {
                    if ( positionalIndex < positionalArgs.size() ) {
                        positionalArgs[ positionalIndex ].value = arg;
                        positionalArgs[ positionalIndex ].isSet = true;
                    }
                    else {
                        PositionalArg extraArg;
                        extraArg.name  = "arg" + std::to_string( positionalIndex + 1 );
                        extraArg.value = arg;
                        extraArg.isSet = true;
                        positionalArgs.push_back( extraArg );
                    }

                    positionalIndex++;
                }
                else {
                    error( "Unexpected positional argument: " + arg );
                }

                i++;
            }

            // Validate required positionals
            if ( positionalIndex < minPositionalArgs ) {
                error( "Not enough positional arguments (minimum " + std::to_string( minPositionalArgs ) + " required)" );
            }

            // Validate named required positionals
            for (const auto& arg : positionalArgs) {
                if ( ( ( arg.flags & PositionalOptional ) == 0 ) && !arg.isSet && !arg.name.empty() ) {
                    error( "Required positional argument missing: " + arg.name );
                }
            }

            // Validate required options
            for (const auto& [ name, opt ] : optionsMap) {
                if ( opt.isRequired && !opt.isSet ) {
                    error( "Required option missing: " + opt.longName );
                }
            }
        }

        // Option access methods
        bool isSet( const std::string& option ) const {
            // Try with -- prefix first
            auto it = optionsMap.find( "--" + option );

            if ( it != optionsMap.end() ) {
                return it->second.isSet;
            }

            // Try with - prefix (short option)
            if ( option.size() == 1 ) {
                it = optionsMap.find( "-" + option );

                if ( it != optionsMap.end() ) {
                    return it->second.isSet;
                }
            }

            return false;
        }

        bool hasValue( const std::string& option ) const {
            auto it = findOption( option );

            return it != optionsMap.end() && !it->second.values.empty();
        }

        std::string value( const std::string& option ) const {
            auto it = findOption( option );

            if ( ( it == optionsMap.end() ) || it->second.values.empty() ) {
                return "";
            }

            return it->second.values.front();
        }

        std::vector<std::string> values( const std::string& option ) const {
            auto it = findOption( option );

            if ( it == optionsMap.end() ) {
                return {};
            }

            return it->second.values;
        }

        // Positional argument access methods
        bool isPositionalSet( const std::string& name ) const {
            auto it = positionalArgsMap.find( name );

            return it != positionalArgsMap.end() &&
                   it->second < positionalArgs.size() &&
                   positionalArgs[ it->second ].isSet;
        }

        std::string positionalValue( const std::string& name ) const {
            auto it = positionalArgsMap.find( name );

            if ( ( it == positionalArgsMap.end() ) || ( it->second >= positionalArgs.size() ) ) {
                return "";
            }

            return positionalArgs[ it->second ].value;
        }

        bool isPositionalSet( size_t index ) const {
            return index < positionalArgs.size() && positionalArgs[ index ].isSet;
        }

        std::string positionalValue( size_t index ) const {
            if ( index >= positionalArgs.size() ) {
                return "";
            }

            return positionalArgs[ index ].value;
        }

        std::vector<std::string> getPositionalArgs() const {
            std::vector<std::string> result;

            for (const auto& arg : positionalArgs) {
                if ( arg.isSet ) {
                    result.push_back( arg.value );
                }
            }
            return result;
        }

        std::vector<std::string> getPassedOptionNames() const {
            std::vector<std::string> passedOptions;

            for (const auto& name : longOptionNames) {
                if ( isSet( name ) ) {
                    passedOptions.push_back( "--" + name );

                    if ( !shortOptionToName.empty() ) {
                        for (const auto& [ shortName, longName ] : shortOptionToName) {
                            if ( longName == name ) {
                                passedOptions.push_back( "-" + std::string( 1, shortName ) );
                                break;
                            }
                        }
                    }
                }
            }
            return passedOptions;
        }

    private:
        struct Option {
            std::string              longName;
            char                     shortName = '\0';
            int                      flags     = 0;
            std::string              helpText;
            bool                     isRequired = false;
            std::vector<std::string> values;
            bool                     isSet = false;
        };

        struct PositionalArg {
            std::string name;
            std::string helpText;
            int         flags;
            std::string value;
            bool        isSet = false;
        };

        std::unordered_map<std::string, Option> optionsMap;

        std::unordered_map<char, std::string> shortOptionToName;
        std::vector<std::string> longOptionNames;

        std::vector<PositionalArg> positionalArgs;
        std::unordered_map<std::string, size_t> positionalArgsMap;
        size_t minPositionalArgs = 0;
        size_t maxPositionalArgs = 0;

        std::string mName;
        std::string mVersion = "1.0";
        std::string mDescr   = "";

        std::function<void(bool)> mHelpCallback = nullptr;
        std::function<void()> mmVersionCallback = nullptr;

        void error( const std::string& message ) const {
            std::cerr << "Error: " << message << std::endl;
            showHelp( true );
            exit( 1 );
        }

        char findAvailableShortOption( const std::string& longName ) {
            if ( longName.empty() ) { return '\0'; }

            // Try first character of long name
            char candidate = tolower( longName[ 0 ] );

            if ( isValidShortOption( candidate ) ) {
                return candidate;
            }

            // Try subsequent characters
            for (size_t i = 1; i < longName.size(); i++) {
                candidate = tolower( longName[ i ] );

                if ( isValidShortOption( candidate ) ) {
                    return candidate;
                }
            }

            return '\0';
        }

        bool isValidShortOption( char c ) {
            return isalnum( c ) && shortOptionToName.find( c ) == shortOptionToName.end();
        }

        std::unordered_map<std::string, Option>::const_iterator findOption( const std::string& name ) const {
            // Try with -- prefix first
            auto it = optionsMap.find( "--" + name );

            if ( it != optionsMap.end() ) { return it; }

            // Try with - prefix (short option)
            if ( name.size() == 1 ) {
                it = optionsMap.find( "-" + name );

                if ( it != optionsMap.end() ) { return it; }
            }

            // Try exact match (might already have prefix)
            it = optionsMap.find( name );

            if ( it != optionsMap.end() ) { return it; }

            return optionsMap.end();
        }

        void showHelp( bool isError = false ) const {
            if ( mHelpCallback ) {
                mHelpCallback( isError );
            }

            auto& out = isError ? std::cerr : std::cout;

            out << "Usage: " << mName << " [options]";

            // Show required positionals
            for (size_t i = 0; i < minPositionalArgs; i++) {
                out << " " << ( i < positionalArgs.size() ? positionalArgs[ i ].name : "arg" + std::to_string( i + 1 ) );
            }

            // Show optional positionals
            for (size_t i = minPositionalArgs; i < maxPositionalArgs; i++) {
                out << " [" << ( i < positionalArgs.size() ? positionalArgs[ i ].name : "arg" + std::to_string( i + 1 ) ) << "]";
            }

            out << "\n\nOptions:\n";

            // Print options
            for (const auto& name : longOptionNames) {
                const auto& opt = optionsMap.at( "--" + name );

                out << "  --" << name;

                if ( opt.shortName != '\0' ) {
                    out << ", -" << opt.shortName;
                }

                if ( opt.flags & RequiredArgument ) {
                    out << " ARG";
                }
                else if ( opt.flags & OptionalArgument ) {
                    out << " [ARG]";
                }

                out << "\t" << opt.helpText << "\n";
            }

            // Print positional arguments help if any are named
            if ( !positionalArgs.empty() ) {
                out << "\nPositional arguments:\n";
                for (size_t i = 0; i < positionalArgs.size(); i++) {
                    out << "  " << positionalArgs[ i ].name;

                    if ( i >= minPositionalArgs ) {
                        out << " (optional)";
                    }

                    out << "\t" << positionalArgs[ i ].helpText << "\n";
                }
            }

            out << std::endl;
        }

        void showmVersion() const {
            if ( mmVersionCallback ) {
                mmVersionCallback();
            }

            std::cout << mName << " " << mVersion << "\n";

            if ( !mDescr.empty() ) {
                std::cout << mDescr << "\n";
            }

            std::cout << std::endl;
        }
};
