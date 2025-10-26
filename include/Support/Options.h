

 #ifndef PLANKTON_OPTIONS_H
 #define PLANKTON_OPTIONS_H
 
 #include <map>
 #include <set>
 #include <string>
 #include <vector>
 #include <iostream>
 #include <unordered_set>
 #include <sstream>
 #include <limits>
 #include <cassert>
 #include <algorithm>
 
 
 /**
  * Option Class for different tools
  */
 enum TOOL
 {
     DASM,
     SIM,
     LSP,
 };
 
 class OptionBase
 {
 protected:
     /// Name/description pairs.
     typedef std::pair<std::string, std::string> PossibilityDescription;
     typedef std::vector<std::pair<std::string, std::string>> PossibilityDescriptions;
 
     /// Value/name/description tuples. If [1] is the value on the commandline for an option, we'd
     /// set the value for the associated Option to [0].
     template <typename T>
     using OptionPossibility = std::tuple<T, std::string, std::string>;
 
 protected:
     OptionBase(const std::string& name, const std::string& description, std::vector<TOOL> tools)
             : OptionBase(name, description, tools, {})
     {
     }
 
     OptionBase(const std::string& name, const std::string& description, std::vector<TOOL> tools, PossibilityDescriptions possibilityDescriptions)
             : name(name), description(description), possibilityDescriptions(possibilityDescriptions)
     {
         assert(name[0] != '-' && "OptionBase: name starts with '-'");
         assert(!isHelpName(name) && "OptionBase: reserved help name");
         for (auto& t : tools)
         {
             assert(getOptionsMap(t).find(name) == getOptionsMap(t).end() && "OptionBase: duplicate option");
         }
 
         // Types with empty names (i.e., OptionMultiple) can handle things themselves.
         if (!name.empty())
         {
             // Standard name=value option.
             for (auto& t : tools)
             {
                 getOptionsMap(t)[name] = this;
             }
         }
     }
 
     /// From a given string, set the value of this option.
     virtual bool parseAndSetValue(std::string value) = 0;
 
     /// Whether this option represents a boolean. Important as such
     /// arguments don't require a value.
     virtual bool isBool() const
     {
         return false;
     }
 
     /// Whether this option is an OptionMultiple.
     virtual bool isMultiple() const
     {
         return false;
     }
 
     /// Can this option be set?
     virtual bool canSet() const = 0;
 
 public:
     /// Parse all constructed OptionBase children, returning positional arguments
     /// in the order they appeared.
     static std::vector<std::string> parseOptions(int argc, char *argv[], const std::string& description, TOOL t, std::string callFormat)
     {
         const std::string usage = buildUsage(description, t, std::string(argv[0]), callFormat);
 
         std::vector<std::string> positionalArguments;
 
         if (argc == 1)
             usageAndExit(usage, false);
 
         for (int i = 1; i < argc; ++i)
         {
             std::string arg(argv[i]);
             if (arg.empty()) continue;
             if (arg[0] != '-')
             {
                 // Positional argument. NOT a value to another argument because we
                 // "skip" over evaluating those without the corresponding argument.
                 positionalArguments.push_back(arg);
                 continue;
             }
 
             // Chop off '-'.
             arg = arg.substr(1);
 
             std::string argName;
             std::string argValue;
             OptionBase *opt = nullptr;
 
             size_t equalsSign = arg.find('=');
             if (equalsSign != std::string::npos)
             {
                 // Argument has an equal sign, i.e. argName=argValue
                 argName = arg.substr(0, equalsSign);
                 if (isHelpName(argName)) usageAndExit(usage, false);
 
                 opt = getOption(t, argName);
                 if (opt == nullptr)
                 {
                     std::cout << "Unknown option: " << argName << std::endl;
                     usageAndExit(usage, true);
                 }
 
                 argValue = arg.substr(equalsSign + 1);
             }
             else
             {
                 argName = arg;
                 if (isHelpName(argName)) usageAndExit(usage, false);
 
                 opt = getOption(t, argName);
                 if (opt == nullptr)
                 {
                     std::cout << "Unknown option: " << argName << std::endl;
                     usageAndExit(usage, true);
                 }
 
                 // No equals sign means we may need next argument.
                 if (opt->isBool())
                 {
                     // Booleans do not accept -arg true/-arg false.
                     // They must be -arg=true/-arg=false.
                     argValue = "true";
                 }
                 else if (opt->isMultiple())
                 {
                     // Name is the value and will be converted to an enum.
                     argValue = argName;
                 }
                 else if (i + 1 < argc)
                 {
                     // On iteration, we'll skip the value.
                     ++i;
                     argValue = std::string(argv[i]);
                 }
                 else
                 {
                     std::cout << "Expected value for: " << argName << std::endl;
                     usageAndExit(usage, true);
                 }
             }
 
             if (!opt->canSet())
             {
                 std::cout << "Unable to set: " << argName << "; check for duplicates" << std::endl;
                 usageAndExit(usage, true);
             }
 
             bool valueSet = opt->parseAndSetValue(argValue);
             if (!valueSet)
             {
                 std::cout << "Bad value for: " << argName << std::endl;
                 usageAndExit(usage, true);
             }
         }
 
         return positionalArguments;
     }
 
 private:
     /// Sets the usage member to a usage string, built from the static list of options.
     /// argv0 is argv[0] and callFormat is how the command should be used, minus the command
     /// name (e.g. "[options] <input-bitcode...>".
     static std::string buildUsage(
             const std::string& description, TOOL t, const std::string& argv0, const std::string& callFormat
     )
     {
         // Determine longest option to split into two columns: options and descriptions.
         unsigned longest = 4;
         for (const std::pair<std::string, OptionBase *> nopt : getOptionsMap(t))
         {
             const std::string& name = std::get<0>(nopt);
             const OptionBase *option = std::get<1>(nopt);
             if (option->isMultiple())
             {
                 // For Multiple, description goes in left column.
                 if (option->description.length() > longest) longest = option->description.length();
             }
             else
             {
                 if (name.length() > longest) longest = name.length();
             }
 
             for (const PossibilityDescription &pd : option->possibilityDescriptions)
             {
                 const std::string& possibility = std::get<0>(pd);
                 if (possibility.length() + 3 > longest) longest = possibility.length() + 3;
             }
         }
 
         std::stringstream ss;
 
         ss << description << std::endl << std::endl;
 
         ss << "USAGE:" << std::endl;
         ss << "  " << argv0 << " " << callFormat << std::endl;
         ss << std::endl;
 
         ss << "OPTIONS:" << std::endl;
 
         // Required as we have OptionMultiples doing a many-to-one in options.
         std::unordered_set<const OptionBase *> handled;
         for (const std::pair<std::string, OptionBase *> nopt : getOptionsMap(t))
         {
             const std::string& name = std::get<0>(nopt);
             const OptionBase *option = std::get<1>(nopt);
             if (handled.find(option) != handled.end()) continue;
             handled.insert(option);
 
             if (option->isMultiple())
             {
                 // description
                 //   -name1      - description
                 //   -name2      - description
                 //   ...
                 ss << "  " << option->description << std::endl;
                 for (const PossibilityDescription &pd : option->possibilityDescriptions)
                 {
                     const std::string& possibility = std::get<0>(pd);
                     const std::string description1 = std::get<1>(pd);
                     ss << "    -" << possibility << std::string(longest - possibility.length() + 2, ' ');
                     ss << "- " << description1 << std::endl;
                 }
             }
             else
             {
                 // name  - description
                 // or
                 // name     - description
                 //   =opt1    - description
                 //   =opt2    - description
                 //   ...
                 ss << "  -" << name << std::string(longest - name.length() + 2, ' ');
                 ss << "- " << option->description << std::endl;
                 for (const PossibilityDescription &pd : option->possibilityDescriptions)
                 {
                     const std::string& possibility = std::get<0>(pd);
                     const std::string& description1 = std::get<1>(pd);
                     ss << "    =" << possibility << std::string(longest - possibility.length() + 2, ' ');
                     ss << "- " << description1 << std::endl;
                 }
             }
         }
 
         // Help message.
         ss << std::endl;
         ss << "  -help" << std::string(longest - 4 + 2, ' ') << "- show usage and exit" << std::endl;
         ss << "  -h" << std::string(longest - 1 + 2, ' ') << "- show usage and exit" << std::endl;
 
         // How to set boolean options.
         ss << std::endl;
         ss << "Note: for boolean options, -name true and -name false are invalid." << std::endl;
         ss << "      Use -name, -name=true, or -name=false." << std::endl;
 
         return ss.str();
     }
 
     /// Find option based on name in options map. Returns nullptr if not found.
     static OptionBase *getOption(TOOL t, const std::string& optName)
     {
         auto optIt = getOptionsMap(t).find(optName);
         if (optIt == getOptionsMap(t).end()) return nullptr;
         else return optIt->second;
     }
 
     /// Print usage and exit. If error is set, print to stderr and exits with code 1.
     static void usageAndExit(const std::string& usage, bool error)
     {
         std::cout << usage;
         std::exit(error ? 1 : 0);
     }
 
     /// Returns whether name is one of the reserved help options.
     static bool isHelpName(const std::string& name)
     {
         static std::vector<std::string> helpNames = {"help", "h", "-help"};
         return std::find(helpNames.begin(), helpNames.end(), name) != helpNames.end();
     }
 
 protected:
     // Return the name/description part of OptionsPossibilities (second and third fields).
     template<typename T>
     static PossibilityDescriptions extractPossibilityDescriptions(const std::vector<OptionPossibility<T>> possibilities)
     {
         PossibilityDescriptions possibilityDescriptions;
         for (const OptionPossibility<T> &op : possibilities)
         {
             possibilityDescriptions.push_back(std::make_pair(std::get<1>(op), std::get<2>(op)));
         }
 
         return possibilityDescriptions;
     }
 
     /// Not unordered map so we can have sorted names when building the usage string.
     /// Map of option names to their object.
     static std::map<std::string, OptionBase *> &getOptionsMap(TOOL t)
     {
         // Not static member to avoid initialisation order problems.
         static std::map<TOOL, std::map<std::string, OptionBase *>> options;
         return options[t];
     }
 
 
 protected:
     std::string name;
     std::string description;
     /// For when we have possibilities like in an OptionMap.
     PossibilityDescriptions possibilityDescriptions;
 };
 
 /// General -name=value options.
 /// Retrieve value by Opt().
 template <typename T>
 class Option : public OptionBase
 {
 public:
     Option(const std::string& name, const std::string description, std::vector<TOOL> tools, T init)
             : OptionBase(name, description, tools), isExplicitlySet(false), value(init)
     {
         assert(!name.empty() && "Option: empty option name given");
     }
 
     virtual bool canSet() const override
     {
         // Don't allow duplicates.
         return !isExplicitlySet;
     }
 
     virtual bool parseAndSetValue(const std::string s) override
     {
         isExplicitlySet = fromString(s, value);
         return isExplicitlySet;
     }
 
     virtual bool isBool() const override
     {
         return std::is_same<T, bool>::value;
     }
 
     void setValue(T v)
     {
         value = v;
     }
 
     T operator()() const
     {
         return value;
     }
 
 private:
     // Convert string to boolean, returning whether we succeeded.
     static bool fromString(const std::string& s, bool& value)
     {
         if (s == "true")
             value = true;
         else if (s == "false")
             value = false;
         else
             return false;
         return true;
     }
 
     // Convert string to string, always succeeds.
     static bool fromString(const std::string& s, std::string &value)
     {
         value = s;
         return true;
     }
 
     // Convert string to u32_t, returning whether we succeeded.
     static bool fromString(const std::string& s, unsigned &value)
     {
         // We won't allow anything except [0-9]+.
         if (s.empty()) return false;
         for (char c : s)
         {
             if (!(c >= '0' && c <= '9')) return false;
         }
 
         // Use strtoul because we're not using exceptions.
         assert(sizeof(unsigned long) >= sizeof(unsigned));
         const unsigned long sv = std::strtoul(s.c_str(), nullptr, 10);
 
         // Out of range according to strtoul, or according to us compared to u32_t.
         if (errno == ERANGE || sv > std::numeric_limits<unsigned>::max()) return false;
         value = sv;
         return true;
     }
 
 private:
     bool isExplicitlySet;
     T value;
 };
 
 /// Carries around command line options.
 class Options
 {
 public:
     Options() = delete;
 
     /// dasm
     static const Option<bool> Split;
     static const Option<unsigned> SeparateGroup;
     static const Option<unsigned> SeparateGroupParallel;
     static const Option<bool> NoDebug;
     static const Option<bool> EnableMeta;
     static const Option<std::string> LtiFile;
     static const Option<std::string> DebugFile;
     static const Option<unsigned> Ncores;
     static const Option<std::string> FilePath;
     static const Option<std::string> OutputFilename;
     static const Option<bool> DisassemblyOnly;
     static const Option<bool> NoOpt;
     static const Option<bool> NoPeep;
     static const Option<bool> StackOnly;
     static const Option<bool> GlobalOnly;
     static const Option<bool> ParamOnly;
     static const Option<bool> PromoteOnly;
     static const Option<bool> NoStackDisam;
     static const Option<bool> BitcodeOnly;
     static const Option<bool> EnableVTable;
     static const Option<bool> Sound;
 
     // decoder
     static const Option<bool> StrictDsm;
     static const Option<bool> DumpFM;
     static const Option<bool> EnableVerify;
     static const Option<std::string> SelectFuncs;
 
     // dsm generator
     static const Option<std::string> DsmFileName;
 
     // value protect
     static const Option<bool> ProtectAlloca;
     static const Option<bool> ProtectSt;
 
     // emulation
     static const Option<bool> WithLoop;
 
     // complex type
     static const Option<std::string> StaticInfoName;
 
     // combination
     static const Option<std::string> BinaryList;
     static const Option<std::string> BinaryListDir;
     static const Option<std::string> LinkedOutput;
 
     /// similarity
     static const Option<std::string> TargetBitcode;
     static const Option<std::string> ReferenceBitcode;
     static const Option<bool> UseForceCov;
     static const Option<bool> UseEmuIcmp;
     static const Option<bool> DumpROC;
     static const Option<std::string> PersistStore;
     static const Option<bool> PersistStoreOverWrite;
     static const Option<std::string> PersistComp;
     static const Option<std::string> TargetJson;
     static const Option<std::string> ReferJson;
     static const Option<std::string> SelectTarget;
     static const Option<std::string> SelectRefer;
     static const Option<std::string> ReferBin;
     static const Option<bool> DebugEntry;
     static const Option<bool> DumpEntry;
     static const Option<bool> ReOpt;
     static const Option<std::string> SourceBitcode;
     static const Option<std::string> TargetBin;
     static const Option<unsigned> InlineThreshold;
     static const Option<bool> ExactCompare;
 
     /// licence
     static const Option<std::string> LicenceFile;
 };
 

 
 #endif //PLANKTON_OPTIONS_H