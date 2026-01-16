#include "IR/PDG/CypherQuery.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <sstream>

namespace pdg {

// ============================================================================
// CypherParser implementation
// ============================================================================

std::unique_ptr<CypherQuery> CypherParser::parse(const std::string &query) {
  clearError();
  trim(const_cast<std::string &>(query));

  if (query.empty()) {
    setError(CypherErrorCode::PARSE_ERROR, "Empty query", 1, 1);
    return nullptr;
  }

  size_t pos = 0;
  std::vector<std::string> tokens = tokenize(query);

  if (tokens.empty()) {
    setError(CypherErrorCode::PARSE_ERROR, "No valid tokens found", 1, 1);
    return nullptr;
  }

  return parseQuery(tokens, pos, CypherQueryParameters{});
}

void CypherParser::trim(std::string &s) {
  if (s.empty())
    return;

  size_t start = 0;
  while (start < s.size() &&
         std::isspace(static_cast<unsigned char>(s[start]))) {
    start++;
  }

  size_t end = s.size();
  while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
    end--;
  }

  s = s.substr(start, end - start);
}

bool CypherParser::isAlpha(char c) {
  return std::isalpha(static_cast<unsigned char>(c));
}

bool CypherParser::isDigit(char c) {
  return std::isdigit(static_cast<unsigned char>(c));
}

bool CypherParser::isAlphaNumeric(char c) {
  return isAlpha(c) || isDigit(c) || c == '_' || c == '$';
}

std::vector<std::string> CypherParser::tokenize(const std::string &s) {
  std::vector<std::string> tokens;
  std::string current;
  bool inString = false;
  char stringChar = '"';

  for (size_t i = 0; i < s.size(); ++i) {
    char c = s[i];

    if (inString) {
      if (c == '\\' && i + 1 < s.size()) {
        current += c;
        current += s[++i];
      } else if (c == stringChar) {
        current += c;
        tokens.push_back(current);
        current.clear();
        inString = false;
      } else {
        current += c;
      }
      continue;
    }

    if (c == '"' || c == '\'') {
      if (!current.empty()) {
        tokens.push_back(current);
        current.clear();
      }
      current += c;
      stringChar = c;
      inString = true;
      continue;
    }

    if (std::isspace(static_cast<unsigned char>(c))) {
      if (!current.empty()) {
        tokens.push_back(current);
        current.clear();
      }
      continue;
    }

    if (c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}' ||
        c == ',' || c == ';' || c == ':' || c == '-' || c == '>' || c == '<' ||
        c == '=' || c == '!' || c == '.' || c == '@' || c == '#') {
      if (!current.empty()) {
        tokens.push_back(current);
        current.clear();
      }
      if (c == '-' && i + 1 < s.size() && s[i + 1] == '>') {
        tokens.push_back("->");
        i++;
      } else if (c == '<' && i + 1 < s.size() && s[i + 1] == '-') {
        tokens.push_back("<-");
        i++;
      } else {
        std::string single(1, c);
        tokens.push_back(single);
      }
      continue;
    }

    current += c;
  }

  if (!current.empty()) {
    tokens.push_back(current);
  }

  return tokens;
}

