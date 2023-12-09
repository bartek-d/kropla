/********************************************************************************************************
 kropla -- a program to play Kropki; file generatedata.cc -- main file.
    Copyright (C) 2015,2016,2017,2020 Bartek Dyda,
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
#include <queue>
#include <set>
#include <string>

#include "allpattgen.h"
#include "game.h"
#include "patterns.h"
#include "sgf.h"

class PatternStats
{
    pattern3_t findSmallestPatt(pattern3_t p) const;
    struct Stats
    {
        uint32_t present{0};
        uint32_t present_once{0};
        uint32_t present_once_close_to_move0{0};
        uint32_t present_once_close_to_move1{0};
        uint32_t played{0};
        uint32_t played_close_to_move0{0};
        uint32_t played_close_to_move1{0};
        double getFrequency() const
        {
            return static_cast<double>(played) / present;
        }
        bool operator<(const Stats& other) const
        {
            if (0 == present && 0 < other.present) return true;
            if (0 == other.present) return false;
            return getFrequency() < other.getFrequency();
        }
    };
    std::set<pti> present_once{};
    std::set<pti> present_once_close_to_move0{};
    std::set<pti> present_once_close_to_move1{};
    std::array<Stats, PATTERN3_SIZE> stats;
    double most_frequent{0.0};

   public:
    void resetPresentOnce()
    {
        present_once.clear();
        present_once_close_to_move0.clear();
        present_once_close_to_move1.clear();
    }
    void storeStatsForPattern(pattern3_t p, bool wasMoveHere, int whoPlays,
                              pti place, pti move0, pti move1);
    void show() const;
    uint32_t getPresent(pattern3_t p) { return stats[p].present; }
    pattern3_val getValue(pattern3_t p_raw);
    std::string getDescription(pattern3_t p_raw) const;
};

pattern3_t PatternStats::findSmallestPatt(pattern3_t p) const
{
    if (p >= PATTERN3_SIZE) return p;
    pattern3_t smallest = p;
    for (int i = 0; i < 3; ++i)
    {
        p = Pattern3::rotate(p);
        if (p < smallest) smallest = p;
    }
    p = Pattern3::reflect(p);
    if (p < smallest) smallest = p;
    for (int i = 0; i < 3; ++i)
    {
        p = Pattern3::rotate(p);
        if (p < smallest) smallest = p;
    }
    return smallest;
}

void PatternStats::storeStatsForPattern(pattern3_t p, bool wasMoveHere,
                                        int whoPlays, pti place, pti move0,
                                        pti move1)
{
    if (whoPlays == 2)
    {
        p = Pattern3::reverseColour(p);
    }
    const pattern3_t smallest = findSmallestPatt(p);
    if (smallest >= PATTERN3_SIZE) return;
    ++stats[smallest].present;
    if (wasMoveHere) ++stats[smallest].played;
    if (present_once.find(smallest) == present_once.end())
    {
        present_once.insert(smallest);
        ++stats[smallest].present_once;
    }
    if (coord.isInNeighbourhood(place, move0))
    {
        if (present_once_close_to_move0.find(smallest) ==
            present_once_close_to_move0.end())
        {
            present_once_close_to_move0.insert(smallest);
            ++stats[smallest].present_once_close_to_move0;
        }
        if (wasMoveHere) ++stats[smallest].played_close_to_move0;
    }
    if (coord.isInNeighbourhood(place, move1))
    {
        if (present_once_close_to_move1.find(smallest) ==
            present_once_close_to_move1.end())
        {
            present_once_close_to_move1.insert(smallest);
            ++stats[smallest].present_once_close_to_move1;
        }
        if (wasMoveHere) ++stats[smallest].played_close_to_move1;
    }
}

void PatternStats::show() const
{
    struct StatsAndPatt
    {
        pattern3_t patt;
        Stats stats;
        bool operator<(const StatsAndPatt& other) const
        {
            return stats < other.stats;
        }
    };
    std::priority_queue<StatsAndPatt, stdb::vector<StatsAndPatt>> queue;
    for (pattern3_t p = 0; p < PATTERN3_SIZE; ++p)
    {
        if (stats[p].present > 0)
        {
            queue.push(StatsAndPatt{p, stats[p]});
        }
    }
    while (not queue.empty())
    {
        auto sp = queue.top();
        std::cout << "Frequency " << sp.stats.getFrequency() << " ("
                  << sp.stats.played << " / " << sp.stats.present
                  << " counted once: " << sp.stats.played << " / "
                  << sp.stats.present_once << " = "
                  << static_cast<double>(sp.stats.played) /
                         sp.stats.present_once
                  << " counted once close to m0: "
                  << sp.stats.played_close_to_move0 << " / "
                  << sp.stats.present_once_close_to_move0 << " = "
                  << static_cast<double>(sp.stats.played_close_to_move0) /
                         sp.stats.present_once_close_to_move0
                  << " counted once close to m1: "
                  << sp.stats.played_close_to_move1 << " / "
                  << sp.stats.present_once_close_to_move1 << " = "
                  << static_cast<double>(sp.stats.played_close_to_move1) /
                         sp.stats.present_once_close_to_move1
                  << ") --> pattern:\n"
                  << Pattern3::show(sp.patt) << std::endl;
        queue.pop();
    }
}

pattern3_val PatternStats::getValue(pattern3_t p_raw)
{
    auto p = findSmallestPatt(p_raw);
    const auto& s = stats[p];
    /*
    if (most_frequent < 1.0) {
      most_frequent = std::accumulate(stats.begin(),
                                      stats.end(), 0,
                                      [](uint32_t maks, const auto& el) { return
    std::max(maks, el.present); });
    }
    double coeff = std::min(20 * s.present / most_frequent, 1.0);
    */
    double freq = 0.3 * static_cast<double>(s.played) / (s.present_once + 9) +
                  0.7 * static_cast<double>(s.played_close_to_move0) /
                      (s.present_once_close_to_move0 + 9);
    return std::round(freq * 100);
}

