#pragma once

#include "Verification/SymbolicAbstraction/Utils/Utils.h"
#include "Verification/SymbolicAbstraction/Core/AbstractValue.h"
#include "Verification/SymbolicAbstraction/Core/DomainConstructor.h"

#include <limits>

namespace symbolic_abstraction {
namespace domains {

/**
 * Octagon domain for a pair of LLVM scalar values.
 * Tracks constraints of the form:  ±x ± y ≤ c
 * 
 * Four directions:
 *   c[0]:  +x - y  ≤  c[0]
 *   c[1]:  -x + y  ≤  c[1]
 *   c[2]:  +x + y  ≤  c[2]
 *   c[3]:  -x - y  ≤  c[3]
 * 
 * States:
 *   TOP:      Top_ == true
 *   BOTTOM:   Bottom_ == true  OR  inconsistent bounds
 *   VALUE:    !Top_ && !Bottom_ with finite bounds
 */
class Octagon : public AbstractValue {
    const FunctionContext &Fctx_;
    RepresentedValue X_;
    RepresentedValue Y_;

    bool Top_ = true;
    bool Bottom_ = true;
    
    // Bounds for the four octagonal directions
    // c[0]: x-y, c[1]: -x+y, c[2]: x+y, c[3]: -x-y
    int64_t C_[4];
    
    static constexpr int64_t INF = std::numeric_limits<int64_t>::max();
    
    void initializeToTop() {
        for (int i = 0; i < 4; ++i) C_[i] = INF;
    }
    
    bool isInconsistent() const;
    void checkConsistency();

  public:
    Octagon(const FunctionContext &fctx, RepresentedValue x, RepresentedValue y)
        : Fctx_(fctx), X_(x), Y_(y) {
        initializeToTop();
    }

    // AbstractValue interface
    bool joinWith(const AbstractValue &other) override;
    bool meetWith(const AbstractValue &other) override;
    bool updateWith(const ConcreteState &cstate) override;
    z3::expr toFormula(const ValueMapping &vmap, z3::context &ctx) const override;

    void havoc() override { 
        Top_ = true; 
        Bottom_ = false;
        initializeToTop();
    }
    
    void resetToBottom() override { 
        Top_ = false; 
        Bottom_ = true;
    }
    
    bool isTop() const override { return Top_ && !Bottom_; }
    bool isBottom() const override { return Bottom_; }

    AbstractValue *clone() const override { return new Octagon(*this); }
    bool isJoinableWith(const AbstractValue &other) const override;

    void prettyPrint(PrettyPrinter &out) const override;
};

} // namespace domains
} // namespace symbolic_abstraction

