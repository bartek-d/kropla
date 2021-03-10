#include "game.h"
#include "sgf.h"
#include "safety.h"
#include "utils.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <map>

namespace {

struct MoveDescription {
  int move;
  int who;
  bool operator<(const MoveDescription& other) const { return std::tie(move, who) < std::tie(other.move, other.who); }
  bool operator==(const MoveDescription& other) const { return std::tie(move, who) == std::tie(other.move, other.who); }
};
using MoveSuggestions = std::map<MoveDescription, pti>;

MoveSuggestions convertToMap(const std::vector<Safety::ValueForBoth>& move_value)
{
  MoveSuggestions m;
  for (int i=0; i<move_value.size(); ++i) {
    if (move_value[i][0]) m[MoveDescription{i, 1}] = move_value[i][0];
    if (move_value[i][1]) m[MoveDescription{i, 2}] = move_value[i][1];
  }
  return m;
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
  Safety safety;
  safety.init(&game);
  EXPECT_EQ(0.75f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("cb", isometry, coord))));
  EXPECT_EQ(1.50f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("bd", isometry, coord))));
  EXPECT_EQ(0.75f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("df", isometry, coord))));
  EXPECT_EQ(0.0f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("ef", isometry, coord))));
  EXPECT_EQ(0.0f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("ff", isometry, coord))));
  EXPECT_EQ(1.0f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("fc", isometry, coord))));

  MoveSuggestions suggestions = convertToMap(safety.getMoveValues());
  constexpr pti bad_move = -10;
  constexpr pti good_move = 10;
  auto move_at1 = [&](auto pstr, pti value) -> std::pair<const MoveDescription, pti> {
										     return {MoveDescription{coord.sgfToPti(applyIsometry(pstr, isometry, coord)), 1}, value};
		 };
  auto move_at2 = [&](auto pstr, pti value) -> std::pair<const MoveDescription, pti> {
										     return {MoveDescription{coord.sgfToPti(applyIsometry(pstr, isometry, coord)), 2}, value};
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
  Safety safety;
  safety.init(&game);
  EXPECT_EQ(0.0f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("bb", isometry, coord))));
  EXPECT_EQ(2.0f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("db", isometry, coord))));
  EXPECT_EQ(0.0f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("fb", isometry, coord))));
  EXPECT_EQ(0.5f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("fc", isometry, coord))));
  EXPECT_EQ(0.5f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("fe", isometry, coord))));
  EXPECT_EQ(0.0f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("ff", isometry, coord))));
  EXPECT_EQ(1.0f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("df", isometry, coord))));
  EXPECT_EQ(0.0f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("bf", isometry, coord))));
  EXPECT_EQ(1.0f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("bd", isometry, coord))));
  MoveSuggestions suggestions = convertToMap(safety.getMoveValues());
  constexpr pti bad_move = -10;
  auto move_at1 = [&](auto pstr, pti value) -> std::pair<const MoveDescription, pti> {
										     return {MoveDescription{coord.sgfToPti(applyIsometry(pstr, isometry, coord)), 1}, value};
		 };
  auto move_at2 = [&](auto pstr, pti value) -> std::pair<const MoveDescription, pti> {
										     return {MoveDescription{coord.sgfToPti(applyIsometry(pstr, isometry, coord)), 2}, value};
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
  Safety safety;
  safety.init(&game);
  EXPECT_EQ(0.75f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("bb", isometry, coord))));
  EXPECT_EQ(0.0f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("bd", isometry, coord))));
  EXPECT_EQ(1.0f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("bf", isometry, coord))));
  EXPECT_EQ(0.75f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("df", isometry, coord))));
  MoveSuggestions suggestions = convertToMap(safety.getMoveValues());
  constexpr pti good_move = 10;
  constexpr pti x_player = 2;
  auto move_at_for = [&](auto pstr, pti value, pti who) -> std::pair<const MoveDescription, pti> {
										     return {MoveDescription{coord.sgfToPti(applyIsometry(pstr, isometry, coord)), who}, value};
		 };

  auto move_at1 = [&](auto pstr, pti value) -> std::pair<const MoveDescription, pti> {
										     return {MoveDescription{coord.sgfToPti(applyIsometry(pstr, isometry, coord)), 1}, value};
		 };
  auto move_at2 = [&](auto pstr, pti value) -> std::pair<const MoveDescription, pti> {
										     return {MoveDescription{coord.sgfToPti(applyIsometry(pstr, isometry, coord)), 2}, value};
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


class IsometryFixtureS3b :public ::testing::TestWithParam<unsigned> {
};

TEST_P(IsometryFixtureS3b, safetyIsCorrectlyInitialised3b)
{
  const unsigned isometry = GetParam();
  auto sgf = constructSgfFromGameBoard("..xxx.."
				       "xoooox."
				       ".x...o."
				       ".x....."
				       ".o...o."
				       ".oxxxox"
				       "...oox.");
  Game game = constructGameFromSgfWithIsometry(sgf, isometry);
  Safety safety;
  safety.init(&game);
  EXPECT_EQ(0.0f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("bb", isometry, coord))));
  EXPECT_EQ(0.0f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("cb", isometry, coord))));
  EXPECT_EQ(0.0f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("db", isometry, coord))));
  EXPECT_EQ(0.0f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("eb", isometry, coord))));
  EXPECT_EQ(0.0f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("cf", isometry, coord))));
  EXPECT_EQ(0.0f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("df", isometry, coord))));
  EXPECT_EQ(0.0f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("ef", isometry, coord))));
  EXPECT_EQ(0.5f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("fc", isometry, coord))));
  EXPECT_EQ(0.5f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("fe", isometry, coord))));
  EXPECT_EQ(0.5f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("ff", isometry, coord))));
}

