/********************************************************************************************************
 kropla -- a program to play Kropki; file get_cnn_prob.cc -- reading NN.
    Copyright (C) 2021 Bartek Dyda,
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

/*
 Set
   export OMP_NUM_THREADS=1
 or otherwise it will be slow.
*/

#include "get_cnn_prob.h"

#include "cnn_hash_table.h"
#include "cnn_workers.h"

//#include "board.h"

#include <boost/multi_array.hpp>
#include <chrono>  // chrono::high_resolution_clock, only to measure elapsed time
#include <cmath>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>

namespace
{
using Array3dim = boost::multi_array<float, 3>;
using Array3dimRef = boost::multi_array_ref<float, 3>;
int planes{0};
std::unique_ptr<workers::WorkersPoolBase> workers_pool = nullptr;
int planes2{0};
std::unique_ptr<workers::WorkersPoolBase> workers_pool2 = nullptr;

}  // namespace

namespace global
{
extern std::string program_path;
}  // namespace global

void initialiseCnn()
{
    // not thread safe
    static bool workers_active = false;
    if (not workers_active)
    {
        constexpr int max_planes = 20;
        const std::size_t memory_needed =
            coord.maxSize * sizeof(float) * max_planes + sizeof(uint32_t);
        const bool use_this_thread = false;
        workers_pool = workers::buildWorkerPool(
            global::program_path + "cnn.config", memory_needed, coord.wlkx,
            use_this_thread);
        planes = workers_pool->getPlanes();
        workers_pool2 = workers::buildWorkerPool(
            global::program_path + "cnn2.config", memory_needed, coord.wlkx,
            use_this_thread);
        planes2 = workers_pool2->getPlanes();
        workers_active = true;
    }
}

std::vector<float> getInputForCnn(const Game& game, int planes)
{
    if (coord.wlkx != coord.wlky)
    {
        return {};
    }
    std::vector<float> res(planes * coord.wlkx * coord.wlky);
    Array3dimRef data{res.data(),
                      boost::extents[planes][coord.wlkx][coord.wlky]};
    for (int x = 0; x < coord.wlkx; ++x)
        for (int y = 0; y < coord.wlky; ++y)
        {
            int p = coord.ind(x, y);
            int on_move = game.whoNowMoves();
            int opponent = 3 - on_move;
            data[0][x][y] = (game.whoseDotMarginAt(p) == 0) ? 1.0f : 0.0f;
            data[1][x][y] = (game.whoseDotMarginAt(p) == on_move) ? 1.0f : 0.0f;
            data[2][x][y] =
                (game.whoseDotMarginAt(p) == opponent) ? 1.0f : 0.0f;
            data[3][x][y] = game.isInTerr(p, on_move) > 0 ? 1.0f : 0.0f;
            data[4][x][y] = game.isInTerr(p, opponent) > 0 ? 1.0f : 0.0f;
            data[5][x][y] = std::min<pti>(game.isInEncl(p, on_move), 2) * 0.5f;
            data[6][x][y] = std::min<pti>(game.isInEncl(p, opponent), 2) * 0.5f;
            if (planes == 7) continue;
            data[7][x][y] = std::min<pti>(game.isInBorder(p, on_move), 2) * 0.5f;
            data[8][x][y] = std::min<pti>(game.isInBorder(p, opponent), 2) * 0.5f;
            data[9][x][y] = std::min(game.getTotalSafetyOf(p), 2.0f) * 0.5f;
            if (planes == 10) continue;
            //	data[10][x][y] = (coord.dist[p] == 1) ? 1 : 0;
            //	data[11][x][y] = (coord.dist[p] == 4) ? 1 : 0;
            data[10][x][y] = (coord.dist[p] == 1) ? 1 : 0;
            //	data[11][x][y] = (coord.dist[p] == 4) ? 1 : 0;
            data[11][x][y] = 1;
            data[12][x][y] = 0;  // where for thr such that opp_dots>0
            data[13][x][y] = 0;  // where for thr such that opp_dots>0
            data[14][x][y] =
                0;  // where0 for thr2 such that minwin2 > 0 and isSafe
            data[15][x][y] =
                0;  // where0 for thr2 such that minwin2 > 0 and isSafe
            data[16][x][y] =
                (game.threats[on_move - 1].is_in_2m_encl[p] > 0) ? 1.0f : 0.0f;
            data[17][x][y] =
                (game.threats[opponent - 1].is_in_2m_encl[p] > 0) ? 1.0f : 0.0f;
            data[18][x][y] =
                (game.threats[on_move - 1].is_in_2m_miai[p] > 1) ? 1.0f : 0.0f;
            data[19][x][y] =
                (game.threats[opponent - 1].is_in_2m_miai[p] > 1) ? 1.0f : 0.0f;
        }

    if (planes > 12)
    {
        for (int player = 0; player < 2; ++player)
        {
            int which2 = (player + 1 == game.whoNowMoves()) ? 14 : 15;
            if (which2 < planes)
            {
                for (auto& t : game.threats[player].threats2m)
                {
                    if (t.min_win2 && t.isSafe())
                    {
                        data[which2][coord.x[t.where0]][coord.y[t.where0]] =
                            1.0f - std::pow(0.75f, t.min_win2);
                    }
                }
            }
            int which = (player + 1 == game.whoNowMoves()) ? 12 : 13;
            if (which < planes)
            {
                for (auto& t : game.threats[player].threats)
                {
                    if (t.where && t.singular_dots)
                    {
                        data[which][coord.x[t.where]][coord.y[t.where]] =
                            1.0f - std::pow(0.75f, t.singular_dots);
                    }
                }
            }
        }
    }
    return res;
}

