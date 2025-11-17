/********************************************************************************************************
 kropla -- a program to play Kropki; file simplegame.cc -- main file.
    Copyright (C) 2025 Bartek Dyda,
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

#include "simplegame.h"

#include <algorithm>

/********************************************************************************************************
  Connections class
*********************************************************************************************************/
bool OneConnection::operator!=(const OneConnection &other) const
{
    for (int j = 0; j < 4; j++)
        if (other.groups_id[j] != groups_id[j]) return true;
    return false;
}

/// Finds unique groups in the neighbourhood (i.e., ug[i] != ug[j] for i!=j and
/// i,j<count_ug). Does not change ug[i] for i>=count_ug.
/// @param[out] unique_groups  The array where the unique groups' numbers are
/// stored.
/// @return                    The number of unique groups (count_ug).
int OneConnection::getUniqueGroups(std::array<pti, 4> &unique_groups) const
{
    if (groups_id[0] == 0) return 0;
    unique_groups[0] = groups_id[0];
    if (groups_id[1] == 0) return 1;
    int ug = 1;
    for (int g = 1; g < 4; g++)
    {
        if (groups_id[g] == 0) break;
        for (int j = 0; j < ug; j++)
            if (unique_groups[j] == groups_id[g]) goto was_saved;
        unique_groups[ug++] = groups_id[g];
    was_saved:;
    }
    return ug;
}

// ******************************************************

void SimpleGame::initWorm()
{
    // set outside points to MASK_DOT in worm[]
    for (int i = 0; i < coord.getSize(); i++)
    {
        if (coord.dist[i] < 0) worm[i] = MASK_DOT;
    }
}

void SimpleGame::reserveMemory()
{
    worm = std::vector<pti>(coord.getSize(), 0);
    nextDot = std::vector<pti>(coord.getSize(), 0);
    lastWormNo[0] = 1;
    lastWormNo[1] = 2;
    nowMoves = 1;
    connects[0] = std::vector<OneConnection>(coord.getSize(), OneConnection());
    connects[1] = std::vector<OneConnection>(coord.getSize(), OneConnection());
    initWorm();
    safety_soft.init(this);
}

