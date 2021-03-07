#include "game.h"
#include "sgf.h"
#include "safety.h"
#include "utils.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <ostream>
#include <iostream>

namespace {

std::ostream& operator<<(std::ostream& os, const Safety::MoveSuggestions& sugg)
{
  os << "["<< coord.showPt(sugg.move) << ", "<< sugg.who << ", " << sugg.value << "]";
  return os;
}

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
  constexpr pti both_players = 3;
  auto move_at = [&](auto pstr, pti value) {
		   return Safety::MoveSuggestions{
		     coord.sgfToPti(applyIsometry(pstr, isometry, coord)),
		     both_players,
		     value};
		 };
  EXPECT_THAT(suggestions, testing::UnorderedElementsAre(move_at("ad", bad_move),
							 move_at("gc", bad_move),
							 move_at("gf", bad_move),
							 move_at("fg", bad_move),
							 move_at("eg", good_move)));
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
  constexpr pti both_players = 3;
  auto move_at = [&](auto pstr, pti value) {
		   return Safety::MoveSuggestions{
		     coord.sgfToPti(applyIsometry(pstr, isometry, coord)),
		     both_players,
		     value};
		 };
  EXPECT_THAT(suggestions, testing::UnorderedElementsAre(move_at("ab", bad_move),
							 move_at("ba", bad_move),
							 move_at("da", bad_move),
							 move_at("fa", bad_move),
							 move_at("gb", bad_move),
							 move_at("gf", bad_move),
							 move_at("fg", bad_move),
							 move_at("dg", bad_move),
							 move_at("bg", bad_move),
							 move_at("af", bad_move),
							 move_at("ad", bad_move)));
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
  constexpr pti bad_move = -10;
  constexpr pti good_move = 10;
  constexpr pti both_players = 3;
  constexpr pti x_player = 2;
  auto move_at = [&](auto pstr, pti value) {
		   return Safety::MoveSuggestions{
		     coord.sgfToPti(applyIsometry(pstr, isometry, coord)),
		     both_players,
		     value};
		 };
  auto move_at_for = [&](auto pstr, pti value, pti who) {
		   return Safety::MoveSuggestions{
		     coord.sgfToPti(applyIsometry(pstr, isometry, coord)),
		     who,
		     value};
		 };

  EXPECT_THAT(suggestions, testing::UnorderedElementsAre(
							 move_at("cb", good_move),
							 move_at("ad", good_move),
							 move_at("cf", good_move),
							 move_at_for("ac", good_move, x_player),
							 move_at_for("bc", good_move, x_player),
							 move_at_for("ae", good_move, x_player),
							 move_at_for("be", good_move, x_player)));

}

INSTANTIATE_TEST_CASE_P(
        Par,
        IsometryFixtureS3,
        ::testing::Values(0,1,2,3,4,5,6,7));



}