INSTANTIATE_TEST_CASE_P(
        Par,
        IsometryFixtureS3b,
        ::testing::Values(0,1,2,3,4,5,6,7));


class IsometryFixtureS3c :public ::testing::TestWithParam<unsigned> {
};

TEST_P(IsometryFixtureS3c, safetyIsCorrectlyInitialised3c)
{
  const unsigned isometry = GetParam();
  auto sgf = constructSgfFromGameBoard("......."
				       ".....oo"
				       ".....xo"
				       "....xxo"
				       ".oxxxox"
				       ".oxxoox"
				       ".....o.");
  Game game = constructGameFromSgfWithIsometry(sgf, isometry);
  Safety safety;
  safety.init(&game);
  EXPECT_EQ(0.0f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("fg", isometry, coord))));
  game.makeSgfMove(applyIsometry("dg", isometry, coord), 1);
  safety.updateAfterMove(&game, safety.getUpdateValueForAllMargins());
  EXPECT_EQ(0.0f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("fg", isometry, coord))));
}

INSTANTIATE_TEST_CASE_P(
        Par,
        IsometryFixtureS3c,
        ::testing::Values(0,1,2,3,4,5,6,7));


class IsometryFixtureS3d :public ::testing::TestWithParam<unsigned> {
};

TEST_P(IsometryFixtureS3d, safetyIsCorrectlyInitialisedAndDotsAtCornersAreIgnored)
{
  const unsigned isometry = GetParam();
  auto sgf = constructSgfFromGameBoard(".x....o"
				       "xo....."
				       "......."
				       ".x....."
				       "......."
				       "xo.o..."
				       ".x....x");
  Game game = constructGameFromSgfWithIsometry(sgf, isometry);
  Safety safety;
  safety.init(&game);
  EXPECT_EQ(0.75f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("bb", isometry, coord))));
  EXPECT_EQ(0.0f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("bd", isometry, coord))));
  EXPECT_EQ(1.0f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("bf", isometry, coord))));
  EXPECT_EQ(0.75f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("df", isometry, coord))));
  MoveSuggestions suggestions = convertToMap(safety.getMoveValues());
  constexpr pti good_move = 10;
  constexpr pti x_player = 2;
  auto move_at_for = [&](auto pstr, pti value, pti who) -> std::pair<const MoveDescription, pti> {
										     return {MoveDescription{coord.sgfToPti(applyIsometry(pstr, isometry, coord)), who}, value};
		 };

  auto move_at1 = [&](auto pstr, pti value) -> std::pair<const MoveDescription, pti> {
										     return {MoveDescription{coord.sgfToPti(applyIsometry(pstr, isometry, coord)), 1}, value};
		 };
  auto move_at2 = [&](auto pstr, pti value) -> std::pair<const MoveDescription, pti> {
										     return {MoveDescription{coord.sgfToPti(applyIsometry(pstr, isometry, coord)), 2}, value};
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
        IsometryFixtureS3d,
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
  Safety safety;
  safety.init(&game);
  EXPECT_EQ(0.75f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("ef", isometry, coord))));
  game.makeSgfMove(applyIsometry("cf", isometry, coord), 1);
  safety.updateAfterMove(&game, safety.getUpdateValueForAllMargins());
  EXPECT_EQ(0.0f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("ef", isometry, coord))));
  auto moveSugg = safety.getCurrentlyAddedSugg();
  auto move = [&](auto pstr) -> pti { return coord.sgfToPti(applyIsometry(pstr, isometry, coord)); };

  EXPECT_THAT(moveSugg[0], testing::UnorderedElementsAre(move("eg")));
  EXPECT_THAT(moveSugg[1], testing::UnorderedElementsAre(move("eg"), move("dg"), move("df")));
  game.makeSgfMove(applyIsometry("cg", isometry, coord), 2);
  safety.updateAfterMove(&game, safety.getUpdateValueForAllMargins());
  auto moveSuggNow = safety.getCurrentlyAddedSugg();
  auto moveSuggPrev = safety.getPreviouslyAddedSugg();
  EXPECT_THAT(moveSuggNow[0], testing::UnorderedElementsAre(move("bf")));
  EXPECT_THAT(moveSuggNow[1], testing::UnorderedElementsAre(move("bf")));
  EXPECT_THAT(moveSuggPrev[0], testing::UnorderedElementsAre(move("eg")));
  EXPECT_THAT(moveSuggPrev[1], testing::UnorderedElementsAre(move("eg"), move("dg"), move("df")));
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
  Safety safety;
  safety.init(&game);
  game.makeSgfMove(applyIsometry("cf", isometry, coord), 1);
  safety.updateAfterMove(&game, safety.getUpdateValueForAllMargins());
  auto moveSugg = safety.getCurrentlyAddedSugg();
  auto move = [&](auto pstr) -> pti { return coord.sgfToPti(applyIsometry(pstr, isometry, coord)); };

  EXPECT_THAT(moveSugg[0], testing::UnorderedElementsAre(move("eg")));
  EXPECT_THAT(moveSugg[1], testing::UnorderedElementsAre(move("eg"), move("dg"), move("df")));
  game.makeSgfMove(applyIsometry("df", isometry, coord), 2);
  safety.updateAfterMove(&game, safety.getUpdateValueForAllMargins());
  auto moveSuggNow = safety.getCurrentlyAddedSugg();
  auto moveSuggPrev = safety.getPreviouslyAddedSugg();
  EXPECT_TRUE(moveSuggNow[0].empty());  
  EXPECT_TRUE(moveSuggNow[1].empty());  
  EXPECT_TRUE(moveSuggPrev[0].empty());
  EXPECT_TRUE(moveSuggPrev[1].empty());
}

