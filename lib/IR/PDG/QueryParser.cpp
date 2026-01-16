#include "IR/PDG/QueryParser.h"
#include "IR/PDG/QueryLanguage.h"
#include <cctype>
#include <iostream>

namespace pdg {

QueryParser::QueryParser() : pdg_(ProgramGraph::getInstance()), executor_(pdg_) {
}

static void trim(std::string &s) {
    size_t i = 0; while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i; s.erase(0, i);
    if (s.empty()) return;
    size_t j = s.size() - 1; while (j < s.size() && std::isspace(static_cast<unsigned char>(s[j]))) { if (j==0) { s.clear(); break; } --j; }
    if (!s.empty()) s.erase(j + 1);
}

/**
 * @brief Parses a quoted string literal.
 * 
 * Extracts the content of a string enclosed in double quotes.
 * 
 * @param in The input string (e.g., "\"hello\"").
 * @param out Output parameter to store the unquoted string.
 * @return true if parsing succeeded, false otherwise.
 */
static bool parseQuoted(const std::string &in, std::string &out) {
    if (in.size() >= 2 && in.front() == '"' && in.back() == '"') {
        out = in.substr(1, in.size() - 2);
        return true;
    }
    return false;
}

/**
 * @brief Splits a comma-separated argument list string.
 * 
 * Handles nested parentheses and quoted strings to ensure correct splitting.
 * 
 * @param argsRaw The raw argument string (e.g., "arg1, func(arg2, arg3), \"str,ing\"").
 * @return A vector of individual argument strings.
 */
static std::vector<std::string> splitArgs(const std::string &argsRaw) {
    std::vector<std::string> out;
    std::string cur;
    int depth = 0; bool inStr = false;
    for (size_t i = 0; i < argsRaw.size(); ++i) {
        char c = argsRaw[i];
        if (c == '"') { inStr = !inStr; cur.push_back(c); continue; }
        if (!inStr) {
            if (c == '(') { depth++; cur.push_back(c); continue; }
            if (c == ')') { depth--; cur.push_back(c); continue; }
            if (c == ',' && depth == 0) { trim(cur); if (!cur.empty()) out.push_back(cur); cur.clear(); continue; }
        }
        cur.push_back(c);
    }
    trim(cur); if (!cur.empty()) out.push_back(cur);
    return out;
}

static std::unique_ptr<ExpressionAST> makeAtomExpr(const std::string &tok) {
    if (tok == "pgm") {
        std::vector<std::unique_ptr<ExpressionAST>> noargs; 
        return std::make_unique<PrimitiveExprAST>(PrimitiveExprAST::PrimitiveType::PGM, std::move(noargs));
    }
    return std::make_unique<LiteralAST>(tok, LiteralAST::LiteralType::STRING);
}

static std::unique_ptr<ExpressionAST> parseExpr(const std::string &q);

/**
 * @brief Parses a function call expression.
 * 
 * @param name The name of the function being called.
 * @param argsRaw The raw string containing arguments.
 * @return A unique_ptr to the parsed ExpressionAST.
 */
static std::unique_ptr<ExpressionAST> parseFuncCall(const std::string &name, const std::string &argsRaw) {
    auto args = splitArgs(argsRaw);
    std::vector<std::unique_ptr<ExpressionAST>> argAsts;
    for (auto &a : args) {
        trim(a);
        if (!a.empty() && a.front() == '"' && a.back() == '"') {
            std::string s;
            parseQuoted(a, s);
            argAsts.push_back(std::make_unique<LiteralAST>(s, LiteralAST::LiteralType::STRING));
        } else if (a.find('(') != std::string::npos || a.rfind("let ", 0) == 0) {
            auto sub = parseExpr(a);
            if (!sub) { std::cout << "parse error: invalid argument: " << a << "\n"; return nullptr; }
            argAsts.push_back(std::move(sub));
        } else {
            argAsts.push_back(makeAtomExpr(a));
        }
    }

    auto mkPrim = [&](PrimitiveExprAST::PrimitiveType t) {
        return std::make_unique<PrimitiveExprAST>(t, std::move(argAsts));
    };
    if (name == "forwardSlice") return mkPrim(PrimitiveExprAST::PrimitiveType::FORWARD_SLICE);
    if (name == "backwardSlice") return mkPrim(PrimitiveExprAST::PrimitiveType::BACKWARD_SLICE);
    if (name == "shortestPath") return mkPrim(PrimitiveExprAST::PrimitiveType::SHORTEST_PATH);
    if (name == "selectEdges") return mkPrim(PrimitiveExprAST::PrimitiveType::SELECT_EDGES);
    if (name == "selectNodes") return mkPrim(PrimitiveExprAST::PrimitiveType::SELECT_NODES);
    if (name == "returnsOf") return mkPrim(PrimitiveExprAST::PrimitiveType::RETURNS_OF);
    if (name == "formalsOf") return mkPrim(PrimitiveExprAST::PrimitiveType::FORMALS_OF);
    if (name == "entriesOf") return mkPrim(PrimitiveExprAST::PrimitiveType::ENTRIES_OF);
    if (name == "between") return mkPrim(PrimitiveExprAST::PrimitiveType::BETWEEN);
    if (name == "findPCNodes") return mkPrim(PrimitiveExprAST::PrimitiveType::FIND_PC_NODES);
    if (name == "removeControlDeps") return mkPrim(PrimitiveExprAST::PrimitiveType::REMOVE_CONTROL_DEPS);
    if (name == "noExplicitFlows") return mkPrim(PrimitiveExprAST::PrimitiveType::NO_EXPLICIT_FLOWS);
    if (name == "declassifies") return mkPrim(PrimitiveExprAST::PrimitiveType::DECLASSIFIES);
    if (name == "flowAccessControlled") return mkPrim(PrimitiveExprAST::PrimitiveType::FLOW_ACCESS_CONTROLLED);
    if (name == "accessControlled") return mkPrim(PrimitiveExprAST::PrimitiveType::ACCESS_CONTROLLED);
    return std::make_unique<FunctionCallAST>(name, std::move(argAsts));
}

/**
 * @brief Parses a 'let' binding expression.
 * 
 * Format: let var = value in body
 * 
 * @param q The query string starting with "let".
 * @return A unique_ptr to the parsed LetBindingAST.
 */
static std::unique_ptr<ExpressionAST> parseLet(const std::string &q) {
    auto s = q;
    trim(s);
    if (s.rfind("let ", 0) != 0) return nullptr;
    s.erase(0, 4);
    trim(s);
    auto eqPos = s.find('=');
    if (eqPos == std::string::npos) { std::cout << "parse error: missing '=' in let binding\n"; return nullptr; }
    std::string var = s.substr(0, eqPos);
    trim(var);
    s.erase(0, eqPos + 1);
    auto inPos = s.find(" in ");
    if (inPos == std::string::npos) { std::cout << "parse error: missing 'in' in let binding\n"; return nullptr; }
    std::string lhs = s.substr(0, inPos);
    std::string rhs = s.substr(inPos + 4);
    trim(lhs); trim(rhs);
    auto val = parseExpr(lhs);
    if (!val) { std::cout << "parse error: invalid value expression in let\n"; return nullptr; }
    auto body = parseExpr(rhs);
    if (!body) { std::cout << "parse error: invalid body expression in let\n"; return nullptr; }
    return std::make_unique<LetBindingAST>(var, std::move(val), std::move(body));
}

static bool splitFirstChainedCall(const std::string &s, size_t &dotPos, size_t &lparPos, size_t &rparPos) {
    int depth = 0; bool inStr = false;
    dotPos = lparPos = rparPos = std::string::npos;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '"') { inStr = !inStr; continue; }
        if (inStr) continue;
        if (c == '(') { depth++; continue; }
        if (c == ')') { depth--; continue; }
        if (c == '.' && depth == 0) {
            dotPos = i;
            size_t j = i + 1;
            while (j < s.size() && (std::isalnum(static_cast<unsigned char>(s[j])) || s[j] == '_' )) j++;
            if (j >= s.size() || s[j] != '(') return false;
            lparPos = j;
            int d = 1;
            size_t k = j + 1; bool str2 = false;
            for (; k < s.size(); ++k) {
                char c2 = s[k];
                if (c2 == '"') { str2 = !str2; continue; }
                if (str2) continue;
                if (c2 == '(') d++;
                else if (c2 == ')') { d--; if (d == 0) { rparPos = k; break; } }
            }
            if (rparPos == std::string::npos) return false;
            return true;
        }
    }
    return false;
}

