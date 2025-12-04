#pragma once

#include "Analysis/Sprattus/Utils/Utils.h"
#include "Analysis/Sprattus/Core/AbstractValue.h"
#include "Analysis/Sprattus/Core/ConcreteState.h"
#include "Analysis/Sprattus/Core/FunctionContext.h"
#include "Analysis/Sprattus/Core/Expression.h"

#include "Analysis/Sprattus/Domains/Product.h"
#include <memory>

#include <llvm/IR/Value.h>
#include <llvm/IR/CFG.h>

namespace sprattus
{
class FunctionContext;

namespace domains
{
using std::unique_ptr;

/**
 *  This class represents a value in the Abstract Domain of predicates, i.e.
 *  for a given predicate `p`, it stores its state in the lattice given by the
 *  following Hasse diagram:
 *
 *           TOP
 *         /    \
 *      TRUE   FALSE
 *         \    /
 *         BOTTOM
 *
 *  Here, `TRUE` means that `p` is true in every program run, `FALSE` that `p`
 *  is false in every program run, `TOP` that both cases might occur and
 *  `BOTTOM` that none of them occur.
 */
class Predicates : public AbstractValue
{
  public:
    typedef std::function<Expression(Expression, Expression)> pred_t;
    enum Value { BOTTOM, TRUE, FALSE, TOP };

  private:
    const FunctionContext& FunctionContext_;
    Expression Predicate_;
    Value Val_ = BOTTOM;

  public:
    Predicates(const FunctionContext& fctx, Expression predicate)
        : FunctionContext_(fctx), Predicate_(predicate)
    {
    }

    virtual bool joinWith(const AbstractValue& av_other) override;

    virtual bool meetWith(const AbstractValue& av_other) override;

    virtual bool updateWith(const ConcreteState& cstate) override;

    virtual z3::expr toFormula(const ValueMapping& vmap,
                               z3::context& zctx) const override;

    virtual void havoc() override { Val_ = TOP; }

    virtual AbstractValue* clone() const override
    {
        return new Predicates(*this);
    }

    virtual bool isTop() const override { return Val_ == TOP; }
    virtual bool isBottom() const override { return Val_ == BOTTOM; }

    virtual void prettyPrint(PrettyPrinter& out) const override;

    virtual void resetToBottom() override { Val_ = BOTTOM; }

    virtual bool isJoinableWith(const AbstractValue& other) const override;

    Value getValue() const { return Val_; }

#ifdef ENABLE_DYNAMIC
    template <class Archive> void save(Archive& archive) const
    {
        archive(Predicate_);
        archive(Val_);
    }

    template <class Archive> void load(Archive& archive) {}

    template <class Archive>
    static void load_and_construct(Archive& archive,
                                   cereal::construct<Predicates>& construct)
    {
        auto& fctx = cereal::get_user_data<FunctionContext>(archive);
        auto predicate = Expression::LoadFrom(archive);
        construct(fctx, predicate);
        archive(construct->Val_);
    }
#endif
};

/**
 *  A wrapper for `Predicates` to make it compatible with parameterization
 *  strategies.
 *  It uses the PRED function to construct an Expression containing its two
 *  arguments and uses it for a Predicates domain.
 */
template <Predicates::pred_t* PRED> class PredicatesWrapper : public Predicates
{
  public:
    PredicatesWrapper(const FunctionContext& fctx, Expression left,
                      Expression right)
        : Predicates(fctx, (*PRED)(left, right))
    {
    }

    virtual ~PredicatesWrapper() {}
};

} // namespace domains
} // namespace sprattus

#ifdef ENABLE_DYNAMIC
CEREAL_REGISTER_TYPE(sprattus::domains::Predicates);
#endif
