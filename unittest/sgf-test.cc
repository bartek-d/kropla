#include "sgf.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <iostream>

namespace
{
TEST(SgfTree, constructsLinearTree)
{
    SgfTree tree;
    tree.changeBoardSize(20, 20);
    tree.makeMove({"B", {"bb"}});
    tree.makeMove({"W", {"cc"}});
    tree.makeMove({"B", {"dd"}});
    tree.makeMove({"W", {"ef"}});
    std::string expectedSgf{
        "(;FF[4]GM[40]CA[UTF-8]AP[board.cc:SgfTree]RU[Punish=0,Holes=1,AddTurn="
        "0,MustSurr=0,MinArea=0,Pass=0,Stop=0,LastSafe=0,ScoreTerr=0,"
        "InstantWin=0]SZ[20];B[bb];W[cc];B[dd];W[ef])"};
    EXPECT_EQ(expectedSgf, tree.toString());
}

TEST(SgfTree, constructsTreeWithBranches)
{
    SgfTree tree;
    tree.changeBoardSize(20, 20);
    tree.makeMove({"B", {"bb"}});
    tree.makeMove({"W", {"cc"}});
    tree.saveCursor();
    tree.makeMove({"B", {"dd"}});
    tree.makeMove({"W", {"ef"}});
    tree.restoreCursor();
    tree.makeMove({"B", {"gh"}});
    tree.makeMove({"W", {"ih"}});
    tree.restoreCursor();
    tree.makeMove({"B", {"dd"}});
    tree.makeMove({"W", {"jj"}});

    std::string expectedSgf{
        "(;FF[4]GM[40]CA[UTF-8]AP[board.cc:SgfTree]RU[Punish=0,Holes=1,AddTurn="
        "0,MustSurr=0,MinArea=0,Pass=0,Stop=0,LastSafe=0,ScoreTerr=0,"
        "InstantWin=0]SZ[20];B[bb];W[cc](;B[dd](;W[jj])(;W[ef]))(;B[gh];W[ih])"
        ")"};
    EXPECT_EQ(expectedSgf, tree.toString());
}

}  // namespace
