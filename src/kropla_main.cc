/********************************************************************************************************
 kropla -- a program to play Kropki; file kropla_main.cc -- main kropla file.
    Copyright (C) 2015,2016,2017,2018,2019,2020 Bartek Dyda,
    email: bartekdyda (at) protonmail (dot) com

    Some parts are inspired by Pachi http://pachi.or.cz/
      by Petr Baudis and Jean-loup Gailly

    This file is part of Kropla.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************************************************/
#include <fstream>
#include <iostream>
#include <string>

#include "game.h"
#include "montecarlo.h"
#include "sgf.h"

/* sample sgf */

std::string sgf185253(
    "(;FF[4]GM[40]CA[UTF-8]AP[zagram.org]SZ[30]RU[Punish=0,Holes=1,AddTurn=0,"
    "MustSurr=0,MinArea=0,Pass=0,Stop=0,LastSafe=0,ScoreTerr=0,InstantWin=15]"
    "EV[Wielki Turniej Pamięci Arosa 2013]RO[2 (faza "
    "grupowa)]PB[michals]PW[grzesiek2f]TM[180]OT[10]DT[2013-11-11]RE[W+11]BR["
    "1505]WR[1334];B[oo];W[pm];B[nn];W[qp];B[pp];W[pn];B[pq];W[nl];B[qq];W[rp];"
    "B[rq];W[tp];B[sp];W[so];B[sq];W[rn];B[to];W[tn];B[uo];W[uq];B[un];W[tm];B["
    "um];W[po];B[op];W[vq];B[ul];W[ts];B[rt];W[rs];B[qs];W[st];B[rr];W[ss];B["
    "qt];W[sv];B[ru];W[tw];B[rv];W[rw];B[qw];W[rx];B[qx];W[wo];B[tk];W[xm];B["
    "nm];W[ol];B[ml];W[nj];B[sj];W[mk];B[ll];W[kj];B[kk];W[lk];B[kl];W[jj];B["
    "ri];W[ls];B[js];W[ku];B[kt];W[kp];B[ip];W[mu];B[lt];W[mt];B[lu];W[mv];B["
    "lv];W[mw];B[ms];W[ns];B[mr];W[ot];B[nr];W[lx];B[os];W[nt];B[pt];W[jn];B["
    "jo];W[il];B[kn];W[jm];B[km];W[ik];B[kw];W[kx];B[jw];W[iu];B[jx];W[kz];B["
    "jy];W[lA];B[jz];W[jA];B[gu];W[hr];B[ht];W[is];B[it];W[hq];B[iq];W[ir];B["
    "jr];W[fr];B[iA];W[ho];B[hp];W[gp];B[io];W[hn];B[ko];W[fo];B[ry];W[sy];B["
    "sz];W[qy];B[rz];W[py];B[ty];W[sx];B[tx];W[sw];B[uz];W[ox];B[tr];W[vs];B["
    "ur];W[vw];B[us];W[su];B[vr];W[wq];B[wr];W[xp];B[xr];W[qv];B[yq];W[zo];B["
    "zp];W[yn];B[Ao];W[xk];B[qh];W[Ap];B[zn];W[yo];B[An];W[zq];B[yp];W[Bq];B["
    "vt];W[At];B[ys];W[zu];B[zt];W[zs];B[yt];W[zr];B[yr];W[Bu];B[yu];W[zv];B["
    "yv];W[yw];B[xw];W[yx];B[xx];W[xy];B[wy];W[pw.oxpyqyrxrwqvpwox];B[wv];W[ux]"
    ";B[uy];W[wx];B[vy];W[vx];B[xi];W[yj];B[wj];W[wk];B[vk];W[Aj];B[Bm];W[Bk];"
    "B[Cl];W[Ck];B[yi];W[zi];B[xj];W[yl];B[pg];W[ne];B[of];W[nf];B[ng];W[ug];B["
    "ue];W[wf];B[vf];W[wg];B[vg];W[wh];B[vh];W[wi];B[vi];W[mg];B[nh];W[li];B["
    "oe];W[nd];B[od];W[nc];B[oc];W[yf];B[we];W[dc];B[dg];W[eg];B[eh];W[fh];B["
    "ef];W[fg];B[di];W[ei];B[dh];W[ej];B[ff];W[gf];B[id];W[hc];B[jb];W[ic];B["
    "jc];W[jd];B[kd];W[je];B[ke];W[jf];B[kf];W[kg];B[kc];W[lf];B[em];W[fm];B["
    "el];W[fl];B[ek];W[fk];B[dj];W[gi];B[en];W[df];B[xe];W[zg];B[xf];W[xg];B["
    "vj];W[zh];B[Al];W[zl];B[Bl];W[Ak];B[ey];W[fw];B[ev];W[fz];B[fy];W[gz];B["
    "gy];W[hz];B[hy];W[iz];B[iB];W[jB];B[iC];W[jC];B[eB];W[eA];B[dA];W[gC];B["
    "hC];W[gB];B[ez];W[fA];B[dC];W[fC];B[cv];W[cz];B[cA];W[by];B[cx];W[dz];B["
    "du];W[et];B[fu];W[cs];B[bw];W[bx];B[cw];W[qB];B[xz];W[yy];B[tB];W[pC];B["
    "qA];W[pA];B[rB];W[qz];B[rA];W[rC];B[sC];W[uC];B[uB];W[wB];B[vC];W[tC];B["
    "xC];W[sD];B[wz];W[sB.rCsDtCsBrC];B[tz];W[vB];B[yz];W[wC];B[vD];W[xB];B[zz]"
    ";W[Aw];B[Ay];W[Ax];B[By];W[Cw];B[Bp];W[Aq];B[Cp];W[Cq];B[ye];W[ze];B[zd];"
    "W[sc];B[sd];W[td];B[te];W[rd];B[se];W[qc];B[re];W[qe];B[qf];W[tb];B[ud];W["
    "tc];B[uc];W[yc];B[yd];W[xc];B[wc];W[wb];B[vb];W[ub];B[xd];W[va];B[vc];W["
    "Ae];B[Ad];W[Ce];B[xb];W[yb];B[wa.vbwcxbwavb];W[yC];B[xD];W[Cz];B[AB];W[BC]"
    ";B[zC];W[yB];B[AC];W[AA];B[zA];W[BA];B[zB];W[zy];B[Az];W[cp];B[eo];W[ep];"
    "B[dt];W[ds];B[es];W[er];B[fs];W[gs];B[gt.dtduevfugtfsesdt];W[do];B[hs];W["
    "gr];B[ck];W[cl];B[bk];W[de];B[bm];W[oi];B[pj];W[oh];B[og];W[qk];B[qj];W["
    "rk];B[oj];W[nk];B[ni];W[mi];B[mh];W[lh];B[sl];W[rm];B[sm];W[sn];B[rl];W["
    "ql];B[pd];W[qd];B[bh];W[tl];B[sk];W[vl];B[uk];W[vm];B[vn];W[wn];B[vp];W["
    "wp];B[Dn];W[Do];B[Co];W[Dm];B[Cm];W[Dt];B[Ds];W[Cs];B[Dr];W[Cr];B[Du];W["
    "Cu];B[Dv];W[Cv];B[vv];W[uw];B[uv];W[uu];B[vu];W[ut];B[pu];W[ou];B[pv];W["
    "ov];B[mD];W[mC];B[lD];W[lC];B[ky];W[ly];B[nD];W[nC];B[oD];W[oC];B[kD];W["
    "cn];B[cm];W[fn];B[fc];W[fd];B[ec];W[ed];B[gc];W[gd];B[hb];W[ib];B[ia];W["
    "hd];B[gb];W[lb];B[ja];W[pb];B[nb];W[ob];B[mb];W[le];B[lc];W[lg];B[la];W["
    "ka];B[kb.kblcmblakb];W[ld];B[mc];W[cg];B[ch];W[dm];B[dk];W[dn];B[dl];W[al]"
    ";B[bl.blcmdlckbl];W[bg];B[be];W[ce];B[bd];W[cd];B[Dh];W[Ch];B[yh];W[yg];B["
    "Di];W[Ci];B[Dj];W[Cj];B[Dg];W[Cg];B[Df];W[Cf];B[Bd];W[Be];B[zm];W[bt];B["
    "bq];W[cq];B[br];W[bu];B[aw];W[cr];B[md];W[me];B[cy];W[bz];B[dy];W[iy];B["
    "ix];W[aj];B[bj];W[ai];B[bi];W[ra];B[sa];W[sb];B[qa];W[qb];B[Bx];W[Bw];B["
    "vA];W[sA];B[tA];W[eC];B[dB];W[on];B[pk];W[pl];B[kC];W[kB];B[tv];W[tu];B["
    "qD];W[qC];B[in];W[im];B[bf];W[bc];B[cb];W[cc];B[pe];W[jq];B[kr];W[lq];B["
    "lr];W[no];B[mp];W[mo];B[zb.ipiqjrkrlrmrnrosptqsrrqqppoonnnmmlllkmknjoip];"
    "W[ya];B[zc];W[Bb];B[Ab];W[Bc];B[Cd];W[ww];B[xv];W[hA];B[hB];W[fB];B[bo];W["
    "co];B[lw];W[sr];B[tq];W[up];B[vo];W[rj];B[jk];W[ij];B[av];TB[."
    "chbibjckdjdich.ducvcwcxdyeyfygyhyixjwkwlvluktjsithtgufuevdu."
    "krjsktltmslrkr.ognhniojpjqjriqhpgog.peofpgqfpe."
    "qfpgqhrisjtkukvjvivhvgvfuetesereqf.uztAuBvAuz.vcuduevfwexdwcvc."
    "vrusvtvuwvxvyuytysxrwrvr.vyuzvAwzvy.AlzmAnBmAl.BmAnAoBpCoDnCmBm]TW[."
    "dccddeeddc.docpcqcrdserfrgrhqgpfoepdo."
    "eddedfegfgfheiejfkflfmfnfogphohnimilikijjjkjlilhkgjfjejdichdgdfded."
    "gzfAgBhAgz.kzjAkBlAkz.likjlkmknjmili."
    "lykzlAkBlCmCnCoCpCqBpAqzpyoxpwovountmumvmwlxly.melfmgnfme."
    "qlpmpnpoqprpsornrmql.raqbqcrdscsbra.tssttuutts.wkvlvmwnxmylxkwk."
    "xmwnwoxpyoynxm.yjxkylzlAkAjziyj.zeyfzgzhziAjBkCjCiChCgCfBeAeze."
    "zvywyxzyAxAwzv.AqzrzsAtBuCuDtCsCrBqAq.AtzuzvAwBwCvBuAt])");

