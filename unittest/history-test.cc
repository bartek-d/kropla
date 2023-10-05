#include "history.h"

#include <gtest/gtest.h>

#include <string>

#include "board.h"
#include "game.h"

TEST(History, givenMovesWithoutEncl_savesThemToLastReply)
{
    clearLastGoodReplies();
    {
        History history;
        history.push_back(1, false, false);
        history.push_back(101, false, false);
        EXPECT_EQ(0, history.getLastGoodReplyFor(1));
        EXPECT_EQ(0, history.getLastGoodReplyFor(2));
        history.push_back(2, false, false);
        EXPECT_EQ(0, history.getLastGoodReplyFor(1));
        EXPECT_EQ(0, history.getLastGoodReplyFor(2));
        history.push_back(102, false, false);
        history.push_back(3, false, false);
        history.push_back(103, false, false);
        const float black_wins = 1.0f;
        history.updateGoodReplies(2, black_wins);
    }
    {
        History history;
        history.push_back(1, false, false);
        history.push_back(101, false, false);
        EXPECT_EQ(2, history.getLastGoodReplyFor(1));
        EXPECT_EQ(0, history.getLastGoodReplyFor(2));
        history.push_back(2, false, false);
        EXPECT_EQ(0, history.getLastGoodReplyFor(1));
        EXPECT_EQ(0, history.getLastGoodReplyFor(2));
    }
}
