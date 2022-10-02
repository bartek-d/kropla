/********************************************************************************************************
 kropla -- a program to play Kropki; file check_accuracy.cc -- main file for
testing NN. Copyright (C) 2021 Bartek Dyda, email: bartekdyda (at) protonmail
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

#include "game.h"
#include "sgf.h"
//#include "caffe/mcaffe.h"
#include "get_cnn_prob.h"
//#include <boost/multi_array.hpp>

#include <fstream>
#include <iostream>
#include <queue>
#include <set>
#include <string>

void gatherDataFromPosition(Game& game, Move& move, unsigned move_no)
{
    const auto [is_cnn_available, probs] = getCnnInfo(game);
    struct MoveAndProb
    {
        int x;
        int y;
        float prob;
        bool operator<(const MoveAndProb& other) const
        {
            return prob < other.prob;
        }
    };
    std::priority_queue<MoveAndProb, std::vector<MoveAndProb>> queue;
    for (int y = 0; y < coord.wlky; ++y)
    {
        for (int x = 0; x < coord.wlkx; ++x)
        {
            queue.push(MoveAndProb{x, y, probs[coord.ind(x, y)]});
        }
    }
    game.show();
    std::cout << "Move #" << move_no << " of player #" << game.whoNowMoves()
              << ": " << coord.showPt(move.ind) << std::endl;

    int shown = 0;
    for (int i = 0; shown < 20 && not queue.empty(); ++i)
    {
        auto mp = queue.top();
        if (game.whoseDotMarginAt(coord.ind(mp.x, mp.y)) == 0)
        {
            std::cout << "Move #" << i << ": (" << mp.x << ", " << mp.y
                      << ") --> " << mp.prob;
            if (coord.ind(mp.x, mp.y) == move.ind) std::cout << " *****";
            std::cout << std::endl;
            ++shown;
        }
        queue.pop();
    }
    std::cout << std::endl;
}

std::pair<Move, std::vector<std::string>> getMoveFromSgfNode(const Game& game,
                                                             SgfNode node)
{
    if (node.findProp("B") != node.props.end())
    {
        const auto& value = node.findProp("B")->second;
        if (not value.empty())
        {
            return game.extractSgfMove(value.front(), 1);
        }
    }
    if (node.findProp("W") != node.props.end())
    {
        const auto& value = node.findProp("W")->second;
        if (not value.empty())
        {
            return game.extractSgfMove(value.front(), 2);
        }
    }
    return {Move{}, {}};
}

std::pair<unsigned, unsigned> getSize(SgfNode& node)
{
    auto sz_pos = node.findProp("SZ");
    std::string sz = (sz_pos != node.props.end()) ? sz_pos->second[0] : "";
    if (sz.find(':') == std::string::npos)
    {
        if (sz != "")
        {
            int x = stoi(sz);
            return {x, x};
        }
    }
    else
    {
        std::string::size_type i = sz.find(':');
        int x = stoi(sz.substr(0, i));
        int y = stoi(sz.substr(i + 1));
        return {x, y};
    }
    return {0, 0};
}

void gatherDataFromSgfSequence(SgfSequence& seq,
                               const std::map<int, bool>& whichSide)
{
    const auto [x, y] = getSize(seq[0]);
    if (x != y)
    {
        std::cout << "  ... size: " << x << "x" << y << std::endl;
        return;
    }
    const unsigned start_from = 5;
    const unsigned go_to = std::min<unsigned>(
        (x * y * 19) / 20,  // use moves until 95% of board is full
        seq.size() - 1);
    Game game(SgfSequence(seq.begin(), seq.begin() + start_from), go_to);
    for (unsigned i = start_from; i < go_to; ++i)
    {
        if (whichSide.at(game.whoNowMoves()))
        {
            auto [move, points_to_enclose] = getMoveFromSgfNode(game, seq[i]);
            if (move.ind != 0 and move.who == game.whoNowMoves())
            {
                gatherDataFromPosition(game, move, i + 2);
            }
        }
        // std::cerr << "Trying to play at: " << seq[i].toString() << std::endl;
        game.replaySgfSequence({seq[i]}, 1);
    }
}

std::set<std::string> readLines(const std::string& filename)
{
    std::ifstream t(filename);
    std::set<std::string> output;
    for (std::string line; std::getline(t, line);)
    {
        if (not line.empty()) output.emplace(line);
    }
    return output;
}

int main(int argc, char* argv[])
{
    std::cerr << "CNN info will be read from file cnn.config" << std::endl;
    if (argc < 2)
    {
        std::cerr << "at least 1 parameter needed, sgf_file(s)" << std::endl;
        return 1;
    }

    for (int nfile = 1; nfile < argc; ++nfile)
    {
        std::string sgf_file{argv[nfile]};
        std::ifstream t(sgf_file);
        std::stringstream buffer;
        buffer << t.rdbuf();
        std::string s = buffer.str();

        SgfParser parser(s);
        auto seq = parser.parseMainVar();
        auto blue = seq[0].findProp("PB")->second[0];
        auto red = seq[0].findProp("PW")->second[0];
        std::cout << sgf_file << " -- game: " << blue << " -- " << red << "  ";
        gatherDataFromSgfSequence(seq, {{1, true}, {2, true}});
    }
}