bool SimpleGame::placeDot(int x, int y, int who, bool notInTerrOrEncl,
                          uint32_t atari_neighb_code, bool isInBorder_ind_who,
                          bool isInBorder_ind_opp, int &update_soft_safety)
{
    const pti ind = coord.ind(x, y);
    if (notInTerrOrEncl)
    {
        const bool is_in_terr_with_atari = false;
        history.push_back(ind, is_in_terr_with_atari, isInBorder_ind_who,
                          isInBorder_ind_opp, atari_neighb_code);
    }
    else
    {
        // check for an anti-reduction move, which would be bad for the opponent
        // Move would be bad, if all neighbours are 'who' dots, or all but one
        // empty point
        assert(coord.dist[ind] >= 1);  // because we're inside a threat
        int count = 0;
        for (int i = 0; i < 4; i++)
        {
            if (whoseDotMarginAt(ind + coord.nb4[i]) == 0)
            {
                count++;
            }
            else if (whoseDotMarginAt(ind + coord.nb4[i]) == (who ^ 3))
            {
                count = -1;
                break;
            }
        }
        const bool is_in_terr_with_atari = (count == 0 || count == 1);
        const bool is_in_encl_border = false;
        const bool is_in_opp_encl_border = false;
        history.push_back(ind, is_in_terr_with_atari, is_in_encl_border,
                          is_in_opp_encl_border, atari_neighb_code);
    }

    // if (mustSurround) { ... place the dot, do the necessary surrounds }
    pti numb[4];
    int count = 0;
    for (int i = 0; i < 4; i++)
    {
        pti nb = ind + coord.nb4[i];
        if (whoseDotMarginAt(nb) == who)
        {
            // check if it was already saved
            for (int j = 0; j < count; j++)
                if (numb[j] == worm[nb]) goto AlreadyThere;
            numb[count++] = worm[nb];
        AlreadyThere:;
        }
    }

    bool update_safety_dame =
        false;  // if some worms start/stop being safe, we need to recalculate
                // dame moves on the first line
    update_soft_safety =
        0;  // if some worms gained hard-safety (from 0 to +1/+2 or from +1 to
            // +2), then we need to recalculate soft safety
    bool nonisolated = (count > 0);
    if (count)
    {
        while (count >= 2)
        {
            // glue the smaller to the larger worm
            if ((descr.at(numb[count - 1]).safety <= 1 ||
                 descr.at(numb[count - 2]).safety <= 1) and
                (descr.at(numb[count - 1]).safety +
                     descr.at(numb[count - 2]).safety >=
                 2))
            {
                update_safety_dame = true;
            }
            if ((descr.at(numb[count - 1]).safety <= 1 ||
                 descr.at(numb[count - 2]).safety <= 1) and
                (descr.at(numb[count - 1]).safety +
                     descr.at(numb[count - 2]).safety >=
                 1))
            {
                update_soft_safety = safety_soft.getUpdateValueForAllMargins();
            }
            if (descr.at(numb[count - 1]).dots[0] +
                    descr.at(numb[count - 1]).dots[1] >
                descr.at(numb[count - 2]).dots[0] +
                    descr.at(numb[count - 2]).dots[1])
            {
                wormMergeSame(numb[count - 1], numb[count - 2]);
                numb[count - 2] = numb[count - 1];
            }
            else
            {
                wormMergeSame(numb[count - 2], numb[count - 1]);
            }
            count--;
        }
        // add our dot
        pti next = nextDot[descr.at(numb[0]).leftmost];
        nextDot[descr.at(numb[0]).leftmost] = ind;
        nextDot[ind] = next;
        worm[ind] = numb[0];
        descr.at(numb[0]).leftmost = std::min(descr.at(numb[0]).leftmost, ind);
        descr.at(numb[0]).dots[who - 1]++;
    }
    else
    {
        // 'isolated' dot (note: new enclosure is possible also here due to
        // diagonal connections)
        assert(who == 1 || who == 2);
        pti c = (lastWormNo[who - 1] += CONST_WORM_INCR);
        assert(descr.find(c) == descr.end());
        worm[ind] = c;
        nextDot[ind] = ind;
        auto &dsc = descr[c];
        // WormDescr dsc;
        dsc.dots[0] = (who == 1);
        dsc.dots[1] = (who == 2);
        dsc.leftmost = ind;
        dsc.group_id = c;
        dsc.safety = 0;
        // descr.insert({c, std::move(dsc)});
    }
    // update safety info
    {
        int dist = coord.dist[ind];
        if (dist == 0)
        {
            if (descr.at(worm[ind]).safety == 1)
            {
                update_soft_safety =
                    safety_soft
                        .getUpdateValueForAllMargins();  // we made something
                                                         // completely secure
            }
            descr.at(worm[ind]).safety = WormDescr::SAFE_VALUE;
            for (int i = 0; i < 4; i++)
            {
                pti nb = ind + coord.nb4[i];
                if (coord.dist[nb] == 1 and worm[nb])
                {
                    descr.at(worm[nb])
                        .safety--;  // it may happen that worm[nb]==worm[ind],
                                    // but we may safely decrease SAFE_VALUE by
                                    // one
                    if (descr.at(worm[nb]).safety == 1)
                    {
                        update_safety_dame =
                            true;  // worm at nb stopped being safe
                        update_soft_safety =
                            safety_soft.getUpdateValueForAllMargins();
                    }
                    break;
                }
            }
            if (update_soft_safety != safety_soft.getUpdateValueForAllMargins())
            {
                update_soft_safety =
                    safety_soft.getUpdateValueForMarginsContaining(
                        ind);  // needed when played on the edge
            }
        }
        else if (dist == 1)
        {
            bool was_unsafe_and_nonisolated =
                nonisolated and !descr.at(worm[ind]).isSafe();
            auto safety_level_before = descr.at(worm[ind]).safety;
            for (int i = 0; i < 4; i++)
            {
                pti nb = ind + coord.nb4[i];
                if (coord.dist[nb] == 0 and worm[nb] == 0)
                {
                    descr.at(worm[ind])
                        .safety++;  // without break, because of the 4 corner
                                    // places (1,1), etc. which are close to the
                                    // edge from 2 sides
                }
            }
            if (was_unsafe_and_nonisolated and descr.at(worm[ind]).isSafe())
                update_safety_dame = true;
            if (nonisolated and safety_level_before < 2 and
                safety_level_before < descr.at(worm[ind]).safety)
            {
                update_soft_safety = safety_soft.getUpdateValueForAllMargins();
            }
            else
            {
                if (update_soft_safety !=
                    safety_soft.getUpdateValueForAllMargins())
                {
                    update_soft_safety =
                        safety_soft.getUpdateValueForMarginsContaining(
                            ind);  // needed when played on the edge
                }
            }
        }
    }

    // check diag neighbours
    pti cm[4];
    int top = 0;
    pti our_group_id = descr.at(worm[ind]).group_id;
    for (int i = 0; i < 8; i += 2)
    {
        pti nb = ind + coord.nb8[i];
        if (whoseDotMarginAt(nb) == who and worm[nb] != worm[ind])
        {
            // connection!
            if (descr.at(worm[nb]).group_id != our_group_id)
            {
                for (int j = 0; j < top; j++)
                {
                    if (cm[j] == descr.at(worm[nb]).group_id) goto check_ok;
                }
                cm[top++] = descr.at(worm[nb]).group_id;
            check_ok:;
            }
            // add to neighbours if needed
            if (std::find(descr.at(worm[ind]).neighb.begin(),
                          descr.at(worm[ind]).neighb.end(),
                          worm[nb]) == descr.at(worm[ind]).neighb.end())
            {
                descr.at(worm[ind]).neighb.push_back(worm[nb]);
                descr.at(worm[nb]).neighb.push_back(worm[ind]);
            }
        }
    }
    while (top >= 1)
    {
        for (auto &d : descr)
        {
            if (d.second.group_id == cm[top - 1])
                d.second.group_id = our_group_id;
        }
        connectionsRenameGroup(our_group_id, cm[top - 1]);
        top--;
    }

    connectionsRecalculateNeighb(ind, who);

    return update_safety_dame;
}

