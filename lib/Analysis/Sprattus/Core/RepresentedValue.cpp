#include "Analysis/Sprattus/Core/RepresentedValue.h"
#include "Analysis/Sprattus/Core/repr.h"

#include <iostream>

namespace sprattus
{
std::ostream& operator<<(std::ostream& out, const RepresentedValue& value)
{
    return out << repr(value);
}
} // namespace sprattus
