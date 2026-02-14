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
        if (p.where == coord.sgfToPti(applyIsometry("da", isometry, coord)))
        {
            const auto border = dfs.findBorder(p);
            EXPECT_EQ(10, border.size());
            for (auto b : border) std::cout << coord.showPt(b) << " -- ";
            std::cout << std::endl;
        }
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

TEST_P(DfsIsometryFixture, weFindNoThreats1mOnFullBoard)
{
    const unsigned isometry = GetParam();
    auto sgf = constructSgfFromGameBoard(
        "ooooooo"
        "o.oxoxo"
        "oox..oo"
        "oo.xxoo"
        "o.ooooo"
        "o.o..oo"
        "ooooooo");
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

    ASSERT_EQ(0, dfs.aps.size());
}

TEST_P(DfsIsometryFixture, weFindMultipleThreats1mNoThreats)
{
    const unsigned isometry = GetParam();
    auto sgf = constructSgfFromGameBoard(
        "oxoxoxo"
        "xoxoxox"
        "oxoxoxo"
        "xoxoxox"
        "oxoxoxo"
        "xoxoxox"
        "o.o.o.o");
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

    ASSERT_EQ(0, dfs.aps.size());
}

TEST_P(DfsIsometryFixture, weFindMultipleThreats1mMany)
{
    const unsigned isometry = GetParam();
    auto sgf = constructSgfFromGameBoard(
        "oooxo.o"
        "o.o.o.o"
        "o.o.o.o"
        "o.oxo.o"
        "o.ooo.o"
        "o.....o"
        "ooooooo");
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

    ASSERT_EQ(15, dfs.aps.size());
}

TEST_P(DfsIsometryFixture, weFindAThreatWithComplicatedBorder)
{
    const unsigned isometry = GetParam();
    auto sgf = constructSgfFromGameBoard(
        "oooxooo"
        "o.oxo.o"
        "o.o...o"
        "o.oxo.o"
        "o.ooo.o"
        "o.....o"
        "ooooooo");
    Game game = constructGameFromSgfWithIsometry(sgf, isometry);
    const int playerO = 1;
    OnePlayerDfs dfs;
    dfs.player = playerO;
    dfs.AP(game.getSimpleGame(), coord.first, coord.last);
    for (auto p : dfs.aps)
    {
        std::cout << "ap: " << coord.showPt(p.where) << std::endl;
        std::cout << "  wnetrze: " << p.seq0 << " -- " << p.seq1 << std::endl;
        if (p.where == coord.sgfToPti(applyIsometry("dc", isometry, coord)))
        {
            const auto border = dfs.findBorder(p);
            const std::size_t expectedSize = (p.seq1 - p.seq0 == 1) ? 4 : 26;
            EXPECT_EQ(expectedSize, border.size());
            for (auto b : border) std::cout << coord.showPt(b) << " -- ";
            std::cout << std::endl;
        }
    }
    std::cout << coord.showColouredBoard(dfs.discovery);
    std::cout << coord.showColouredBoard(dfs.low);

    ASSERT_EQ(15, dfs.aps.size());
}

TEST_P(DfsIsometryFixture, weFindAThreatWithComplicatedBorder2)
{
    const unsigned isometry = GetParam();
    auto sgf = constructSgfFromGameBoard(
        "oooxooo"
        "o.oxo.o"
        "o.....o"
        "o.oxo.o"
        "o.ooo.o"
        "o.....o"
        "ooooooo");
    Game game = constructGameFromSgfWithIsometry(sgf, isometry);
    const int playerO = 1;
    OnePlayerDfs dfs;
    dfs.player = playerO;
    dfs.AP(game.getSimpleGame(), coord.first, coord.last);
    for (auto p : dfs.aps)
    {
        std::cout << "ap: " << coord.showPt(p.where) << std::endl;
        std::cout << "  wnetrze: " << p.seq0 << " -- " << p.seq1 << std::endl;
        dfs.findBorder(p);  // to check cleaning up
        if (p.where == coord.sgfToPti(applyIsometry("dc", isometry, coord)))
        {
            const auto border = dfs.findBorder(p);
            const std::size_t expectedSize = (p.seq1 - p.seq0 == 1) ? 4 : 20;
            EXPECT_EQ(expectedSize, border.size());
            for (auto b : border) std::cout << coord.showPt(b) << " -- ";
            std::cout << std::endl;
        }
    }
    std::cout << coord.showColouredBoard(dfs.discovery);
    std::cout << coord.showColouredBoard(dfs.low);

    ASSERT_EQ(4, dfs.aps.size());
}

TEST_P(DfsIsometryFixture, weFindAThreatWithComplicatedBorder3)
{
    const unsigned isometry = GetParam();
    auto sgf = constructSgfFromGameBoard(
        "oooxooo"
        "o.oxo.o"
        "o.....o"
        "o..xo.o"
        "o..oo.o"
        "o.....o"
        "ooooooo");
    Game game = constructGameFromSgfWithIsometry(sgf, isometry);
    const int playerO = 1;
    OnePlayerDfs dfs;
    dfs.player = playerO;
    dfs.AP(game.getSimpleGame(), coord.first, coord.last);
    for (auto p : dfs.aps)
    {
        std::cout << "ap: " << coord.showPt(p.where) << std::endl;
        std::cout << "  wnetrze: " << p.seq0 << " -- " << p.seq1 << std::endl;
        dfs.findBorder(p);  // to check cleaning up
        if (p.where == coord.sgfToPti(applyIsometry("dc", isometry, coord)))
        {
            const auto border = dfs.findBorder(p);
            const std::size_t expectedSize = 20;
            EXPECT_EQ(expectedSize, border.size());
            for (auto b : border) std::cout << coord.showPt(b) << " -- ";
            std::cout << std::endl;
        }
    }
    std::cout << coord.showColouredBoard(dfs.discovery);
    std::cout << coord.showColouredBoard(dfs.low);

    ASSERT_EQ(3, dfs.aps.size());
}