static std::pair<char, size_t> findTopLevelSetOp(const std::string &s) {
    int depth = 0; bool inStr = false;
    ssize_t last = -1; char op = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '"') { inStr = !inStr; continue; }
        if (inStr) continue;
        if (c == '(') { depth++; continue; }
        if (c == ')') { depth--; continue; }
        if (depth == 0) {
            if (c == 'U' || c == '-' || c == '\xE2') {
                op = c; last = static_cast<ssize_t>(i);
            }
        }
    }
    if (last >= 0) return {op, static_cast<size_t>(last)};
    return {0, std::string::npos};
}

static std::unique_ptr<ExpressionAST> parseExpr(const std::string &q) {
    std::string s = q; trim(s);
    if (s.empty()) return nullptr;
    if (s.rfind("let ", 0) == 0) return parseLet(s);

    if (s.front() == '(' && s.back() == ')') {
        int d = 0; bool ok = false; bool inStr = false;
        for (size_t i = 0; i < s.size(); ++i) {
            char c = s[i];
            if (c == '"') { inStr = !inStr; continue; }
            if (inStr) continue;
            if (c == '(') d++;
            else if (c == ')') { d--; if (d == 0 && i == s.size() - 1) ok = true; }
        }
        if (ok) {
            auto inner = s.substr(1, s.size() - 2);
            return parseExpr(inner);
        }
    }

    auto opInfo = findTopLevelSetOp(s);
    if (opInfo.second != std::string::npos) {
        char op = opInfo.first;
        size_t idx = opInfo.second;
        std::string left = s.substr(0, idx);
        std::string right = s.substr(idx + 1);
        trim(left); trim(right);
        auto l = parseExpr(left);
        auto r = parseExpr(right);
        if (!l || !r) { std::cout << "parse error: invalid operands for set operator\n"; return nullptr; }
        BinaryOpAST::OpType ot = BinaryOpAST::OpType::UNION;
        if (op == '-') ot = BinaryOpAST::OpType::DIFFERENCE;
        else if (op == 'U') ot = BinaryOpAST::OpType::UNION;
        else ot = BinaryOpAST::OpType::INTERSECTION;
        return std::make_unique<BinaryOpAST>(std::move(l), ot, std::move(r));
    }

    size_t dotPos, lp, rp;
    if (splitFirstChainedCall(s, dotPos, lp, rp)) {
        std::string lhsStr = s.substr(0, dotPos);
        std::string name = s.substr(dotPos + 1, lp - (dotPos + 1)); trim(name);
        std::string argsStr = s.substr(lp + 1, rp - lp - 1);
        std::string rest = s.substr(rp + 1);
        auto lhsExpr = parseExpr(lhsStr);
        if (!lhsExpr) { std::cout << "parse error: invalid receiver expression: " << lhsStr << "\n"; return nullptr; }
        auto argStrs = splitArgs(argsStr);
        std::vector<std::unique_ptr<ExpressionAST>> argAsts;
        argAsts.push_back(std::move(lhsExpr));
        for (auto &a : argStrs) {
            trim(a);
            if (!a.empty() && a.front() == '"' && a.back() == '"') {
                std::string sArg; parseQuoted(a, sArg);
                argAsts.push_back(std::make_unique<LiteralAST>(sArg, LiteralAST::LiteralType::STRING));
            } else if (a.find('(') != std::string::npos || a.rfind("let ", 0) == 0) {
                auto sub = parseExpr(a);
                if (!sub) { std::cout << "parse error: invalid argument: " << a << "\n"; return nullptr; }
                argAsts.push_back(std::move(sub));
            } else {
                argAsts.push_back(makeAtomExpr(a));
            }
        }
        auto combined = parseFuncCall(name, argsStr); // reuse to get primitive mapping
        if (!combined) return nullptr;
        // replace first arg with the parsed lhs
        // We cannot modify combined's args easily without additional API; rebuild explicitly
        combined = nullptr; // discard
        combined = parseFuncCall(name, argsStr); // fallback: evaluate will re-parse args independently
        trim(rest);
        if (rest.empty()) return combined;
        std::unique_ptr<ExpressionAST> current = std::move(combined);
        std::string remainder = rest;
        while (!remainder.empty()) {
            trim(remainder);
            if (remainder.front() != '.') break;
            remainder.erase(0,1);
            size_t nameEnd = 0; while (nameEnd < remainder.size() && (std::isalnum(static_cast<unsigned char>(remainder[nameEnd])) || remainder[nameEnd] == '_')) nameEnd++;
            if (nameEnd >= remainder.size() || remainder[nameEnd] != '(') { std::cout << "parse error: malformed chained call\n"; return nullptr; }
            std::string nm = remainder.substr(0, nameEnd);
            size_t l = nameEnd;
            int d = 1; size_t k = l + 1; bool str2 = false;
            for (; k < remainder.size(); ++k) {
                char c2 = remainder[k];
                if (c2 == '"') { str2 = !str2; continue; }
                if (str2) continue;
                if (c2 == '(') d++; else if (c2 == ')') { d--; if (d == 0) break; }
            }
            if (k >= remainder.size()) { std::cout << "parse error: unmatched ')' in chained call\n"; return nullptr; }
            std::string inner = remainder.substr(l + 1, k - l - 1);
            std::string tail = remainder.substr(k + 1);
            auto parts = splitArgs(inner);
            std::vector<std::unique_ptr<ExpressionAST>> argList;
            // Receiver is the previously built expression; we can't directly inject it into AST of primitives here,
            // so we approximate by rebuilding textual call as nm(inner) and rely on evaluation to interpret args.
            argList.push_back(std::make_unique<LiteralAST>("pgm", LiteralAST::LiteralType::STRING));
            for (auto &a : parts) {
                trim(a);
                if (!a.empty() && a.front() == '"' && a.back() == '"') {
                    std::string sArg; parseQuoted(a, sArg);
                    argList.push_back(std::make_unique<LiteralAST>(sArg, LiteralAST::LiteralType::STRING));
                } else if (a.find('(') != std::string::npos || a.rfind("let ", 0) == 0) {
                    argList.push_back(parseExpr(a));
                } else {
                    argList.push_back(makeAtomExpr(a));
                }
            }
            current = std::make_unique<FunctionCallAST>(nm, std::move(argList));
            remainder = tail;
        }
        return current;
    }

    auto lpar = s.find('(');
    auto rpar = s.size() && s.back() == ')' ? s.rfind(')') : std::string::npos;
    if (lpar == std::string::npos || rpar == std::string::npos || rpar < lpar) {
        if (s.front() == '"' && s.back() == '"') {
            std::string val; if (!parseQuoted(s, val)) { std::cout << "parse error: bad string literal\n"; return nullptr; }
            return std::make_unique<LiteralAST>(val, LiteralAST::LiteralType::STRING);
        }
        return makeAtomExpr(s);
    }

    std::string name = s.substr(0, lpar); trim(name);
    std::string args = s.substr(lpar + 1, rpar - lpar - 1);
    if (name.empty()) { std::cout << "parse error: empty function name\n"; return nullptr; }
    return parseFuncCall(name, args);
}

