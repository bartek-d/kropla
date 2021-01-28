#include "game.h"
#include "sgf.h"
#include "safety.h"
#include "utils.h"
#include <gtest/gtest.h>

namespace {

class IsometryFixtureS :public ::testing::TestWithParam<unsigned> {
};
  
TEST_P(IsometryFixtureS, safetyIsCorrectlyInitialised1)
{
  const unsigned isometry = GetParam();
  auto sgf = constructSgfFromGameBoard(".....x."
				       "..o...."
				       ".....x."
				       ".x....."
				       "......."
				       "...oxo."
				       ".......");
  Game game = constructGameFromSgfWithIsometry(sgf, isometry);
  Safety safety(game);
  EXPECT_EQ(0.75f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("cb", isometry, coord))));
  EXPECT_EQ(1.50f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("bd", isometry, coord))));
  EXPECT_EQ(0.75f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("df", isometry, coord))));
  EXPECT_EQ(0.0f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("ef", isometry, coord))));
  EXPECT_EQ(0.0f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("ff", isometry, coord))));
  EXPECT_EQ(1.0f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("fc", isometry, coord))));
}

INSTANTIATE_TEST_CASE_P(
        Par,
        IsometryFixtureS,
        ::testing::Values(0,1,2,3,4,5,6,7));

  
class IsometryFixtureS2 :public ::testing::TestWithParam<unsigned> {
};
  
TEST_P(IsometryFixtureS2, safetyIsCorrectlyInitialised2)
{
  const unsigned isometry = GetParam();
  auto sgf = constructSgfFromGameBoard("......."
				       ".o.o.o."
				       ".....x."
				       ".x....."
				       ".....x."
				       ".x.o.o."
				       ".......");
  Game game = constructGameFromSgfWithIsometry(sgf, isometry);
  Safety safety(game);
  EXPECT_EQ(0.0f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("bb", isometry, coord))));
  EXPECT_EQ(2.0f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("db", isometry, coord))));
  EXPECT_EQ(0.0f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("fb", isometry, coord))));
  EXPECT_EQ(0.5f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("fc", isometry, coord))));
  EXPECT_EQ(0.5f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("fe", isometry, coord))));
  EXPECT_EQ(0.0f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("ff", isometry, coord))));
  EXPECT_EQ(1.0f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("df", isometry, coord))));
  EXPECT_EQ(0.0f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("bf", isometry, coord))));
  EXPECT_EQ(1.0f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("bd", isometry, coord))));
}

INSTANTIATE_TEST_CASE_P(
        Par,
        IsometryFixtureS2,
        ::testing::Values(0,1,2,3,4,5,6,7));



}
