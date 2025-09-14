/*
 * LLVM Value Domain Implementation
 * Abstract domain for LLVM values with constant propagation and interval analysis
 */

#include <Analysis/sparta/LLVMAbstractInterpreter.h>

#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/Support/raw_ostream.h>

#include <boost/optional.hpp>

#include <algorithm>
#include <cassert>
#include <climits>

namespace sparta {
namespace llvm_ai {

// ============================================================================
// LLVMValueDomain Implementation
// ============================================================================

bool LLVMValueDomain::leq(const LLVMValueDomain& other) const {
    if (is_bottom()) return true;
    if (other.is_bottom()) return false;
    if (other.is_top()) return true;
    if (is_top()) return false;
    
    if (m_kind != other.m_kind) return false;
    
    switch (m_kind) {
        case ValueKind::Constant:
            return m_int_constant == other.m_int_constant;
        case ValueKind::Interval:
            return m_interval.m_low >= other.m_interval.m_low && 
                   m_interval.m_high <= other.m_interval.m_high;
        case ValueKind::Pointer:
            return m_pointer_base == other.m_pointer_base;
        default:
            return false;
    }
}

bool LLVMValueDomain::equals(const LLVMValueDomain& other) const {
    if (m_kind != other.m_kind) return false;
    
    switch (m_kind) {
        case ValueKind::Bottom:
        case ValueKind::Top:
        case ValueKind::Unknown:
            return true;
        case ValueKind::Constant:
            return m_int_constant == other.m_int_constant;
        case ValueKind::Interval:
            return m_interval.m_low == other.m_interval.m_low && 
                   m_interval.m_high == other.m_interval.m_high;
        case ValueKind::Pointer:
            return m_pointer_base == other.m_pointer_base;
    }
    return false;
}

void LLVMValueDomain::join_with(const LLVMValueDomain& other) {
    if (is_bottom()) {
        *this = other;
        return;
    }
    if (other.is_bottom() || is_top()) {
        return;
    }
    if (other.is_top()) {
        set_to_top();
        return;
    }
    
    if (m_kind == other.m_kind) {
        switch (m_kind) {
            case ValueKind::Constant:
                if (m_int_constant != other.m_int_constant) {
                    // Join two different constants -> interval
                    int64_t low = std::min(m_int_constant, other.m_int_constant);
                    int64_t high = std::max(m_int_constant, other.m_int_constant);
                    m_kind = ValueKind::Interval;
                    m_interval.m_low = low;
                    m_interval.m_high = high;
                }
                break;
            case ValueKind::Interval:
                m_interval.m_low = std::min(m_interval.m_low, other.m_interval.m_low);
                m_interval.m_high = std::max(m_interval.m_high, other.m_interval.m_high);
                break;
            case ValueKind::Pointer:
                if (m_pointer_base != other.m_pointer_base) {
                    set_to_top();
                }
                break;
            default:
                break;
        }
    } else {
        // Different kinds -> top
        set_to_top();
    }
}

void LLVMValueDomain::widen_with(const LLVMValueDomain& other) {
    if (is_bottom()) {
        *this = other;
        return;
    }
    if (other.is_bottom() || is_top()) {
        return;
    }
    
    if (m_kind == ValueKind::Interval && other.m_kind == ValueKind::Interval) {
        // Widening for intervals: extend to infinity if bounds are increasing
        if (other.m_interval.m_low < m_interval.m_low) {
            m_interval.m_low = INT64_MIN;
        }
        if (other.m_interval.m_high > m_interval.m_high) {
            m_interval.m_high = INT64_MAX;
        }
    } else {
        join_with(other);
    }
}

void LLVMValueDomain::meet_with(const LLVMValueDomain& other) {
    if (is_bottom() || other.is_top()) {
        return;
    }
    if (other.is_bottom()) {
        set_to_bottom();
        return;
    }
    if (is_top()) {
        *this = other;
        return;
    }
    
    if (m_kind == other.m_kind) {
        switch (m_kind) {
            case ValueKind::Constant:
                if (m_int_constant != other.m_int_constant) {
                    set_to_bottom();
                }
                break;
            case ValueKind::Interval: {
                int64_t new_low = std::max(m_interval.m_low, other.m_interval.m_low);
                int64_t new_high = std::min(m_interval.m_high, other.m_interval.m_high);
                if (new_low > new_high) {
                    set_to_bottom();
                } else {
                    m_interval.m_low = new_low;
                    m_interval.m_high = new_high;
                    if (new_low == new_high) {
                        m_kind = ValueKind::Constant;
                        m_int_constant = new_low;
                    }
                }
                break;
            }
            case ValueKind::Pointer:
                if (m_pointer_base != other.m_pointer_base) {
                    set_to_bottom();
                }
                break;
            default:
                break;
        }
    } else {
        set_to_bottom();
    }
}

void LLVMValueDomain::narrow_with(const LLVMValueDomain& other) {
    meet_with(other);
}

// ============================================================================
// Arithmetic Operations
// ============================================================================

LLVMValueDomain LLVMValueDomain::add(const LLVMValueDomain& other) const {
    if (is_bottom() || other.is_bottom()) return LLVMValueDomain::bottom();
    if (is_top() || other.is_top()) return LLVMValueDomain::top();
    
    if (m_kind == ValueKind::Constant && other.m_kind == ValueKind::Constant) {
        // Overflow check
        if ((other.m_int_constant > 0 && m_int_constant > INT64_MAX - other.m_int_constant) ||
            (other.m_int_constant < 0 && m_int_constant < INT64_MIN - other.m_int_constant)) {
            return LLVMValueDomain::top();
        }
        return LLVMValueDomain(m_int_constant + other.m_int_constant);
    }
    
    auto this_interval = get_interval();
    auto other_interval = other.get_interval();
    
    if (this_interval && other_interval) {
        int64_t low = this_interval->first + other_interval->first;
        int64_t high = this_interval->second + other_interval->second;
        
        // Overflow check
        if (low > high) return LLVMValueDomain::top();
        
        return LLVMValueDomain(low, high);
    }
    
    return LLVMValueDomain::top();
}

LLVMValueDomain LLVMValueDomain::sub(const LLVMValueDomain& other) const {
    if (is_bottom() || other.is_bottom()) return LLVMValueDomain::bottom();
    if (is_top() || other.is_top()) return LLVMValueDomain::top();
    
    if (m_kind == ValueKind::Constant && other.m_kind == ValueKind::Constant) {
        return LLVMValueDomain(m_int_constant - other.m_int_constant);
    }
    
    auto this_interval = get_interval();
    auto other_interval = other.get_interval();
    
    if (this_interval && other_interval) {
        int64_t low = this_interval->first - other_interval->second;
        int64_t high = this_interval->second - other_interval->first;
        return LLVMValueDomain(low, high);
    }
    
    return LLVMValueDomain::top();
}

LLVMValueDomain LLVMValueDomain::mul(const LLVMValueDomain& other) const {
    if (is_bottom() || other.is_bottom()) return LLVMValueDomain::bottom();
    if (is_top() || other.is_top()) return LLVMValueDomain::top();
    
    if (m_kind == ValueKind::Constant && other.m_kind == ValueKind::Constant) {
        return LLVMValueDomain(m_int_constant * other.m_int_constant);
    }
    
    // For intervals, multiplication is more complex
    // For now, we approximate with top for non-constant cases
    return LLVMValueDomain::top();
}

LLVMValueDomain LLVMValueDomain::div(const LLVMValueDomain& other) const {
    if (is_bottom() || other.is_bottom()) return LLVMValueDomain::bottom();
    if (is_top() || other.is_top()) return LLVMValueDomain::top();
    
    // Check for division by zero
    if (other.m_kind == ValueKind::Constant && other.m_int_constant == 0) {
        return LLVMValueDomain::bottom(); // Undefined
    }
    
    if (m_kind == ValueKind::Constant && other.m_kind == ValueKind::Constant) {
        return LLVMValueDomain(m_int_constant / other.m_int_constant);
    }
    
    return LLVMValueDomain::top();
}

// ============================================================================
// Comparison Operations
// ============================================================================

LLVMValueDomain LLVMValueDomain::icmp_eq(const LLVMValueDomain& other) const {
    if (is_bottom() || other.is_bottom()) return LLVMValueDomain::bottom();
    
    if (m_kind == ValueKind::Constant && other.m_kind == ValueKind::Constant) {
        return LLVMValueDomain(m_int_constant == other.m_int_constant ? 1 : 0);
    }
    
    // For intervals, check if they can be equal
    auto this_interval = get_interval();
    auto other_interval = other.get_interval();
    
    if (this_interval && other_interval) {
        bool can_be_equal = !(this_interval->second < other_interval->first ||
                             other_interval->second < this_interval->first);
        if (!can_be_equal) {
            return LLVMValueDomain(static_cast<int64_t>(0)); // Definitely false
        }
        if (this_interval->first == this_interval->second &&
            other_interval->first == other_interval->second &&
            this_interval->first == other_interval->first) {
            return LLVMValueDomain(static_cast<int64_t>(1)); // Definitely true
        }
    }
    
    return LLVMValueDomain(static_cast<int64_t>(0), static_cast<int64_t>(1)); // Could be either true or false
}

LLVMValueDomain LLVMValueDomain::icmp_ne(const LLVMValueDomain& other) const {
    auto eq_result = icmp_eq(other);
    if (eq_result.is_constant()) {
        auto constant = eq_result.get_constant();
        if (constant) {
            return LLVMValueDomain(*constant == 0 ? 1 : 0);
        }
    }
    return LLVMValueDomain(0, 1);
}

LLVMValueDomain LLVMValueDomain::icmp_slt(const LLVMValueDomain& other) const {
    if (is_bottom() || other.is_bottom()) return LLVMValueDomain::bottom();
    
    if (m_kind == ValueKind::Constant && other.m_kind == ValueKind::Constant) {
        return LLVMValueDomain(m_int_constant < other.m_int_constant ? 1 : 0);
    }
    
    auto this_interval = get_interval();
    auto other_interval = other.get_interval();
    
    if (this_interval && other_interval) {
        if (this_interval->second < other_interval->first) {
            return LLVMValueDomain(static_cast<int64_t>(1)); // Definitely true
        }
        if (this_interval->first >= other_interval->second) {
            return LLVMValueDomain(static_cast<int64_t>(0)); // Definitely false
        }
    }
    
    return LLVMValueDomain(static_cast<int64_t>(0), static_cast<int64_t>(1)); // Could be either
}

LLVMValueDomain LLVMValueDomain::icmp_sle(const LLVMValueDomain& other) const {
    if (is_bottom() || other.is_bottom()) return LLVMValueDomain::bottom();
    
    if (m_kind == ValueKind::Constant && other.m_kind == ValueKind::Constant) {
        return LLVMValueDomain(m_int_constant <= other.m_int_constant ? 1 : 0);
    }
    
    auto this_interval = get_interval();
    auto other_interval = other.get_interval();
    
    if (this_interval && other_interval) {
        if (this_interval->second <= other_interval->first) {
            return LLVMValueDomain(static_cast<int64_t>(1)); // Definitely true
        }
        if (this_interval->first > other_interval->second) {
            return LLVMValueDomain(static_cast<int64_t>(0)); // Definitely false
        }
    }
    
    return LLVMValueDomain(static_cast<int64_t>(0), static_cast<int64_t>(1)); // Could be either
}

// ============================================================================
// Factory Methods
// ============================================================================

LLVMValueDomain LLVMValueDomain::from_llvm_constant(const llvm::Constant* c) {
    if (auto* ci = llvm::dyn_cast<llvm::ConstantInt>(c)) {
        if (ci->getBitWidth() <= 64) {
            return LLVMValueDomain(ci->getSExtValue());
        }
    } else if (llvm::isa<llvm::ConstantPointerNull>(c)) {
        return LLVMValueDomain(static_cast<int64_t>(0));
    } else if (auto* gv = llvm::dyn_cast<llvm::GlobalValue>(c)) {
        return LLVMValueDomain(gv);
    }
    
    return LLVMValueDomain::top();
}

LLVMValueDomain LLVMValueDomain::from_llvm_value(const llvm::Value* v) {
    if (auto* c = llvm::dyn_cast<llvm::Constant>(v)) {
        return from_llvm_constant(c);
    }
    
    return LLVMValueDomain::top();
}

// ============================================================================
// Output Operators
// ============================================================================

std::ostream& operator<<(std::ostream& os, const LLVMValueDomain& domain) {
    switch (domain.m_kind) {
        case LLVMValueDomain::ValueKind::Bottom:
            os << "⊥";
            break;
        case LLVMValueDomain::ValueKind::Top:
        case LLVMValueDomain::ValueKind::Unknown:
            os << "⊤";
            break;
        case LLVMValueDomain::ValueKind::Constant:
            os << domain.m_int_constant;
            break;
        case LLVMValueDomain::ValueKind::Interval:
            os << "[" << domain.m_interval.m_low << ", " << domain.m_interval.m_high << "]";
            break;
        case LLVMValueDomain::ValueKind::Pointer:
            os << "ptr(" << domain.m_pointer_base << ")";
            break;
    }
    return os;
}

} // namespace llvm_ai
} // namespace sparta