void SimpleGame::wormMergeAny(pti dst, pti src)
// merge worm number 'src' to 'dst',
{
    if ((dst & MASK_DOT) == (src & MASK_DOT))
        wormMergeSame(dst, src);
    else
        wormMergeOther(dst, src);
}

void SimpleGame::wormMergeOther(pti dst, pti src)
// note: removing (src) may disconnect some other worms,
//  but this is NOT recalculated. So after removing all enemy worms
//  one must check again which are connected. However, neighbour's list is
//  updated.
{
    if ((dst & MASK_DOT) == 1)
    {
        score[0].dots += descr.at(src).dots[1];
        score[1].dots -= descr.at(src).dots[0];
    }
    else
    {
        score[0].dots -= descr.at(src).dots[1];
        score[1].dots += descr.at(src).dots[0];
    }
    // remove (src) from all its former neighbours
    for (auto n : descr.at(src).neighb)
    {
        descr.at(n).neighb.erase(std::find(descr.at(n).neighb.begin(),
                                           descr.at(n).neighb.end(), src));
    }
    wormMerge_common(dst, src);
}

void SimpleGame::wormMergeSame(pti dst, pti src)
{
    WormDescr &descr_src = descr.at(src);
    WormDescr &descr_dst = descr.at(dst);
    for (auto n : descr_src.neighb)
    {
        if (n == dst ||  // remove (src) from (n)==(dst)'s neighbours, note: src
                         // is not always a neighbour of dst
            std::find(descr_dst.neighb.begin(), descr_dst.neighb.end(), n) !=
                descr_dst.neighb.end())
        {  // n was already a neighbour of (dst), so just remove (src) as n's
           // neighbour
            descr.at(n).neighb.erase(std::find(descr.at(n).neighb.begin(),
                                               descr.at(n).neighb.end(), src));
        }
        else
        {
            // n!=dst was not a neighbour of (dst), so replace (src) to (dst) as
            // n's neighbour and add (n) as a new dst's neighbour
            std::replace(descr.at(n).neighb.begin(), descr.at(n).neighb.end(),
                         src, dst);
            descr_dst.neighb.push_back(n);
        }
    }
    // if (dst) and (src) were in different groups, merge them
    if (descr_src.group_id != descr_dst.group_id)
    {
        pti old_gid = descr_src.group_id;
        pti new_gid = descr_dst.group_id;
        for (auto &d : descr)
        {
            if (d.second.group_id == old_gid) d.second.group_id = new_gid;
        }
        connectionsRenameGroup(new_gid, old_gid);
    }
    // common part
    wormMerge_common(dst, src);
}

