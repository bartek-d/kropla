#include "game.h"
#include "sgf.h"
#include <gtest/gtest.h>

#include <iostream>

namespace {

std::string applyIsometry(const std::string sgfCoord, unsigned isometry, Coord &coord)
{
  std::string result{};
  auto reflect = [&coord](char c) { return coord.numberToLetter(coord.wlkx - 1 - coord.sgfCoordToInt(c)).front(); };
  bool oddChar = true;
  // isometry & 1: reflect w/r to Y
  // isometry & 2: reflect w/r to X
  // isometry & 4: reflect w/r to x==y (swap x--y)
  for (auto c : sgfCoord) {
    if (c == '.') {
      result.push_back(c);
      continue;
    }
    if (oddChar) {
      result.push_back((isometry & 1) ? reflect(c) : c);
    } else {
      char newChar = (isometry & 2) ? reflect(c) : c;
      char lastChar = result.back();
      if (isometry & 4) {
	// switch last two
	std::swap(newChar, lastChar);
      }
      result.back() = lastChar;
      result.push_back(newChar);
    }
    oddChar = not oddChar;
  }
  return result;
}

TEST(Isometry, applyIsometryWorks)
{
  coord.changeSize(30, 30);
  EXPECT_EQ("ab.ff", applyIsometry("ab.ff", 0, coord));
  EXPECT_EQ("Db.yf", applyIsometry("ab.ff", 1, coord));
  EXPECT_EQ("aC.fy", applyIsometry("ab.ff", 2, coord));
  EXPECT_EQ("DC.yy", applyIsometry("ab.ff", 3, coord));
  EXPECT_EQ("ba.ff", applyIsometry("ab.ff", 4, coord));
  EXPECT_EQ("bD.fy", applyIsometry("ab.ff", 5, coord));
  EXPECT_EQ("Ca.yf", applyIsometry("ab.ff", 6, coord));
  EXPECT_EQ("CD.yy", applyIsometry("ab.ff", 7, coord));
}
  
Game constructGameFromSgfWithIsometry(const std::string& sgf, unsigned isometry)
{
  SgfParser parser(sgf);
  auto seq = parser.parseMainVar();
  Coord coord(10, 10);
  auto sz_pos = seq[0].findProp("SZ");
  std::string sz = (sz_pos != seq[0].props.end()) ? sz_pos->second[0] : "";
  if (sz.find(':') == std::string::npos) {
    if (sz != "") {
      int x = stoi(sz);
      coord.changeSize(x, x);
    }
  } else {
    std::string::size_type i = sz.find(':');
    int x = stoi(sz.substr(0, i));
    int y = stoi(sz.substr(i+1));
    coord.changeSize(x, y);
  }
  for (SgfNode &node : seq) {
    for (SgfProperty &prop : node.props) {
      if (prop.first == "B" or prop.first == "W" or prop.first == "AB" or prop.first == "AW") {
	for (auto &value : prop.second)
	  value = applyIsometry(value, isometry, coord);
      }
    }
  }
  Game game(seq, 1000);
  return game;
}
  
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

}
