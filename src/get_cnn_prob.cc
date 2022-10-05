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

#include "caffe/mcaffe.h"

//#include "board.h"

#include <boost/multi_array.hpp>
#include <cmath>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>

namespace
{
MCaffe cnn;
std::mutex caffe_mutex;
bool madeQuiet = false;
int planes = 10;
using Array3dim = boost::multi_array<float, 3>;
constexpr int DEFAULT_CNN_BOARD_SIZE = 20;
}  // namespace

void initialiseCnn()
{
    if (not madeQuiet)
    {
        cnn.quiet_caffe("kropla");
        madeQuiet = true;
    }
    {
        std::string number_of_planes{};
        std::string model_file_name{};
        std::string weights_file_name{};
        std::ifstream t("cnn.config");
        if (std::getline(t, number_of_planes))
            if (std::getline(t, model_file_name))
                std::getline(t, weights_file_name);
        planes = std::stoi(number_of_planes);
        if (planes != 7 and planes != 10 and planes != 20)
        {
            std::cerr << "Unsupported number of planes (" << planes
                      << "), assuming 10." << std::endl;
            planes = 10;
        }
        cnn.caffe_init(coord.wlkx, model_file_name, weights_file_name,
                       DEFAULT_CNN_BOARD_SIZE);
    }
}

std::pair<bool, std::vector<float>> getCnnInfo(Game& game)
try
{
    if (coord.wlkx != coord.wlky)
    {
        return {false, {}};
    }
    Array3dim data{boost::extents[planes][coord.wlkx][coord.wlky]};
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
            data[5][x][y] = std::min(game.isInEncl(p, on_move), 2) * 0.5f;
            data[6][x][y] = std::min(game.isInEncl(p, opponent), 2) * 0.5f;
            if (planes == 7) continue;
            data[7][x][y] = std::min(game.isInBorder(p, on_move), 2) * 0.5f;
            data[8][x][y] = std::min(game.isInBorder(p, opponent), 2) * 0.5f;
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
    std::unique_lock<std::mutex> lock{caffe_mutex};
    auto res = cnn.caffe_get_data(static_cast<float*>(&data[0][0][0]),
                                  coord.wlkx, planes, coord.wlky);
    lock.unlock();

    std::vector<float> probs(coord.getSize(), 0.0f);
    for (int x = 0; x < coord.wlkx; ++x)
    {
        for (int y = 0; y < coord.wlky; ++y)
        {
            probs[coord.ind(x, y)] = res[x * coord.wlky + y];
        }
    }
    return {true, probs};
}
catch (const CaffeException& exc)
{
    std::cerr << "Failed to load cnn" << std::endl;
    return {false, {}};
}

void updatePriors(Game& game, Treenode* children, int depth)
{
    const auto [is_cnn_available, probs] = getCnnInfo(game);
    std::cerr << "Trying to update priors for "
              << children->parent->showParents()
              << " from CNN: " << is_cnn_available << std::endl;
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
    const float prior_max =
        (depth == 1) ? 800.0f : (depth == 2 ? 400.0f : 200.0f);
    const float min_to_show = 0.05f;
    for (auto* ch = children; true; ++ch)
    {
        float prob = probs[ch->move.ind];
        const bool show_this = (prob >= min_to_show) and (depth <= 2);
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
    std::cerr << "Updated priors from CNN!" << std::endl;
}
