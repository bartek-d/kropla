#include "game.h"
#include "sgf.h"
#include "safety.h"
#include "utils.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>

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
  auto suggestions = safety.getMovesInfo(game);
  constexpr pti bad_move = -10;
  constexpr pti good_move = 10;
  auto move_at1 = [&](auto pstr, pti value) -> std::pair<const Safety::MoveDescription, pti> {
										     return {Safety::MoveDescription{coord.sgfToPti(applyIsometry(pstr, isometry, coord)), 1}, value};
		 };
  auto move_at2 = [&](auto pstr, pti value) -> std::pair<const Safety::MoveDescription, pti> {
										     return {Safety::MoveDescription{coord.sgfToPti(applyIsometry(pstr, isometry, coord)), 2}, value};
		 };
  EXPECT_THAT(suggestions, testing::UnorderedElementsAre(move_at1("ad", bad_move), move_at2("ad", bad_move),
							 move_at1("gc", bad_move), move_at2("gc", bad_move),
							 move_at1("gf", bad_move), move_at2("gf", bad_move),
							 move_at1("fg", bad_move), move_at2("fg", bad_move),
							 move_at1("eg", good_move),move_at2("eg", good_move)));
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
  auto suggestions = safety.getMovesInfo(game);
  constexpr pti bad_move = -10;
  auto move_at1 = [&](auto pstr, pti value) -> std::pair<const Safety::MoveDescription, pti> {
										     return {Safety::MoveDescription{coord.sgfToPti(applyIsometry(pstr, isometry, coord)), 1}, value};
		 };
  auto move_at2 = [&](auto pstr, pti value) -> std::pair<const Safety::MoveDescription, pti> {
										     return {Safety::MoveDescription{coord.sgfToPti(applyIsometry(pstr, isometry, coord)), 2}, value};
		 };
  EXPECT_THAT(suggestions, testing::UnorderedElementsAre(move_at1("ab", bad_move), move_at2("ab", bad_move),
							 move_at1("ba", bad_move), move_at2("ba", bad_move),
							 move_at1("da", bad_move), move_at2("da", bad_move),
							 move_at1("fa", bad_move), move_at2("fa", bad_move),
							 move_at1("gb", bad_move), move_at2("gb", bad_move),
							 move_at1("gf", bad_move), move_at2("gf", bad_move),
							 move_at1("fg", bad_move), move_at2("fg", bad_move),
							 move_at1("dg", bad_move), move_at2("dg", bad_move),
							 move_at1("bg", bad_move), move_at2("bg", bad_move),
							 move_at1("af", bad_move), move_at2("af", bad_move),
							 move_at1("ad", bad_move), move_at2("ad", bad_move)));
}

INSTANTIATE_TEST_CASE_P(
        Par,
        IsometryFixtureS2,
        ::testing::Values(0,1,2,3,4,5,6,7));


class IsometryFixtureS3 :public ::testing::TestWithParam<unsigned> {
};

TEST_P(IsometryFixtureS3, safetyIsCorrectlyInitialised3)
{
  const unsigned isometry = GetParam();
  auto sgf = constructSgfFromGameBoard(".x....."
				       "xo....."
				       "......."
				       ".x....."
				       "......."
				       "xo.o..."
				       ".x.....");
  Game game = constructGameFromSgfWithIsometry(sgf, isometry);
  Safety safety(game);
  EXPECT_EQ(0.75f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("bb", isometry, coord))));
  EXPECT_EQ(0.0f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("bd", isometry, coord))));
  EXPECT_EQ(1.0f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("bf", isometry, coord))));
  EXPECT_EQ(0.75f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("df", isometry, coord))));
  auto suggestions = safety.getMovesInfo(game);
  constexpr pti good_move = 10;
  constexpr pti x_player = 2;
  auto move_at_for = [&](auto pstr, pti value, pti who) -> std::pair<const Safety::MoveDescription, pti> {
										     return {Safety::MoveDescription{coord.sgfToPti(applyIsometry(pstr, isometry, coord)), who}, value};
		 };

  auto move_at1 = [&](auto pstr, pti value) -> std::pair<const Safety::MoveDescription, pti> {
										     return {Safety::MoveDescription{coord.sgfToPti(applyIsometry(pstr, isometry, coord)), 1}, value};
		 };
  auto move_at2 = [&](auto pstr, pti value) -> std::pair<const Safety::MoveDescription, pti> {
										     return {Safety::MoveDescription{coord.sgfToPti(applyIsometry(pstr, isometry, coord)), 2}, value};
		 };

  EXPECT_THAT(suggestions, testing::UnorderedElementsAre(
							 move_at1("cb", good_move), move_at2("cb", good_move),
							 move_at1("ad", good_move), move_at2("ad", good_move),
							 move_at1("cf", good_move), move_at2("cf", good_move),
							 move_at_for("ac", good_move, x_player),
							 move_at_for("bc", good_move, x_player),
							 move_at_for("ae", good_move, x_player),
							 move_at_for("be", good_move, x_player)));

}

