#pragma once

#include "Alias/OriginAA/Flags.h"
#include "llvm/Demangle/Demangle.h"
#include <string>
#include <algorithm>
#include <regex>

// Helper function to trim leading and trailing spaces
inline std::string trim(const std::string &str)
{
    auto start = str.find_first_not_of(" \t");
    auto end = str.find_last_not_of(" \t");
    return (start == std::string::npos) ? "" : str.substr(start, end - start + 1);
}

// Helper function to get demangled name without hash suffix
// TODO: we still see:
// std::sync::mutex::MutexGuard<T>::new::_$u7b$$u7b$closure$u7d$$u7d$ and std::sync::mutex::MutexGuard<T>::new
inline std::string getDemangledName(std::string mangledName)
{
    std::string demangled = llvm::demangle(mangledName); // the output looks like "std::thread::spawn::hc6f148c1a1888888"
    // Remove hash suffix: keep up to the last "::"
    size_t last_colon = demangled.rfind("::");
    if (last_colon != std::string::npos)
    {
        demangled = demangled.substr(0, last_colon);
    }

    // Replace $LT$ with < and $GT$ with >
    size_t pos = 0;
    while ((pos = demangled.find("$LT$", pos)) != std::string::npos)
    {
        demangled.replace(pos, 4, "<");
        pos += 1;
    }
    pos = 0;
    while ((pos = demangled.find("$GT$", pos)) != std::string::npos)
    {
        demangled.replace(pos, 4, ">");
        pos += 1;
    }

    // Replace .. with ::
    pos = 0;
    while ((pos = demangled.find("..", pos)) != std::string::npos)
    {
        demangled.replace(pos, 2, "::");
        pos += 2;
    }

    // Replace $u20$ with " ", e.g., xxx as xxx
    pos = 0;
    while ((pos = demangled.find("$u20$", pos)) != std::string::npos)
    {
        demangled.replace(pos, 5, " ");
        pos += 1;
    }

    // if the 1st char is _, remove it
    if (!demangled.empty() && demangled[0] == '_')
    {
        demangled.erase(0, 1);
    }

    if (DebugMode)
        llvm::errs() << "Demangled function name: " << demangled << "\n";

    return demangled;
}

// check if a value is a debug pointer, e.g., %f.dbg.spill
static bool isDbgPointer(const llvm::Value *V)
{
    if (!V)
        return false;
    if (const auto *inst = llvm::dyn_cast<llvm::Instruction>(V))
    {
        llvm::StringRef name = inst->getName();
        return name.contains(".dbg.");
    }
    return false;
}

// Helper function to convert LLVM Type to string
static std::string getTypeAsString(const llvm::Type *type)
{
    std::string typeStr;
    llvm::raw_string_ostream rso(typeStr);
    type->print(rso);
    return rso.str();
}

// Helper function to strip Rust-style hash suffix from function names
// e.g., 17he2469db56cab90c3E from _ZN4demo16spawn_user_query17he2469db56cab90c3E
static std::string stripRustHash(const std::string &fnName)
{
    std::regex hashPattern("^(.*)h[0-9a-fA-F]{16,}$");
    std::smatch match;
    if (std::regex_match(fnName, match, hashPattern))
    {
        return match[1].str().substr(0, match[1].length() - 2); // Remove the trailing '17h'
    }
    return fnName;
}