void SimpleGame::wormMerge_common(pti dst, pti src)
// common part of MergeOther and MergeSame
{
    WormDescr &descr_src = descr.at(src);
    WormDescr &descr_dst = descr.at(dst);
    pti leftmost = descr_src.leftmost;
    pti x = leftmost;
    do
    {
        worm[x] = dst;
        x = nextDot[x];
    } while (x != leftmost);
    descr_dst.dots[0] += descr_src.dots[0];
    descr_dst.dots[1] += descr_src.dots[1];
    pti n = nextDot[descr_dst.leftmost];
    nextDot[descr_dst.leftmost] = nextDot[descr_src.leftmost];
    nextDot[descr_src.leftmost] = n;
    descr_dst.leftmost = std::min(descr_dst.leftmost, leftmost);
    descr_dst.safety +=
        descr_src
            .safety;  // note: this is possible only when safety has at least 32
                      // bits, otherwise we should check for overflow
    descr.erase(src);
}

/// Checks if empty points p1, p2 are connected, i.e., if they both have the
/// same group of who as a neighbour.
bool SimpleGame::haveConnection(pti p1, pti p2, int who) const
{
    std::array<pti, 4> unique_groups1 = {0, 0, 0, 0};
    int ug1 = connects[who - 1][p1].getUniqueGroups(unique_groups1);
    if (ug1 == 0) return false;
    std::array<pti, 4> unique_groups2 = {0, 0, 0, 0};
    int ug2 = connects[who - 1][p2].getUniqueGroups(unique_groups2);
    if (ug2 == 0) return false;
    for (int i = 0; i < ug1; i++)
    {
        for (int j = 0; j < ug2; j++)
            if (unique_groups1[i] == unique_groups2[j]) return true;
    }
    return false;
}

void SimpleGame::connectionsRenameGroup(pti dst, pti src)
{
    int g = (dst & MASK_DOT) - 1;
    assert(g == 0 || g == 1);
    for (auto &p : connects[g])
    {
        for (int j = 0; j < 4; j++)
            if (p.groups_id[j] == src) p.groups_id[j] = dst;
    }
}

void SimpleGame::connectionsRecalculateCode(pti ind, int who)
{
    // TODO: we could use pattern3 and delete codes completely
    if (!isDotAt(ind))
    {
        int code = 0;  // the codes for the neighbourhood for player who
        for (int i = 7; i >= 0; i--)
        {
            pti nb = ind + coord.nb8[i];
            if (whoseDotMarginAt(nb) == who)
            {
                code |= 1;
            }
            code <<= 1;
        }
        code >>= 1;
        connects[who - 1][ind].code = code;
    }
    else
    {
        connects[who - 1][ind].code = 0;
    }
}

void SimpleGame::connectionsRecalculateConnect(pti ind, int who)
{
    int g = who - 1;
    auto new_connections = coord.connections_tab_simple[connects[g][ind].code];
    for (int j = 0; j < 4; j++)
    {
        if (new_connections[j] >= 0)
        {
            pti pt = ind + coord.nb8[new_connections[j]];
            connects[g][ind].groups_id[j] = descr.at(worm[pt]).group_id;
        }
        else
        {
            connects[g][ind].groups_id[j] = 0;
        }
    }
}

void SimpleGame::connectionsRecalculatePoint(pti ind, int who)
{
    connectionsRecalculateCode(ind, who);
    connectionsRecalculateConnect(ind, who);
}

// Used after placing dot of who at [ind].
void SimpleGame::connectionsRecalculateNeighb(pti ind, int who)
{
    for (int i = 0; i < 8; i++)
    {
        pti nb = ind + coord.nb8[i];
        if (whoseDotMarginAt(nb) == 0)
        {
            connects[who - 1][nb].code |= (1 << (i ^ 4));
            // connectionsRecalculatePoint(nb, who);
            connectionsRecalculateConnect(nb, who);
        }
    }
    connectionsRecalculatePoint(ind, 1);
    connectionsRecalculatePoint(ind, 2);
}

void SimpleGame::connectionsReset(pti ind, int who)
{
    connects[who - 1][ind] = OneConnection();
}

void SimpleGame::findConnections()
{
    for (int x = 0; x < coord.wlkx; x++)
    {
        pti ind = coord.ind(x, 0);
        for (int y = 0; y < coord.wlky; y++)
        {
            connectionsRecalculatePoint(ind, 1);
            connectionsRecalculatePoint(ind, 2);
            ind += coord.S;
        }
    }
}