INSTANTIATE_TEST_CASE_P(
        Par,
        IsometryFixtureS3,
        ::testing::Values(0,1,2,3,4,5,6,7));


class IsometryFixtureS4 :public ::testing::TestWithParam<unsigned> {
};
  
TEST_P(IsometryFixtureS4, moveSuggestionsAreCorrect)
{
  const unsigned isometry = GetParam();
  auto sgf = constructSgfFromGameBoard(".....o."
				       "..o...."
				       ".....x."
				       ".x....."
				       "......."
				       "....xo."
				       ".......");
  Game game = constructGameFromSgfWithIsometry(sgf, isometry);
  Safety safety(game);
  EXPECT_EQ(0.75f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("ef", isometry, coord))));
  game.makeSgfMove(applyIsometry("cf", isometry, coord), 1);
  safety.updateAfterMove(game);
  EXPECT_EQ(0.0f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("ef", isometry, coord))));
  auto moveSugg = safety.getCurrentlyAddedSugg();
  auto move_at1 = [&](auto pstr, pti value) -> std::pair<const Safety::MoveDescription, pti> {
										     return {Safety::MoveDescription{coord.sgfToPti(applyIsometry(pstr, isometry, coord)), 1}, value};
		 };
  auto move_at2 = [&](auto pstr, pti value) -> std::pair<const Safety::MoveDescription, pti> {
										     return {Safety::MoveDescription{coord.sgfToPti(applyIsometry(pstr, isometry, coord)), 2}, value};
		 };

  constexpr pti good_move = 10;
  EXPECT_THAT(moveSugg, testing::UnorderedElementsAre(
						      move_at1("eg", good_move), move_at2("eg", good_move),
						      move_at2("dg", good_move), move_at2("df", good_move)));
  game.makeSgfMove(applyIsometry("cg", isometry, coord), 2);
  safety.updateAfterMove(game);
  auto moveSuggNow = safety.getCurrentlyAddedSugg();
  auto moveSuggPrev = safety.getPreviouslyAddedSugg();
  EXPECT_THAT(moveSuggNow, testing::UnorderedElementsAre(
							 move_at1("bf", good_move), move_at2("bf", good_move)));
  
  EXPECT_THAT(moveSuggPrev, testing::UnorderedElementsAre(
							  move_at1("eg", good_move), move_at2("eg", good_move),
							  move_at2("dg", good_move), move_at2("df", good_move)));

}

INSTANTIATE_TEST_CASE_P(
        Par,
        IsometryFixtureS4,
        ::testing::Values(0,1,2,3,4,5,6,7));


class IsometryFixtureS5 :public ::testing::TestWithParam<unsigned> {
};
  
TEST_P(IsometryFixtureS5, moveSuggestionsAreCorrectlyRemovedWhenNoLongerMakeSense)
{
  const unsigned isometry = GetParam();
  auto sgf = constructSgfFromGameBoard(".....o."
				       "..o...."
				       ".....x."
				       ".x....."
				       "......."
				       "....xo."
				       ".......");
  Game game = constructGameFromSgfWithIsometry(sgf, isometry);
  Safety safety(game);
  game.makeSgfMove(applyIsometry("cf", isometry, coord), 1);
  safety.updateAfterMove(game);
  auto moveSugg = safety.getCurrentlyAddedSugg();
  auto move_at1 = [&](auto pstr, pti value) -> std::pair<const Safety::MoveDescription, pti> {
										     return {Safety::MoveDescription{coord.sgfToPti(applyIsometry(pstr, isometry, coord)), 1}, value};
		 };
  auto move_at2 = [&](auto pstr, pti value) -> std::pair<const Safety::MoveDescription, pti> {
										     return {Safety::MoveDescription{coord.sgfToPti(applyIsometry(pstr, isometry, coord)), 2}, value};
		 };

  constexpr pti good_move = 10;
  EXPECT_THAT(moveSugg, testing::UnorderedElementsAre(
						      move_at1("eg", good_move), move_at2("eg", good_move),
						      move_at2("dg", good_move), move_at2("df", good_move)));
  game.makeSgfMove(applyIsometry("df", isometry, coord), 2);
  safety.updateAfterMove(game);
  auto moveSuggNow = safety.getCurrentlyAddedSugg();
  auto moveSuggPrev = safety.getPreviouslyAddedSugg();
  EXPECT_TRUE(moveSuggNow.empty());  
  EXPECT_TRUE(moveSuggPrev.empty());

}

INSTANTIATE_TEST_CASE_P(
        Par,
        IsometryFixtureS5,
        ::testing::Values(0,1,2,3,4,5,6,7));
  

}
