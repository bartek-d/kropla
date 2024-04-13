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

TEST_P(IsometryFixture, chooseSafetyResponse_doesNotGiveMovesInAtari)
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

TEST_P(IsometryFixture, chooseSafetyMoveReturnsNoMoveBecauseEverythingIsSafe)
{
    unsigned isometry = GetParam();
    //  http://eidokropki.reaktywni.pl/#33vc3yXjZ:0,0
    Game game = constructGameFromSgfWithIsometry(
        "(;GM[40]FF[4]CA[UTF-8]SZ[9];B[bc];W[eb];B[bd];W[fb])", isometry);
    const int whoMoves = 1;
    auto move = game.chooseSafetyMove(whoMoves);
    EXPECT_EQ(0, move.ind);
}

TEST_P(IsometryFixture, forDotsThatAreAlreadySafe_dontGetMovesThatMakeThemSafe)
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

TEST_P(IsometryFixture, weFindThreat2mDistantFromThePointPlayed)
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

TEST_P(IsometryFixture,
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

TEST_P(IsometryFixture, weFindThreat2mWithBothPointsCloseToThePointPlayed)
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

TEST_P(IsometryFixture,
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

TEST_P(
    IsometryFixture,
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

TEST_P(IsometryFixture, weFindThreat2mWhenThereWasThreatIn1m)
{
    const unsigned isometry = GetParam();
    auto sgf = constructSgfFromGameBoard(
        ".ooo..."
        "ox..o.."
        "oxxooo."
        "oxxooo."
        "....o.."
        "......."
        "..xx...");
    Game game = constructGameFromSgfWithIsometry(sgf, isometry);
    game.makeSgfMove(applyIsometry("be", isometry, coord), 1);
    game.makeSgfMove(applyIsometry("ce", isometry, coord), 2);
    const auto& thr = game.getAllThreatsForPlayer(0);
    EXPECT_TRUE(containsThreat2m(
        thr, coord.sgfToPti(applyIsometry("cf", isometry, coord)),
        coord.sgfToPti(applyIsometry("de", isometry, coord))));
    EXPECT_TRUE(containsThreat2m(
        thr, coord.sgfToPti(applyIsometry("cf", isometry, coord)),
        coord.sgfToPti(applyIsometry("df", isometry, coord))));
}

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
    int count = std::count_if(thr.threats2m.begin(), thr.threats2m.end(),
                              [&](auto& t)
                              {
                                  return (t.where0 == coord.sgfToPti("gk") and
                                          t.min_win2 > 5 and t.isSafe());
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
    auto makeMoveAndCheckZobrist = [&zobr, &game](auto move, auto who)
    {
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
    auto getZobr = [&](auto move, auto who)
    { return coord.zobrist_dots[who - 1][coord.sgfToPti(move)]; };
    uint64_t zobr = getZobr("ba", 1) ^ getZobr("ca", 2) ^ getZobr("ab", 1) ^
                    getZobr("bb", 2) ^ getZobr("cb", 1) ^ getZobr("db", 2);
    EXPECT_EQ(zobr, game.getZobrist());
    game.makeSgfMove("bc.bccbbaabbc", 1);
    zobr ^= getZobr("bc", 1) ^ coord.zobrist_encl[0][coord.sgfToPti("bb")];
    EXPECT_EQ(zobr, game.getZobrist());
}

TEST_P(IsometryFixture, deleteUnnecessaryThreats)
{
    const unsigned isometry = GetParam();
    const std::string sgf_start{
        "(;SZ[20]PB[kropla_c1db66445b6:7000]PW[kropla:7000];B[kl];W[ki];B[lj];"
        "W[li];B[mj];W[mi];B[nj]"
        ";W[ni];B[fk];W[oj];B[ok];W[pk];B[ol];W[pl];B[pm];W[qm];B[pn];W[qn];B["
        "po];W[pj];B[jk];W[qo]"
        ";B[pp];W[kj];B[kk];W[qp];B[ij];W[pq];B[oq];W[nq];B[or];W[np];B[op];W["
        "mr];B[mn];W[no];B[nn]"
        ";W[hm];B[hl];W[im];B[il];W[gm];B[kn];W[jm];B[km];W[ms];B[qq];W[pr];B["
        "ps];W[qr];B[rq];W[qs]"
        ";B[os];W[oo];B[on];W[oh];B[gl];W[fm];B[ql];W[rl];B[qk];W[rk];B[qj];W["
        "ri];B[qi];W[qh];B[hh]"};
    Game game1 = constructGameFromSgfWithIsometry(
        sgf_start + ";W[sj];B[eh];W[pi])", isometry);
    Game game2 = constructGameFromSgfWithIsometry(
        sgf_start + ";W[pi];B[eh];W[sj])", isometry);
    EXPECT_EQ(game1.threats[1].is_in_encl, game2.threats[1].is_in_encl);
    EXPECT_EQ(game1.threats[1].is_in_terr, game2.threats[1].is_in_terr);
    EXPECT_EQ(game1.threats[1].is_in_border, game2.threats[1].is_in_border);
}

void checkIfGamesAreSame(const Game& game1, const Game& game2)
{
    for (int32_t i = coord.first; i <= coord.last; ++i)
    {
        EXPECT_EQ(game1.threats[0].is_in_encl[i],
                  game2.threats[0].is_in_encl[i])
            << "  where i -> " << coord.showPt(i);
        EXPECT_EQ(game1.threats[1].is_in_encl[i],
                  game2.threats[1].is_in_encl[i])
            << "  where i -> " << coord.showPt(i);
        EXPECT_EQ(game1.threats[0].is_in_terr[i],
                  game2.threats[0].is_in_terr[i])
            << "  where i -> " << coord.showPt(i);
        EXPECT_EQ(game1.threats[1].is_in_terr[i],
                  game2.threats[1].is_in_terr[i])
            << "  where i -> " << coord.showPt(i);
        EXPECT_EQ(game1.threats[0].is_in_border[i],
                  game2.threats[0].is_in_border[i])
            << "  where i -> " << coord.showPt(i);
        EXPECT_EQ(game1.threats[1].is_in_border[i],
                  game2.threats[1].is_in_border[i])
            << "  where i -> " << coord.showPt(i);
        if (coord.dist[i] >= 0 and game1.isInTerr(i, 1) == 0 and
            game1.isInTerr(i, 2) == 0)
        {
            EXPECT_EQ(game1.threats[0].is_in_2m_encl[i],
                      game2.threats[0].is_in_2m_encl[i]);
            EXPECT_EQ(game1.threats[1].is_in_2m_encl[i],
                      game2.threats[1].is_in_2m_encl[i]);
            EXPECT_EQ(game1.threats[0].is_in_2m_miai[i],
                      game2.threats[0].is_in_2m_miai[i]);
            EXPECT_EQ(game1.threats[1].is_in_2m_miai[i],
                      game2.threats[1].is_in_2m_miai[i]);
        }
    }
}

TEST_P(IsometryFixture, inconsistencyInThreats2m_shouldBeRestrictedToInsideTerr)
{
    // it would be maybe better if threats2m were more consistent, but at least
    // they should be outside territories
    const unsigned isometry = GetParam();
    const std::string sgf_start{
        "(;SZ[20]PB[kropla:7000]PW[kropla_c1db66445b6:7000];B[kl];W[ki];B[lj]"
        ";W[mh];B[il];W[kj];B[kk];W[mj];B[li];W[lh];B[lk];W[mi];B[kh];W[ii]"
        ";B[kg];W[hj];B[nl];W[ig];B[jh];W[ih];B[lf];W[nf];B[ji];W[jj];B[me]"
        ";W[ne];B[md];W[nd];B[jk];W[ij];B[mc];W[mk];B[ml];W[ok];B[nk];W[nj]"
        ";B[oj];W[pj];B[oi];W[pi];B[oh];W[ng];B[ph];W[qh];B[pg];W[qg];B[pf]"
        ";W[qe];B[pe];W[pd];B[nc];W[od];B[ej];W[gl];B[hm];W[gm];B[gn];W[fn]"
        ";B[go];W[fo];B[gp];W[fj];B[ei];W[ek];B[ol];W[pk];B[fi];W[dk];B[gj]"
        ";W[hk];B[fk.ejfkgjfiej];W[fl];B[jp];W[jf];B[qm];W[kf];B[lg];W[pl]"
        ";B[pm];W[rk];B[rl];W[rf];B[sk];W[rj];B[ci];W[ql];B[ms];W[rm];B[sl]"
        ";W[sn];B[pp];W[qn];B[pn];W[qo];B[po];W[kr];B[js];W[jr];B[ir];W[ks]"
        ";B[kt];W[bh];B[lq];W[bj];B[dg];W[is];B[jt];W[iq];B[hr];W[bg];B[de]"
        ";W[ob];B[jq];W[lt];B[ls];W[cj];B[nb];W[ff];B[cf];W[nq];B[or];W[nr]"
        ";B[ns];W[fd];B[be];W[jc];B[dh];W[ke];B[hd];W[le];B[mf];W[hc];B[gc]"
        ";W[gd];B[ic];W[hb];B[ib];W[id];B[he];W[jb];B[ia];W[ie];B[no];W[oq]"
        ";B[pr];W[gb];B[fc];W[na];B[ma];W[ed];B[eb];W[pq];B[gg];W[hf];B[qp]"
        ";W[dd];B[gf];W[ge.gdgehfieidhcgd];B[qq];W[hs];B[gr];W[os];B[qr]"
        ";W[oc];B[bf];W[ef];B[sh];W[hl];B[im];W[df];B[cg];W[es];B[gq];W[rh]"
        ";B[bn];W[eq];B[dn];W[si];B[co];W[sf];B[sg];W[sc];B[ce];W[bp];B[an]"
        ";W[qb];B[gh];W[cp];B[ti];W[cm];B[cn];W[dj];B[eh];W[sj];B[ee];W[fe]"
        ";B[lb];W[lc];B[ld];W[kc];B[mb];W[dm];B[en];W[mg];B[em];W[bs];B[kd]"
        ";W[el];B[jd];W[ja];B[gs];W[gt];B[sd];W[rd];B[se];W[fm];B[ea];W[re]"
        ";B[rb];W[dp];B[fs];W[ft];B[er];W[dr];B[fr];W[ec];B[fb];W[ps];B[ds]"
        ";W[et];B[cs];W[cr];B[rc];W[mq];B[lr];W[qc];B[bl];W[sp];B[rp];W[ro]"
        ";B[kq.irjsktlslrkqjqir];W[bm];B[al];W[am];B[rg];W[tj];B[sm];W[rn]"
        ";B[sb];W[tc];B[aq];W[pa];B[bq];W[ch];B[di];W[sq];B[cq];W[dq];B[cl]"
        ";W[rs];B[fg];W[rr];B[fp];W[tr];B[br];W[eo];B[qs];W[db];B[qt];W[ss]"
        ";B[jg];W[bc];B[cb];W[ab];B[ca];W[da];B[je];W[if];B[cc];W[to];B[rq]"
        ";W[cd];B[eg];W[bd];B[ik];W[tf];B[dc];W[ck];B[bi];W[ao];B[bk];W[hi]"
        ";B[gi];W[dl];B[bb];W[aa];B[kb];W[ac];B[ep];W[ad];B[bo];W[ae];B[do]"
        ";W[ap];B[af];W[ag];B[ah];W[ai];B[aj];W[ak];B[ar];W[as];B[at];W[ba]"
        ";B[bt];W[ct];B[dt];W[fa];B[fq];W[ga];B[gk];W[ha];B[hg];W[hh];B[ht]"
        ";W[it];B[ka];W[la];B[mt];W[nt];B[oa];W[ot];B[pt];W[qa];B[ra];W[rt]"
        ";B[sa];W[st];B[ta];W[tb];B[td];W[te];B[tg];W[th];B[tk];W[tl];B[tm]"
        ";W[tn];B[tp];W[jo];B[tq];W[kn];B[in];W[lo];B[kp];W[mn];B[lm];W[om]"
        ";B[ll];W[hn];B[nm];W[nn];B[io];W[ho];B[ip];W[ts];B[tt];W[oo];B[mp]"
        ";W[lp];B[qi];W[qf];B[qj];W[qk];B[np];W[op.lolpmqnqopoonnmnlo]"};
    Game game1 = constructGameFromSgfWithIsometry(
        sgf_start + "B[jm];W[on];B[km])", isometry);
    Game game2 = constructGameFromSgfWithIsometry(
        sgf_start + "B[km];W[on];B[jm])", isometry);
    checkIfGamesAreSame(game1, game2);
}

TEST_P(IsometryFixture, deleteUnnecessaryThreats2)
{
    const unsigned isometry = GetParam();
    const std::string sgf_start{
        "(;SZ[20]PB[kropla_c1db66445b6:7000]PW[kropla:7000];B[kl];W[ki];B[lj];"
        "W[li];B[mj];W[nh];B[jj];W[kj];B[kk];W[ji];B[ij];W[mi];B[nj];W[jf];B["
        "ii];W[ih];B[hh];W[ig];B[oi];W[oh];B[pi];W[ph];B[qi];W[qh];B[ri];W[hg];"
        "B[gh];W[gl];B[jk];W[gg];B[fh];W[fg];B[gk];W[fk];B[gj];W[hl];B[fl];W["
        "fm];B[el];W[gn];B[ek];W[me];B[eh];W[pe];B[hk];W[rh];B[si];W[sh];B[re];"
        "W[qf];B[rf];W[em];B[dm];W[dn];B[cn];W[do];B[co];W[dp];B[cl];W[cp];B["
        "qe];W[pf];B[bp];W[bq];B[cq];W[dq];B[cr];W[dr];B[ds];W[es];B[cs];W[bo];"
        "B[ap];W[fr];B[gp];W[bn];B[cm];W[fp];B[gq];W[fq];B[go];W[fo];B[hn];W["
        "gr];B[io];W[im];B[jo];W[hm];B[hr];W[in];B[ho];W[hs];B[ir];W[is];B[jr];"
        "W[jn];B[kn];W[ko];B[kp];W[lo];B[km];W[gm];B[lp];W[mo];B[np];W[mp];B["
        "mq];W[ks];B[no];W[nq];B[mr];W[mn];B[nm];W[ns];B[nr];W[oq];B[or];W[nn];"
        "B[on];W[oo];B[ms];W[pp];B[kr];W[pn];B[om];W[pr];B[ps];W[qr];B[pm];W["
        "qn];B[qm];W[kb];B[rm];W[qs];B[nt];W[mm];B[ml];W[eg.mompnqoqppoonnmo];"
        "B[kd];W[lc];B[jc];W[hb];B[jb];W[ja];B[hc];W[gc];B[ib];W[ia];B[gb];W["
        "ha];B[gd];W[fc];B[fd];W[ed];B[ec];W[fb];B[ee];W[dd];B[de];W[hd];B[he];"
        "W[ic.gchdichbgc];B[fe];W[ce];B[df];W[dg];B[cf];W[ie];B[ld];W[md];B[cd]"
        ";W[dc];B[be.becfdecdbe];W[eb.dcedfcebdc];B[mc];W[lb];B[nc];W[ob];B[nd]"
        ";W[pd];B[ne];W[rs];B[na];W[nb];B[mb];W[ma];B[pb];W[oa];B[oc];W[pc];B["
        "qp];W[ro];B[rq];W[sq];B[sp];W[rp];B[so];W[dh];B[ft];W[et];B[tj];W[tk];"
        "B[qq];W[sr];B[sk];W[di];B[rn];W[ga.ooppoqprqrrssrsqrproqnpnoo."
        "fbgchbgafb];B[rg];W[ej];B[fj.ekflgkfjek];W[ls];B[tg];W[lr];B[lq];W[sl]"
        ";B[tl];W[tm];B[qg]"};
    Game game1 = constructGameFromSgfWithIsometry(
        sgf_start + ";W[pg];B[sm];W[id])", isometry);
    Game game2 = constructGameFromSgfWithIsometry(
        sgf_start + ";W[id];B[sm];W[pg])", isometry);
    checkIfGamesAreSame(game1, game2);
}

TEST_P(IsometryFixture, threatsIn1move)
{
    const unsigned isometry = GetParam();
    auto sgf = constructSgfFromGameBoard(
        "...x..."
        "..xox.."
        ".oo...."
        "ox....."
        "oxo...."
        ".ox...."
        "..oo...");
    const Game game = constructGameFromSgfWithIsometry(sgf, isometry);
    EXPECT_EQ(1, game.isInBorder(
                     coord.sgfToPti(applyIsometry("cd", isometry, coord)), 1));
    EXPECT_EQ(1, game.isInBorder(
                     coord.sgfToPti(applyIsometry("dd", isometry, coord)), 1));
    EXPECT_EQ(1, game.isInBorder(
                     coord.sgfToPti(applyIsometry("df", isometry, coord)), 1));
    EXPECT_EQ(1, game.isInBorder(
                     coord.sgfToPti(applyIsometry("dc", isometry, coord)), 2));

    EXPECT_EQ(3, game.isInBorder(
                     coord.sgfToPti(applyIsometry("bf", isometry, coord)), 1));
}

TEST_P(IsometryFixture, ladderEscape)
{
    const unsigned isometry = GetParam();
    auto sgf = constructSgfFromGameBoard(
        "...x..."
        "..xox.."
        "......."
        "......."
        "..o...."
        ".ox...."
        "..oo...");
    const Game game = constructGameFromSgfWithIsometry(sgf, isometry);
    const int ESC_WINS = -1;
    const auto next_att = coord.sgfToPti(applyIsometry("ef", isometry, coord));
    const auto next_def = coord.sgfToPti(applyIsometry("de", isometry, coord));
    EXPECT_EQ((std::make_tuple(ESC_WINS, next_att, next_def)),
              game.checkLadder(
                  2, coord.sgfToPti(applyIsometry("df", isometry, coord))));
    EXPECT_EQ(0, std::get<0>(game.checkLadder(
                     1, coord.sgfToPti(applyIsometry("dc", isometry, coord)))));
    EXPECT_EQ(0, std::get<0>(game.checkLadder(
                     1, coord.sgfToPti(applyIsometry("ee", isometry, coord)))));
}

TEST_P(IsometryFixture, ladderWorks)
{
    const unsigned isometry = GetParam();
    auto sgf = constructSgfFromGameBoard(
        "...x..."
        "..xo..."
        "..x...."
        "......."
        "..o..o."
        ".ox...."
        "..oo...");
    const Game game = constructGameFromSgfWithIsometry(sgf, isometry);
    const int ATT_WINS = 1;
    const auto next_att = coord.sgfToPti(applyIsometry("ef", isometry, coord));
    const auto next_def = coord.sgfToPti(applyIsometry("de", isometry, coord));
    EXPECT_EQ((std::make_tuple(ATT_WINS, next_att, next_def)),
              game.checkLadder(
                  2, coord.sgfToPti(applyIsometry("df", isometry, coord))));
}

TEST_P(IsometryFixture, complicatedLadderEscape)
{
    const unsigned isometry = GetParam();
    std::vector<std::string> positions{
        "......."
        "......."
        "......."
        "......."
        ".xo...."
        ".ox...."
        "..oo...",

        "......."
        "......."
        "......."
        "..x...."
        "..o...."
        ".ox...."
        "..oo...",

        "......."
        "......."
        "..xx..."
        ".xo...o"
        ".xo...."
        ".ox...."
        "..oo...",

        "......."
        "......."
        "......."
        "......."
        "..o.xo."
        ".ox...."
        "..oo...",

        "......."
        "......."
        ".....x."
        "......o"
        "..o...."
        ".ox...."
        "..oo..."};
    const int ESC_WINS = -1;
    const auto next_att = coord.sgfToPti(applyIsometry("ef", isometry, coord));
    const auto next_def = coord.sgfToPti(applyIsometry("de", isometry, coord));
    for (const auto& pos : positions)
    {
        auto sgf = constructSgfFromGameBoard(pos);
        const Game game = constructGameFromSgfWithIsometry(sgf, isometry);
        EXPECT_EQ((std::make_tuple(ESC_WINS, next_att, next_def)),
                  game.checkLadder(
                      2, coord.sgfToPti(applyIsometry("df", isometry, coord))));
    }
}

TEST_P(IsometryFixture, complicatedLadderCapture)
{
    const unsigned isometry = GetParam();
    std::vector<std::string> positions{
        "......."
        "......."
        "......."
        "..o...o"
        ".xo...."
        ".ox...."
        "..oo...",

        "......."
        "......."
        "......."
        "......o"
        "..o.xo."
        ".ox...."
        "..oo...",

        "......."
        "......."
        "......."
        ".....x."
        "..o..o."
        ".ox..x."
        "..oo..."};
    const int ATT_WINS = 1;
    const auto next_att = coord.sgfToPti(applyIsometry("ef", isometry, coord));
    const auto next_def = coord.sgfToPti(applyIsometry("de", isometry, coord));
    int nr = 0;
    for (const auto& pos : positions)
    {
        auto sgf = constructSgfFromGameBoard(pos);
        const Game game = constructGameFromSgfWithIsometry(sgf, isometry);
        EXPECT_EQ((std::make_tuple(ATT_WINS, next_att, next_def)),
                  game.checkLadder(
                      2, coord.sgfToPti(applyIsometry("df", isometry, coord))))
            << "Position number " << nr;
        ++nr;
    }
}

INSTANTIATE_TEST_CASE_P(Par, IsometryFixture,
                        ::testing::Values(0, 1, 2, 3, 4, 5, 6, 7));

}  // namespace
