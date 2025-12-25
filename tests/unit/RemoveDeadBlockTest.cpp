/**
 * @file RemoveDeadBlockTest.cpp
 * @brief Unit tests for RemoveDeadBlock transform pass
 */

#include "Transform/RemoveDeadBlock.h"

#include <gtest/gtest.h>
#include <llvm/AsmParser/Parser.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/SourceMgr.h>

using namespace llvm;

class RemoveDeadBlockTest : public ::testing::Test {
protected:
  LLVMContext context;
  std::unique_ptr<Module> parseModule(const char *source) {
    SMDiagnostic err;
    auto module = parseAssemblyString(source, err, context);
    if (!module) {
      err.print("RemoveDeadBlockTest", errs());
    }
    return module;
  }
};

// Test that unreachable blocks are removed
TEST_F(RemoveDeadBlockTest, RemovesDeadBlock) {
  const char *source = R"(
    define i32 @test() {
    entry:
      br label %exit
    dead:
      %x = add i32 1, 2
      br label %exit
    exit:
      ret i32 0
    }
  )";

  auto module = parseModule(source);
  ASSERT_NE(module, nullptr);

  Function *F = module->getFunction("test");
  ASSERT_NE(F, nullptr);

  // Count blocks before transform
  unsigned blockCount = 0;
  bool foundDead = false;
  for (auto &BB : *F) {
    blockCount++;
    if (BB.getName() == "dead") {
      foundDead = true;
    }
  }
  EXPECT_EQ(blockCount, 3);
  EXPECT_TRUE(foundDead);

  // Run the pass
  ModuleAnalysisManager MAM;
  RemoveDeadBlockPass pass;
  pass.run(*module, MAM);

  // Verify dead block is removed
  blockCount = 0;
  foundDead = false;
  for (auto &BB : *F) {
    blockCount++;
    if (BB.getName() == "dead") {
      foundDead = true;
    }
  }
  EXPECT_LE(blockCount, 2);
  EXPECT_FALSE(foundDead);
}

// Test that entry block is not removed
TEST_F(RemoveDeadBlockTest, PreservesEntryBlock) {
  const char *source = R"(
    define i32 @test() {
    entry:
      ret i32 0
    }
  )";

  auto module = parseModule(source);
  ASSERT_NE(module, nullptr);

  Function *F = module->getFunction("test");
  ASSERT_NE(F, nullptr);

  BasicBlock *entryBB = &F->getEntryBlock();

  ModuleAnalysisManager MAM;
  RemoveDeadBlockPass pass;
  pass.run(*module, MAM);

  // Entry block should still exist
  EXPECT_EQ(&F->getEntryBlock(), entryBB);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

