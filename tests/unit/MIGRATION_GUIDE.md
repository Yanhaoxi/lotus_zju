# Migration Guide: Phasar to Lotus Unit Tests

This guide provides recommendations for migrating unit tests from the phasar-development repository to Lotus.

## Key Differences Between Phasar and Lotus

### 1. IR Database Management
**Phasar:**
```cpp
LLVMProjectIRDB IRDB(PathToLLTestFiles + "test.ll");
const auto *F = IRDB.getFunctionDefinition("main");
```

**Lotus:**
Lotus uses different IR reading mechanisms. Check `include/Utils/LLVM/IO/ReadIR.h` for available functions.

### 2. Control Flow Graphs
**Phasar:**
```cpp
LLVMBasedCFG Cfg;
auto Preds = Cfg.getPredsOf(Inst);
auto Succs = Cfg.getSuccsOf(Inst);
```

**Lotus:**
Lotus uses ICFG with a different API. See `include/IR/ICFG/ICFG.h` for the graph interface.

### 3. Helper Functions
**Phasar provides:**
- `getNthInstruction(F, n)` - Get the nth instruction in a function
- `getNthTermInstruction(F, n)` - Get the nth terminator instruction
- `getNthStoreInstruction(F, n)` - Get the nth store instruction
- `llvmIRToStableString(I)` - Convert IR to stable string representation

**Lotus:**
These helper functions don't exist in Lotus. You'll need to:
1. Iterate through instructions manually
2. Use LLVM's standard APIs to query instruction properties
3. Create helper functions if needed

### 4. Test Utilities

**Phasar:**
```cpp
#include "TestConfig.h"
using namespace psr::unittest;

std::string Path = PathToLLTestFiles + "control_flow/test.ll";
```

**Lotus:**
```cpp
#include "TestUtils/TestConfig.h"
using namespace lotus::unittest;

std::string Path = PathToLLTestFiles + "control_flow/test.ll";
```

## Migration Strategy

### Step 1: Assess the Test
Determine what the test is actually testing:
- Algorithm/logic tests → Easier to migrate
- API-specific tests → May need complete rewrite
- Infrastructure tests → Evaluate if relevant to Lotus

### Step 2: Identify Dependencies
List all phasar-specific dependencies:
- `LLVMProjectIRDB`
- `LLVMBasedCFG` / `LLVMBasedICFG`
- `HelperAnalyses`
- Helper functions
- Analysis problem classes

### Step 3: Find Lotus Equivalents
Map phasar components to Lotus:

| Phasar Component | Lotus Equivalent | Location |
|------------------|------------------|----------|
| `LLVMProjectIRDB` | IR reading utilities | `Utils/LLVM/IO/ReadIR.h` |
| `LLVMBasedCFG` | LLVM's CFG APIs | `llvm/IR/CFG.h` |
| `LLVMBasedICFG` | ICFG | `IR/ICFG/ICFG.h` |
| `BitVectorSet` | Check if available | `Utils/General/ADT/` |
| `EquivalenceClassMap` | ✅ Available | `Utils/General/ADT/EquivalenceClassMap.h` |
| Dataflow solvers | IFDS/IDE framework | `Dataflow/IFDS/` |

### Step 4: Adapt or Rewrite
**If migrating directly:**
1. Replace phasar includes with Lotus equivalents
2. Update namespace from `psr` to appropriate Lotus namespace
3. Adapt API calls to Lotus's interface
4. Update helper function calls
5. Test and fix compilation errors

**If rewriting:**
1. Understand the test's intent
2. Write test using Lotus APIs from scratch
3. Ensure test coverage matches original
4. Document differences if behavior diverges

## Examples

### Example 1: Simple ADT Test (Direct Migration)

**Phasar:**
```cpp
#include "phasar/Utils/EquivalenceClassMap.h"
#include "gtest/gtest.h"

using namespace psr;

TEST(EquivalenceClassMap, ctorEmpty) {
  EquivalenceClassMap<int, std::string> M;
  EXPECT_EQ(M.size(), 0U);
}
```

**Lotus:**
```cpp
#include "Utils/General/ADT/EquivalenceClassMap.h"
#include "gtest/gtest.h"

using namespace psr;  // Lotus kept the same namespace

TEST(EquivalenceClassMap, ctorEmpty) {
  EquivalenceClassMap<int, std::string> M;
  EXPECT_EQ(M.size(), 0U);
}
```