TEST_P(DfsIsometryFixture, weFindThreatsInsideTerr)
{
    const unsigned isometry = GetParam();
    auto sgf = constructSgfFromGameBoard(
        "ooo.ooo"
        "o.oxo.o"
        "o..o..o"
        "o..xo.o"
        "o..oo.o"
        "o.....o"
        "ooooooo");
    Game game = constructGameFromSgfWithIsometry(sgf, isometry);
    const int playerO = 1;
    OnePlayerDfs dfs;
    dfs.player = playerO;
    dfs.AP(game.getSimpleGame(), coord.first, coord.last);
    dfs.findTerritoriesAndEnclosuresInside(game.getSimpleGame(), coord.first,
                                           coord.last);

    for (auto p : dfs.aps)
    {
        std::cout << "ap: " << coord.showPt(p.where) << std::endl;
        std::cout << "  wnetrze: " << p.seq0 << " -- " << p.seq1 << std::endl;
        dfs.findBorder(p);  // to check cleaning up
                            /*
        if (p.where == coord.sgfToPti(applyIsometry("dc", isometry, coord)))
        {
            const auto border = dfs.findBorder(p);
            const std::size_t expectedSize = 20;
            EXPECT_EQ(expectedSize, border.size());
            for (auto b : border) std::cout << coord.showPt(b) << " -- ";
            std::cout << std::endl;
        }
	*/
    }
    std::cout << coord.showColouredBoard(dfs.discovery);
    std::cout << coord.showColouredBoard(dfs.low);
    for (std::size_t i = 0; i < dfs.seq.size(); ++i)
    {
        std::cout << coord.showPt(dfs.seq[i]) << '\t';
        if (i % 10 == 9) std::cout << std::endl;
    }
    ASSERT_EQ(21, dfs.aps.size());
}

TEST_P(DfsIsometryFixture, weFindThreatsInsideTerrAlsoWhenLefttopPointIsAThreat)
{
    const unsigned isometry = GetParam();
    auto sgf = constructSgfFromGameBoard(
        "ooooooo"
        "o..xx.o"
        "o.ooooo"
        "o.o...."
        ".o....."
        "......."
        ".......");
    Game game = constructGameFromSgfWithIsometry(sgf, isometry);
    const int playerO = 1;
    OnePlayerDfs dfs;
    dfs.player = playerO;
    dfs.AP(game.getSimpleGame(), coord.first, coord.last);
    dfs.findTerritoriesAndEnclosuresInside(game.getSimpleGame(), coord.first,
                                           coord.last);

    for (auto p : dfs.aps)
    {
        std::cout << "ap: " << coord.showPt(p.where) << std::endl;
        std::cout << "  wnetrze: " << p.seq0 << " -- " << p.seq1 << std::endl;
        dfs.findBorder(p);  // to check cleaning up
                            /*
        if (p.where == coord.sgfToPti(applyIsometry("dc", isometry, coord)))
        {
            const auto border = dfs.findBorder(p);
            const std::size_t expectedSize = 20;
            EXPECT_EQ(expectedSize, border.size());
            for (auto b : border) std::cout << coord.showPt(b) << " -- ";
            std::cout << std::endl;
        }
	*/
    }
    std::cout << coord.showColouredBoard(dfs.discovery);
    std::cout << coord.showColouredBoard(dfs.low);
    for (std::size_t i = 0; i < dfs.seq.size(); ++i)
    {
        std::cout << coord.showPt(dfs.seq[i]) << '\t';
        if (i % 10 == 9) std::cout << std::endl;
    }
    ASSERT_EQ(7, dfs.aps.size());
}

TEST_P(DfsIsometryFixture, weFindThreatsInsideTerrAlsoWhenLefttopPointIsOppDot)
{
    const unsigned isometry = GetParam();
    auto sgf = constructSgfFromGameBoard(
        "..o...."
        ".o.o..."
        "oxx.ooo"
        "o.oo..."
        ".o....."
        "......."
        ".......");
    Game game = constructGameFromSgfWithIsometry(sgf, isometry);
    const int playerO = 1;
    OnePlayerDfs dfs;
    dfs.player = playerO;
    dfs.AP(game.getSimpleGame(), coord.first, coord.last);
    dfs.findTerritoriesAndEnclosuresInside(game.getSimpleGame(), coord.first,
                                           coord.last);

    std::cout << coord.showColouredBoard(dfs.discovery);
    std::cout << coord.showColouredBoard(dfs.low);
    for (std::size_t i = 0; i < dfs.seq.size(); ++i)
    {
        std::cout << coord.showPt(dfs.seq[i]) << '\t';
        if (i % 10 == 9) std::cout << std::endl;
    }
    ASSERT_EQ(1, dfs.aps.size());
}

INSTANTIATE_TEST_CASE_P(Par, DfsIsometryFixture,
                        ::testing::Values(0, 1, 2, 3, 4, 5, 6, 7));

}  // namespace