std::unique_ptr<CypherQuery>
CypherParser::parseQuery(std::vector<std::string> &tokens, size_t &pos,
                         const CypherQueryParameters &params) {
  auto query = std::make_unique<CypherQuery>();

  while (hasMore(tokens, pos)) {
    std::string token = peek(tokens, pos);
    std::string upperToken = token;
    std::transform(upperToken.begin(), upperToken.end(), upperToken.begin(),
                   ::toupper);

    if (upperToken == "MATCH") {
      consume(tokens, pos);
      // Helper lambda to get uppercased peek
      auto peekUpper = [&tokens, &pos]() -> std::string {
        if (pos < tokens.size()) {
          std::string t = tokens[pos];
          std::transform(t.begin(), t.end(), t.begin(), ::toupper);
          return t;
        }
        return "";
      };
      while (hasMore(tokens, pos) && peekUpper() != "WHERE" &&
             peekUpper() != "RETURN" && peekUpper() != "WITH" &&
             peekUpper() != ";") {
        auto pattern = parsePattern(tokens, pos);
        if (!pattern) {
          return nullptr;
        }
        query->addPattern(std::move(pattern));
        // Handle comma-separated patterns
        if (hasMore(tokens, pos) && peek(tokens, pos) == ",") {
          consume(tokens, pos);
        }
      }
    } else if (upperToken == "WHERE") {
      consume(tokens, pos);
      auto whereClause = parseWhereClause(tokens, pos);
      if (!whereClause) {
        return nullptr;
      }
      query->setWhereClause(std::move(whereClause));
    } else if (upperToken == "RETURN") {
      consume(tokens, pos);
      // Helper lambda to get uppercased peek
      auto peekUpper = [&tokens, &pos]() -> std::string {
        if (pos < tokens.size()) {
          std::string t = tokens[pos];
          std::transform(t.begin(), t.end(), t.begin(), ::toupper);
          return t;
        }
        return "";
      };
      while (hasMore(tokens, pos) && peekUpper() != "ORDER" &&
             peekUpper() != "LIMIT" && peekUpper() != ";") {
        auto item = parseReturnItem(tokens, pos);
        if (!item) {
          return nullptr;
        }
        query->addReturnItem(std::move(item));
        if (hasMore(tokens, pos) && peek(tokens, pos) == ",") {
          consume(tokens, pos);
        }
      }
    } else if (upperToken == "ORDER") {
      consume(tokens, pos);
      if (hasMore(tokens, pos) &&
          (peek(tokens, pos) == "BY" || peek(tokens, pos) == "by")) {
        consume(tokens, pos);
        auto orderBy = parseOrderBy(tokens, pos);
        if (orderBy) {
          query->setOrderBy(std::move(orderBy));
        }
      } else {
        setError(CypherErrorCode::SYNTAX_ERROR, "Expected BY after ORDER", 0,
                 0);
        return nullptr;
      }
    } else if (upperToken == "LIMIT") {
      consume(tokens, pos);
      if (hasMore(tokens, pos)) {
        std::string limitStr = consume(tokens, pos);
        try {
          int limit = std::stoi(limitStr);
          query->setLimit(limit);
        } catch (...) {
          setError(CypherErrorCode::SYNTAX_ERROR,
                   "Invalid LIMIT value: " + limitStr, 0, 0);
          return nullptr;
        }
      } else {
        setError(CypherErrorCode::SYNTAX_ERROR, "Expected value after LIMIT", 0,
                 0);
        return nullptr;
      }
    } else if (upperToken == "WITH") {
      consume(tokens, pos);
      // Helper lambda to get uppercased peek
      auto peekUpper = [&tokens, &pos]() -> std::string {
        if (pos < tokens.size()) {
          std::string t = tokens[pos];
          std::transform(t.begin(), t.end(), t.begin(), ::toupper);
          return t;
        }
        return "";
      };
      while (hasMore(tokens, pos) && peekUpper() != "MATCH" &&
             peekUpper() != "RETURN" && peekUpper() != ";") {
        auto item = parseReturnItem(tokens, pos);
        if (item) {
          query->addWithItem(std::move(item));
        }
        if (hasMore(tokens, pos) && peek(tokens, pos) == ",") {
          consume(tokens, pos);
        }
      }
    } else if (token == ";") {
      break;
    } else {
      setError(CypherErrorCode::SYNTAX_ERROR, "Unexpected token: " + token, 0,
               0);
      return nullptr;
    }
  }

  if (query->getPatterns().empty()) {
    setError(CypherErrorCode::SYNTAX_ERROR, "MATCH clause is required", 0, 0);
    return nullptr;
  }

  return query;
}

std::unique_ptr<CypherPatternElement>
CypherParser::parsePattern(std::vector<std::string> &tokens, size_t &pos) {
  if (!hasMore(tokens, pos)) {
    setError(CypherErrorCode::SYNTAX_ERROR, "Expected pattern", 0, 0);
    return nullptr;
  }

  auto pattern =
      std::make_unique<CypherPatternElement>(parseNodePattern(tokens, pos));

  if (!pattern->getStartNode()) {
    return nullptr;
  }

  if (hasMore(tokens, pos)) {
    std::string token = peek(tokens, pos);
    if (token == "[" || token == "-" || token == "<-") {
      auto rel = parseRelationshipPattern(tokens, pos);
      if (!rel) {
        return nullptr;
      }
      pattern->setRelationship(std::move(rel));
    }
  }

  if (hasMore(tokens, pos) && peek(tokens, pos) == "(") {
    pattern->setEndNode(parseNodePattern(tokens, pos));
  }

  return pattern;
}

