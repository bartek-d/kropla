/********************************************************************************************************
 kropla -- a program to play Kropki; file history.cc.
    Copyright (C) 2022 Bartek Dyda,
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

#include "history.h"

#include <unordered_map>

History::History() : history{0, 0} {}

void History::push_back(u32 ind, bool terr, bool encl_border,
                        bool opp_encl_border, uint32_t atari_neighb_code)
{
    history.push_back(ind | (terr ? HISTORY_TERR : 0) |
                      (encl_border ? HISTORY_ENCL_BORDER : 0) |
                      (opp_encl_border ? HISTORY_OPP_ENCL_BORDER : 0) |
                      (atari_neighb_code << HISTORY_ATARI_CODE_SHIFT));
}

void History::setEnclosureInLastMove() { history.back() |= HISTORY_ENCL_MOVE; }

void History::xorAtariNeigbhCodeInLastMove(uint32_t atari_neighb_code)
{
    history.back() ^= (atari_neighb_code << HISTORY_ATARI_CODE_SHIFT);
}

History::u32 History::getLast() const
{
    return history.back() & history_move_MASK;
}

History::u32 History::getLastButOne() const { return get(history.size() - 2); }

std::size_t History::size() const { return history.size(); }

History::u32 History::get(int i) const
{
    return history[i] & history_move_MASK;
}

bool History::isInEnclBorder(int i) const
{
    return history[i] & HISTORY_ENCL_BORDER;
}

bool History::isInTerrWithAtari(int i) const
{
    return history[i] & HISTORY_TERR;
}

bool History::isEnclosure(int i) const
{
    return history[i] & HISTORY_ENCL_MOVE;
}

namespace
{
thread_local std::unordered_map<uint32_t, pti> last_good_reply{};

uint32_t getHash(uint32_t last_move, uint32_t last_but_one, uint32_t who)
{
    return (last_move << 16) | (last_but_one << 1) | who;
}
}  // namespace

pti History::getLastGoodReplyFor(int who_replies) const
{
    if (isEnclosure(history.size() - 1) or isEnclosure(history.size() - 2))
        return 0;
    const auto hash = getHash(getLast(), getLastButOne(), who_replies);
    const auto it = last_good_reply.find(hash);
    if (it == last_good_reply.end()) return 0;
    return it->second;
}

void History::saveGoodReplyAt(int i, int who) const
{
    const auto hash = getHash(get(i - 1), get(i - 2), who);
    last_good_reply.insert_or_assign(hash, get(i));
}

void History::forgetReplyAt(int i, int who) const
{
    const auto hash = getHash(get(i - 1), get(i - 2), who);
    const auto it = last_good_reply.find(hash);
    if (it == last_good_reply.end()) return;
    it->second = 0;
}

void History::updateGoodReplies(int lastWho, float abs_value)
{
    const int first_move = 2;
    const int last_move = std::min<int>(0.8 * (history.size() - 1),
                                        0.8 * coord.wlkx * coord.wlky);
    const int who_is_at_zero_move =
        ((history.size() - 1) % 2 == 0) ? lastWho : 3 - lastWho;
    const bool black_won = abs_value > 0.7;
    const bool white_won = abs_value < 0.3;
    const bool player_at_even_indices_won =
        (who_is_at_zero_move == 1 ? black_won : white_won);
    const bool player_at_odd_indices_won =
        (who_is_at_zero_move == 1 ? white_won : black_won);
    for (int third_move = first_move + 2; third_move <= last_move;)
    {
        if (isEnclosure(third_move))
        {
            third_move += 3;
            continue;
        }
        if (isEnclosure(third_move - 1))
        {
            third_move += 2;
            continue;
        }
        if (isEnclosure(third_move - 2))
        {
            ++third_move;
            continue;
        }
        const bool was_good_reply = (third_move % 2 == 0)
                                        ? player_at_even_indices_won
                                        : player_at_odd_indices_won;
        const int who_is_now = (third_move % 2 == 0) ? who_is_at_zero_move
                                                     : 3 - who_is_at_zero_move;
        if (was_good_reply)
            saveGoodReplyAt(third_move, who_is_now);
        else
            forgetReplyAt(third_move, who_is_now);
        ++third_move;
    }
}

void clearLastGoodReplies() { last_good_reply.clear(); }
