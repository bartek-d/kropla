/********************************************************************************************************
 kropla -- a program to play Kropki; file simplegame.h.
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

#pragma once
#include <array>
#include <cstdint>
#include <set>
#include <unordered_map>
#include <vector>

#include "../3rdparty/short_alloc.h"
#include "board.h"
#include "dfs.h"
#include "history.h"
#include "safety.h"

namespace krb
{
typedef std::set<pti, std::greater<pti>> PointsSet;
template <class T, std::size_t ElemSize = 200>
using SmallVector =
    std::vector<T, short_alloc<T, ElemSize * sizeof(T), alignof(T)>>;
}  // namespace krb

/********************************************************************************************************
  Worm description class
*********************************************************************************************************/

struct WormDescr
{
    pti dots[2];   // number of dots of players
    pti leftmost;  // the dot of the worm with the smallest index (the top one
                   // from the leftmost)
    pti group_id;  // some positive number which is common for worms in the same
                   // group (i.e., connected) and different otherwise
    int32_t safety;  // safety info, ==0 not safe, ==1 partially safe, >=2 safe
    bool isSafe() const { return safety >= 2; }
    const static int32_t SAFE_VALUE =
        20000;  // safety := SAFE_VALUE when the worm touches the edge
    const static int32_t SAFE_THRESHOLD = 10000;
    krb::SmallVector<pti, 6>::allocator_type::arena_type arena_neighb;
    krb::SmallVector<pti, 6> neighb{
        arena_neighb};  // numbers of other worms that touch this one
    std::string show() const;
    WormDescr(const WormDescr& other)
        : leftmost{other.leftmost},
          group_id{other.group_id},
          safety{other.safety},
          neighb{other.neighb, arena_neighb}
    {
        dots[0] = other.dots[0];
        dots[1] = other.dots[1];
    }
    WormDescr() = default;
    WormDescr& operator=(const WormDescr&) = delete;
    WormDescr& operator=(WormDescr&&) = delete;
    ~WormDescr() = default;
};

/********************************************************************************************************
  Connections class
*********************************************************************************************************/
struct OneConnection
{
    std::array<pti, 4> groups_id{
        0, 0, 0, 0};  // id's of connected groups, the same may appear more than
                      // once, 0-filled at the end if necessary
    int code{0};      // code of the neighbourhood used by coord.connections_tab
    // int() and != are mainly for debugging, to print and check connections
    operator int() const
    {
        return (groups_id[0] != 0) + (groups_id[1] != 0) + (groups_id[2] != 0) +
               (groups_id[3] != 0);
    }
    bool operator!=(const OneConnection& other) const;
    // returns the number of groups in the neighbourhood
    int count() const
    {
        int count = 0;
        while (count < 4 && groups_id[count] != 0) count++;
        return count;
    }
    bool contains(pti what) const
    {
        return (groups_id[0] == what) or (groups_id[1] == what) or
               (groups_id[2] == what) or (groups_id[3] == what);
    }
    int getUniqueGroups(std::array<pti, 4>& ug) const;
};

struct SimpleGame
{
    std::vector<pti> worm;
    std::vector<pti> nextDot;
    std::unordered_map<pti, WormDescr> descr;

    Score score[2];
    int lastWormNo[2];  // lastWormNo used in worm, for players 1,2
    int nowMoves;       // =1 or 2
    std::array<ImportantRectangle, 2> rectangle{};
    History history{};

    Safety safety_soft;

    // worm[] has worm-id if it is >= 4 && <= MASK_WORM_NO,
    // worm[] & MASK_DOT can have 4 values: 0=empty, 1,2 = dots, 3=point outside
    // the board
    static const int MASK_DOT = 3;
    static const int CONST_WORM_INCR = 4;  // =MASK_DOT+1
    static const int MASK_WORM_NO = 0xfff;
    static const int MASK_MARK = 0x2000;    // used by some functions
    static const int MASK_BORDER = 0x4000;  //
    //
    bool isDotAt(pti ind) const
    {
        assert(worm[ind] >= 0 && worm[ind] <= MASK_WORM_NO);
        return (worm[ind] >= CONST_WORM_INCR);
    }
    int whoseDotMarginAt(pti ind) const { return (worm[ind] & MASK_DOT); }
    int whoseDotAt(pti ind) const
    {
        int v[4] = {0, 1, 2, 0};
        return v[worm[ind] & MASK_DOT];
    }
    int getSafetyOf(pti ind) const { return descr.at(worm[ind]).safety; }

    const History& getHistory() const { return history; }
    void updateGoodReplies(int lastWho, float abs_value)
    {
        history.updateGoodReplies(lastWho, abs_value);
    }
    void setEnclosureInLastMove() { history.setEnclosureInLastMove(); }
    void xorAtariNeigbhCodeInLastMove(uint32_t atari_neighb_code)
    {
        history.xorAtariNeigbhCodeInLastMove(atari_neighb_code);
    }

    void initWorm();
    void reserveMemory();
    int whoNowMoves() const { return nowMoves; };

    float getTotalSafetyOf(pti ind) const
    {
        return whoseDotAt(ind)
                   ? descr.at(worm[ind]).safety + safety_soft.getSafetyOf(ind)
                   : 0.0f;
    }

    bool placeDot(int x, int y, int who, bool notInTerrOrEncl,
                  uint32_t atari_neighb_code, bool isInBorder_ind_who,
                  bool isInBorder_ind_opp, int& update_soft_safety);

    const std::vector<OneConnection>& getConnects(int ind) const
    {
        return connects[ind];
    }

    void connectionsRecalculateCode(pti ind, int who);
    void connectionsRecalculateConnect(pti ind, int who);
    void connectionsRecalculatePoint(pti ind, int who);
    void connectionsRecalculateNeighb(pti ind, int who);
    void connectionsRenameGroup(pti dst, pti src);
    void connectionsReset(pti ind, int who);

    bool haveConnection(pti p1, pti p2, int who) const;

    void wormMergeSame(pti dst, pti src);
    void wormMergeOther(pti dst, pti src);

    void findConnections();

   private:
    std::vector<OneConnection> connects[2];

    void wormMergeAny(pti dst, pti src);
    void wormMerge_common(pti dst, pti src);
};
