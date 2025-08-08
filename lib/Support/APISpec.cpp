#include "Support/APISpec.h"

#include <cctype>
#include <fstream>
#include <sstream>

using namespace lotus;

namespace {
static inline std::string trim(const std::string &s) {
  size_t b = 0;
  while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
  size_t e = s.size();
  while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
  return s.substr(b, e - b);
}

static inline bool isCommentOrBlank(const std::string &line) {
  for (char c : line) {
    if (!std::isspace(static_cast<unsigned char>(c))) {
      return c == '#';
    }
  }
  return true;
}
}

static SpecOpKind toOpKind(const std::string &tok) {
  if (tok == "IGNORE") return SpecOpKind::Ignore;
  if (tok == "ALLOC") return SpecOpKind::Alloc;
  if (tok == "COPY") return SpecOpKind::Copy;
  if (tok == "EXIT") return SpecOpKind::Exit;
  if (tok == "MOD") return SpecOpKind::Mod;
  if (tok == "REF") return SpecOpKind::Ref;
  // Fallback: treat unknown as IGNORE to be conservative
  return SpecOpKind::Ignore;
}

static QualifierKind toQualifier(const std::string &q) {
  if (q == "V") return QualifierKind::Value;
  if (q == "R") return QualifierKind::Region;
  if (q == "D") return QualifierKind::Data;
  return QualifierKind::Unknown;
}

bool APISpec::parseLine(const std::string &line,
                        std::string &outFunc,
                        SpecOpKind &outOp,
                        std::vector<std::string> &outTokens) {
  outFunc.clear();
  outTokens.clear();

  std::string s = trim(line);
  if (s.empty() || isCommentOrBlank(s)) return false;

  std::istringstream iss(s);
  std::string func, op;
  if (!(iss >> func)) return false;
  if (!(iss >> op)) return false;
  outFunc = func;
  outOp = toOpKind(op);

  std::string tok;
  while (iss >> tok) outTokens.push_back(tok);
  return true;
}

ValueSelector APISpec::parseSelector(const std::string &token) {
  // Examples: Ret, Arg0, Arg1, AfterArg2, STATIC, NULL
  if (token == "Ret") return ValueSelector{SelectorKind::Ret, -1, true};
  if (token == "STATIC") return ValueSelector{SelectorKind::Static, -1, true};
  if (token == "NULL") return ValueSelector{SelectorKind::Null, -1, true};
  if (token.rfind("Arg", 0) == 0) {
    int idx = std::stoi(token.substr(3));
    return ValueSelector{SelectorKind::Arg, idx, true};
  }
  if (token.rfind("AfterArg", 0) == 0) {
    int idx = std::stoi(token.substr(8));
    return ValueSelector{SelectorKind::AfterArg, idx, true};
  }
  return ValueSelector{SelectorKind::Ret, -1, false};
}

QualifierKind APISpec::parseQualifier(const std::string &token) {
  return toQualifier(token);
}

void APISpec::applyAlloc(FunctionSpec &spec, const std::vector<std::string> &tokens) {
  spec.isAllocator = true;
  AllocEffect eff;
  eff.sizeArgIndex = -1;
  // Some ALLOC lines provide an ArgN hint, e.g., "malloc ALLOC Arg0".
  if (!tokens.empty() && tokens[0].rfind("Arg", 0) == 0) {
    try {
      eff.sizeArgIndex = std::stoi(tokens[0].substr(3));
    } catch (...) {
      eff.sizeArgIndex = -1;
    }
  }
  spec.allocs.push_back(eff);
}

void APISpec::applyCopy(FunctionSpec &spec, const std::vector<std::string> &tokens) {
  // Expect pattern: COPY <DstSel> <DstQual> <SrcSel> <SrcQual>
  if (tokens.size() < 4) return;
  ValueSelector dstSel = parseSelector(tokens[0]);
  auto dstQ = parseQualifier(tokens[1]);
  ValueSelector srcSel = parseSelector(tokens[2]);
  auto srcQ = parseQualifier(tokens[3]);
  if (!dstSel.isValid || !srcSel.isValid) return;
  spec.copies.push_back(CopyEffect{dstSel, dstQ, srcSel, srcQ});
}

void APISpec::applyIgnore(FunctionSpec &spec) { spec.isIgnored = true; }

void APISpec::applyExit(FunctionSpec &spec) { spec.isExit = true; }

void APISpec::applyModRef(FunctionSpec &spec, SpecOpKind op, const std::vector<std::string> &tokens) {
  // Expect: (MOD|REF) <Sel> <Qual>
  if (tokens.size() < 2) return;
  ValueSelector sel = parseSelector(tokens[0]);
  auto q = parseQualifier(tokens[1]);
  if (!sel.isValid) return;
  spec.modref.push_back(ModRefEffect{op, sel, q});
}

bool APISpec::loadFile(const std::string &path, std::string &errorMessage) {
  errorMessage.clear();
  std::ifstream in(path);
  if (!in.is_open()) {
    errorMessage = "Failed to open spec file: " + path;
    return false;
  }
  std::string line;
  while (std::getline(in, line)) {
    std::string func;
    SpecOpKind op;
    std::vector<std::string> toks;
    if (!parseLine(line, func, op, toks)) continue;
    auto &spec = nameToSpec[func];
    if (spec.functionName.empty()) spec.functionName = func;
    switch (op) {
      case SpecOpKind::Ignore:
        applyIgnore(spec);
        break;
      case SpecOpKind::Exit:
        applyExit(spec);
        break;
      case SpecOpKind::Alloc:
        applyAlloc(spec, toks);
        break;
      case SpecOpKind::Copy:
        applyCopy(spec, toks);
        break;
      case SpecOpKind::Mod:
      case SpecOpKind::Ref:
        applyModRef(spec, op, toks);
        break;
    }
  }
  return true;
}

bool APISpec::loadFiles(const std::vector<std::string> &paths, std::string &errorMessage) {
  for (const auto &p : paths) {
    std::string err;
    if (!loadFile(p, err)) {
      errorMessage = err;
      return false;
    }
  }
  return true;
}

const FunctionSpec *APISpec::get(const std::string &functionName) const {
  auto it = nameToSpec.find(functionName);
  if (it == nameToSpec.end()) return nullptr;
  return &it->second;
}

bool APISpec::isIgnored(const std::string &functionName) const {
  auto *s = get(functionName);
  return s && s->isIgnored;
}

bool APISpec::isExitLike(const std::string &functionName) const {
  auto *s = get(functionName);
  return s && s->isExit;
}

bool APISpec::isAllocatorLike(const std::string &functionName) const {
  auto *s = get(functionName);
  return s && s->isAllocator;
}

std::vector<CopyEffect> APISpec::getCopies(const std::string &functionName) const {
  auto *s = get(functionName);
  if (!s) return {};
  return s->copies;
}

std::vector<ModRefEffect> APISpec::getModRefs(const std::string &functionName) const {
  auto *s = get(functionName);
  if (!s) return {};
  return s->modref;
}


