#include "Verification/SymbolicAbstraction/Core/ConcreteState.h"

#include "Verification/SymbolicAbstraction/Core/repr.h"
#include "Verification/SymbolicAbstraction/Core/ValueMapping.h"
#include "Verification/SymbolicAbstraction/Utils/Z3APIExtension.h"
#include "Verification/SymbolicAbstraction/Core/MemoryModel.h"

namespace symbolic_abstraction
{
ConcreteState::ConcreteState(const ValueMapping& vmap, z3::model model)
    : FunctionContext_(vmap.fctx()), VMap_(new ValueMapping(vmap)),
      Model_(new z3::model(model))
{
    ManagedValues_.reserve(FunctionContext_.representedValues().size());
    for (llvm::Value* value : FunctionContext_.representedValues()) {
        z3::expr e = model.eval(vmap[value], true);
        ManagedValues_.push_back(e);
    }
    Values_ = &ManagedValues_[0];
}

std::ostream& operator<<(std::ostream& out, const ConcreteState::Value& val)
{
    if (val.Tag_ == 0)
        return out << "Value(?)";
    else
        return out << repr((const z3::expr&)val);
}

std::ostream& operator<<(std::ostream& out, const ConcreteState& cstate)
{
    auto& rvals = cstate.FunctionContext_.representedValues();
    bool needs_comma = false;

    out << "{";
    for (unsigned i = 0; i < rvals.size(); i++) {
        if (needs_comma)
            out << ", ";
        else
            needs_comma = true;

        return out << repr(rvals[i]) << ": " << repr(cstate.Values_[i]);
    }
    out << "}";

    return out;
}
} // namespace symbolic_abstraction
