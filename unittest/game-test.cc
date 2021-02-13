#include "game.h"
#include "sgf.h"
#include "utils.h"
#include <gtest/gtest.h>

#include <iostream>
#include <set>

namespace {


class IsometryFixture :public ::testing::TestWithParam<unsigned> {
};

TEST_P(IsometryFixture, chooseSafetyMove) {
  unsigned isometry = GetParam();
  //  http://eidokropki.reaktywni.pl/#fWwBwH2R:0,0
  Game game = constructGameFromSgfWithIsometry("(;GM[40]FF[4]CA[UTF-8]SZ[9];B[ed];W[ee];B[fd];W[fe];B[hd];W[fh])", isometry);
  const int whoMoves = 1;
  std::set<std::string> expectedMoves{ applyIsometry("id", isometry, coord),
				       applyIsometry("fi", isometry, coord) };
  std::set<std::string> seenMoves{};
  for (int tries = 1000; tries > 0 and seenMoves.size() < expectedMoves.size(); --tries) {
    auto move = game.chooseSafetyMove(whoMoves);
    ASSERT_NE(0, move.ind);
    seenMoves.insert(coord.indToSgf(move.ind));
  }
  EXPECT_EQ(expectedMoves, seenMoves);
}

INSTANTIATE_TEST_CASE_P(
        Par,
        IsometryFixture,
        ::testing::Values(0,1,2,3,4,5,6,7));


class IsometryFixture2 :public ::testing::TestWithParam<unsigned> {
};

TEST_P(IsometryFixture2, chooseSafetyMoveReturnsNoMoveBecauseEverythingIsSafe) {
  unsigned isometry = GetParam();
  //  http://eidokropki.reaktywni.pl/#33vc3yXjZ:0,0
  Game game = constructGameFromSgfWithIsometry("(;GM[40]FF[4]CA[UTF-8]SZ[9];B[bc];W[eb];B[bd];W[fb])", isometry);
  const int whoMoves = 1;
  auto move = game.chooseSafetyMove(whoMoves);
  EXPECT_EQ(0, move.ind);
}


INSTANTIATE_TEST_CASE_P(
        Par,
        IsometryFixture2,
        ::testing::Values(0,1,2,3,4,5,6,7));

}

TEST(extractSgfMove, forMoveNotWithMinimalArea)
{
  std::string sgf_szkrab9506{"(;FF[4]GM[40]CA[UTF-8]SZ[30]PB[deeppurple]BR[1867]PW[bobek_]WR[1171]DT[2007-11-23 00:57:07]AP[www.szkrab.net.pl:0.0.1]RE[B+R];B[po];W[qo];B[qn];W[ro];B[rn];W[pn];B[so];W[rp];B[sp];W[oo];B[pp];W[sn];B[rq];W[qq];B[qp.qprqspsornqnpoppqp];W[rr];B[sr];W[tr];B[qr];W[rs];B[ss];W[rt];B[pq.pqqrrqqppppq];W[st];B[ts];W[us];B[tt];W[ut];B[tu];W[to];B[tn];W[sm];B[ur];W[vr];B[tq.tqurtsttsssrrqsptq];W[un];B[tm];W[um];B[tl])"};

  SgfParser parser(sgf_szkrab9506);
  auto seq = parser.parseMainVar();
  unsigned move_number = 34;
  Game game(SgfSequence(seq.begin(), seq.begin() + move_number + 1), 1000);
  auto [move, points_to_enclose] = game.extractSgfMove("tq.tqurtsttsssrrqsptq", 1);
  EXPECT_EQ(1, move.who);
  EXPECT_EQ(coord.sgfToPti("tq"), move.ind);
  std::set<pti> expected_interior{coord.sgfToPti("sq"), coord.sgfToPti("tr")};
  ASSERT_EQ(1, move.enclosures.size());
  ASSERT_EQ(2, move.enclosures[0]->interior.size());
  ASSERT_TRUE(expected_interior.find(move.enclosures[0]->interior[0]) != expected_interior.end());
  ASSERT_TRUE(expected_interior.find(move.enclosures[0]->interior[1]) != expected_interior.end());
  game.show();
}
