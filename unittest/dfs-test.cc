#include "dfs.h"

#include <gtest/gtest.h>

#include <array>
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
    ASSERT_EQ(2, dfs.aps.size());
    auto expectedWheres = getSetOfPoints("da db", isometry, coord);
    EXPECT_TRUE(expectedWheres.contains(dfs.aps[0].where));
    EXPECT_TRUE(expectedWheres.contains(dfs.aps[1].where));
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
    auto expectedWheres = getSetOfPoints("da dc df dg", isometry, coord);
    EXPECT_TRUE(expectedWheres.contains(dfs.aps[0].where));
    EXPECT_TRUE(expectedWheres.contains(dfs.aps[1].where));
    EXPECT_TRUE(expectedWheres.contains(dfs.aps[2].where));
    EXPECT_TRUE(expectedWheres.contains(dfs.aps[3].where));
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

    ASSERT_EQ(8, dfs.aps.size());
    auto expectedWheres =
        getSetOfPoints("da fa dc df dg bf bg ab", isometry, coord);
    for (const auto &aps : dfs.aps)
        EXPECT_TRUE(expectedWheres.contains(aps.where));
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
    auto expectedWheres = getSetOfPoints(
        "bc bd be bf cf df ef ff fe fd fc ec dc", isometry, coord);
    for (const auto &aps : dfs.aps)
        EXPECT_TRUE(expectedWheres.contains(aps.where));
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
        dfs.findBorder(p);  // to check cleaning up
        if (p.where == coord.sgfToPti(applyIsometry("dc", isometry, coord)))
        {
            const auto border = dfs.findBorder(p);
            const std::size_t expectedSize = (p.seq1 - p.seq0 == 1) ? 4 : 20;
            EXPECT_EQ(expectedSize, border.size());
        }
    }

    const std::set<std::set<pti>> expectedInteriors{
        getSetOfPoints(
            "bb bc bd be bf cc cd ce cf dd de df ec ed ee ef fb fc fd fe ff",
            isometry, coord),
        getSetOfPoints("bb", isometry, coord),
        getSetOfPoints("fb", isometry, coord),
        getSetOfPoints("dd", isometry, coord),
    };
    const auto enclosures = dfs.findAllEnclosures();
    for (const auto &encl : enclosures)
        EXPECT_TRUE(expectedInteriors.contains(
            std::set<pti>(encl.interior.begin(), encl.interior.end())));

    ASSERT_EQ(4, dfs.aps.size());
}

TEST_P(DfsIsometryFixture, weFindAThreatWithComplicatedBorder2OnALargerBoard)
{
    const unsigned isometry = GetParam();
    auto sgf = constructSgfFromGameBoard(
        "..........."
        "..........."
        "..oooxooo.."
        "..o.oxo.o.."
        "..o.....o.."
        "..o.oxo.o.."
        "..o.ooo.o.."
        "..o.....o.."
        "..ooooooo.."
        "..........."
        "...........");
    Game game = constructGameFromSgfWithIsometry(sgf, isometry);
    const int playerO = 1;
    OnePlayerDfs dfs;
    dfs.player = playerO;
    dfs.AP(game.getSimpleGame(), coord.first, coord.last);
    for (auto p : dfs.aps)
    {
        dfs.findBorder(p);  // to check cleaning up
        if (p.where == coord.sgfToPti(applyIsometry("fe", isometry, coord)))
        {
            const auto border = dfs.findBorder(p);
            const std::size_t expectedSize = (p.seq1 - p.seq0 == 1) ? 4 : 20;
            EXPECT_EQ(expectedSize, border.size());
        }
    }

    const std::set<std::set<pti>> expectedInteriors{
        getSetOfPoints("dd de df dg dh ee ef eg eh ff fg fh ge gf gg gh hd he "
                       "hf hg hh fe fd fc",
                       isometry, coord),
        getSetOfPoints(
            "dd de df dg dh ee ef eg eh ff fg fh ge gf gg gh hd he hf hg hh",
            isometry, coord),
        getSetOfPoints("dd", isometry, coord),
        getSetOfPoints("hd", isometry, coord),
        getSetOfPoints("ff", isometry, coord),
    };
    const auto enclosures = dfs.findAllEnclosures();
    for (const auto &encl : enclosures)
        EXPECT_TRUE(expectedInteriors.contains(
            std::set<pti>(encl.interior.begin(), encl.interior.end())));

    ASSERT_EQ(5, dfs.aps.size());
}

