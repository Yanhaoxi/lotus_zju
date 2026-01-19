//===----------------------------------------------------------------------===//
/// @file SExpr.h
/// @brief S-Expression parser and representation
///
/// This file provides utilities for parsing and representing S-expressions,
/// a notation for nested list data commonly used in Lisp family languages.
///
/// The implementation uses Boost.Variant to represent the three possible
/// types of S-expression elements: tokens, integers, and lists.
///
///===----------------------------------------------------------------------===//

#ifndef __SEXPR_H__
#define __SEXPR_H__

#include <istream>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include <boost/variant.hpp>

/// @brief S-Expression representation
///
/// This class represents an S-expression, which can be a token (symbol),
/// an integer, or a nested list of S-expressions.
///
/// @par Example:
/// @code
/// SExpr expr("(a b (c d))");
/// @endcode
class SExpr final {
public:
  /// @brief Default constructor creates an empty list
  SExpr() : SExpr(List{{}}) {}

  /// @brief Construct from integer
  /// @param i The integer value
  SExpr(int i) : SExpr(Int{i}) {}

  /// @brief Construct from C string
  /// @param s The string value
  SExpr(const char *s) : SExpr(Token{s}) {}

  /// @brief Construct from string
  /// @param s The string value
  SExpr(std::string s) : SExpr(Token{std::move(s)}) {}

  /// @brief Construct from vector of S-expressions
  /// @param l The list of S-expressions
  SExpr(std::vector<SExpr> l) : SExpr(List{std::move(l)}) {}

  /// @brief Construct from initializer list
  /// @param l The initializer list
  SExpr(std::initializer_list<SExpr> l) : SExpr(List{l}) {}

  /// @brief Enumeration of S-expression kinds
  enum Kind : int {
    TOKEN, ///< Token (symbol) type
    INT,   ///< Integer type
    LIST,  ///< List type
  };

  /// @brief Token variant representing a symbol
  class Token {
  public:
    std::string name; ///< The token string
  };

  /// @brief Integer variant
  class Int {
  public:
    int value; ///< The integer value
  };

  /// @brief List variant
  class List {
  public:
    std::vector<SExpr> elems; ///< List elements
  };

  /// @brief Get the kind of this S-expression
  /// @return The Kind (TOKEN, INT, or LIST)
  Kind kind() const { return Kind(variant.which()); }

  /// @brief Get the token value
  /// @return Const reference to the token
  /// @pre kind() == TOKEN
  const Token &token() const { return boost::get<Token>(variant); }

  /// @brief Get the integer value
  /// @return Const reference to the integer
  /// @pre kind() == INT
  const Int &num() const { return boost::get<Int>(variant); }

  /// @brief Get the list value (non-const)
  /// @return Reference to the list
  /// @pre kind() == LIST
  List &list() { return boost::get<List>(variant); }

  /// @brief Get the list value (const)
  /// @return Const reference to the list
  /// @pre kind() == LIST
  const List &list() const { return boost::get<List>(variant); }

private:
  /// Internal variant type
  typedef boost::variant<Token, Int, List> vartype;

  /// @brief Private constructor from variant
  SExpr(vartype variant) : variant(std::move(variant)) {}

  vartype variant; ///< The internal variant storage
};

/// @brief Parse an S-expression from a stream
/// @param is The input stream
/// @param sexp The S-expression to populate
/// @return Reference to the input stream
std::istream &operator>>(std::istream &is, SExpr &sexp);

/// @brief Write an S-expression to a stream
/// @param os The output stream
/// @param sexp The S-expression to write
/// @return Reference to the output stream
std::ostream &operator<<(std::ostream &os, const SExpr &sexp);

#endif
