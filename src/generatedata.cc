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

#include <iostream>
#include <fstream>
#include <string>


void gatherDataFromPosition(Game& game)
{
  std::cerr << "Gather data from position: " << std::endl;
  game.show();
}

void gatherDataFromSgfSequence(const SgfSequence &seq, const std::map<int, bool> &whichSide)
{
  const int start_from = 5;
  const int infinity = 10000;
  Game game(SgfSequence(seq.begin(), seq.begin() + start_from), infinity);
  for (int i = start_from; i < seq.size() - 1; ++i) {
    if (whichSide.at(game.whoNowMoves())) {
      gatherDataFromPosition(game);
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

      
}