### Example 2: CFG Test (Needs Rewrite)

**Phasar:**
```cpp
TEST(LLVMBasedCFGTest, HandleMultipleSuccessors) {
  LLVMBasedCFG Cfg;
  LLVMProjectIRDB IRDB(PathToLLTestFiles + "control_flow/branch_cpp.ll");
  const auto *F = IRDB.getFunctionDefinition("main");
  const auto *BRInst = getNthTermInstruction(F, 1);
  auto Succs = Cfg.getSuccsOf(BRInst);
  ASSERT_EQ(Succs.size(), 2);
}
```

**Lotus (approach):**
```cpp
TEST(ICFGTest, HandleMultipleSuccessors) {
  // Read IR module
  LLVMContext context;
  auto module = readIRFromFile("control_flow/branch.ll", context);
  
  // Build ICFG
  ICFG icfg;
  ICFGBuilder builder(&icfg);
  builder.build(module.get());
  
  // Find branch instruction manually
  Function *F = module->getFunction("main");
  BranchInst *BRInst = nullptr;
  for (auto &BB : *F) {
    if (auto *BI = dyn_cast<BranchInst>(BB.getTerminator())) {
      BRInst = BI;
      break;
    }
  }
  
  // Query ICFG for successors
  auto *Node = icfg.getIntraBlockNode(BRInst->getParent());
  ASSERT_NE(Node, nullptr);
  ASSERT_EQ(Node->getOutEdges().size(), 2);
}
```

## Common Migration Patterns

### Pattern 1: Replacing getNthInstruction

**Before:**
```cpp
const auto *Inst = getNthInstruction(F, 5);
```

**After:**
```cpp
const llvm::Instruction *Inst = nullptr;
int count = 0;
for (auto &BB : *F) {
  for (auto &I : BB) {
    if (count == 5) {
      Inst = &I;
      break;
    }
    count++;
  }
  if (Inst) break;
}
```

### Pattern 2: Replacing LLVMProjectIRDB

**Before:**
```cpp
LLVMProjectIRDB IRDB(PathToLLTestFiles + "test.ll");
const auto *F = IRDB.getFunctionDefinition("main");
```

**After:**
```cpp
LLVMContext context;
SMDiagnostic err;
auto module = parseIRFile(PathToLLTestFiles + "test.ll", err, context);
Function *F = module->getFunction("main");
```

## Creating New Test Utilities

If you find yourself repeating code, create utilities in `TestUtils/`:

```cpp
// TestUtils/LLVMTestHelpers.h
namespace lotus::unittest {

inline const llvm::Instruction* 
getNthInstruction(const llvm::Function* F, unsigned N) {
  unsigned count = 0;
  for (auto &BB : *F) {
    for (auto &I : BB) {
      if (count == N) return &I;
      count++;
    }
  }
  return nullptr;
}

inline const llvm::Instruction* 
getNthTermInstruction(const llvm::Function* F, unsigned N) {
  unsigned count = 0;
  for (auto &BB : *F) {
    if (auto *Term = BB.getTerminator()) {
      count++;
      if (count == N) return Term;
    }
  }
  return nullptr;
}

} // namespace lotus::unittest
```

## Testing Your Migration

1. **Build Test:**
   ```bash
   cd build
   make <test_name>
   ```

2. **Run Test:**
   ```bash
   ./bin/<test_name>
   ```

3. **Debug Failures:**
   - Check API differences
   - Verify test data files exist
   - Ensure correct linking of libraries
   - Use `llvm-dis` to inspect `.ll` files if needed

## Recommendations

1. **Start Simple:** Begin with utility/ADT tests that have minimal dependencies
2. **Build Infrastructure:** Create helper functions as needed
3. **Document Changes:** Note significant deviations from phasar behavior
4. **Incremental Approach:** Migrate tests gradually, fixing build/link issues
5. **Consider Value:** Not all tests may be worth migrating - focus on high-value tests

## Future Work

To make future migrations easier:
- Create a `LotusTestHelpers` utility library
- Standardize test data organization
- Document Lotus testing best practices
- Build analysis problem test harnesses
