#include "Analysis/SymbolicAbstraction/Core/RepresentedValue.h"
#include "Analysis/SymbolicAbstraction/Core/repr.h"

#include <iostream>

namespace symbolic_abstraction
{
std::ostream& operator<<(std::ostream& out, const RepresentedValue& value)
{
    return out << repr(value);
}
} // namespace symbolic_abstraction