std::string PatternStats::getDescription(pattern3_t p_raw) const
{
    auto p = findSmallestPatt(p_raw);
    const auto& sp = stats[p];
    std::stringstream buffer;
    buffer << "Frequency " << sp.getFrequency() << " (" << sp.played << " / "
           << sp.present << " counted once: " << sp.played << " / "
           << sp.present_once << " = "
           << static_cast<double>(sp.played) / sp.present_once
           << " counted once close to m0: " << sp.played_close_to_move0 << " / "
           << sp.present_once_close_to_move0 << " = "
           << static_cast<double>(sp.played_close_to_move0) /
                  sp.present_once_close_to_move0
           << " counted once close to m1: " << sp.played_close_to_move1 << " / "
           << sp.present_once_close_to_move1 << " = "
           << static_cast<double>(sp.played_close_to_move1) /
                  sp.present_once_close_to_move1
           << ")";
    return buffer.str();
}

PatternStats patt_stats{};

Pattern3 patt3;

void generatePatt3()
{
    patt3.generate({
        // hane pattern - enclosing hane
        "XOX"
        ".H."
        "???",
        "52",
        // hane pattern - non-cutting hane
        "YO."
        ".H."
        "?.?",
        "53",
        // hane pattern - magari 		// 0.32
        "XO?"
        "XH."
        "x.?",
        "32",  // TODO: empty triange for O possible, when upper ? == O
        // hane pattern - thin hane 		// 0.22
        "XOO"
        ".H."
        "?.?"
        "X",
        "22",
        // generic pattern - katatsuke or diagonal attachment; similar to magari
        // // 0.37
        ".Q."
        "YH."
        "...",
        "37",  // TODO: is it good? seems like Q should be in corner for the
               // diagonal attachment
        // cut1 pattern (kiri) - unprotected cut 	// 0.28
        "XOo"
        "OHo"
        "###",
        "28",
        //
        "XO?"
        "OHo"
        "*o*",
        "28",
        // cut1 pattern (kiri) - peeped cut 	// 0.21
        "XO?"
        "OHX"
        "???",
        "21",
        // cut2 pattern (de) 			// 0.19
        "?X?"
        "OHO"
        "ooo",
        "19",
        // cut keima (not in Mogo) 		// 0.82
        "OX?"
        "oHO"  // original was: "?.O", but if ?=O, then it's not a cut
        "?o?",
        "52",  // oo? has some pathological tsumego cases
        // side pattern - block side cut 	// 0.20
        "OX?"
        "XHO"
        "###",
        "20",
        // different dame moves
        "X?O"
        "XH?"
        "XXX",
        "-2000",
        // dame
        "?OO"
        "XHO"
        "XX?",
        "-2000",
        // dame
        "X?O"
        "XHO"
        "X?O",
        "-2000",
        // dame
        "XX?"
        "XHO"
        "XX?",
        "-2000",
        // edge dame
        "X?O"
        "XHO"
        "###",
        "-2000",
        // edge dame
        "XX?"
        "XH?"
        "###",
        "-2000",
        // edge dame
        "X?O"
        "XHO"
        "###",
        "-2000",
        // corner -- always dame
        "#??"
        "#H?"
        "###",
        "-2000",
    });
}