std::unique_ptr<CypherNodePattern>
CypherParser::parseNodePattern(std::vector<std::string> &tokens, size_t &pos) {
  if (!hasMore(tokens, pos)) {
    setError(CypherErrorCode::SYNTAX_ERROR, "Expected '(' for node pattern", 0,
             0);
    return nullptr;
  }

  if (peek(tokens, pos) != "(") {
    setError(CypherErrorCode::SYNTAX_ERROR,
             "Expected '(' but found: " + peek(tokens, pos), 0, 0);
    return nullptr;
  }
  consume(tokens, pos);

  std::string variable;
  std::string label;

  if (hasMore(tokens, pos)) {
    std::string next = peek(tokens, pos);
    if (next != ":" && next != ")" && next != "{") {
      variable = consume(tokens, pos);
      if (variable.empty()) {
        return nullptr;
      }
    }
  }

  if (hasMore(tokens, pos) && peek(tokens, pos) == ":") {
    consume(tokens, pos);
    if (hasMore(tokens, pos)) {
      label = consume(tokens, pos);
      if (label.empty()) {
        setError(CypherErrorCode::SYNTAX_ERROR, "Expected label after ':'", 0,
                 0);
        return nullptr;
      }
    }
  }

  if (hasMore(tokens, pos) && peek(tokens, pos) == "{") {
    consume(tokens, pos);
    while (hasMore(tokens, pos) && peek(tokens, pos) != "}") {
      std::string key = consume(tokens, pos);
      if (hasMore(tokens, pos) && peek(tokens, pos) == ":") {
        consume(tokens, pos);
        std::string value = consume(tokens, pos);
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
          value = value.substr(1, value.size() - 2);
        }
      }
      if (hasMore(tokens, pos) && peek(tokens, pos) == ",") {
        consume(tokens, pos);
      }
    }
    if (hasMore(tokens, pos) && peek(tokens, pos) == "}") {
      consume(tokens, pos);
    }
  }

  if (!hasMore(tokens, pos) || peek(tokens, pos) != ")") {
    setError(CypherErrorCode::SYNTAX_ERROR,
             "Expected ')' to close node pattern", 0, 0);
    return nullptr;
  }
  consume(tokens, pos);

  return std::make_unique<CypherNodePattern>(variable, label);
}

std::unique_ptr<CypherRelationshipPattern>
CypherParser::parseRelationshipPattern(std::vector<std::string> &tokens,
                                       size_t &pos) {
  bool bidirectional = false;

  // Handle leading direction
  if (hasMore(tokens, pos)) {
    std::string token = peek(tokens, pos);
    if (token == "<-") {
      bidirectional = true;
      consume(tokens, pos);
    } else if (token == "-") {
      consume(tokens, pos);
    }
  }

  // Check for relationship pattern in brackets
  if (hasMore(tokens, pos) && peek(tokens, pos) == "[") {
    consume(tokens, pos); // consume '['

    std::string variable;
    std::string type;
    int minHops = 1;
    int maxHops = 1;

    // Check for variable-length pattern [*] or [*0..5]
    if (hasMore(tokens, pos) && peek(tokens, pos) == "*") {
      consume(tokens, pos); // consume '*'
      minHops = 1;
      maxHops = -1; // -1 means unlimited

      // Check for range notation [min..max]
      if (hasMore(tokens, pos) && peek(tokens, pos) == "[") {
        consume(tokens, pos); // consume '['
        if (hasMore(tokens, pos)) {
          std::string minStr = consume(tokens, pos);
          try {
            minHops = std::stoi(minStr);
          } catch (...) {
            minHops = 0;
          }
        }
        if (hasMore(tokens, pos) && peek(tokens, pos) == ".") {
          consume(tokens, pos); // consume '.'
          if (hasMore(tokens, pos) && peek(tokens, pos) == ".") {
            consume(tokens, pos); // consume '.'
            if (hasMore(tokens, pos)) {
              std::string maxStr = consume(tokens, pos);
              try {
                maxHops = std::stoi(maxStr);
              } catch (...) {
                maxHops = -1;
              }
            }
          }
        }
        if (hasMore(tokens, pos) && peek(tokens, pos) == "]") {
          consume(tokens, pos); // consume ']'
        }
      }

      if (hasMore(tokens, pos) && peek(tokens, pos) == "]") {
        consume(tokens, pos); // consume ']'

        // Handle trailing direction
        if (hasMore(tokens, pos)) {
          std::string next = peek(tokens, pos);
          if (next == "<-") {
            consume(tokens, pos);
            bidirectional = true;
          } else if (next == "->") {
            consume(tokens, pos);
          } else if (next == "-") {
            consume(tokens, pos);
            if (hasMore(tokens, pos) && peek(tokens, pos) == ">") {
              consume(tokens, pos);
            }
          }
        }

        auto rel = std::make_unique<CypherRelationshipPattern>(variable, type,
                                                               bidirectional);
        rel->setMinHops(minHops);
        rel->setMaxHops(maxHops);
        return rel;
      }
    }

    if (hasMore(tokens, pos)) {
      std::string next = peek(tokens, pos);
      if (next != ":" && next != "]") {
        variable = consume(tokens, pos);
      }
    }

    if (hasMore(tokens, pos) && peek(tokens, pos) == ":") {
      consume(tokens, pos);
      if (hasMore(tokens, pos) && peek(tokens, pos) != "]") {
        type = consume(tokens, pos);
      }
    }

    // Handle properties in braces
    if (hasMore(tokens, pos) && peek(tokens, pos) == "{") {
      consume(tokens, pos);
      while (hasMore(tokens, pos) && peek(tokens, pos) != "}") {
        consume(tokens, pos);
        if (hasMore(tokens, pos) && peek(tokens, pos) == ":") {
          consume(tokens, pos);
          consume(tokens, pos);
        }
        if (hasMore(tokens, pos) && peek(tokens, pos) == ",") {
          consume(tokens, pos);
        }
      }
      if (hasMore(tokens, pos) && peek(tokens, pos) == "}") {
        consume(tokens, pos);
      }
    }

    if (!hasMore(tokens, pos) || peek(tokens, pos) != "]") {
      setError(CypherErrorCode::SYNTAX_ERROR,
               "Expected ']' to close relationship pattern", 0, 0);
      return nullptr;
    }
    consume(tokens, pos); // consume ']'

    // Handle trailing direction
    if (hasMore(tokens, pos)) {
      std::string next = peek(tokens, pos);
      if (next == "<-") {
        consume(tokens, pos);
        bidirectional = true;
      } else if (next == "->") {
        consume(tokens, pos);
      } else if (next == "-") {
        consume(tokens, pos);
        if (hasMore(tokens, pos) && peek(tokens, pos) == ">") {
          consume(tokens, pos);
        }
      }
    }

    auto rel = std::make_unique<CypherRelationshipPattern>(variable, type,
                                                           bidirectional);
    rel->setMinHops(1);
    rel->setMaxHops(1);
    return rel;
  }

  // No bracket pattern, just direction
  if (hasMore(tokens, pos)) {
    std::string token = peek(tokens, pos);
    if (token == "<-") {
      consume(tokens, pos);
      bidirectional = true;
    } else if (token == "->") {
      consume(tokens, pos);
    } else if (token == "-") {
      consume(tokens, pos);
      if (hasMore(tokens, pos) && peek(tokens, pos) == ">") {
        consume(tokens, pos);
      }
    }
  }

  return std::make_unique<CypherRelationshipPattern>("", "", bidirectional);
}