INSTANTIATE_TEST_CASE_P(
        Par,
        IsometryFixtureS5,
        ::testing::Values(0,1,2,3,4,5,6,7));
  

class IsometryFixtureS6 :public ::testing::TestWithParam<unsigned> {
};

TEST_P(IsometryFixtureS6, correctlyAssignedNoDame)
{
  const unsigned isometry = GetParam();
  auto sgf = constructSgfFromGameBoard("....xo."
				       ".xo.ox."
				       ".xooox."
				       ".xxxxx."
				       "......."
				       "....xo."
				       ".......");
  Game game = constructGameFromSgfWithIsometry(sgf, isometry);
  Safety safety;
  safety.init(&game);
  EXPECT_EQ(0.0f, safety.getSafetyOf(coord.sgfToPti(applyIsometry("cb", isometry, coord))));
  MoveSuggestions moveSugg = convertToMap(safety.getMoveValues());
  auto it = moveSugg.find(MoveDescription{coord.sgfToPti(applyIsometry("ca", isometry, coord)), 2});
  EXPECT_TRUE(it != moveSugg.end() and it->second > 0);
}

INSTANTIATE_TEST_CASE_P(
        Par,
        IsometryFixtureS6,
        ::testing::Values(0,1,2,3,4,5,6,7));

}

TEST(Safety, getUpdateValueForMarginsContaining)
{
  Safety safety;
  EXPECT_EQ(0, safety.getUpdateValueForMarginsContaining(coord.ind(0,0)));
  EXPECT_EQ(1, safety.getUpdateValueForMarginsContaining(coord.ind(2,1)));
  EXPECT_EQ(0, safety.getUpdateValueForMarginsContaining(coord.ind(2,2)));
  EXPECT_EQ(9, safety.getUpdateValueForMarginsContaining(coord.ind(1,1)));
  EXPECT_EQ(2, safety.getUpdateValueForMarginsContaining(coord.ind(coord.wlkx-2,3)));
}
