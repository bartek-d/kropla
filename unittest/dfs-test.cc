#include "dfs.h"

#include <gtest/gtest.h>

#include <iostream>
#include <set>

#include "sgf.h"
#include "utils.h"

namespace
{
class DfsIsometryFixture : public ::testing::TestWithParam<unsigned>
{
};

TEST_P(DfsIsometryFixture, weFindThreats1m)
{
    const unsigned isometry = GetParam();
    auto sgf = constructSgfFromGameBoard(
        "......."
        "..o.o.."
        ".oxx.o."
        ".o.xxo."
        "..o.oo."
        "...o..."
        ".......");
    Game game = constructGameFromSgfWithIsometry(sgf, isometry);
    const int playerO = 1;
    OnePlayerDfs dfs;
    dfs.player = playerO;
    dfs.AP(game.getSimpleGame(), coord.first, coord.last);
    for (auto p : dfs.aps)
    {
        std::cout << "ap: " << coord.showPt(p.where) << std::endl;
        std::cout << "  wnetrze: " << p.seq0 << " -- " << p.seq1 << std::endl;
    }
    std::cout << coord.showColouredBoard(dfs.discovery);
    std::cout << coord.showColouredBoard(dfs.low);

    ASSERT_EQ(2, dfs.aps.size());
}

TEST_P(DfsIsometryFixture, weFindMultipleThreats1m)
{
    const unsigned isometry = GetParam();
    auto sgf = constructSgfFromGameBoard(
        "......."
        "..oxo.."
        ".ox..o."
        ".o.xxo."
        "..oooo."
        "..o..o."
        "....o..");
    Game game = constructGameFromSgfWithIsometry(sgf, isometry);
    const int playerO = 1;
    OnePlayerDfs dfs;
    dfs.player = playerO;
    dfs.AP(game.getSimpleGame(), coord.first, coord.last);
    for (auto p : dfs.aps)
    {
        std::cout << "ap: " << coord.showPt(p.where) << std::endl;
        std::cout << "  wnetrze: " << p.seq0 << " -- " << p.seq1 << std::endl;
    }
    std::cout << coord.showColouredBoard(dfs.discovery);
    std::cout << coord.showColouredBoard(dfs.low);

    ASSERT_EQ(4, dfs.aps.size());
}

TEST_P(DfsIsometryFixture, weFindMultipleThreats1mWithLotsOfDots)
{
    const unsigned isometry = GetParam();
    auto sgf = constructSgfFromGameBoard(
        "oo..x.x"
        "..oxoxo"
        ".ox..ox"
        ".o.xxoo"
        "o.oooox"
        "o.o..ox"
        "o...o.x");
    Game game = constructGameFromSgfWithIsometry(sgf, isometry);
    const int playerO = 1;
    OnePlayerDfs dfs;
    dfs.player = playerO;
    dfs.AP(game.getSimpleGame(), coord.first, coord.last);
    for (auto p : dfs.aps)
    {
        std::cout << "ap: " << coord.showPt(p.where) << std::endl;
        std::cout << "  wnetrze: " << p.seq0 << " -- " << p.seq1 << std::endl;
    }
    std::cout << coord.showColouredBoard(dfs.discovery);
    std::cout << coord.showColouredBoard(dfs.low);

    ASSERT_EQ(8, dfs.aps.size());
}

INSTANTIATE_TEST_CASE_P(Par, DfsIsometryFixture,
                        ::testing::Values(0, 1, 2, 3, 4, 5, 6, 7));

}  // namespace