std::unique_ptr<CypherWhereClause>
CypherParser::parseWhereClause(std::vector<std::string> &tokens, size_t &pos) {
  return parseBooleanExpression(tokens, pos);
}

std::unique_ptr<CypherWhereClause>
CypherParser::parseBooleanExpression(std::vector<std::string> &tokens,
                                     size_t &pos) {
  if (!hasMore(tokens, pos)) {
    setError(CypherErrorCode::SYNTAX_ERROR,
             "Expected expression in WHERE clause", 0, 0);
    return nullptr;
  }

  std::string token = peek(tokens, pos);

  if (token == "NOT" || token == "not") {
    consume(tokens, pos);
    auto expr = parseBooleanExpression(tokens, pos);
    return CypherWhereClause::makeNot(std::move(expr));
  }

  if (token == "(") {
    consume(tokens, pos);
    auto left = parseBooleanExpression(tokens, pos);
    if (!left)
      return nullptr;

    if (hasMore(tokens, pos)) {
      std::string op = peek(tokens, pos);
      if (op == "AND" || op == "and" || op == "OR" || op == "or") {
        consume(tokens, pos);
        auto right = parseBooleanExpression(tokens, pos);
        if (!right)
          return nullptr;

        if (op == "AND" || op == "and") {
          return CypherWhereClause::makeAnd(std::move(left), std::move(right));
        } else {
          return CypherWhereClause::makeOr(std::move(left), std::move(right));
        }
      }
    }

    if (hasMore(tokens, pos) && peek(tokens, pos) == ")") {
      consume(tokens, pos);
    }
    return left;
  }

  return parseComparison(tokens, pos);
}

