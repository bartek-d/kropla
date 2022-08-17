#include "utils.h"

#include <gtest/gtest.h>

#include <string>

#include "board.h"
#include "game.h"
#include "sgf.h"

TEST(SgfConstruct, works)
{
    auto result = constructSgfFromGameBoard(
        "...o..x"
        "..o.x.x"
        "......."
        "......."
        "......."
        "......."
        "..o.o..");
    std::string expectedSgf =
        "(;GM[40]FF[4]CA[UTF-8]SZ[7];B[eg];W[gb];B[cg];W[eb];B[cb];W[ga];B[da]"
        ")";
    EXPECT_EQ(expectedSgf, result);
}

TEST(SgfConstruct, worksAlsoForIncompleteBoard)
{
    auto result = constructSgfFromGameBoard(
        "...o.."
        ".....x"
        "......"
        ".xx..."
        "......");
    std::string expectedSgf =
        "(;GM[40]FF[4]CA[UTF-8]SZ[6];B[da];W[cd];W[bd];W[fb])";
    EXPECT_EQ(expectedSgf, result);
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
