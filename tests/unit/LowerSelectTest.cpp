/**
 * @file LowerSelectTest.cpp
 * @brief Unit tests for LowerSelect transform pass
 */

#include "Transform/LowerSelect.h"

#include <gtest/gtest.h>
#include <llvm/AsmParser/Parser.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/SourceMgr.h>

using namespace llvm;

class LowerSelectTest : public ::testing::Test {
protected:
  LLVMContext context;
  std::unique_ptr<Module> parseModule(const char *source) {
    SMDiagnostic err;
    auto module = parseAssemblyString(source, err, context);
    if (!module) {
      err.print("LowerSelectTest", errs());
    }
    return module;
  }
};

// Test that select instructions are converted to branches
TEST_F(LowerSelectTest, LowersSelect) {
  const char *source = R"(
    define i32 @test(i1 %cond, i32 %a, i32 %b) {
      %result = select i1 %cond, i32 %a, i32 %b
      ret i32 %result
    }
  )";

  auto module = parseModule(source);
  ASSERT_NE(module, nullptr);

  Function *F = module->getFunction("test");
  ASSERT_NE(F, nullptr);

  // Count select instructions before transform
  unsigned selectCount = 0;
  for (auto &BB : *F) {
    for (auto &I : BB) {
      if (isa<SelectInst>(&I)) {
        selectCount++;
      }
    }
  }
  EXPECT_EQ(selectCount, 1);

  // Run the pass
  ModuleAnalysisManager MAM;
  LowerSelectPass pass;
  pass.run(*module, MAM);

  // Verify no select instructions remain (pointer selects are skipped)
  selectCount = 0;
  unsigned phiCount = 0;
  for (auto &BB : *F) {
    for (auto &I : BB) {
      if (isa<SelectInst>(&I)) {
        selectCount++;
      }
      if (isa<PHINode>(&I)) {
        phiCount++;
      }
    }
  }
  EXPECT_EQ(selectCount, 0);
  EXPECT_GE(phiCount, 1); // Should have a PHI node from the transformation
}

// Test that pointer selects are not transformed
TEST_F(LowerSelectTest, SkipsPointerSelect) {
  const char *source = R"(
    define i8* @test(i1 %cond, i8* %a, i8* %b) {
      %result = select i1 %cond, i8* %a, i8* %b
      ret i8* %result
    }
  )";

  auto module = parseModule(source);
  ASSERT_NE(module, nullptr);

  ModuleAnalysisManager MAM;
  LowerSelectPass pass;
  pass.run(*module, MAM);

  // Pointer selects should remain
  Function *F = module->getFunction("test");
  unsigned selectCount = 0;
  for (auto &BB : *F) {
    for (auto &I : BB) {
      if (isa<SelectInst>(&I)) {
        selectCount++;
      }
    }
  }
  EXPECT_EQ(selectCount, 1);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