std::unique_ptr<CypherWhereClause>
CypherParser::parseComparison(std::vector<std::string> &tokens, size_t &pos) {
  if (!hasMore(tokens, pos)) {
    setError(CypherErrorCode::SYNTAX_ERROR, "Expected comparison expression", 0,
             0);
    return nullptr;
  }

  std::string variable = consume(tokens, pos);
  std::string property;

  if (hasMore(tokens, pos) && peek(tokens, pos) == ".") {
    consume(tokens, pos);
    if (hasMore(tokens, pos)) {
      property = consume(tokens, pos);
    }
  }

  if (!hasMore(tokens, pos)) {
    return CypherWhereClause::makeExists(variable);
  }

  std::string op = consume(tokens, pos);

  std::string value;
  if (hasMore(tokens, pos)) {
    value = consume(tokens, pos);
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
      value = value.substr(1, value.size() - 2);
    }
  }

  CypherComparisonOp comparisonOp = CypherComparisonOp::EQUALS;
  std::string upperOp = op;
  std::transform(upperOp.begin(), upperOp.end(), upperOp.begin(), ::toupper);

  if (upperOp == "=" || upperOp == "==") {
    comparisonOp = CypherComparisonOp::EQUALS;
  } else if (upperOp == "!=" || upperOp == "<>") {
    comparisonOp = CypherComparisonOp::NOT_EQUALS;
  } else if (upperOp == "<") {
    comparisonOp = CypherComparisonOp::LESS_THAN;
  } else if (upperOp == "<=") {
    comparisonOp = CypherComparisonOp::LESS_THAN_OR_EQUAL;
  } else if (upperOp == ">") {
    comparisonOp = CypherComparisonOp::GREATER_THAN;
  } else if (upperOp == ">=") {
    comparisonOp = CypherComparisonOp::GREATER_THAN_OR_EQUAL;
  } else if (upperOp == "CONTAINS") {
    comparisonOp = CypherComparisonOp::CONTAINS;
  }

  return CypherWhereClause::makeComparison(variable, property, comparisonOp,
                                           value);
}

std::unique_ptr<CypherReturnItem>
CypherParser::parseReturnItem(std::vector<std::string> &tokens, size_t &pos) {
  if (!hasMore(tokens, pos)) {
    setError(CypherErrorCode::SYNTAX_ERROR, "Expected return item", 0, 0);
    return nullptr;
  }

  std::string variable = consume(tokens, pos);
  std::string alias;

  // Handle property access (e.g., n.id -> consume . and property)
  if (hasMore(tokens, pos) && peek(tokens, pos) == ".") {
    consume(tokens, pos); // consume '.'
    if (hasMore(tokens, pos)) {
      std::string prop = consume(tokens, pos);
      variable += "." + prop; // Combine into single variable
    }
  }

  if (hasMore(tokens, pos) &&
      (peek(tokens, pos) == "AS" || peek(tokens, pos) == "as")) {
    consume(tokens, pos);
    if (hasMore(tokens, pos)) {
      alias = consume(tokens, pos);
    }
  }

  return std::make_unique<CypherReturnItem>(variable, alias);
}

std::unique_ptr<CypherOrderBy>
CypherParser::parseOrderBy(std::vector<std::string> &tokens, size_t &pos) {
  if (!hasMore(tokens, pos)) {
    setError(CypherErrorCode::SYNTAX_ERROR, "Expected variable for ORDER BY", 0,
             0);
    return nullptr;
  }

  std::string variable = consume(tokens, pos);

  // Handle property access (e.g., n.id -> consume . and property)
  if (hasMore(tokens, pos) && peek(tokens, pos) == ".") {
    consume(tokens, pos); // consume '.'
    if (hasMore(tokens, pos)) {
      consume(tokens, pos); // consume property name
    }
  }

  CypherOrderBy::Direction dir = CypherOrderBy::Direction::ASC;

  if (hasMore(tokens, pos)) {
    std::string next = peek(tokens, pos);
    std::string upperNext = next;
    std::transform(upperNext.begin(), upperNext.end(), upperNext.begin(),
                   ::toupper);
    if (upperNext == "DESC") {
      dir = CypherOrderBy::Direction::DESC;
      consume(tokens, pos);
    } else if (upperNext == "ASC") {
      consume(tokens, pos);
    }
  }

  return std::make_unique<CypherOrderBy>(variable, dir);
}

bool CypherParser::hasMore(const std::vector<std::string> &tokens, size_t pos) {
  return pos < tokens.size();
}

const std::string &CypherParser::peek(const std::vector<std::string> &tokens,
                                      size_t pos) {
  static const std::string empty = "";
  if (pos < tokens.size()) {
    return tokens[pos];
  }
  return empty;
}

const std::string &CypherParser::consume(std::vector<std::string> &tokens,
                                         size_t &pos) {
  static const std::string empty = "";
  if (pos < tokens.size()) {
    return tokens[pos++];
  }
  return empty;
}

// ============================================================================
// CypherResult implementation
// ============================================================================

std::string CypherResult::toString() const {
  std::ostringstream oss;

  switch (type_) {
  case ResultType::NODES:
    oss << "Result(" << nodes_.size() << " nodes)";
    break;
  case ResultType::RELATIONSHIPS:
    oss << "Result(" << relationships_.size() << " relationships)";
    break;
  case ResultType::PATHS:
    oss << "Result(paths)";
    break;
  case ResultType::SCALAR:
    oss << scalarValue_;
    break;
  case ResultType::INTEGER:
    oss << integerValue_;
    break;
  case ResultType::BOOLEAN:
    oss << (booleanValue_ ? "true" : "false");
    break;
  }

  return oss.str();
}

