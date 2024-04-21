#include "history.h"

#include <gtest/gtest.h>

#include <string>

#include "board.h"
#include "game.h"
#include "sgf.h"
#include "utils.h"

TEST(History, givenMovesWithoutEncl_savesThemToLastReply)
{
    clearLastGoodReplies();
    {
        History history;
        history.push_back(1, false, false, false, 0);
        history.push_back(101, false, false, false, 0);
        EXPECT_EQ(0, history.getLastGoodReplyFor(1));
        EXPECT_EQ(0, history.getLastGoodReplyFor(2));
        history.push_back(2, false, false, false, 0);
        EXPECT_EQ(0, history.getLastGoodReplyFor(1));
        EXPECT_EQ(0, history.getLastGoodReplyFor(2));
        history.push_back(102, false, false, false, 0);
        history.push_back(3, false, false, false, 0);
        history.push_back(103, false, false, false, 0);
        const float black_wins = 1.0f;
        history.updateGoodReplies(2, black_wins);
    }
    {
        History history;
        history.push_back(1, false, false, false, 0);
        history.push_back(101, false, false, false, 0);
        EXPECT_EQ(2, history.getLastGoodReplyFor(1));
        EXPECT_EQ(0, history.getLastGoodReplyFor(2));
        history.push_back(2, false, false, false, 0);
        EXPECT_EQ(0, history.getLastGoodReplyFor(1));
        EXPECT_EQ(0, history.getLastGoodReplyFor(2));
    }
}

TEST(History, savesCorrectAtariCodes)
{
    unsigned isometry = 0;
    Game game = constructGameFromSgfWithIsometry(
        "(;GM[40]FF[4]CA[UTF-8]SZ[13];B[fg];W[eg];B[eh];W[fh];B[gh];W[fi];B[hh]"
        ";W[gg];B[gf];W[hg];B[ig];W[hf];B[he];W[if];B[jf];W[ie];B[je];W[id])",
        isometry);

    const auto h = game.getHistory();
    for (int move = 2; move < 20; ++move)
    {
        EXPECT_FALSE(h.isInTerrWithAtari(move));
        EXPECT_FALSE(h.isInEnclBorder(move));
    }

    EXPECT_EQ(coord.sgfToPti("fg"), h.get(2));
    EXPECT_EQ(coord.sgfToPti("eg"), h.get(3));
    EXPECT_EQ(coord.sgfToPti("eh"), h.get(4));
    EXPECT_EQ(coord.sgfToPti("fh"), h.get(5));
    EXPECT_EQ(coord.sgfToPti("gh"), h.get(6));
    for (int move = 2; move <= 6; ++move)
    {
        EXPECT_FALSE(h.isInOppEnclBorder(move));
        EXPECT_EQ(0, h.getAtariCode(move));
    }

    const uint32_t N = 1;
    const uint32_t S = 4;
    const uint32_t W = 8;

    EXPECT_EQ(coord.sgfToPti("fi"), h.get(7));
    EXPECT_TRUE(h.isInOppEnclBorder(7));
    EXPECT_EQ(N, h.getAtariCode(7));

    EXPECT_EQ(coord.sgfToPti("hh"), h.get(8));
    EXPECT_EQ(coord.sgfToPti("gg"), h.get(9));
    EXPECT_EQ(coord.sgfToPti("gf"), h.get(10));
    for (int move = 8; move <= 10; ++move)
    {
        EXPECT_FALSE(h.isInOppEnclBorder(move));
        EXPECT_EQ(0, h.getAtariCode(move));
    }

    EXPECT_EQ(coord.sgfToPti("hg"), h.get(11));
    EXPECT_TRUE(h.isInOppEnclBorder(11));
    EXPECT_EQ(W, h.getAtariCode(11));

    EXPECT_EQ(coord.sgfToPti("ig"), h.get(12));
    EXPECT_EQ(coord.sgfToPti("he"), h.get(14));
    EXPECT_EQ(coord.sgfToPti("jf"), h.get(16));
    EXPECT_EQ(coord.sgfToPti("je"), h.get(18));
    for (int move = 12; move < 20; move += 2)
    {
        EXPECT_FALSE(h.isInOppEnclBorder(move));
        EXPECT_EQ(0, h.getAtariCode(move));
    }

    EXPECT_EQ(coord.sgfToPti("hf"), h.get(13));
    EXPECT_EQ(S, h.getAtariCode(13));
    EXPECT_EQ(coord.sgfToPti("if"), h.get(15));
    EXPECT_EQ(W, h.getAtariCode(15));
    EXPECT_EQ(coord.sgfToPti("ie"), h.get(17));
    EXPECT_EQ(S, h.getAtariCode(17));
    EXPECT_EQ(coord.sgfToPti("id"), h.get(19));
    EXPECT_EQ(S, h.getAtariCode(19));
    for (int move = 13; move < 20; move += 2)
    {
        EXPECT_TRUE(h.isInOppEnclBorder(move));
    }
}

TEST(History, savesCorrectAtariCodes_caseWithEnclosures)
{
    unsigned isometry = 0;
    Game game = constructGameFromSgfWithIsometry(
        "(;GM[40]FF[4]CA[UTF-8]SZ[13];B[fg];W[eg];B[eh];W[fh];B[dh];W[ef];B[ff]"
        ";W[dg];B[cg];W[gh];B[ee];W[df];B[cf];W[hg];B[de.cfcgdhehfgffeedecf];W["
        "hj];B[hi];W[gi];B[hh];W[ii];B[ih];W[fj];B[gg];W[jh];B[ig];W[if];B[hf."
        "gghhighfgg])",
        isometry);

    const auto h = game.getHistory();
    for (int move = 2; move <= 28; ++move)
    {
        EXPECT_FALSE(h.isInTerrWithAtari(move));
        EXPECT_EQ((move == 16 or move == 28), h.isInEnclBorder(move));
        EXPECT_EQ((move == 16 or move == 28), h.isEnclosure(move));
    }

    const uint32_t E = 2;
    const uint32_t S = 4;
    const uint32_t W = 8;

    EXPECT_EQ(E + S, h.getAtariCode(13));
    EXPECT_TRUE(h.isInOppEnclBorder(13));

    EXPECT_EQ(W, h.getAtariCode(22));
    EXPECT_TRUE(h.isInOppEnclBorder(22));

    EXPECT_EQ(S, h.getAtariCode(26));
    EXPECT_TRUE(h.isInOppEnclBorder(26));
}
