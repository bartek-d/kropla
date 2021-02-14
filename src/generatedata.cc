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
#include <set>

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
  /*
  std::cerr << "Gather data from position: " << std::endl;
  game.show();
  std::cerr << "Move " << move.show() << " was about to play." << std::endl;
  */
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

std::pair<unsigned, unsigned> getSize(SgfNode& node)
{
  auto sz_pos = node.findProp("SZ");
  std::string sz = (sz_pos != node.props.end()) ? sz_pos->second[0] : "";
  if (sz.find(':') == std::string::npos) {
    if (sz != "") {
      int x = stoi(sz);
      return {x, x};
    }
  } else {
    std::string::size_type i = sz.find(':');
    int x = stoi(sz.substr(0, i));
    int y = stoi(sz.substr(i+1));
    return {x, y};
  }
  return {0,0};
}

void gatherDataFromSgfSequence(SgfSequence &seq, const std::map<int, bool> &whichSide)
{
  const unsigned start_from = 5;
  const auto [x, y] = getSize(seq[0]);
  const unsigned go_to = std::min<unsigned>((x*y*3) / 5,     // use moves until 60% of board is full
					    seq.size() - 1);
  Game game(SgfSequence(seq.begin(), seq.begin() + start_from), go_to);
  for (unsigned i = start_from; i < go_to; ++i) {
    if (whichSide.at(game.whoNowMoves())) {
      auto [move, points_to_enclose] = getMoveFromSgfNode(game, seq[i]);
      if (points_to_enclose.empty() and move.ind != 0 and move.who == game.whoNowMoves()) {
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
  for (std::string line; std::getline(t, line); ) {
    if (not line.empty())
      output.emplace(line);
  }
  return output;
}


int main(int argc, char* argv[]) {
  if (argc < 4) {
    std::cerr << "at least 3 parameters needed, players_file out_file sgf_file(s)" << std::endl;
    return 1;
  }

  auto players = readLines(argv[1]);
  for (auto& p : players) {
    std::cout << "PLAYER: " << p << std::endl;
  }

  
  std::string out_file{argv[2]};

  for (int nfile = 3; nfile < argc; ++nfile) {
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
    gatherDataFromSgfSequence(seq, {
				    {1, players.find(blue) != players.end()},
				    {2, players.find(red) != players.end()}
      });
  }

  patt_stats.show();

}
