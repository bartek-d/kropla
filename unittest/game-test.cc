#include "game.h"

#include <gtest/gtest.h>

#include <iostream>
#include <set>

#include "sgf.h"
#include "utils.h"

namespace
{
class IsometryFixture : public ::testing::TestWithParam<unsigned>
{
};

TEST_P(IsometryFixture, chooseSafetyMove)
{
    unsigned isometry = GetParam();
    //  http://eidokropki.reaktywni.pl/#fWwBwH2R:0,0
    Game game = constructGameFromSgfWithIsometry(
        "(;GM[40]FF[4]CA[UTF-8]SZ[9];B[ed];W[ee];B[fd];W[fe];B[hd];W[fh];B[bd];"
        "W[bc])",
        isometry);
    const int whoMoves = 1;
    std::set<std::string> expectedMoves{applyIsometry("ad", isometry, coord),
                                        applyIsometry("ac", isometry, coord)};
    std::set<std::string> seenMoves{};
    for (int tries = 1000;
         tries > 0 and seenMoves.size() < expectedMoves.size(); --tries)
    {
        auto move = game.chooseSafetyMove(whoMoves);
        ASSERT_NE(0, move.ind);
        seenMoves.insert(coord.indToSgf(move.ind));
    }
    EXPECT_EQ(expectedMoves, seenMoves);
}

INSTANTIATE_TEST_CASE_P(Par, IsometryFixture,
                        ::testing::Values(0, 1, 2, 3, 4, 5, 6, 7));

class IsometryFixtureSR : public ::testing::TestWithParam<unsigned>
{
};

TEST(IsometryFixtureSR, chooseSafetyResponse_doesNotGiveMovesInAtari)
{
    std::string sgf{
        "(;FF[4]GM[40]CA[UTF-8]AP[board.cc:SgfTree]RU[Punish=0,Holes=1,AddTurn="
        "0,MustSurr=0,MinArea=0,Pass=0,Stop=0,LastSafe=0,ScoreTerr=0,"
        "InstantWin=0]SZ[20];B[ij];W[li];B[ig];W[kl];B[lj];W[kj];B[ki];W[ji];B["
        "kh];W[jj];B[lk];W[mi];B[jh];W[ii];B[ik];W[kk];B[nk];W[hi];B[gk];W[nj];"
        "B[mk];W[ok];B[oj];W[oi];B[mn];W[pj.njokpjoinj];B[nm];W[gj];B[fg];W[fk]"
        ";B[dk];W[fl];B[eo];W[ej];B[dh];W[dj];B[cj];W[ck];B[bk];W[cl];B[bl];W["
        "ci];B[bj];W[di];B[bg];W[cm];B[bo];W[eh];B[eg];W[ch];B[cg];W[dg."
        "chdiehdgch];B[df];W[hg];B[hf];W[if];B[hh];W[gg];B[gh];W[gf];B[ff];W["
        "he.gfhgifhegf];B[ih];W[fh];B[lf];W[fe];B[ef];W[gi];B[cf];W[cd];B[bc];"
        "W[bd];B[ad];W[ae];B[be])"};
    // reply in rollout: W[bf]C[soft]
    Game game = constructGameFromSgfWithIsometry(sgf, 0);
    const int who_moves = 2;
    auto move = game.chooseSoftSafetyResponse(who_moves);
    EXPECT_EQ(0, move.ind);
}

INSTANTIATE_TEST_CASE_P(Par, IsometryFixtureSR,
                        ::testing::Values(0, 1, 2, 3, 4, 5, 6, 7));

class IsometryFixture2 : public ::testing::TestWithParam<unsigned>
{
};

TEST_P(IsometryFixture2, chooseSafetyMoveReturnsNoMoveBecauseEverythingIsSafe)
{
    unsigned isometry = GetParam();
    //  http://eidokropki.reaktywni.pl/#33vc3yXjZ:0,0
    Game game = constructGameFromSgfWithIsometry(
        "(;GM[40]FF[4]CA[UTF-8]SZ[9];B[bc];W[eb];B[bd];W[fb])", isometry);
    const int whoMoves = 1;
    auto move = game.chooseSafetyMove(whoMoves);
    EXPECT_EQ(0, move.ind);
}

INSTANTIATE_TEST_CASE_P(Par, IsometryFixture2,
                        ::testing::Values(0, 1, 2, 3, 4, 5, 6, 7));

class IsometryFixtureS9 : public ::testing::TestWithParam<unsigned>
{
};

TEST_P(IsometryFixtureS9,
       forDotsThatAreAlreadySafe_dontGetMovesThatMakeThemSafe)
{
    std::string sgf{
        "(;FF[4]GM[40]CA[UTF-8]AP[board.cc:SgfTree]RU[Punish=0,Holes=1,AddTurn="
        "0,MustSurr=0,MinArea=0,Pass=0,Stop=0,LastSafe=0,ScoreTerr=0,"
        "InstantWin=0]SZ[20];B[ij];W[li];B[ig];W[kl];B[lj];W[kj];B[ki];W[ji];B["
        "kh];W[jj];B[lk];W[mi];B[jh];W[ii];B[ik];W[kk];B[nk];W[hi];B[gk];W[nj];"
        "B[mk];W[ok];B[oj];W[oi];B[mn];W[pj.njokpjoinj];B[nm];W[gj];B[fg];W[fk]"
        ";B[dk];W[fl];B[eo];W[ej];B[dh];W[dj];B[cj];W[ck];B[bk];W[cl];B[bl];W["
        "ci];B[bj];W[di];B[bg];W[cm];B[bo];W[eh];B[eg];W[ch];B[cg];W[dg."
        "chdiehdgch];B[df];W[hg];B[hf];W[if];B[hh];W[gg];B[gh];W[gf];B[ff];W["
        "he.gfhgifhegf];B[ih];W[fh];B[lf];W[fe];B[ef];W[gi];B[cf];W[cd];B[bc];"
        "W[bd];B[ad];W[ae];B[be];W[bf];B[ce];W[dd];B[db];W[bb];B[ac];W[cc];B["
        "cb];W[el];B[en];W[gn];B[gp];W[qj];B[ql];W[qn];B[rm];W[af];B[rn];W[ag];"
        "B[bh];W[bi];B[ah];W[ai];B[ib];W[kb];B[mb];W[ka])"};
    // reply in rollout: B[ia]C[saf]
    Game game = constructGameFromSgfWithIsometry(sgf, 0);
    const int who_moves = 1;
    auto moves = game.getSafetyMoves(who_moves);
    EXPECT_EQ(1, moves.size());
}

INSTANTIATE_TEST_CASE_P(Par, IsometryFixtureS9,
                        ::testing::Values(0, 1, 2, 3, 4, 5, 6, 7));

TEST(extractSgfMove, forMoveNotWithMinimalArea)
{
    std::string sgf_szkrab9506{
        "(;FF[4]GM[40]CA[UTF-8]SZ[30]PB[deeppurple]BR[1867]PW[bobek_]WR[1171]"
        "DT[2007-11-23 "
        "00:57:07]AP[www.szkrab.net.pl:0.0.1]RE[B+R];B[po];W[qo];B[qn];W[ro];B["
        "rn];W[pn];B[so];W[rp];B[sp];W[oo];B[pp];W[sn];B[rq];W[qq];B[qp."
        "qprqspsornqnpoppqp];W[rr];B[sr];W[tr];B[qr];W[rs];B[ss];W[rt];B[pq."
        "pqqrrqqppppq];W[st];B[ts];W[us];B[tt];W[ut];B[tu];W[to];B[tn];W[sm];B["
        "ur];W[vr];B[tq.tqurtsttsssrrqsptq];W[un];B[tm];W[um];B[tl])"};

    SgfParser parser(sgf_szkrab9506);
    auto seq = parser.parseMainVar();
    unsigned move_number = 34;
    Game game(SgfSequence(seq.begin(), seq.begin() + move_number + 1), 1000);
    auto [move, points_to_enclose] =
        game.extractSgfMove("tq.tqurtsttsssrrqsptq", 1);
    EXPECT_EQ(1, move.who);
    EXPECT_EQ(coord.sgfToPti("tq"), move.ind);
    std::set<pti> expected_interior{coord.sgfToPti("sq"), coord.sgfToPti("tr")};
    ASSERT_EQ(1, move.enclosures.size());
    ASSERT_EQ(2, move.enclosures[0]->interior.size());
    ASSERT_TRUE(expected_interior.find(move.enclosures[0]->interior[0]) !=
                expected_interior.end());
    ASSERT_TRUE(expected_interior.find(move.enclosures[0]->interior[1]) !=
                expected_interior.end());
}

TEST(smallMultimap, works)
{
    SmallMultimap<7, 7> map{};
    EXPECT_EQ(0, map.getNumberOfGroups());
    map.addPair(3, 111);
    EXPECT_EQ(1, map.getNumberOfGroups());
    EXPECT_EQ(1, map.numberOfElems(0));
    EXPECT_EQ(111, map.elem(0, 0));
    map.addPair(4, 112);
    map.addPair(3, 113);
    EXPECT_EQ(2, map.getNumberOfGroups());
    EXPECT_EQ(2, map.numberOfElems(0));
    EXPECT_EQ(1, map.numberOfElems(1));
    map.addPair(4, 115);
    map.addPair(5, 116);
    EXPECT_EQ(3, map.getNumberOfGroups());
}

class IsometryFixture3 : public ::testing::TestWithParam<unsigned>
{
};

bool containsThreat2m(const AllThreats& thr, pti move0, pti move1)
{
    auto it =
        std::find_if(thr.threats2m.begin(), thr.threats2m.end(),
                     [move0](const auto& t2m) { return t2m.where0 == move0; });
    if (it == thr.threats2m.end()) return false;
    auto it2 =
        std::find_if(it->thr_list.begin(), it->thr_list.end(),
                     [move1](const auto& t) { return t.where == move1; });
    return (it2 != it->thr_list.end());
}

TEST_P(IsometryFixture3, weFindThreat2mDistantFromThePointPlayed)
{
    unsigned isometry = GetParam();
    // http://eidokropki.reaktywni.pl/#2aub0edZj:0,0
    Game game = constructGameFromSgfWithIsometry(
        "(;GM[40]FF[4]CA[UTF-8]SZ[20];B[fg];W[gg];B[ge];W[gf];B[gh];W[jg];B[hh]"
        ";W[jf];B[ff])",
        isometry);
    const auto& thr = game.getAllThreatsForPlayer(0);
    game.show();
    std::cout << coord.showBoard(thr.is_in_2m_encl) << std::endl;
    for (const auto& t2 : thr.threats2m)
    {
        std::cout << coord.showPt(t2.where0) << "; min_win: " << t2.min_win
                  << ", " << t2.min_win2
                  << ", win_move_count=" << t2.win_move_count
                  << ", flags= " << t2.flags << std::endl;
        if (not t2.is_in_encl2.empty())
            std::cout << coord.showBoard(t2.is_in_encl2) << std::endl;
        for (const auto& z : t2.thr_list)
        {
            std::cout << "   " << coord.showPt(z.where) << "; type: " << z.type
                      << ", terr_points: " << z.terr_points
                      << ", opp_dots: " << z.opp_dots << std::endl;
        }
    }
    auto point1 = coord.sgfToPti(applyIsometry("gf", isometry, coord));
    auto point2 = coord.sgfToPti(applyIsometry("gg", isometry, coord));
    auto point3 = coord.sgfToPti(applyIsometry("hg", isometry, coord));
    EXPECT_EQ(1, thr.is_in_2m_encl[point1]);
    EXPECT_EQ(1, thr.is_in_2m_encl[point2]);
    EXPECT_EQ(0, thr.is_in_2m_encl[point3]);
    EXPECT_TRUE(containsThreat2m(
        thr, coord.sgfToPti(applyIsometry("hf", isometry, coord)),
        coord.sgfToPti(applyIsometry("hg", isometry, coord))));
    EXPECT_TRUE(containsThreat2m(
        thr, coord.sgfToPti(applyIsometry("hf", isometry, coord)),
        coord.sgfToPti(applyIsometry("ig", isometry, coord))));
    EXPECT_TRUE(containsThreat2m(
        thr, coord.sgfToPti(applyIsometry("hg", isometry, coord)),
        coord.sgfToPti(applyIsometry("hf", isometry, coord))));
    EXPECT_TRUE(containsThreat2m(
        thr, coord.sgfToPti(applyIsometry("ig", isometry, coord)),
        coord.sgfToPti(applyIsometry("hf", isometry, coord))));
}

INSTANTIATE_TEST_CASE_P(Par, IsometryFixture3,
                        ::testing::Values(0, 1, 2, 3, 4, 5, 6, 7));

class IsometryFixture4 : public ::testing::TestWithParam<unsigned>
{
};

TEST_P(IsometryFixture4,
       weFindThreat2mWithOnePointCloseAndOneDistantFromThePointPlayed)
{
    const unsigned isometry = GetParam();
    auto sgf = constructSgfFromGameBoard(
        "......."
        "..o.o.."
        ".oxx.o."
        ".o.xxo."
        "....oo."
        "......."
        ".......");
    Game game = constructGameFromSgfWithIsometry(sgf, isometry);
    game.makeSgfMove(applyIsometry("ce", isometry, coord), 1);
    const auto& thr = game.getAllThreatsForPlayer(0);
    EXPECT_TRUE(containsThreat2m(
        thr, coord.sgfToPti(applyIsometry("de", isometry, coord)),
        coord.sgfToPti(applyIsometry("da", isometry, coord))));
    EXPECT_TRUE(containsThreat2m(
        thr, coord.sgfToPti(applyIsometry("da", isometry, coord)),
        coord.sgfToPti(applyIsometry("de", isometry, coord))));
    EXPECT_TRUE(containsThreat2m(
        thr, coord.sgfToPti(applyIsometry("de", isometry, coord)),
        coord.sgfToPti(applyIsometry("db", isometry, coord))));
    EXPECT_TRUE(containsThreat2m(
        thr, coord.sgfToPti(applyIsometry("db", isometry, coord)),
        coord.sgfToPti(applyIsometry("de", isometry, coord))));
    EXPECT_TRUE(containsThreat2m(
        thr, coord.sgfToPti(applyIsometry("df", isometry, coord)),
        coord.sgfToPti(applyIsometry("da", isometry, coord))));
    EXPECT_TRUE(containsThreat2m(
        thr, coord.sgfToPti(applyIsometry("da", isometry, coord)),
        coord.sgfToPti(applyIsometry("df", isometry, coord))));
    EXPECT_TRUE(containsThreat2m(
        thr, coord.sgfToPti(applyIsometry("df", isometry, coord)),
        coord.sgfToPti(applyIsometry("db", isometry, coord))));
    EXPECT_TRUE(containsThreat2m(
        thr, coord.sgfToPti(applyIsometry("db", isometry, coord)),
        coord.sgfToPti(applyIsometry("df", isometry, coord))));
}

INSTANTIATE_TEST_CASE_P(Par, IsometryFixture4,
                        ::testing::Values(0, 1, 2, 3, 4, 5, 6, 7));

class IsometryFixture5 : public ::testing::TestWithParam<unsigned>
{
};

TEST_P(IsometryFixture5, weFindThreat2mWithBothPointsCloseToThePointPlayed)
{
    const unsigned isometry = GetParam();
    auto sgf = constructSgfFromGameBoard(
        "...o..."
        "..o.o.."
        ".oxx.o."
        ".o.xxo."
        ".....o."
        "......."
        ".......");
    Game game = constructGameFromSgfWithIsometry(sgf, isometry);
    game.makeSgfMove(applyIsometry("de", isometry, coord), 1);
    const auto& thr = game.getAllThreatsForPlayer(0);
    EXPECT_TRUE(containsThreat2m(
        thr, coord.sgfToPti(applyIsometry("cd", isometry, coord)),
        coord.sgfToPti(applyIsometry("ee", isometry, coord))));
    EXPECT_TRUE(containsThreat2m(
        thr, coord.sgfToPti(applyIsometry("ee", isometry, coord)),
        coord.sgfToPti(applyIsometry("cd", isometry, coord))));
    EXPECT_TRUE(containsThreat2m(
        thr, coord.sgfToPti(applyIsometry("ce", isometry, coord)),
        coord.sgfToPti(applyIsometry("ee", isometry, coord))));
    EXPECT_TRUE(containsThreat2m(
        thr, coord.sgfToPti(applyIsometry("ee", isometry, coord)),
        coord.sgfToPti(applyIsometry("ce", isometry, coord))));
    EXPECT_TRUE(containsThreat2m(
        thr, coord.sgfToPti(applyIsometry("cd", isometry, coord)),
        coord.sgfToPti(applyIsometry("ef", isometry, coord))));
    EXPECT_TRUE(containsThreat2m(
        thr, coord.sgfToPti(applyIsometry("ef", isometry, coord)),
        coord.sgfToPti(applyIsometry("cd", isometry, coord))));
    EXPECT_TRUE(containsThreat2m(
        thr, coord.sgfToPti(applyIsometry("ce", isometry, coord)),
        coord.sgfToPti(applyIsometry("ef", isometry, coord))));
    EXPECT_TRUE(containsThreat2m(
        thr, coord.sgfToPti(applyIsometry("ef", isometry, coord)),
        coord.sgfToPti(applyIsometry("ce", isometry, coord))));
}

INSTANTIATE_TEST_CASE_P(Par, IsometryFixture5,
                        ::testing::Values(0, 1, 2, 3, 4, 5, 6, 7));

class IsometryFixture6 : public ::testing::TestWithParam<unsigned>
{
};

TEST_P(IsometryFixture6,
       weFindThreat2mWithBothPointsToEachOtherAndOneCloseToThePointPlayed)
{
    const unsigned isometry = GetParam();
    auto sgf = constructSgfFromGameBoard(
        "...o..."
        "..o.o.."
        ".oxx.o."
        ".o.xxo."
        ".....o."
        "......."
        ".......");
    Game game = constructGameFromSgfWithIsometry(sgf, isometry);
    game.makeSgfMove(applyIsometry("ce", isometry, coord), 1);
    const auto& thr = game.getAllThreatsForPlayer(0);
    EXPECT_TRUE(containsThreat2m(
        thr, coord.sgfToPti(applyIsometry("de", isometry, coord)),
        coord.sgfToPti(applyIsometry("ee", isometry, coord))));
    EXPECT_TRUE(containsThreat2m(
        thr, coord.sgfToPti(applyIsometry("ee", isometry, coord)),
        coord.sgfToPti(applyIsometry("de", isometry, coord))));
    EXPECT_TRUE(containsThreat2m(
        thr, coord.sgfToPti(applyIsometry("df", isometry, coord)),
        coord.sgfToPti(applyIsometry("ee", isometry, coord))));
    EXPECT_TRUE(containsThreat2m(
        thr, coord.sgfToPti(applyIsometry("ee", isometry, coord)),
        coord.sgfToPti(applyIsometry("df", isometry, coord))));
    EXPECT_TRUE(containsThreat2m(
        thr, coord.sgfToPti(applyIsometry("de", isometry, coord)),
        coord.sgfToPti(applyIsometry("ef", isometry, coord))));
    EXPECT_TRUE(containsThreat2m(
        thr, coord.sgfToPti(applyIsometry("ef", isometry, coord)),
        coord.sgfToPti(applyIsometry("de", isometry, coord))));
    EXPECT_TRUE(containsThreat2m(
        thr, coord.sgfToPti(applyIsometry("df", isometry, coord)),
        coord.sgfToPti(applyIsometry("ef", isometry, coord))));
    EXPECT_TRUE(containsThreat2m(
        thr, coord.sgfToPti(applyIsometry("ef", isometry, coord)),
        coord.sgfToPti(applyIsometry("df", isometry, coord))));
}

INSTANTIATE_TEST_CASE_P(Par, IsometryFixture6,
                        ::testing::Values(0, 1, 2, 3, 4, 5, 6, 7));

class IsometryFixture7 : public ::testing::TestWithParam<unsigned>
{
};

TEST_P(
    IsometryFixture7,
    weFindThreat2mWithOnePointCloseAndOneDistantFromThePointPlayed_evenIfTheOneCloseMakesEnclosure)
{
    const unsigned isometry = GetParam();
    auto sgf = constructSgfFromGameBoard(
        ".o.o..."
        "ox..o.."
        "..xx.o."
        ".o.xxo."
        "..oo.o."
        "......."
        ".......");
    Game game = constructGameFromSgfWithIsometry(sgf, isometry);
    game.makeSgfMove(applyIsometry("cb", isometry, coord), 1);
    const auto& thr = game.getAllThreatsForPlayer(0);
    EXPECT_TRUE(containsThreat2m(
        thr, coord.sgfToPti(applyIsometry("bc", isometry, coord)),
        coord.sgfToPti(applyIsometry("ee", isometry, coord))));
    EXPECT_TRUE(containsThreat2m(
        thr, coord.sgfToPti(applyIsometry("ee", isometry, coord)),
        coord.sgfToPti(applyIsometry("bc", isometry, coord))));
    EXPECT_TRUE(containsThreat2m(
        thr, coord.sgfToPti(applyIsometry("bc", isometry, coord)),
        coord.sgfToPti(applyIsometry("ef", isometry, coord))));
    EXPECT_TRUE(containsThreat2m(
        thr, coord.sgfToPti(applyIsometry("ef", isometry, coord)),
        coord.sgfToPti(applyIsometry("bc", isometry, coord))));
}

INSTANTIATE_TEST_CASE_P(Par, IsometryFixture7,
                        ::testing::Values(0, 1, 2, 3, 4, 5, 6, 7));

TEST(Threats2mTest,
     miaiIsFoundWhenTwoSecondMovesAreInsideTerr_zagram355111_after_move107)
{
    // It seems everything is correct here, but we should add prior for semeai
    // threats2m.
    std::string sgf{
        "(;FF[4]GM[40]CA[UTF-8]AP[zagram.org]SZ[15]RU[Punish=0,Holes=1,AddTurn="
        "0,MustSurr=0,MinArea=0,Pass=0,Stop=0,LastSafe=0,ScoreTerr=0,"
        "InstantWin=0]HA[4]AB[ld][dl][ll][dd]PB[bot Amelia]PW[Сер "
        "Берримор]TM[60]OT[10]BL[72]WL[75]DT[2021-04-19]RE[W+R];W[hg]WL[70];B["
        "ih]BL[67.2];W[jf]WL[77.2];B[ig]BL[62.3];W[if]WL[84.8];B[hf]BL[57];W["
        "gg]WL[92.7];B[he]BL[52.2];W[ke]WL[100.5];B[hi]BL[47.6];W[fh]WL[107.3];"
        "B[kg]BL[42.2];W[le]WL[114.7];B[kd]BL[38.7];W[jd]WL[121.2];B[gj]BL[35."
        "3];W[gd]WL[127.5];B[hd]BL[32];W[hc]WL[135];B[id]BL[28.4];W[ic]WL[141."
        "7];B[jc]BL[31.1];W[je]WL[137.8];B[jb]BL[32.5];W[fe]WL[144.6];B[me]BL["
        "29.5];W[ff.feffgghgifjejdichcgdfe]WL[151.2];B[mf]BL[30.8];W[jg]WL[156."
        "7];B[jh]BL[34.8];W[kh]WL[164.2];B[lg]BL[32.8];W[li]WL[172.5];B[ki]BL["
        "29.2];W[lh]WL[180.4];B[kj]BL[32.3];W[ni]WL[188.2];B[ei]BL[29.1];W[fi]"
        "WL[195];B[fj]BL[32.6];W[hh]WL[202.9];B[gm]BL[29.4];W[gi]WL[209.7];B["
        "ii]BL[31.3];W[eh]WL[216.8];B[di]BL[32.9];W[cg]WL[225];B[dh]BL[29.4];W["
        "dg]WL[233];B[ng]BL[30.9];W[mj]WL[240.5];B[mh]BL[32.1];W[nh]WL[248.4];"
        "B[mg]BL[32];W[lj]WL[254.9];B[kk]BL[29.3];W[bh]WL[261.4];B[bi]BL[30.7];"
        "W[ai]WL[266.7];B[bj]BL[32.6];W[cc]WL[271.1];B[dc]BL[29.6];W[db]WL[279."
        "1];B[eb]BL[31.1];W[ec]WL[287.6];B[cd]BL[32.9];W[bc]WL[295];B[fb]BL[29."
        "3];W[fd]WL[301.3];B[bf]BL[30.9];W[be]WL[308.1];B[bg]BL[33.6];W[cf]WL["
        "310.1];B[ah]BL[29.8];W[ch]WL[318.1];B[cb]BL[31.2];W[da]WL[324.4];B[fc]"
        "BL[32.8];W[ed]WL[329.3];B[ad]BL[29.3];W[bd."
        "bdbecfdgehfhggfffeedecdbccbd]WL[327.5];B[mi]BL[30.9];W[nk]WL[333.9];B["
        "nl]BL[33.3];W[ci]WL[338.6];B[cj]BL[29];W[im]WL[344.6];B[il]BL[30.8];W["
        "jl]WL[351.6];B[jm]BL[32.5];W[km]WL[358];B[jn]BL[32.4];W[kl]WL[366.6];"
        "B[jk]BL[30.1];W[in]WL[364.1];B[kn]BL[32.4];W[lm]WL[372.3];B[nn]BL[28."
        "9];W[ml]WL[375.3];B[lk]BL[32.1];W[mm]WL[374.8];B[mk]BL[31.7];W[nj]WL["
        "382.4];B[nm]BL[29.6];W[hl]WL[388.3];B[ik])"};
    unsigned isometry = 0;
    Game game = constructGameFromSgfWithIsometry(sgf, isometry);
    game.makeSgfMove("ej", 2);
    const auto& thr = game.getAllThreatsForPlayer(1);
    EXPECT_LE(1, thr.is_in_2m_miai[coord.sgfToPti("kk")]);
    game.makeSgfMove("ek", 1);
    const auto& thr2 = game.getAllThreatsForPlayer(1);
    EXPECT_LE(1, thr2.is_in_2m_miai[coord.sgfToPti("kk")]);
    int count =
        std::count_if(thr.threats2m.begin(), thr.threats2m.end(), [&](auto& t) {
            return (t.where0 == coord.sgfToPti("gk") and t.min_win2 > 5 and
                    t.isSafe());
        });
    EXPECT_EQ(1, count);
}

bool hasOverlappingEnclosures(const Move& move)
{
    if (move.enclosures.size() <= 1) return false;
    std::vector<int> interior_count(coord.getSize(), 0);
    for (const auto& encl : move.enclosures)
    {
        for (const auto pt : encl->interior)
        {
            ++interior_count[pt];
            if (interior_count[pt] >= 2)
            {
                std::cout << "Point " << coord.showPt(pt)
                          << " is in 2 enclosures! Move: " << move.show()
                          << std::endl;
                return true;
            }
        }
    }
    return false;
}

TEST(getEnclMoves, shouldNotFindOverlappingEnclosures_eido_u0O9GbZR)
{
    std::string sgf{
        "(;SZ[20]PB[kropla_1df1ee076aa_nocnn]PW[kropla_1df1ee076aa_nocnn_"
        "noThr2mTurnoff];B[dg];W[hj];B[js];W[bo];B[gb];W[dp];B[jk];W[jf];B[ff];"
        "W[dj];B[ks];W[ji];B[ko];W[ej];B[qn];W[il];B[hc];W[km];B[in];W[gi];B["
        "hm];W[ik];B[mn];W[am];B[fr];W[dm];B[qk];W[mk];B[ll];W[mh];B[ml];W[lj];"
        "B[ol];W[pi];B[qg];W[cq];B[mb];W[hg];B[ri];W[ns];B[es];W[nk];B[nl];W["
        "go];B[oj];W[ok];B[pj];W[os];B[mr];W[nr];B[mq];W[pk];B[sg];W[bh];B[pl];"
        "W[ge];B[bf];W[nq];B[qj];W[od];B[ob];W[id];B[sm];W[bg];B[cf];W[hd];B["
        "ld];W[mf];B[jn];W[ne];B[kl];W[gp];B[bl];W[kj];B[gm];W[fn];B[jj];W[ij];"
        "B[bk];W[fe];B[ee];W[np];B[fm];W[en];B[hq];W[th];B[sh];W[hp];B[jb];W["
        "ip];B[dl];W[el];B[em];W[cm];B[dk];W[ci];B[ek];W[tk];B[bm];W[jm];B[fj];"
        "W[fi];B[ni];W[ed];B[mi];W[lh];B[be];W[nh];B[li];W[ki];B[bn];W[iq];B["
        "co];W[dn];B[bp];W[ir];B[gs];W[is];B[it];W[bq];B[do];W[ep];B[fk];W[cl];"
        "B[ck];W[cp];B[ao.aobpcobnao];W[dh];B[eo];W[kc];B[fo];W[gn];B[fp];W[kq]"
        ";B[lp];W[rl];B[ht];W[lr];B[ls];W[ql];B[sk];W[mo];B[ln];W[rn];B[rm];W["
        "qm];B[pn];W[lo];B[kn];W[ms];B[gq."
        "blbmbncodoeofofpgqfrgshtitjskslsmrmqlpkojninhmgmfmemdlckbl];W[lc];B["
        "mc];W[md];B[jc];W[jd];B[oh];W[nj];B[oi];W[sl];B[tl];W[sj];B[rk];W[so];"
        "B[om];W[ss];B[qp];W[qq];B[jl];W[im];B[pp];W[pq];B[qt];W[og];B[mj];W["
        "lk];B[pg];W[fc];B[eg];W[de];B[ef];W[cd];B[of];W[kd];B[gf];W[cg];B[df];"
        "W[ng];B[hf];W[pf];B[ig];W[if];B[qf];W[pe];B[no];W[mp];B[eh];W[oo];B["
        "ei];W[nn.monpoonnmo];B[gg];W[kg];B[hh.gghhighfgg];W[ii];B[gj];W[hi];B["
        "di];W[cj];B[he];W[qe];B[gc];W[gd];B[tj];W[ti];B[si];W[le.kdlemdlckd];"
        "B[ie];W[ch];B[db];W[po];B[qo];W[qs];B[re];W[rd];B[sd];W[sn];B[rp."
        "ompnqnrmsmtlskrkqkplom];W[sc];B[rc];W[qd];B[sb];W[se];B[tc.rcsdtcsbrc]"
        ";W[rf.qerfserdqe];B[te];W[rq];B[bb];W[kb];B[ka];W[fb];B[fa];W[pb];B["
        "pa];W[qc];B[qa];W[la];B[lb];W[cb];B[eb];W[ca];B[cc];W[fd];B[bc];W[ad];"
        "B[bd];W[sf];B[tf];W[ra];B[nb];W[mm];B[lm];W[rb];B[qb];W[nm."
        "mfngogpfpeodnemf];B[ib];W[ha];B[sq];W[sp];B[sr];W[tg];B[cs];W[ft];B["
        "ds];W[ga];B[bs];W[aq];B[rr];W[rs];B[qr];W[ps];B[dr];W[eq];B[pr];W[oq];"
        "B[ai];W[hb])"};
    unsigned isometry = 0;
    Game game = constructGameFromSgfWithIsometry(sgf, isometry);
    TreenodeAllocator alloc;
    Treenode root;
    root.parent = &root;
    const int whoseMove = 1;
    const int depth = 1;
    game.generateListOfMoves(alloc, &root, depth, whoseMove);
    root.children = alloc.getLastBlock();
    Treenode* ch = root.children;
    for (;;)
    {
        std::cout << ch->move.show() << std::endl;
        EXPECT_FALSE(hasOverlappingEnclosures(ch->move));
        if (ch->isLast()) break;
        ch++;
    }
}

TEST(Zobrist, worksForNonEnclMoves)
{
    auto sgf = constructSgfFromGameBoard(
        "......."
        "......."
        "......."
        "......."
        "......."
        "......."
        ".......");
    Game game = constructGameFromSgfWithIsometry(sgf, 0);
    uint64_t zobr = 0;
    EXPECT_EQ(zobr, game.getZobrist());
    auto makeMoveAndCheckZobrist = [&zobr, &game](auto move, auto who) {
        game.makeSgfMove(move, who);
        zobr ^= coord.zobrist_dots[who - 1][coord.sgfToPti(move)];
        EXPECT_EQ(zobr, game.getZobrist());
    };
    makeMoveAndCheckZobrist("cb", 1);
    makeMoveAndCheckZobrist("dd", 2);
    makeMoveAndCheckZobrist("ea", 1);
    makeMoveAndCheckZobrist("ce", 2);
    makeMoveAndCheckZobrist("cc", 1);
}

TEST(Zobrist, worksForEnclMoves)
{
    auto sgf = constructSgfFromGameBoard(
        ".ox...."
        "oxox..."
        "......."
        "......."
        "......."
        "......."
        ".......");
    Game game = constructGameFromSgfWithIsometry(sgf, 0);
    auto getZobr = [&](auto move, auto who) {
        return coord.zobrist_dots[who - 1][coord.sgfToPti(move)];
    };
    uint64_t zobr = getZobr("ba", 1) ^ getZobr("ca", 2) ^ getZobr("ab", 1) ^
                    getZobr("bb", 2) ^ getZobr("cb", 1) ^ getZobr("db", 2);
    EXPECT_EQ(zobr, game.getZobrist());
    game.makeSgfMove("bc.bccbbaabbc", 1);
    zobr ^= getZobr("bc", 1) ^ coord.zobrist_encl[0][coord.sgfToPti("bb")];
    EXPECT_EQ(zobr, game.getZobrist());
}

}  // namespace
