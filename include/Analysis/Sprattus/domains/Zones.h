#pragma once

#include "Analysis/Sprattus/Utils.h"
#include "Analysis/Sprattus/AbstractValue.h"
#include "Analysis/Sprattus/DomainConstructor.h"

#include <limits>

namespace sprattus {
namespace domains {

/**
 * Zones (Difference-Bound Matrix) domain for a pair of LLVM scalar values.
 * Represents bounds on the difference: Lower_ <= (Left - Right) <= Upper_
 *
 * Invariant: x - y in [Lower_, Upper_]
 * 
 * States:
 *   TOP:      Top_ == true
 *   BOTTOM:   Bottom_ == true  OR  Lower_ > Upper_
 *   VALUE:    !Top_ && !Bottom_ with finite bounds
 */
class Zone : public AbstractValue {
    const FunctionContext &Fctx_;
    RepresentedValue Left_;
    RepresentedValue Right_;

    bool Top_ = true;
    bool Bottom_ = true;
    
    // Bounds: Lower_ <= (Left - Right) <= Upper_
    int64_t Upper_ = std::numeric_limits<int64_t>::max();
    int64_t Lower_ = std::numeric_limits<int64_t>::min();

    bool isInconsistent() const { return Lower_ > Upper_; }
    void checkConsistency() { if (isInconsistent() && !Bottom_) resetToBottom(); }

  public:
    Zone(const FunctionContext &fctx, RepresentedValue left, RepresentedValue right)
        : Fctx_(fctx), Left_(left), Right_(right) {}

    // AbstractValue interface
    bool joinWith(const AbstractValue &other) override;
    bool meetWith(const AbstractValue &other) override;
    bool updateWith(const ConcreteState &cstate) override;
    z3::expr toFormula(const ValueMapping &vmap, z3::context &ctx) const override;

    void havoc() override { 
        Top_ = true; 
        Bottom_ = false;
        Upper_ = std::numeric_limits<int64_t>::max();
        Lower_ = std::numeric_limits<int64_t>::min();
    }
    
    void resetToBottom() override { 
        Top_ = false; 
        Bottom_ = true;
        Upper_ = std::numeric_limits<int64_t>::min();
        Lower_ = std::numeric_limits<int64_t>::max();
    }
    
    bool isTop() const override { return Top_ && !Bottom_; }
    bool isBottom() const override { return Bottom_ || isInconsistent(); }

    AbstractValue *clone() const override { return new Zone(*this); }
    bool isJoinableWith(const AbstractValue &other) const override;

    void prettyPrint(PrettyPrinter &out) const override;

    // Accessors for closure algorithm
    int64_t getUpper() const { return Upper_; }
    int64_t getLower() const { return Lower_; }
    RepresentedValue getLeft() const { return Left_; }
    RepresentedValue getRight() const { return Right_; }
    
    // Tighten bounds (used by closure)
    bool tighten(int64_t new_lower, int64_t new_upper);
};

} // namespace domains
} // namespace sprattus
