/*
 * Taint Analysis Implementation
 * 
 * Author: rainoftime
 */

#include "Dataflow/IFDS/Clients/IFDSTaintAnalysis.h"

#include <ostream>

namespace ifds {

// ============================================================================
// TaintFact Implementation
// ============================================================================

TaintFact::TaintFact() : m_type(ZERO), m_value(nullptr), m_memory_location(nullptr), m_source_inst(nullptr) {}

TaintFact TaintFact::zero() { 
    return TaintFact(); 
}

TaintFact TaintFact::tainted_var(const llvm::Value* v, const llvm::Instruction* source) {
    TaintFact fact;
    fact.m_type = TAINTED_VAR;
    fact.m_value = v;
    fact.m_source_inst = source;
    return fact;
}

TaintFact TaintFact::tainted_memory(const llvm::Value* loc, const llvm::Instruction* source) {
    TaintFact fact;
    fact.m_type = TAINTED_MEMORY;
    fact.m_memory_location = loc;
    fact.m_source_inst = source;
    return fact;
}

bool TaintFact::operator==(const TaintFact& other) const {
    if (m_type != other.m_type) return false;
    switch (m_type) {
        case ZERO: return true;
        case TAINTED_VAR: return m_value == other.m_value;
        case TAINTED_MEMORY: return m_memory_location == other.m_memory_location;
    }
    return false;
}

bool TaintFact::operator<(const TaintFact& other) const {
    if (m_type != other.m_type) return m_type < other.m_type;
    switch (m_type) {
        case ZERO: return false;
        case TAINTED_VAR: return m_value < other.m_value;
        case TAINTED_MEMORY: return m_memory_location < other.m_memory_location;
    }
    return false;
}

bool TaintFact::operator!=(const TaintFact& other) const {
    return !(*this == other);
}

TaintFact::Type TaintFact::get_type() const { 
    return m_type; 
}

const llvm::Value* TaintFact::get_value() const { 
    return m_value; 
}

const llvm::Value* TaintFact::get_memory_location() const { 
    return m_memory_location; 
}

const llvm::Instruction* TaintFact::get_source() const {
    return m_source_inst;
}

TaintFact TaintFact::with_source(const llvm::Instruction* source) const {
    TaintFact fact = *this;
    if (!fact.m_source_inst) {
        fact.m_source_inst = source;
    }
    return fact;
}

bool TaintFact::is_zero() const { 
    return m_type == ZERO; 
}

bool TaintFact::is_tainted_var() const { 
    return m_type == TAINTED_VAR; 
}

bool TaintFact::is_tainted_memory() const { 
    return m_type == TAINTED_MEMORY; 
}

std::ostream& operator<<(std::ostream& os, const TaintFact& fact) {
    switch (fact.m_type) {
        case TaintFact::ZERO: os << "ZERO"; break;
        case TaintFact::TAINTED_VAR: 
            os << "Tainted(" << fact.m_value->getName().str() << ")"; 
            break;
        case TaintFact::TAINTED_MEMORY: 
            os << "TaintedMem(" << fact.m_memory_location->getName().str() << ")"; 
            break;
    }
    return os;
}

} // namespace ifds

// Hash function implementation
namespace std {
size_t hash<ifds::TaintFact>::operator()(const ifds::TaintFact& fact) const {
    size_t h1 = std::hash<int>{}(static_cast<int>(fact.get_type()));
    size_t h2 = 0;
    if (fact.get_value()) {
        h2 = std::hash<const llvm::Value*>{}(fact.get_value());
    } else if (fact.get_memory_location()) {
        h2 = std::hash<const llvm::Value*>{}(fact.get_memory_location());
    }
    return h1 ^ (h2 << 1);
}
} // namespace std
