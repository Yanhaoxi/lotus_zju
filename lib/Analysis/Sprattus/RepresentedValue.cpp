#include "Analysis/Sprattus/RepresentedValue.h"
#include "Analysis/Sprattus/repr.h"

#include <iostream>

namespace sprattus
{
std::ostream& operator<<(std::ostream& out, const RepresentedValue& value)
{
    return out << repr(value);
}
} // namespace sprattus
