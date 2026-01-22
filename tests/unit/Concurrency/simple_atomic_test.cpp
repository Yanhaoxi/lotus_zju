#include "Analysis/Concurrency/MHPAnalysis.h"
#include "Analysis/Concurrency/Cpp11Atomics.h"
#include <llvm/AsmParser/Parser.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/SourceMgr.h>
#include <gtest/gtest.h>

using namespace llvm;
using namespace mhp;

class AtomicHappensBeforeTest : public ::testing::Test {
protected:
  LLVMContext context;
  std::unique_ptr<Module> parseModule(const char *source) {
    SMDiagnostic err;
    auto module = parseAssemblyString(source, err, context);
    if (!module) {
      err.print("AtomicHappensBeforeTest", errs());
    }
    return module;
  }

  const Instruction *findInstructionByName(const Function &func, StringRef name) {
      for (const auto &bb : func) {
          for (const auto &inst : bb) {
              if (inst.getName() == name) {
                  return &inst;
              }
          }
      }
      return nullptr;
  }
};

TEST_F(AtomicHappensBeforeTest, ReleaseAcquireOrdering) {
  const char *source = R"(
    @data = global i32 0, align 4
    @flag = global i8 0, align 1

    declare i32 @pthread_create(i8*, i8*, i8* (i8*)*, i8*)

    define i8* @writer(i8* %arg) {
    entry:
      store i32 42, i32* @data, align 4
      store atomic i8 1, i8* @flag release, align 1
      ret i8* null
    }

    define i8* @reader(i8* %arg) {
    entry:
      %load_flag = load atomic i8, i8* @flag acquire, align 1
      %cond = icmp ne i8 %load_flag, 0
      br i1 %cond, label %if.then, label %if.end

    if.then:
      %load_data = load i32, i32* @data, align 4
      br label %if.end

    if.end:
      ret i8* null
    }

    define i32 @main() {
    entry:
      %writer_tid = alloca i8
      %reader_tid = alloca i8
      call i32 @pthread_create(i8* %writer_tid, i8* null, i8* (i8*)* @writer, i8* null)
      call i32 @pthread_create(i8* %reader_tid, i8* null, i8* (i8*)* @reader, i8* null)
      ret i32 0
    }
  )";

  auto module = parseModule(source);
  ASSERT_NE(module, nullptr);
  
  const Function *writer_func = module->getFunction("writer");
  const Function *reader_func = module->getFunction("reader");
  ASSERT_NE(writer_func, nullptr);
  ASSERT_NE(reader_func, nullptr);

  // Get instructions by position/name
  const Instruction* store_data = &writer_func->getEntryBlock().front();
  const Instruction* load_data = findInstructionByName(*reader_func, "load_data");

  ASSERT_NE(store_data, nullptr);
  ASSERT_TRUE(isa<StoreInst>(store_data));
  ASSERT_NE(load_data, nullptr);
  
  MHPAnalysis mhp(*module);
  mhp.analyze();

  // Due to the release-acquire semantics on @flag, the store to @data in the writer
  // MUST happen-before the load from @data in the reader.
  // Therefore, they CANNOT happen in parallel.
  EXPECT_FALSE(mhp.mayHappenInParallel(store_data, load_data));
  
  // A stronger check is that the store must precede the load.
  EXPECT_TRUE(mhp.mustPrecede(store_data, load_data));
}
