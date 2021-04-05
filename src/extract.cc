/********************************************************************************************************
 kropla -- a program to play Kropki; file extract.cc -- main file for extracting trainign data for NN.
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

#include "game.h"
#include "sgf.h"
#include "patterns.h"
#include "allpattgen.h"

#include <highfive/H5DataSet.hpp>
#include <highfive/H5DataSpace.hpp>
#include <highfive/H5File.hpp>

#include <boost/multi_array.hpp>

#include <iostream>
#include <fstream>
#include <string>
#include <queue>
#include <set>
#include <array>

constexpr int MOVES_USED = 3;
constexpr int PLANES = 7;
constexpr int BSIZE = 20;
constexpr int TAB_SIZE = 16384;

// using Board = float[PLANES][BSIZE][BSIZE];

class DataCollector {
  using Array4dim = boost::multi_array<float, 4>;
  using Array1dim = boost::multi_array<int, 1>;
  Array4dim data{boost::extents[TAB_SIZE][PLANES][BSIZE][BSIZE]};
  std::array<Array1dim, MOVES_USED> data_labels;
  int curr_size = 0;
  int file_no = 1;
public:
  DataCollector() { for (auto &a : data_labels) a.resize(boost::extents[TAB_SIZE]); }
  auto getCurrentArray();
  void save(int move, int label);
  void dump();
  ~DataCollector();
} collector;

auto DataCollector::getCurrentArray()
{
  return data[curr_size];
}

void DataCollector::save(int move, int label)
{
  data_labels[move][curr_size] = label;
  if (move == MOVES_USED - 1) {
    ++curr_size;
    if (curr_size == TAB_SIZE)
      dump();
  }
}
  
void DataCollector::dump()
{
  try {
    std::string filename{"board_" + std::to_string(file_no) + ".h5"};
    HighFive::File file(filename, HighFive::File::ReadWrite | HighFive::File::Create | HighFive::File::Truncate);
    // Create the dataset
    const std::string DATASET_NAME("bdata");
    HighFive::DataSet dataset =
      file.createDataSet<float>(DATASET_NAME, HighFive::DataSpace::From(data));
    dataset.write(data);
    
    for (int m = 0; m < MOVES_USED; ++m) {
      const std::string LABELS_NAME(std::string{"blabels"} + (m==0? "" : std::to_string(m)));
      HighFive::DataSet labels =
	file.createDataSet<int>(LABELS_NAME, HighFive::DataSpace::From(data_labels[m]));
      labels.write(data_labels[m]);
    }
  } catch (const HighFive::Exception& err) {
    // catch and print any HDF5 error
    std::cerr << err.what() << std::endl;
  }
  curr_size = 0;
  ++file_no;
}

DataCollector::~DataCollector()
{
  if (curr_size == 0) return;
  data.resize(boost::extents[curr_size][PLANES][BSIZE][BSIZE]);
  for (int m=0; m<MOVES_USED; ++m)
    data_labels[m].resize(boost::extents[curr_size]);
  dump();
}


int applyIsometry(int p, unsigned isometry)
// isometry & 1: reflect w/r to Y
// isometry & 2: reflect w/r to X
// isometry & 4: reflect w/r to x==y (swap x--y)
{
  int x = coord.x[p];
  int y = coord.y[p];
  if (isometry & 1) x = coord.wlkx - 1 - x;
  if (isometry & 2) y = coord.wlky - 1 - y;
  if (isometry & 4) std::swap(x, y);
  return coord.ind(x, y);
}

void gatherDataFromPosition(Game& game, const std::vector<Move>& moves)
{
  /*
  std::cerr << "Gather data from position: " << std::endl;
  game.show();
  std::cerr << "Move " << move.show() << " was about to play." << std::endl;
  */
  for (unsigned isometry = 0; isometry < 8; ++isometry) {
    auto data = collector.getCurrentArray();
    for (int x = 0; x < BSIZE; ++x)
      for (int y = 0; y < BSIZE; ++y) {
	int p_orig = coord.ind(x, y);
	int p = applyIsometry(p_orig, isometry);
	int on_move = game.whoNowMoves();
	int opponent = 3 - on_move;
	data[0][x][y] = (game.whoseDotMarginAt(p) == 0) ? 1.0f : 0.0f;
	data[1][x][y] = (game.whoseDotMarginAt(p) == on_move) ? 1.0f : 0.0f;
	data[2][x][y] = (game.whoseDotMarginAt(p) == opponent) ? 1.0f : 0.0f;
	data[3][x][y] = game.isInTerr(p, on_move) > 0 ? 1.0f : 0.0f;
	data[4][x][y] = game.isInTerr(p, opponent) > 0 ? 1.0f : 0.0f;
	data[5][x][y] = std::min(game.isInEncl(p, on_move), 2) * 0.5f;
	data[6][x][y] = std::min(game.isInEncl(p, opponent), 2) * 0.5f;
	//data[7][x][y] = std::min(game.isInBorder(p, on_move), 2) * 0.5f;
	//data[8][x][y] = std::min(game.isInBorder(p, opponent), 2) * 0.5f;
	//data[9][x][y] = std::min(game.getTotalSafetyOf(p), 2.0f) * 0.5f;
	//	data[10][x][y] = (coord.dist[p] == 1) ? 1 : 0;
	//	data[11][x][y] = (coord.dist[p] == 4) ? 1 : 0;
      }
    for (int m=0; m<MOVES_USED; ++m) {
      int move_isom = applyIsometry(moves.at(m).ind, isometry);
      int label = coord.x[move_isom] * BSIZE + coord.y[move_isom];
      collector.save(m, label);
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

void gatherDataFromSgfSequence(SgfSequence &seq,
			       const std::map<int, bool> &whichSide)
{
  const auto [x, y] = getSize(seq[0]);
  if (x != BSIZE or y != BSIZE) {
    std::cout << "  ... size: " << x << "x" << y << std::endl;
    return;
  }
  const unsigned start_from = 5;
  const unsigned go_to = std::min<unsigned>((x*y*3) / 5,     // use moves until 60% of board is full
					    seq.size() - MOVES_USED);
  Game game(SgfSequence(seq.begin(), seq.begin() + start_from), go_to);
  for (unsigned i = start_from; i < go_to; ++i) {
    if (whichSide.at(game.whoNowMoves())) {
      std::vector<Move> subsequentMoves;
      for (int j=0; j<MOVES_USED; ++j) {
	auto [move, points_to_enclose] = getMoveFromSgfNode(game, seq[i + j]);
	subsequentMoves.push_back(move);
      }
      if (subsequentMoves[0].ind != 0 and subsequentMoves[0].who == game.whoNowMoves()) {
	gatherDataFromPosition(game, subsequentMoves);
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
  std::set<std::string> allowedPlayers{};
  std::set<std::string> forbiddenPlayers{};
  int min_rank = 4000;
  for (auto& p : players) {
    switch (p[0]) {
    case '!':
      std::cout << "PLAYER FORBIDDEN: " << p.substr(1) << std::endl;
      forbiddenPlayers.insert(p.substr(1));
      break;
    case '>':
      min_rank = std::stoi(p.substr(1));
      std::cout << "MIN RANK: " << min_rank << std::endl;
      break;
    default:
      std::cout << "PLAYER: " << p << std::endl;
      allowedPlayers.insert(p);
      break;
    }
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
    if (seq[0].findProp("PB") == seq[0].props.end() or seq[0].findProp("PW") == seq[0].props.end() or seq[0].findProp("BR") == seq[0].props.end() or seq[0].findProp("WR") == seq[0].props.end())
      continue;
    auto blue = seq[0].findProp("PB")->second[0];
    auto red = seq[0].findProp("PW")->second[0];
    auto blueRank = std::stoi(seq[0].findProp("BR")->second[0]);
    auto redRank = std::stoi(seq[0].findProp("WR")->second[0]);
    bool blueOk = (allowedPlayers.find(blue) != allowedPlayers.end() or blueRank >= min_rank) and (forbiddenPlayers.find(blue) ==forbiddenPlayers.end());
    bool redOk = (allowedPlayers.find(red) != allowedPlayers.end() or redRank >= min_rank) and (forbiddenPlayers.find(red) ==forbiddenPlayers.end());
    std::cout << sgf_file << " -- game: " << blue << " [" << blueRank << "] -- " << red << " [" << redRank << "]  ";
    if (blueOk and redOk) {
      gatherDataFromSgfSequence(seq, {{1, blueOk}, {2, redOk}});
    } else {
      std::cout << "omitted." << std::endl;
    }
      
  }
}
