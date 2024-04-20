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