int main(int argc, char* argv[])
{
    auto getDirectory = [](std::string s)
    {
      return s.substr(0, s.find_last_of('/')+1);
    };
    global::program_path = getDirectory(argv[0]);
    std::string s(sgf185253);
    enum class Mode
    {
        play,
        sgf_move,
        interactive
    } mode;
    if (argc > 1)
    {
        std::string name(argv[1]);
        if (name == "-")
        {
            std::string buf;
            s = "";
            do
            {
                std::getline(std::cin, buf);
                s += buf;
            } while (s.find(")") == std::string::npos);
            mode = Mode::play;
            std::cerr << "Parameter: " << s << std::endl;
        }
        else if (name == "--help")
        {
            std::cerr << R"raws(Usage:
  kropla
    enters interactive mode (play with AI with text board)

  kropla filename [move_number [iterations [threads]]]
    makes one move in the position given in the sgf file 'filename' and then quits,
    if move_number is present, takes the position in the main variation after 'move_number' moves or after the last move, if move_number is larger,
     if iterations is present, runs that many Monte Carlo iterations if it is >=0, uses CNN only if negative,
      if threads is present, uses that many threads for Monte Carlo simulations.

  kropla - [move_number [iterations [threads [msec [komi]]]]]
    as above, but takes the sgf from stdin and does not quit after one move, but rather waits for
    the next moves in the game; this is for use with Kropki program.
    If msec is present and threads>1, 'think' for at most (msec); msec==0 means no time limit.
    If komi is present, sets initial komi to that value. Otherwise leaves default 0 value.
)raws";
            return 0;
        }
        else
        {
            std::ifstream t(argv[1]);
            std::stringstream buffer;
            buffer << t.rdbuf();
            s = buffer.str();
            mode = Mode::sgf_move;
        }
    }
    else
    {  // no parameters = interactive mode
        mode = Mode::interactive;
        s = "(;FF[4]GM[40]CA[UTF-8]AP[kropla]SZ[15]RU[Punish=0,Holes=1,AddTurn="
            "0,MustSurr=0,MinArea=0,Pass=0,Stop=0,LastSafe=0,ScoreTerr=0,"
            "InstantWin=15])";  // no sgf; interactive mode";
    }
    // for debug, save the sgf we have read