TEST_P(DfsIsometryFixture,
       weFindAThreatWithComplicatedBorder2OnALargerBoardWithMargin)
{
    const unsigned isometry = GetParam();
    auto sgf = constructSgfFromGameBoard(
        "..........."
        "..........."
        "..oooxooo.."
        "..o.oxo.o.."
        "..o.....o.."
        "..o.oxo.o.."
        "..o.ooo.o.."
        "..o.....o.."
        "..ooooooo.."
        "..........."
        "...........");
    Game game = constructGameFromSgfWithIsometry(sgf, isometry);
    const int playerO = 1;
    OnePlayerDfs dfs;
    dfs.player = playerO;
    dfs.AP(game.getSimpleGame(), coord.ind(1, 1),
           coord.ind(coord.wlkx - 2, coord.wlky - 2));
    for (auto p : dfs.aps)
    {
        dfs.findBorder(p);  // to check cleaning up
        if (p.where == coord.sgfToPti(applyIsometry("fe", isometry, coord)))
        {
            const auto border = dfs.findBorder(p);
            const std::size_t expectedSize = (p.seq1 - p.seq0 == 1) ? 4 : 20;
            EXPECT_EQ(expectedSize, border.size());
        }
    }

    const std::set<std::set<pti>> expectedInteriors{
        getSetOfPoints("dd de df dg dh ee ef eg eh ff fg fh ge gf gg gh hd he "
                       "hf hg hh fe fd fc",
                       isometry, coord),
        getSetOfPoints(
            "dd de df dg dh ee ef eg eh ff fg fh ge gf gg gh hd he hf hg hh",
            isometry, coord),
        getSetOfPoints("dd", isometry, coord),
        getSetOfPoints("hd", isometry, coord),
        getSetOfPoints("ff", isometry, coord),
    };
    const auto enclosures = dfs.findAllEnclosures();
    for (const auto &encl : enclosures)
        EXPECT_TRUE(expectedInteriors.contains(
            std::set<pti>(encl.interior.begin(), encl.interior.end())));

    ASSERT_EQ(5, dfs.aps.size());
}

TEST_P(DfsIsometryFixture,
       weFindAThreatWithComplicatedBorder2OnALargerBoardWithMargin2)
{
    const unsigned isometry = GetParam();
    auto sgf = constructSgfFromGameBoard(
        "..........."
        "..........."
        "..oooxooo.."
        "..o.oxo.o.."
        "..o.....o.."
        "..o.oxo.o.."
        "..o.ooo.o.."
        "..o.....o.."
        "..ooooooo.."
        "..........."
        "...........");
    Game game = constructGameFromSgfWithIsometry(sgf, isometry);
    const int playerO = 1;
    OnePlayerDfs dfs;
    dfs.player = playerO;
    dfs.AP(game.getSimpleGame(), coord.ind(2, 2),
           coord.ind(coord.wlkx - 3, coord.wlky - 3));
    for (auto p : dfs.aps)
    {
        dfs.findBorder(p);  // to check cleaning up
        if (p.where == coord.sgfToPti(applyIsometry("fe", isometry, coord)))
        {
            const auto border = dfs.findBorder(p);
            const std::size_t expectedSize = (p.seq1 - p.seq0 == 1) ? 4 : 20;
            EXPECT_EQ(expectedSize, border.size());
        }
    }

    const std::set<std::set<pti>> expectedInteriors{
        getSetOfPoints(
            "dd de df dg dh ee ef eg eh ff fg fh ge gf gg gh hd he hf hg hh",
            isometry, coord),
        getSetOfPoints("dd", isometry, coord),
        getSetOfPoints("hd", isometry, coord),
        getSetOfPoints("ff", isometry, coord),
    };
    const auto enclosures = dfs.findAllEnclosures();
    for (const auto &encl : enclosures)
        EXPECT_TRUE(expectedInteriors.contains(
            std::set<pti>(encl.interior.begin(), encl.interior.end())));

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

    ASSERT_EQ(1, dfs.aps.size());
}

TEST_P(DfsIsometryFixture, importantRectangle)
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
    pti corner1 = applyIsometry(coord.ind(1, 1), isometry, coord);
    pti corner2 = applyIsometry(coord.ind(5, 4), isometry, coord);
    pti corner3 = applyIsometry(coord.ind(1, 4), isometry, coord);
    pti corner4 = applyIsometry(coord.ind(5, 1), isometry, coord);

    std::array<pti, 3> expectedLeftTop{
        0, coord.first,
        std::min(std::min(corner1, corner2), std::min(corner3, corner4))};
    std::array<pti, 3> expectedBottomRight{
        0, coord.last,
        std::max(std::max(corner1, corner2), std::max(corner3, corner4))};
    for (int player = 1; player <= 2; ++player)
    {
        ImportantRectangle ir_initialised;
        ImportantRectangle ir_updated;
        ir_initialised.initialise(game.getSimpleGame(), player);
        const auto &history = game.getSimpleGame().getHistory();
        for (std::size_t i = 0; i < history.size(); ++i)
        {
            auto move = history.get(i);
            if (game.whoseDotMarginAt(move) == player)
                ir_updated.update(game.getSimpleGame(), move, player);
        }
        EXPECT_EQ(ir_initialised.getLeftTop(), ir_updated.getLeftTop())
            << " for player " << player;
        EXPECT_EQ(ir_initialised.getBottomRight(), ir_updated.getBottomRight())
            << " for player " << player;
        EXPECT_EQ(expectedLeftTop.at(player), ir_updated.getLeftTop())
            << " for player " << player;
        EXPECT_EQ(expectedBottomRight.at(player), ir_updated.getBottomRight())
            << " for player " << player;
    }
}

INSTANTIATE_TEST_CASE_P(Par, DfsIsometryFixture,
                        ::testing::Values(0, 1, 2, 3, 4, 5, 6, 7));

}  // namespace
