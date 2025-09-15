// FIXME: this file is just used by the gvfa tool, but we have another taint engine based on IFDS. Maybe we need to rename this file...
#include "Analysis/GVFA/TaintUtils.h"

using namespace gvfa;

bool TaintUtils::isKnownSourceFunction(const std::string& funcName) {
    static std::set<std::string> sources = {
        "gets", "fgets", "getchar", "scanf", "fscanf",
        "read", "fread", "recv", "recvfrom", "getenv"
    };
    return sources.find(funcName) != sources.end();
}

bool TaintUtils::isKnownSinkFunction(const std::string& funcName) {
    static std::set<std::string> sinks = {
        "system", "exec", "execl", "execle", "execlp",
        "execv", "execve", "execvp", "popen",
        "strcpy", "strcat", "sprintf", "printf", "fprintf",
        "write", "send", "sendto"
    };
    return sinks.find(funcName) != sinks.end();
}

bool TaintUtils::isKnownSanitizerFunction(const std::string& funcName) {
    static std::set<std::string> sanitizers = {
        "strlen", "strnlen", "strncpy", "strncat", "snprintf"
    };
    return sanitizers.find(funcName) != sanitizers.end();
}