// ============================================================================
// CypherQueryExecutor implementation
// ============================================================================

std::unique_ptr<CypherResult>
CypherQueryExecutor::execute(const CypherQuery &query) {
  CypherQueryStats stats;
  return execute(query, stats);
}

std::unique_ptr<CypherResult>
CypherQueryExecutor::execute(const CypherQuery &query,
                             CypherQueryStats &stats) {
  auto startTime = std::chrono::high_resolution_clock::now();
  lastStats_ = CypherQueryStats();

  std::vector<Node *> resultNodes;
  std::vector<Edge *> resultEdges;

  for (const auto &pattern : query.getPatterns()) {
    auto patternResult = matchPattern(pattern.get());
    if (patternResult) {
      const auto &nodes = patternResult->getNodes();
      resultNodes.insert(resultNodes.end(), nodes.begin(), nodes.end());
      const auto &edges = patternResult->getRelationships();
      resultEdges.insert(resultEdges.end(), edges.begin(), edges.end());
      lastStats_.nodesVisited += nodes.size();
      lastStats_.edgesVisited += edges.size();
    }
  }

  std::sort(resultNodes.begin(), resultNodes.end());
  resultNodes.erase(std::unique(resultNodes.begin(), resultNodes.end()),
                    resultNodes.end());
  std::sort(resultEdges.begin(), resultEdges.end());
  resultEdges.erase(std::unique(resultEdges.begin(), resultEdges.end()),
                    resultEdges.end());

  if (query.getWhereClause() && !resultNodes.empty()) {
    auto filtered = filterByWhere(resultNodes, *query.getWhereClause());
    resultNodes = filtered->getNodes();
  }

  auto result = std::make_unique<CypherResult>(CypherResult::ResultType::NODES);
  for (auto *node : resultNodes) {
    result->addNode(node);
  }
  for (auto *edge : resultEdges) {
    result->addEdge(edge);
  }

  lastStats_.resultsReturned = result->getCount();

  auto endTime = std::chrono::high_resolution_clock::now();
  lastStats_.executionTime =
      std::chrono::duration_cast<std::chrono::microseconds>(endTime -
                                                            startTime);
  stats = lastStats_;

  return result;
}

std::unique_ptr<CypherResult>
CypherQueryExecutor::matchPattern(const CypherPatternElement *pattern) {
  if (!pattern)
    return nullptr;

  auto result = std::make_unique<CypherResult>(CypherResult::ResultType::NODES);

  const auto *startNodePattern = pattern->getStartNode();
  if (!startNodePattern)
    return result;

  auto startNodes =
      matchNodes(startNodePattern->getLabel(), startNodePattern->getVariable());
  if (startNodes) {
    for (auto *node : startNodes->getNodes()) {
      result->addNode(node);

      const auto *rel = pattern->getRelationship();
      if (rel) {
        int maxHops = rel->hasVariableLength() ? rel->getMaxHops() : 1;
        auto traversed = traverse(node, *rel, maxHops);
        if (traversed) {
          for (auto *tNode : traversed->getNodes()) {
            result->addNode(tNode);
          }
        }
      }
    }
  }

  return result;
}

std::unique_ptr<CypherResult>
CypherQueryExecutor::matchNodes(const std::string &label,
                                const std::string &variable) {
  auto result = std::make_unique<CypherResult>(CypherResult::ResultType::NODES);

  static const std::unordered_map<std::string, GraphNodeType> labelMap = {
      {"INST_FUNCALL", GraphNodeType::INST_FUNCALL},
      {"INST_RET", GraphNodeType::INST_RET},
      {"INST_BR", GraphNodeType::INST_BR},
      {"INST_OTHER", GraphNodeType::INST_OTHER},
      {"FUNC_ENTRY", GraphNodeType::FUNC_ENTRY},
      {"PARAM_FORMALIN", GraphNodeType::PARAM_FORMALIN},
      {"PARAM_FORMALOUT", GraphNodeType::PARAM_FORMALOUT},
      {"PARAM_ACTUALIN", GraphNodeType::PARAM_ACTUALIN},
      {"PARAM_ACTUALOUT", GraphNodeType::PARAM_ACTUALOUT},
      {"VAR", GraphNodeType::VAR_OTHER},
      {"FUNC", GraphNodeType::FUNC},
      {"CLASS", GraphNodeType::CLASS}};

  if (label.empty()) {
    for (auto it = pdg_.begin(); it != pdg_.end(); ++it) {
      result->addNode(*it);
    }
  } else {
    auto it = labelMap.find(label);
    if (it != labelMap.end()) {
      for (auto iter = pdg_.begin(); iter != pdg_.end(); ++iter) {
        if ((*iter)->getNodeType() == it->second) {
          result->addNode(*iter);
        }
      }
    }
  }

  if (!variable.empty()) {
    boundVariables_[variable] = result->getNodes();
  }

  return result;
}

