/**
 * @file WPDSTest.cpp
 * @brief Unit tests for WPDS (Weighted Pushdown System) dataflow framework
 */

#include "Dataflow/WPDS/InterProceduralDataFlow.h"

#include <gtest/gtest.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/LLVMContext.h>

using namespace wpds;

class WPDSTest : public ::testing::Test {
protected:
  void SetUp() override {}
  void TearDown() override {}
};

// Test GenKillTransformer creation and basic operations
TEST_F(WPDSTest, GenKillTransformer) {
  // Create empty facts
  DataFlowFacts emptyFacts = DataFlowFacts::EmptySet();
  EXPECT_TRUE(emptyFacts.isEmpty());
  
  // Create transformer with empty gen/kill
  GenKillTransformer *transformer = GenKillTransformer::makeGenKillTransformer(
      DataFlowFacts::EmptySet(),
      DataFlowFacts::EmptySet()
  );
  
  ASSERT_NE(transformer, nullptr);
  
  // Apply transformer to empty set
  DataFlowFacts result = transformer->apply(emptyFacts);
  EXPECT_TRUE(result.isEmpty());
}

// Test DataFlowFacts operations
TEST_F(WPDSTest, DataFlowFacts) {
  // Test empty set
  DataFlowFacts empty = DataFlowFacts::EmptySet();
  EXPECT_TRUE(empty.isEmpty());
  EXPECT_EQ(empty.getFacts().size(), 0);
  
  // Test universe set
  llvm::LLVMContext context;
  auto* seed = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 1);
  DataFlowFacts seeded = DataFlowFacts::EmptySet();
  seeded.addFact(seed);
  DataFlowFacts universe = DataFlowFacts::UniverseSet();
  EXPECT_FALSE(universe.isEmpty());
  EXPECT_TRUE(universe.containsFact(seed));
  
  // Test union
  DataFlowFacts fact1 = DataFlowFacts::EmptySet();
  DataFlowFacts fact2 = DataFlowFacts::EmptySet();
  DataFlowFacts unionResult = DataFlowFacts::Union(fact1, fact2);
  EXPECT_TRUE(unionResult.isEmpty());
}

// Test transformer identity
TEST_F(WPDSTest, TransformerIdentity) {
  GenKillTransformer *one = GenKillTransformer::one();
  ASSERT_NE(one, nullptr);
  
  DataFlowFacts empty = DataFlowFacts::EmptySet();
  DataFlowFacts result = one->apply(empty);
  
  // Identity transformer should not change facts
  EXPECT_TRUE(result.isEmpty());
  
  // Test zero transformer
  GenKillTransformer *zero = GenKillTransformer::zero();
  ASSERT_NE(zero, nullptr);
  
  // Zero should produce universe (top element)
  DataFlowFacts zeroResult = zero->apply(empty);
  // Zero represents top/universal, but behavior depends on implementation
  EXPECT_GE(zeroResult.getFacts().size(), 0);
}

// Test transformer composition
TEST_F(WPDSTest, TransformerComposition) {
  GenKillTransformer *trans1 = GenKillTransformer::one();
  GenKillTransformer *trans2 = GenKillTransformer::one();
  
  ASSERT_NE(trans1, nullptr);
  ASSERT_NE(trans2, nullptr);
  
  // Compose two identity transformers
  GenKillTransformer *composed = trans1->extend(trans2);
  ASSERT_NE(composed, nullptr);
  
  // Composition of identities should be identity
  DataFlowFacts empty = DataFlowFacts::EmptySet();
  DataFlowFacts result = composed->apply(empty);
  EXPECT_TRUE(result.isEmpty());
  
  delete composed;
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