void gatherDataFromPosition(Game& game, Move& move)
{
    /*
    std::cerr << "Gather data from position: " << std::endl;
    game.show();
    std::cerr << "Move " << move.show() << " was about to play." << std::endl;
    */
    patt_stats.resetPresentOnce();
    for (int i = coord.first; i <= coord.last; ++i)
    {
        if (game.whoseDotMarginAt(i) == 0)
        {
            patt_stats.storeStatsForPattern(
                game.readPattern3_at(i), i == move.ind, move.who, move.ind,
                game.getLastMove().ind, game.getLastButOneMove().ind);
        }
    }
}

std::pair<Move, stdb::vector<std::string>> getMoveFromSgfNode(const Game& game,
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
    const unsigned start_from = 5;
    const auto [x, y] = getSize(seq[0]);
    const unsigned go_to = std::min<unsigned>(
        (x * y * 3) / 5,  // use moves until 60% of board is full
        seq.size() - 1);
    Game game(SgfSequence(seq.begin(), seq.begin() + start_from), go_to);
    for (unsigned i = start_from; i < go_to; ++i)
    {
        if (whichSide.at(game.whoNowMoves()))
        {
            auto [move, points_to_enclose] = getMoveFromSgfNode(game, seq[i]);
            if (points_to_enclose.empty() and move.ind != 0 and
                move.who == game.whoNowMoves())
            {
                gatherDataFromPosition(game, move);
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
    if (argc < 4)
    {
        std::cerr
            << "at least 3 parameters needed, players_file out_file sgf_file(s)"
            << std::endl;
        return 1;
    }

    auto players = readLines(argv[1]);
    for (auto& p : players)
    {
        std::cout << "PLAYER: " << p << std::endl;
    }

    generatePatt3();

    std::string out_file{argv[2]};

    for (int nfile = 3; nfile < argc; ++nfile)
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
        std::cout << "Game: " << blue << " -- " << red << std::endl;
        gatherDataFromSgfSequence(seq,
                                  {{1, players.find(blue) != players.end()},
                                   {2, players.find(red) != players.end()}});
    }

    patt_stats.show();

    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++++"
              << std::endl;
    auto all = generateAllPossibleSmallest();
    unsigned not_present = 0;
    unsigned dame = 0;
    unsigned no_atari_and_not_dame = 0;

    for (auto p : all)
    {
        if (patt_stats.getPresent(p) == 0)
        {
            const int who = 1;
            if (p <= 0xffff and patt3.getValue(p, who) >= 0)
            {
                std::cout << "not present (" << no_atari_and_not_dame << "):\n"
                          << Pattern3::show(p) << std::endl;
                ++no_atari_and_not_dame;
            }
            if (patt3.getValue(p, who) < 0) ++dame;
            ++not_present;
        }
    }
    std::cout << "Number not present: total = " << not_present
              << ", without atari and not dame: " << no_atari_and_not_dame
              << ", dame: " << dame << std::endl;
    std::cout << "--------------------------------------------------"
              << std::endl;
    for (unsigned p = 0; p < PATTERN3_SIZE; ++p)
    {
        if (patt_stats.getPresent(p) > 0 and all.find(p) == all.end())
        {
            std::cout << "present but considered impossible: " << p << "\n"
                      << Pattern3::show(p) << std::endl;
        }
    }
    // generate patterns array
    std::ofstream o(out_file);
    o << "std::array<pattern3_val, PATTERN3_SIZE> stats_val{\n";
    std::array<pattern3_val, PATTERN3_SIZE> values;
    for (uint32_t p = 0; p < PATTERN3_SIZE; ++p)
    {
        const int who = 1;
        auto p3v = patt3.getValue(p, who);
        if (p3v < 0)
        {
            // hane: Frequency 0.0467318 (15874 / 339683 counted once: 15874 /
            // 139751 = 0.113588 counted once close to m0: 9685 / 65636 =
            // 0.147556 counted once close to m1: 7190 / 51300 = 0.140156) -->
            // pattern:

            values[p] = p3v;
        }
        else
        {
            auto stats_val = patt_stats.getValue(p);
            values[p] = stats_val;
            if (stats_val > 0)
            {
                double ratio = static_cast<double>(p3v + 1) / (stats_val + 1);
                if (ratio < 0.2 or ratio > 5.0)
                {
                    std::cout << "p3val = " << p3v
                              << "  but stats_val = " << stats_val << "("
                              << patt_stats.getDescription(p) << ")"
                              << " for:\n"
                              << Pattern3::show(p) << std::endl;
                }
            }
        }
        o << values[p] << ", ";
        if (p % 10 == 9) o << "\n";
    }
    o << "};\n";

    std::ofstream obin(out_file + ".bin", std::ios::binary);
    obin.write(reinterpret_cast<const char*>(values.data()), sizeof(values));
}