std::unique_ptr<CypherResult>
CypherQueryExecutor::matchEdges(const std::string &type,
                                const std::string &variable) {
  auto result =
      std::make_unique<CypherResult>(CypherResult::ResultType::RELATIONSHIPS);

  static const std::unordered_map<std::string, EdgeType> typeMap = {
      {"DATA_DEF_USE", EdgeType::DATA_DEF_USE},
      {"DATA_RAW", EdgeType::DATA_RAW},
      {"DATA_READ", EdgeType::DATA_READ},
      {"CONTROLDEP_BR", EdgeType::CONTROLDEP_BR},
      {"CONTROLDEP_ENTRY", EdgeType::CONTROLDEP_ENTRY},
      {"CONTROLDEP_CALLINV", EdgeType::CONTROLDEP_CALLINV},
      {"CONTROLDEP_CALLRET", EdgeType::CONTROLDEP_CALLRET},
      {"PARAMETER_IN", EdgeType::PARAMETER_IN},
      {"PARAMETER_OUT", EdgeType::PARAMETER_OUT}};

  if (type.empty()) {
    for (auto it = pdg_.begin(); it != pdg_.end(); ++it) {
      for (auto *edge : (*it)->getOutEdgeSet()) {
        result->addEdge(edge);
      }
    }
  } else {
    auto it = typeMap.find(type);
    if (it != typeMap.end()) {
      for (auto iter = pdg_.begin(); iter != pdg_.end(); ++iter) {
        for (auto *edge : (*iter)->getOutEdgeSet()) {
          if (edge->getEdgeType() == it->second) {
            result->addEdge(edge);
          }
        }
      }
    }
  }

  if (!variable.empty()) {
    boundRelationships_[variable] = result->getRelationships();
  }

  return result;
}

std::unique_ptr<CypherResult>
CypherQueryExecutor::traverse(Node *start, const CypherRelationshipPattern &rel,
                              int maxHops) {
  auto result = std::make_unique<CypherResult>(CypherResult::ResultType::NODES);

  if (!start)
    return result;

  static const std::unordered_map<std::string, EdgeType> typeMap = {
      {"DATA_DEP", EdgeType::DATA_DEF_USE},
      {"CONTROL_DEP", EdgeType::CONTROLDEP_BR},
      {"CALL", EdgeType::CONTROLDEP_CALLINV},
      {"PARAM_IN", EdgeType::PARAMETER_IN},
      {"PARAM_OUT", EdgeType::PARAMETER_OUT}};

  std::vector<Node *> currentLevel = {start};
  std::unordered_set<Node *> visited;
  visited.insert(start);

  EdgeType edgeType = EdgeType::DATA_DEF_USE;
  if (!rel.getType().empty()) {
    auto it = typeMap.find(rel.getType());
    if (it != typeMap.end()) {
      edgeType = it->second;
    }
  }

  for (int hop = 0; hop < maxHops && !currentLevel.empty(); ++hop) {
    std::vector<Node *> nextLevel;

    for (auto *node : currentLevel) {
      for (auto *edge : node->getOutEdgeSet()) {
        if (edge->getEdgeType() == edgeType || rel.getType().empty()) {
          auto *neighbor = edge->getDstNode();
          if (visited.find(neighbor) == visited.end()) {
            visited.insert(neighbor);
            result->addNode(neighbor);
            nextLevel.push_back(neighbor);
          }
        }
      }

      if (rel.isBidirectional()) {
        for (auto *edge : node->getInEdgeSet()) {
          if (edge->getEdgeType() == edgeType || rel.getType().empty()) {
            auto *neighbor = edge->getSrcNode();
            if (visited.find(neighbor) == visited.end()) {
              visited.insert(neighbor);
              result->addNode(neighbor);
              nextLevel.push_back(neighbor);
            }
          }
        }
      }
    }

    currentLevel = std::move(nextLevel);
  }

  return result;
}

std::unique_ptr<CypherResult>
CypherQueryExecutor::filterByWhere(const std::vector<Node *> &nodes,
                                   const CypherWhereClause &where) {
  auto result = std::make_unique<CypherResult>(CypherResult::ResultType::NODES);

  for (auto *node : nodes) {
    if (evaluateCondition(where, node)) {
      result->addNode(node);
    }
  }

  return result;
}

std::unique_ptr<CypherResult>
CypherQueryExecutor::filterByWhere(const std::vector<Edge *> &edges,
                                   const CypherWhereClause &where) {
  auto result =
      std::make_unique<CypherResult>(CypherResult::ResultType::RELATIONSHIPS);

  for (auto *edge : edges) {
    if (evaluateCondition(where, edge)) {
      result->addEdge(edge);
    }
  }

  return result;
}

