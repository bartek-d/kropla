#include "game.h"
#include "sgf.h"
#include "utils.h"
#include <gtest/gtest.h>

#include <iostream>

namespace {


class IsometryFixture :public ::testing::TestWithParam<unsigned> {
};

TEST_P(IsometryFixture, chooseSafetyMove) {
  unsigned isometry = GetParam();
  //  http://eidokropki.reaktywni.pl/#fWwBwH2R:0,0
  Game game = constructGameFromSgfWithIsometry("(;GM[40]FF[4]CA[UTF-8]SZ[9];B[ed];W[ee];B[fd];W[fe];B[hd];W[fh])", isometry);
  const int whoMoves = 1;
  std::set<std::string> expectedMoves{ applyIsometry("id", isometry, coord),
				       applyIsometry("fi", isometry, coord) };
  std::set<std::string> seenMoves{};
  for (int tries = 1000; tries > 0 and seenMoves.size() < expectedMoves.size(); --tries) {
    auto move = game.chooseSafetyMove(whoMoves);
    ASSERT_NE(0, move.ind);
    seenMoves.insert(coord.indToSgf(move.ind));
  }
  EXPECT_EQ(expectedMoves, seenMoves);
}

INSTANTIATE_TEST_CASE_P(
        Par,
        IsometryFixture,
        ::testing::Values(0,1,2,3,4,5,6,7));


class IsometryFixture2 :public ::testing::TestWithParam<unsigned> {
};

TEST_P(IsometryFixture2, chooseSafetyMoveReturnsNoMoveBecauseEverythingIsSafe) {
  unsigned isometry = GetParam();
  //  http://eidokropki.reaktywni.pl/#33vc3yXjZ:0,0
  Game game = constructGameFromSgfWithIsometry("(;GM[40]FF[4]CA[UTF-8]SZ[9];B[bc];W[eb];B[bd];W[fb])", isometry);
  const int whoMoves = 1;
  auto move = game.chooseSafetyMove(whoMoves);
  EXPECT_EQ(0, move.ind);
}


INSTANTIATE_TEST_CASE_P(
        Par,
        IsometryFixture2,
        ::testing::Values(0,1,2,3,4,5,6,7));

}