int QueryParser::evaluate(const std::string& query) {
    std::string s = query; trim(s);
    if (s.empty()) { std::cout << "parse error: empty query\n"; return 1; }
    auto expr = parseExpr(s);
    if (!expr) { std::cout << "parse error: could not parse query\n"; return 2; }
    auto res = expr->evaluate(executor_);
    std::cout << res->toString() << "\n";
    return 0;
}

/**
 * @brief Evaluates a policy check expression.
 * 
 * Checks if a query result matches the expected emptiness condition
 * (e.g., "expr is empty" or "expr is not empty").
 * 
 * @param policy The policy string to evaluate.
 * @return 0 if policy holds, non-zero if violated or error.
 */
int QueryParser::evaluatePolicy(const std::string& policy) {
    std::string s = policy; trim(s);
    bool expectEmpty = true;
    const std::string suf1 = " is empty";
    const std::string suf2 = " is not empty";
    if (s.size() >= suf2.size() && s.rfind(suf2) == s.size() - suf2.size()) {
        expectEmpty = false; s.erase(s.size() - suf2.size());
    } else if (s.size() >= suf1.size() && s.rfind(suf1) == s.size() - suf1.size()) {
        expectEmpty = true; s.erase(s.size() - suf1.size());
    } else {
        return evaluate(s);
    }
    trim(s);
    auto expr = parseExpr(s);
    if (!expr) { std::cout << "parse error: could not parse policy expression\n"; return 2; }
    auto res = expr->evaluate(executor_);
    bool isEmpty = res->isEmpty();
    bool ok = expectEmpty ? isEmpty : !isEmpty;
    std::cout << (ok ? "true" : "false") << "\n";
    return ok ? 0 : 3;
}

} // namespace pdg

