#pragma once
#include "Analysis/Sprattus/utils.h"
#include "Analysis/Sprattus/ConcreteState.h"
#include "Analysis/Sprattus/PrettyPrinter.h"
#include "Analysis/Sprattus/ResultStore.h"

namespace sprattus
{
class ExpressionBase
{
  public:
    virtual unsigned bits() const = 0;
    virtual z3::expr toFormula(const ValueMapping&) const = 0;
    virtual ConcreteState::Value eval(const ConcreteState&) const = 0;
    virtual void prettyPrint(PrettyPrinter& out) const = 0;
    virtual bool operator==(const ExpressionBase& other) const = 0;
    virtual ~ExpressionBase() = default;
};

class Expression : public ExpressionBase
{
  private:
    shared_ptr<ExpressionBase> Instance_;
    Expression(shared_ptr<ExpressionBase> ptr) : Instance_(ptr) {}

  public:
    Expression(const RepresentedValue& rv);
    Expression(const ConcreteState::Value& value);

    Expression(const FunctionContext& fctx, bool x)
        : Expression(ConcreteState::Value(fctx, x, 1))
    {
    }

    Expression operator-(const Expression& other) const;
    Expression operator+(const Expression& other) const;
    Expression operator*(const Expression& other) const;
    Expression zeroExtend(unsigned bits) const;
    Expression signExtend(unsigned bits) const;
    Expression ule(const Expression& other) const;
    Expression equals(const Expression& other) const;

    Expression(const Expression& other) = default;
    Expression& operator=(const Expression& other) = default;

    friend PrettyPrinter& operator<<(PrettyPrinter& out, const Expression& expr)
    {
        expr.prettyPrint(out);
        return out;
    }

    friend std::ostream& operator<<(std::ostream& out, const Expression& expr)
    {
        PrettyPrinter pp(false);
        expr.prettyPrint(pp);
        return out << pp.str();
    }

    unsigned bits() const override { return Instance_->bits(); }
    unsigned bits(const FunctionContext& fctx) const;

    z3::expr toFormula(const ValueMapping& vmap) const override
    {
        return Instance_->toFormula(vmap);
    }

    ConcreteState::Value eval(const ConcreteState& cstate) const override
    {
        return Instance_->eval(cstate);
    }

    void prettyPrint(PrettyPrinter& out) const override
    {
        Instance_->prettyPrint(out);
    }

    bool operator==(const ExpressionBase& other) const override;

    /**
     * Returns a `RepresentedValue` equal to this expression.
     *
     * If this is an atomic expression consisting of a single value, this method
     * returns that value. Otherwise, it panics.
     */
    RepresentedValue asRepresentedValue() const;

#ifdef ENABLE_DYNAMIC
    // handy shortcut since Expression has no default constructor
    template <class Archive> static inline Expression LoadFrom(Archive& archive)
    {
        Expression e(nullptr);
        archive(e);
        return e;
    }

    template <class Archive> void serialize(Archive& archive)
    {
        archive(Instance_);
    }
#endif
};
} // namespace sprattus