bool CypherQueryExecutor::evaluateCondition(const CypherWhereClause &condition,
                                            Node *node) {
  if (condition.isBooleanOp()) {
    if (condition.getBoolOp() == "NOT") {
      const auto *child = condition.getChild();
      return child && !evaluateCondition(*child, node);
    } else {
      const auto *left = condition.getLeft();
      const auto *right = condition.getRight();
      const std::string &op = condition.getBoolOp();

      bool leftResult = left ? evaluateCondition(*left, node) : true;
      bool rightResult = right ? evaluateCondition(*right, node) : true;

      if (op == "AND") {
        return leftResult && rightResult;
      } else if (op == "OR") {
        return leftResult || rightResult;
      }
    }
  }

  const auto &property = condition.getProperty();
  const auto &value = condition.getValue();
  std::string nodeValue = getNodeProperty(node, property);

  return applyComparison(nodeValue, condition.getComparisonOp(), value);
}

bool CypherQueryExecutor::evaluateCondition(const CypherWhereClause &condition,
                                            Edge *edge) {
  if (condition.isBooleanOp()) {
    if (condition.getBoolOp() == "NOT") {
      const auto *child = condition.getChild();
      return child && !evaluateCondition(*child, edge);
    } else {
      const auto *left = condition.getLeft();
      const auto *right = condition.getRight();
      const std::string &op = condition.getBoolOp();

      bool leftResult = left ? evaluateCondition(*left, edge) : true;
      bool rightResult = right ? evaluateCondition(*right, edge) : true;

      if (op == "AND") {
        return leftResult && rightResult;
      } else if (op == "OR") {
        return leftResult || rightResult;
      }
    }
  }

  const auto &property = condition.getProperty();
  const auto &value = condition.getValue();
  std::string edgeValue = getEdgeProperty(edge, property);

  return applyComparison(edgeValue, condition.getComparisonOp(), value);
}

bool CypherQueryExecutor::applyComparison(const std::string &nodeValue,
                                          CypherComparisonOp op,
                                          const std::string &queryValue) {
  switch (op) {
  case CypherComparisonOp::EQUALS:
    return nodeValue == queryValue;
  case CypherComparisonOp::NOT_EQUALS:
    return nodeValue != queryValue;
  case CypherComparisonOp::LESS_THAN:
    try {
      return std::stoll(nodeValue) < std::stoll(queryValue);
    } catch (...) {
      return nodeValue < queryValue;
    }
  case CypherComparisonOp::LESS_THAN_OR_EQUAL:
    try {
      return std::stoll(nodeValue) <= std::stoll(queryValue);
    } catch (...) {
      return nodeValue <= queryValue;
    }
  case CypherComparisonOp::GREATER_THAN:
    try {
      return std::stoll(nodeValue) > std::stoll(queryValue);
    } catch (...) {
      return nodeValue > queryValue;
    }
  case CypherComparisonOp::GREATER_THAN_OR_EQUAL:
    try {
      return std::stoll(nodeValue) >= std::stoll(queryValue);
    } catch (...) {
      return nodeValue >= queryValue;
    }
  case CypherComparisonOp::IS_NULL:
    return nodeValue.empty();
  case CypherComparisonOp::IS_NOT_NULL:
    return !nodeValue.empty();
  case CypherComparisonOp::CONTAINS:
    return nodeValue.find(queryValue) != std::string::npos;
  case CypherComparisonOp::STARTS_WITH:
    return nodeValue.find(queryValue) == 0;
  case CypherComparisonOp::ENDS_WITH:
    return nodeValue.size() >= queryValue.size() &&
           nodeValue.substr(nodeValue.size() - queryValue.size()) == queryValue;
  case CypherComparisonOp::IN:
    return nodeValue == queryValue;
  }
  return true;
}

std::string CypherQueryExecutor::getNodeProperty(Node *node,
                                                 const std::string &property) {
  if (!node)
    return "";

  if (property == "type" || property == "TYPE") {
    return std::to_string(static_cast<int>(node->getNodeType()));
  } else if (property == "label" || property == "LABEL") {
    return std::to_string(static_cast<int>(node->getNodeType()));
  } else if (property == "func" || property == "FUNC") {
    if (auto *func = node->getFunc()) {
      return func->getName().str();
    }
  }

  return "";
}

std::string CypherQueryExecutor::getEdgeProperty(Edge *edge,
                                                 const std::string &property) {
  if (!edge)
    return "";

  if (property == "type" || property == "TYPE") {
    return std::to_string(static_cast<int>(edge->getEdgeType()));
  }

  return "";
}

} // namespace pdg