std::vector<float> convertToBoard(const std::vector<float>& res)
{
    std::vector<float> probs(coord.getSize(), 0.0f);
    for (int x = 0; x < coord.wlkx; ++x)
    {
        for (int y = 0; y < coord.wlky; ++y)
        {
            probs[coord.ind(x, y)] = res[x * coord.wlky + y];
        }
    }
    return probs;
}

std::pair<bool, std::vector<float>> getCnnInfo(Game& game,
                                               bool use_secondary_cnn)
{
    std::pair<bool, std::vector<float>> fromHT{false, {}};
    if (not use_secondary_cnn)
    {
        auto pos = Position{game.getHistorySize(), game.getZobrist()};
        fromHT = getCnnInfoFromHT(pos);
        if (fromHT.first)
        {
            std::cerr << "in HT !!!!!!!!!!!!!!!!!!!!!" << std::endl;
            return fromHT;
        }
        else
            std::cerr << "not in HT" << std::endl;
    }
    auto input = getInputForCnn(game, use_secondary_cnn ? planes2 : planes);
    auto [success, res] = (use_secondary_cnn ? workers_pool2 : workers_pool)
                              ->getCnnInfo(input, coord.wlkx);
    if (not success) return {false, {}};
    res = convertToBoard(res);
    if (not use_secondary_cnn)
    {
        auto pos = Position{game.getHistorySize(), game.getZobrist()};
        saveCnnInfo(pos, res);
    }
    return {success, std::move(res)};
}

void updatePriors(Game& game, Treenode* children, int depth)
{
    if (children == nullptr) return;
    const int max_depth_for_primary_cnn = 4;

    std::cerr << "Trying to update priors for " << game.getZobrist() << " "
              << children->parent->showParents() << " -> ";
    const auto [is_cnn_available, probs] =
        getCnnInfo(game, depth > max_depth_for_primary_cnn);

    if (not is_cnn_available) return;
    float max = 0.0f;
    for (auto* ch = children; true; ++ch)
    {
        if (not ch->isDame() and probs[ch->move.ind] > max)
        {
            max = probs[ch->move.ind];
        }
        if (ch->isLast()) break;
    }
    if (max == 0.0f)
    {
        std::cerr << "Max is 0.0f, CNN does not work?" << std::endl;
        return;
    }
    const float prior_max = 150.0f / std::sqrt(std::sqrt(max));
    const float min_to_show = 0.05f;
    for (auto* ch = children; true; ++ch)
    {
        float prob = probs[ch->move.ind];
        const bool show_this = (prob >= min_to_show) and (depth <= 2);
        ch->cnn_prob = prob;
        if (prob > 0.001f)
        {
            prob = std::sqrt(prob);
            /*
              if (not ch->isDame()) {
              prob = std::sqrt(prob);
              } else {
              prob *= prob;
              } */
            const int32_t value = prob * prior_max;
            ch->t.playouts += value;
            ch->t.value_sum = ch->t.value_sum.load() + value;
            ch->prior.playouts += value;
            ch->prior.value_sum = ch->prior.value_sum.load() + value;
            if (show_this)
            {
                std::cerr << "   " << ch->show() << "  --> " << value
                          << "  (max: " << prior_max << ")" << std::endl;
            }
        }
        if (ch->isLast()) break;
    }
}

void printCnnStats()
{
    const auto [ht_queries, ht_answers] = getCnnHtStats();
    std::cerr << "Queries of large CNN: " << ht_queries
              << ", from that those read from HT: " << ht_answers << std::endl;
    const auto filename = "htstats.txt";
    std::fstream file(
        filename, std::fstream::out | std::fstream::app | std::fstream::ate);
    file << "Queries of large CNN: " << ht_queries
         << ", from that those read from HT: " << ht_answers << std::endl;
}
