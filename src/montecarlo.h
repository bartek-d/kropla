/********************************************************************************************************
 kropla -- a program to play Kropki; file montecarlo.h.
    Copyright (C) 2020 Bartek Dyda,
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

#pragma once

#include <atomic>

/********************************************************************************************************
  Montecarlo class for Monte Carlo search.
*********************************************************************************************************/
class Game;

class MonteCarlo {
public:
  MonteCarlo();
  std::string findBestMove(Game &pos, int iter_count);
  std::string findBestMoveMT(Game &pos, int threads, int iter_count, int msec);
private:
  int runSimulations(int max_iter_count, unsigned thread_no, unsigned threads_count);
  Treenode* selectBestChild(Treenode *node) const;
  std::shared_ptr<Game> getCopyOfGame(Treenode *node) const;
  void expandNode(TreenodeAllocator &alloc, Treenode *node, Game* game, int depth) const;
  void descend(TreenodeAllocator &alloc, Treenode *node, unsigned seed);
};

namespace montec {
extern Treenode root;
extern std::atomic<bool> finish_sim;
extern bool finish_threads;
extern std::atomic<int> threads_to_be_finished;
extern std::atomic<int64_t> iterations;
extern std::atomic<int64_t> generateMovesCount;
}

void
play_engine(Game &game, std::string &s, int threads_count, int iter_count, int msec);

void
findAndPrintBestMove(Game &game, int threads_count, int iter_count);

void
playInteractively(Game &game, int threads_count, int iter_count);

