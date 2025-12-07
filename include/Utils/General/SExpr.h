#ifndef __SEXPR_H__
#define __SEXPR_H__

#include <istream>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include <boost/variant.hpp>

class SExpr final {
public:
    SExpr() : SExpr(List{{}}) {}
    SExpr(int i) : SExpr(Int{i}) {}
    SExpr(const char *s) : SExpr(Token{s}) {}
    SExpr(std::string s) : SExpr(Token{std::move(s)}) {}
    SExpr(std::vector<SExpr> l) : SExpr(List{std::move(l)}) {}
    SExpr(std::initializer_list<SExpr> l) : SExpr(List{std::move(l)}) {}

    enum Kind : int {
        TOKEN,
        INT,
        LIST,
    };

    class Token {
    public:
        std::string name;
    };

    class Int {
    public:
        int value;
    };

    class List {
    public:
        std::vector<SExpr> elems;
    };

    Kind kind() const { return Kind(variant.which()); }
    const Token &token() const { return boost::get<Token>(variant); }
    const Int &num() const { return boost::get<Int>(variant); }
          List &list()       { return boost::get<List>(variant); }
    const List &list() const { return boost::get<List>(variant); }

private:
    typedef boost::variant<Token,Int,List> vartype;
    SExpr(vartype variant) : variant(std::move(variant)) {}

    vartype variant;
};

std::istream &operator>>(std::istream &is, SExpr &sexp);
std::ostream &operator<<(std::ostream &os, const SExpr &sexp);

#endif