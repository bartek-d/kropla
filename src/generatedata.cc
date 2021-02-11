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

#include "game.h"
#include "sgf.h"
#include "patterns.h"

#include <iostream>
#include <fstream>
#include <string>
#include <queue>

class PatternStats {
  pattern3_t findSmallestPatt(pattern3_t p) const;
  struct Stats {
    uint32_t present{0};
    uint32_t played{0};
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
  std::array<Stats, PATTERN3_SIZE> stats;
public:
  void storeStatsForPattern(pattern3_t p, bool wasMoveHere, int whoPlays);
  void show() const;
};

pattern3_t
PatternStats::findSmallestPatt(pattern3_t p) const
{
  if (p >= PATTERN3_SIZE) return p;
  pattern3_t smallest = p;
  for (int i=0; i<3; ++i) {
    p = Pattern3::rotate(p);
    if (p < smallest) smallest = p;
  }
  p = Pattern3::reflect(p);
  if (p < smallest) smallest = p;
  for (int i=0; i<3; ++i) {
    p = Pattern3::rotate(p);
    if (p < smallest) smallest = p;
  }
  return smallest;
}

void
PatternStats::storeStatsForPattern(pattern3_t p, bool wasMoveHere, int whoPlays)
{
  if (whoPlays == 2) {
    p = Pattern3::reverseColour(p);
  }
  const pattern3_t smallest = findSmallestPatt(p);
  if (smallest >= PATTERN3_SIZE) return;
  ++stats[smallest].present;
  if (wasMoveHere) ++stats[smallest].played;
}

void
PatternStats::show() const
{
  struct StatsAndPatt {
    pattern3_t patt;
    Stats stats;
    bool operator<(const StatsAndPatt& other) const
    {
      return stats < other.stats;
    }
  };
  std::priority_queue<StatsAndPatt, std::vector<StatsAndPatt>> queue;
  for (pattern3_t p = 0; p < PATTERN3_SIZE; ++p) {
    if (stats[p].played > 0) {
      queue.push(StatsAndPatt{p, stats[p]});
    }
  }
  while (not queue.empty()) {
    auto sp = queue.top();
    std::cout << "Frequency " << sp.stats.getFrequency() << " ("
	      << sp.stats.played << " / " << sp.stats.present << ") --> pattern:\n"
	      << Pattern3::show(sp.patt) << std::endl;
    queue.pop();
  }
}



PatternStats patt_stats{};

void gatherDataFromPosition(Game& game, Move& move)
{
  std::cerr << "Gather data from position: " << std::endl;
  game.show();
  std::cerr << "Move " << move.show() << " was about to play." << std::endl;
  for (int i=coord.first; i<=coord.last; ++i) {
    if (game.whoseDotMarginAt(i) == 0) {
      patt_stats.storeStatsForPattern(game.readPattern3_at(i), i == move.ind, move.who);
    }
  }
}

std::pair<Move, std::vector<std::string>>
getMoveFromSgfNode(const Game& game, SgfNode node)
{
  if (node.findProp("B") != node.props.end()) {
    const auto& value = node.findProp("B")->second;
    if (not value.empty()) {
      return game.extractSgfMove(value.front(), 1);
    }
  }
  if (node.findProp("W") != node.props.end()) {
    const auto& value = node.findProp("W")->second;
    if (not value.empty()) {
      return game.extractSgfMove(value.front(), 2);
    }
  }
  return {Move{}, {}};
}

void gatherDataFromSgfSequence(const SgfSequence &seq, const std::map<int, bool> &whichSide)
{
  const unsigned start_from = 5;
  const unsigned infinity = 10000;
  Game game(SgfSequence(seq.begin(), seq.begin() + start_from), infinity);
  for (unsigned i = start_from; i < seq.size() - 1; ++i) {
    if (whichSide.at(game.whoNowMoves())) {
      auto [move, points_to_enclose] = getMoveFromSgfNode(game, seq[i]);
      if (points_to_enclose.empty() and move.ind != 0 and move.who == game.whoNowMoves()) {
	gatherDataFromPosition(game, move);
      }
    }
    std::cerr << "Trying to play at: " << seq[i].toString() << std::endl;
    game.replaySgfSequence({seq[i]}, 1);
  }
}


int main(int argc, char* argv[]) {
  if (argc < 4) {
    std::cerr << "at least 3 parameters needed, players_file out_file sgf_file(s)" << std::endl;
    return 1;
  }

  std::string players_file{argv[1]};
  std::string out_file{argv[2]};

  for (int nfile = 3; nfile < argc; ++nfile) {
    std::string sgf_file{argv[nfile]};
    std::ifstream t(sgf_file);
    std::stringstream buffer;
    buffer << t.rdbuf();
    std::string s = buffer.str();

    SgfParser parser(s);
    auto seq = parser.parseMainVar();
    gatherDataFromSgfSequence(seq, {{1, true}, {2, true}});
  }

  patt_stats.show();

}
