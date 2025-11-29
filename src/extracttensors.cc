/********************************************************************************************************
 kropla -- a program to play Kropki; file extracttensors.cc -- main file for
extracting trainign data for NN. Copyright (C) 2021, 2022, 2023 Bartek Dyda,
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

#include "game.h"
#include "gzip.hpp"
#include "patterns.h"
#include "sgf.h"
#include "string_utils.h"
#include "extractutils.h"

#include <torch/torch.h>

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

void tensorSaver(float* ptr, const std::string& filename, int a, int b, int c,
                 int d)
{
    torch::Tensor output_tensor = torch::from_blob(ptr, {a, b, c, d}).clone();
    torch::save(output_tensor, filename);
}

constexpr int MOVES_USED = 3;
constexpr int PLANES = 20;
constexpr int BSIZEX = 20;
constexpr int BSIZEY = 20;
CompressedData<MOVES_USED, PLANES, BSIZEX, BSIZEY> compressed_data{tensorSaver};

int main(int argc, char* argv[])
{
    if (argc < 3)
    {
        std::cerr << "at least 2 parameters needed, players_file sgf_file(s)"
                  << std::endl;
        return 1;
    }

    auto players = readLines(argv[1]);
    std::set<std::string> allowedPlayers{};
    std::set<std::string> forbiddenPlayers{};
    int min_rank = 4000;
    bool all_allowed = false;
    for (auto& p : players)
    {
        switch (p[0])
        {
            case '!':
                std::cout << "PLAYER FORBIDDEN: " << p.substr(1) << std::endl;
                forbiddenPlayers.insert(p.substr(1));
                break;
            case '>':
                min_rank = std::stoi(p.substr(1));
                std::cout << "MIN RANK: " << min_rank << std::endl;
                break;
            case '$':
                all_allowed = true;
                break;
            default:
                std::cout << "PLAYER: " << p << std::endl;
                allowedPlayers.insert(p);
                break;
        }
    }

    for (int nfile = 2; nfile < argc; ++nfile)
    {
        std::string sgf_file{argv[nfile]};
        std::ifstream t(sgf_file);
        std::stringstream buffer;
        buffer << t.rdbuf();
        std::vector<std::string> sgfs = split(buffer.str(), "\n\n");
        for (const auto& s : sgfs)
        {
            SgfParser parser(s);
            auto seq = parser.parseMainVar();
            if (not all_allowed)
            {
                if (seq[0].findProp("PB") == seq[0].props.end() or
                    seq[0].findProp("PW") == seq[0].props.end() or
                    seq[0].findProp("BR") == seq[0].props.end() or
                    seq[0].findProp("WR") == seq[0].props.end())
                    continue;
            }
            bool must_surround = false;
            {
                auto res = seq[0].findProp("RU");
                if (res != seq[0].props.end() and res->second[0] == "russian")
                    must_surround = true;
            }
            bool blueOk = true;
            bool redOk = true;
            if (not all_allowed)
            {
                auto blue = seq[0].findProp("PB")->second[0];
                auto red = seq[0].findProp("PW")->second[0];
                auto blueRank = robust_stoi(seq[0].findProp("BR")->second[0]);
                auto redRank = robust_stoi(seq[0].findProp("WR")->second[0]);
                blueOk =
                    (allowedPlayers.find(blue) != allowedPlayers.end() or
                     blueRank >= min_rank) and
                    (forbiddenPlayers.find(blue) == forbiddenPlayers.end());
                redOk = (allowedPlayers.find(red) != allowedPlayers.end() or
                         redRank >= min_rank) and
                        (forbiddenPlayers.find(red) == forbiddenPlayers.end());
                std::cout << sgf_file << " -- game: " << blue << " ["
                          << blueRank << "] -- " << red << " [" << redRank
                          << "]  ";
            }
            if (blueOk and redOk)
            {
                gatherDataFromSgfSequence(compressed_data, seq,
                                          {{1, blueOk}, {2, redOk}},
                                          must_surround);
            }
            else
            {
                std::cout << "omitted." << std::endl;
            }
        }
    }
    compressed_data.dump();
}
