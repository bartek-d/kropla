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

TEST(SetOfPointsWithIsometry, works)
{
    coord.changeSize(30, 30);
    EXPECT_EQ((std::set<pti>{coord.ind(0, 1), coord.ind(2, 2)}),
              getSetOfPoints("ab cc", 0, coord));
    EXPECT_EQ((std::set<pti>{coord.ind(29, 1), coord.ind(27, 2)}),
              getSetOfPoints("ab,cc", 1, coord));
    EXPECT_EQ((std::set<pti>{coord.ind(0, 28), coord.ind(2, 27)}),
              getSetOfPoints("ab cc", 2, coord));
    EXPECT_EQ((std::set<pti>{coord.ind(29, 28), coord.ind(27, 27)}),
              getSetOfPoints("ab,cc", 3, coord));
    EXPECT_EQ((std::set<pti>{coord.ind(1, 0), coord.ind(2, 2)}),
              getSetOfPoints("ab cc", 4, coord));
    EXPECT_EQ((std::set<pti>{coord.ind(1, 29), coord.ind(2, 27)}),
              getSetOfPoints("ab cc", 5, coord));
    EXPECT_EQ((std::set<pti>{coord.ind(28, 0), coord.ind(27, 2)}),
              getSetOfPoints("ab cc", 6, coord));
    EXPECT_EQ((std::set<pti>{coord.ind(28, 29), coord.ind(27, 27)}),
              getSetOfPoints("ab cc", 7, coord));
}