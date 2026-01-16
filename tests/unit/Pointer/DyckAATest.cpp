/**
 * @file DyckAATest.cpp
 * @brief Unit tests for DyckAA (unification-based alias analysis)
 */

#include "Alias/DyckAA/DyckAliasAnalysis.h"

#include <gtest/gtest.h>
#include <llvm/AsmParser/Parser.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/SourceMgr.h>

using namespace llvm;

class DyckAATest : public ::testing::Test {
protected:
  LLVMContext context;
  std::unique_ptr<Module> parseModule(const char *source) {
    SMDiagnostic err;
    auto module = parseAssemblyString(source, err, context);
    if (!module) {
      err.print("DyckAATest", errs());
    }
    return module;
  }
};

// Test basic alias analysis on pointer assignment
TEST_F(DyckAATest, SimpleAlias) {
  const char *source = R"(
    define i32 @test() {
      %x = alloca i32
      %p = alloca i32*
      store i32* %x, i32** %p
      %q = load i32*, i32** %p
      ret i32 0
    }
  )";

  auto module = parseModule(source);
  ASSERT_NE(module, nullptr);

  DyckAliasAnalysis DAA;
  DAA.runOnModule(*module);

  Function *F = module->getFunction("test");
  ASSERT_NE(F, nullptr);

  Value *x = nullptr, *q = nullptr;
  for (auto &BB : *F) {
    for (auto &I : BB) {
      if (AllocaInst *AI = dyn_cast<AllocaInst>(&I)) {
        if (AI->getAllocatedType()->isIntegerTy(32)) {
          x = AI;
        }
      }
      if (LoadInst *LI = dyn_cast<LoadInst>(&I)) {
        if (LI->getType()->isPointerTy()) {
          q = LI;
        }
      }
    }
  }

  ASSERT_NE(x, nullptr);
  ASSERT_NE(q, nullptr);

  // %q should alias with %x after load
  bool aliases = DAA.mayAlias(x, q);
  EXPECT_TRUE(aliases);
}

// Test that different allocations don't alias
TEST_F(DyckAATest, NoAlias) {
  const char *source = R"(
    define i32 @test() {
      %x = alloca i32
      %y = alloca i32
      ret i32 0
    }
  )";

  auto module = parseModule(source);
  ASSERT_NE(module, nullptr);

  DyckAliasAnalysis DAA;
  DAA.runOnModule(*module);

  Function *F = module->getFunction("test");
  ASSERT_NE(F, nullptr);

  Value *x = nullptr, *y = nullptr;
  for (auto &BB : *F) {
    for (auto &I : BB) {
      if (AllocaInst *AI = dyn_cast<AllocaInst>(&I)) {
        if (!x)
          x = AI;
        else if (!y)
          y = AI;
      }
    }
  }

  ASSERT_NE(x, nullptr);
  ASSERT_NE(y, nullptr);

  // Different stack allocations should not alias
  bool aliases = DAA.mayAlias(x, y);
  EXPECT_FALSE(aliases);
}

// Test null pointer detection
TEST_F(DyckAATest, NullPointer) {
  const char *source = R"(
    define i32 @test() {
      %x = alloca i32*
      store i32* null, i32** %x
      %p = load i32*, i32** %x
      ret i32 0
    }
  )";

  auto module = parseModule(source);
  ASSERT_NE(module, nullptr);

  DyckAliasAnalysis DAA;
  DAA.runOnModule(*module);

  Function *F = module->getFunction("test");
  ASSERT_NE(F, nullptr);

  Value *p = nullptr;
  for (auto &BB : *F) {
    for (auto &I : BB) {
      if (LoadInst *LI = dyn_cast<LoadInst>(&I)) {
        if (LI->getType()->isPointerTy()) {
          p = LI;
        }
      }
    }
  }

  ASSERT_NE(p, nullptr);

  bool mayBeNull = DAA.mayNull(p);
  EXPECT_TRUE(mayBeNull);
}

TEST_F(DyckAATest, StoreAndLoadAlias) {
  const char *source = R"(
    define i32 @test() {
      %x = alloca i32
      %p = alloca i32*
      %q = alloca i32*
      store i32* %x, i32** %p
      %l1 = load i32*, i32** %p
      store i32* %l1, i32** %q
      %l2 = load i32*, i32** %q
      ret i32 0
    }
  )";

  auto module = parseModule(source);
  ASSERT_NE(module, nullptr);

  DyckAliasAnalysis DAA;
  DAA.runOnModule(*module);

  Function *F = module->getFunction("test");
  ASSERT_NE(F, nullptr);

  Value *x = nullptr;
  Value *l2 = nullptr;
  for (auto &BB : *F) {
    for (auto &I : BB) {
      if (AllocaInst *AI = dyn_cast<AllocaInst>(&I)) {
        if (AI->getAllocatedType()->isIntegerTy(32)) {
          x = AI;
        }
      }
      if (LoadInst *LI = dyn_cast<LoadInst>(&I)) {
        if (LI->getName() == "l2") {
          l2 = LI;
        }
      }
    }
  }

  ASSERT_NE(x, nullptr);
  ASSERT_NE(l2, nullptr);

  EXPECT_TRUE(DAA.mayAlias(x, l2));
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