#ifndef SPEED_TEST
    {
        std::ofstream ofs("debug_mc.txt", std::ofstream::app);
        ofs << "-----------------------------" << std::endl;
        ofs << s << std::endl;
    }
#endif

    SgfParser parser(s);
    auto seq = parser.parseMainVar();
    Game game(seq, (argc > 2 ? std::atoi(argv[2]) : 2000));
    auto end_time = std::chrono::high_resolution_clock::now();
    std::cerr << "init time: "
              << std::chrono::duration_cast<std::chrono::microseconds>(
                     end_time - start_time)
                     .count()
              << " micros" << std::endl;
    //  std::cerr << "Enclosures: obowiazkowe: " << encl_count << ", opcjonalne:
    //  " << opt_encl_count << ", laczna wlk priority: " << priority_count << ",
    //  liczba ruchow: " << moves_count << std::endl;
#ifndef NDEBUG
    game.show();
#endif

    int iter_count = (argc > 3) ? std::atoi(argv[3]) : 2000;
    int threads_count = (argc > 4) ? std::atoi(argv[4]) : 3;
    int msec = (argc > 5) ? std::atoi(argv[5]) : 0;
    int komi = (argc > 6) ? std::atoi(argv[6]) : 0;
    global::komi = komi;

    switch (mode)
    {
        case Mode::play:
            play_engine(game, s, threads_count, iter_count, msec);
            break;
        case Mode::sgf_move:
            findAndPrintBestMove(game, threads_count, iter_count);
            break;
        case Mode::interactive:
            playInteractively(game, threads_count, iter_count);
            break;
    }

#ifdef DEBUG_SGF
    std::cerr.flush();
    std::cout.flush();
    std::cout << std::endl << "Sgf:" << std::endl << game.getSgf() << std::endl;
#endif

#ifndef SPEED_TEST
    if (!game.checkCorrectness(seq))
    {
        std::cerr << "Error" << std::endl;
        game.show();
    }

#endif
}
