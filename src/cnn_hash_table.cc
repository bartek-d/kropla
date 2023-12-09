/********************************************************************************************************
 kropla -- a program to play Kropki; file cnn_hash_table.cc -- storing and
reading NN. Copyright (C) 2023 Bartek Dyda, email: bartekdyda (at) protonmail
(dot) com

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

#include "cnn_hash_table.h"

#include <cstdint>
#include <map>
#include <mutex>
#include <vector>

namespace
{
using CnnInfo = std::vector<float>;

std::map<Position, CnnInfo> table;
std::mutex table_mutex;

uint64_t ht_queries = 0;
uint64_t ht_answers = 0;
}  // namespace

std::pair<bool, std::vector<float>> getCnnInfoFromHT(const Position pos)
{
    std::lock_guard<std::mutex> l(table_mutex);
    ++ht_queries;
    const auto it = table.find(pos);
    if (it == table.end()) return {false, {}};
    ++ht_answers;
    return {true, it->second};
}

void saveCnnInfo(const Position pos, const std::vector<float>& info)
{
    std::lock_guard<std::mutex> l(table_mutex);
    table.try_emplace(pos, info);
}

std::pair<uint64_t, uint64_t> getCnnHtStats()
{
    std::lock_guard<std::mutex> l(table_mutex);
    return {ht_queries, ht_answers};
}
