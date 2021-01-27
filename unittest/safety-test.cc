#include "game.h"
#include "sgf.h"
#include <gtest/gtest.h>
#include <ostream>

std::string constructSgfFromGameBoard(const std::string& board)
{
  // Format of string: . or space = empty, o=blue, x=red; board is always square
  //  "...o.."
  //  "..x.x."
  auto area = board.size();
  unsigned side = 1;
  while (side * side < area) ++side;
  std::ostringstream oss;
  oss << "(;GM[40]FF[4]CA[UTF-8]SZ[" << side << "]";
  std::vector<unsigned> blue{};
  std::vector<unsigned> red{};
  for (unsigned i = 0; i<board.size(); ++i) {
    switch (board[i]) {
    case 'o':
      blue.push_back(i);
      break;
    case 'x':
      red.push_back(i);
      break;
    }
  }
  while (not (blue.empty() and red.empty())) {
    if (not blue.empty()) {
      unsigned move = blue.back();
      blue.pop_back();
      oss << ";B[" << coord.numberToLetter(move % side) << coord.numberToLetter(move / side) << "]";
    }
    if (not red.empty()) {
      unsigned move = red.back();
      red.pop_back();
      oss << ";W[" << coord.numberToLetter(move % side) << coord.numberToLetter(move / side) << "]";
    }
  }
  oss << ")";
  return oss.str();
}


TEST(SgfConstruct, works)
{
  auto result = constructSgfFromGameBoard("...o..x"
					  "..o.x.x"
					  "......."
					  "......."
					  "......."
					  "......."
					  "..o.o..");
  std::string expectedSgf = "(;GM[40]FF[4]CA[UTF-8]SZ[7];B[eg];W[gb];B[cg];W[eb];B[cb];W[ga];B[da])";
  EXPECT_EQ(expectedSgf, result);
}

TEST(SgfConstruct, worksAlsoForIncompleteBoard)
{
  auto result = constructSgfFromGameBoard("...o.."
					  ".....x"
					  "......"
					  ".xx..."
					  "......");
  std::string expectedSgf = "(;GM[40]FF[4]CA[UTF-8]SZ[6];B[da];W[cd];W[bd];W[fb])";
  EXPECT_EQ(expectedSgf, result);
}
