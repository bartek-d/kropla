/********************************************************************************************************
 kropla -- a program to play Kropki; file montecarlo.cc.
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

#include "game.h"
#include "montecarlo.h"
#include "command.h"
#include "get_cnn_prob.h"

#include <chrono>  // chrono::high_resolution_clock, only to measure elapsed time
#include <atomic>
#include <future>
#include <memory>

#include <condition_variable>
#include <mutex>
#include <iostream>

/********************************************************************************************************
  Montecarlo class for Monte Carlo search.
*********************************************************************************************************/
std::mutex mutex_finish_threads;
std::condition_variable cv_finish_threads;
namespace montec {
Treenode root;
std::atomic<bool> finish_sim(false);
std::atomic<int> threads_to_be_finished{0};

std::atomic<int64_t> iterations(0);
std::atomic<int64_t> generateMovesCount{0};

std::mutex cnn_mutex;

bool finish_threads(false);
constexpr int start_increasing = 200;
constexpr real_t increase_komi_threshhold = 0.75;
constexpr real_t decrease_komi_threshhold = 0.15;
constexpr int MC_EXPAND_THRESHOLD = 8;
}

MonteCarlo::MonteCarlo() //: finish_sim(false), finish_threads(false), finished_threads(0), iterations(0)
{
  montec::root.parent = &montec::root;
}

std::string
MonteCarlo::findBestMove(Game &pos, int iter_count)
{
  debug_previous_count = -1;
  montec::root = Treenode();
  montec::root.move = pos.getLastMove();
  montec::root.parent = &montec::root;
  montec::root.game_ptr = std::make_shared<Game>(pos);
  std::cerr << "Descend starts, komi==" << global::komi << std::endl;
#ifdef DEBUG_SGF
  pos.sgf_tree.saveCursor();
#endif

  int komi_change_at = montec::start_increasing;
  TreenodeAllocator alloc;
  constexpr unsigned seed = 1;
  for (int i=0; i<iter_count; i++) {
#ifdef DEBUG_SGF
    pos.sgf_tree.restoreCursor();
#endif
    if ((i & 0x7f) == 0) std::cerr << "iteration = " << i << std::endl;
    if (i >= komi_change_at) {
      komi_change_at *= 6;
      if (montec::root.t.value_sum < montec::root.t.playouts * (1-montec::increase_komi_threshhold)) {  // green zone
	int perspective = 2*montec::root.move.who - 3;   // -1 if we are white, 1 if black (montec::root.move.who is the opponent)
	std::cerr << "Green zone; komi = " << global::komi << ", perspective = " << perspective << ", ratchet = " << global::komi_ratchet << std::endl;
	if (global::komi*perspective < global::komi_ratchet) {
	  std::cerr << "Changing komi from " << global::komi << " to ";
	  global::komi += (montec::root.move.who == 1) ? -2 : 2;
	  std::cerr << global::komi << std::endl;
	}
      } else if (montec::root.t.value_sum > montec::root.t.playouts * (1-montec::decrease_komi_threshhold)) {  // red zone
	int perspective = 2*montec::root.move.who - 3;   // -1 if we are white, 1 if black (montec::root.move.who is the opponent)
	std::cerr << "Red zone; komi = " << global::komi << ", perspective = " << perspective << ", ratchet = " << global::komi_ratchet << std::endl;
	if (global::komi*perspective > 0) {
	  global::komi_ratchet = global::komi*perspective;
	}
	std::cerr << "New ratchet: " << global::komi_ratchet << ". Changing komi from " << global::komi << " to ";
	global::komi -= (montec::root.move.who == 1) ? -2 : 2;
	std::cerr << global::komi << std::endl;
      }
    }
    descend(alloc, &montec::root, seed);
  }
  std::cerr << "Descend ends" << std::endl;
  assert(pos.checkRootListOfMovesCorrectness(montec::root.children));
  //std::sort(montec::root.children.rbegin(), montec::root.children.rend());  // note: reverse iterators to sort descending
  int n = alloc.getSize(montec::root.children);
  if (n>1) {
    montec::root.children[n-1].markAsNotLast();
    std::sort(montec::root.children.load(), montec::root.children.load() + n, [](Treenode &t1, Treenode &t2) { return t1.t.playouts - t1.prior.playouts > t2.t.playouts - t2.prior.playouts; });
    montec::root.children[n-1].markAsLast();
  }
  std::cerr << "Sort ends, root.children.size()==" << n << ", root value = " << montec::root.t.value_sum/montec::root.t.playouts << ", root playouts = " << montec::root.t.playouts << std::endl;
  std::cerr << "root: " << montec::root.show() << std::endl;

  for (int i=0; /*i<15 &&*/ i<n; i++) {
    std::cerr << montec::root.children[i].show() << " value=" << montec::root.children[i].getValue() << std::endl;
    if (i==0) {
      //std::sort(montec::root.children[i].children.rbegin(), montec::root.children[i].children.rend());  // note: reverse iterators to sort descending
      int nn = alloc.getSize(montec::root.children[i].children);
      if (nn > 1) {
	montec::root.children[i].children[nn-1].markAsNotLast();
	std::sort(montec::root.children[i].children.load(), montec::root.children[i].children.load()+nn, [](Treenode &t1, Treenode &t2) { return t1.t.playouts - t1.prior.playouts > t2.t.playouts - t2.prior.playouts; });
	montec::root.children[i].children[nn-1].markAsLast();
      }
      for (int j=0; /*j<15 &&*/ j<nn; j++) {
	std::cerr << "   " << montec::root.children[i].children[j].show() <<  " value=" << montec::root.children[i].children[j].getValue() << std::endl;
      }
    }
  }
  if (montec::root.children != nullptr) {
    return montec::root.children[0].getMoveSgf();
  } else {
    return "";
  }
}

Treenode*
MonteCarlo::selectBestChild(Treenode *node) const
{
  Treenode *ch = node->children;
  Treenode *best = ch;
  real_t bestv = -1e5;
  for(;;) {
    assert(ch->amaf.playouts + ch->t.playouts > 0);
    real_t value = ch->getValue();
    //if (ch->move.who == 2) value = -value;
    if (value > bestv) {
      bestv = value;
      best = ch;
    }
    if (ch->isLast()) break;
    ch++;
  }
  return best;
}

std::shared_ptr<Game>
MonteCarlo::getCopyOfGame(Treenode *node) const
{
  if (std::atomic_load(&node->game_ptr) == nullptr) {
    assert(std::atomic_load(&node->parent->game_ptr) != nullptr);
    std::shared_ptr<Game> game_ptr = std::make_shared<Game>(*std::atomic_load(&node->parent->game_ptr));
    game_ptr->makeMove(node->move);
    std::atomic_store(&node->game_ptr, game_ptr);
  }
  return std::make_shared<Game>(*std::atomic_load(&node->game_ptr));
}

void
MonteCarlo::expandNode(TreenodeAllocator &alloc, Treenode *node, Game* game, int depth) const
{
  constexpr int max_depth_for_cnn = 3;
  game->generateListOfMoves(alloc, node, depth, node->move.who ^ 3);
  if (node->children == nullptr) {
    if (depth > max_depth_for_cnn) {
      node->children = alloc.getLastBlock();
    } else {
      auto lastBlock = alloc.getLastBlock();
      const std::lock_guard<std::mutex> lock(montec::cnn_mutex);
      if (node->children == nullptr) {
	updatePriors(*game, lastBlock, depth);
	node->children = lastBlock;
      }
    }
  }
  else {
    alloc.getLastBlock();
  }
#ifndef NDEBUG
  if (node == node->parent) {
    assert(game->checkRootListOfMovesCorrectness(node->children));
  }
#endif
}

void
MonteCarlo::descend(TreenodeAllocator &alloc, Treenode *node, unsigned seed)
{
  int depth = 1;
  std::shared_ptr<Game> game_ptr;
  for (;;) {
    if (node->children == nullptr) {
      game_ptr = getCopyOfGame(node);
    }
    bool expand = (depth == 1);
    if (node->children == nullptr && (expand || (node->t.playouts - node->prior.playouts) >= montec::MC_EXPAND_THRESHOLD)) {
      expandNode(alloc, node, game_ptr.get(), depth);
    }
    if (node->children == nullptr) {
      break;
    }
    node = selectBestChild(node);
    node->t.playouts += Game::VIRTUAL_LOSS;
    ++depth;
  }
  game_ptr->seedRandomEngine(seed);
  game_ptr->rollout(node, depth);
}

int
MonteCarlo::runSimulations(int max_iter_count, unsigned thread_no, unsigned threads_count)
{
  int komi_change_at = montec::start_increasing;
  TreenodeAllocator alloc;
  int i=0;
  std::cerr << "*** Starting ratchet: " << global::komi_ratchet << std::endl;
  for (;;) {
    if ((i & 0x7f) == 0) std::cerr << "thr " << thread_no << ", iteration = " << i << std::endl;
    if (thread_no == 0) {
      if (montec::iterations >= komi_change_at) {
	komi_change_at *= 6;
	if (montec::root.t.value_sum < montec::root.t.playouts * (1-montec::increase_komi_threshhold)) {  // green zone
	  int perspective = 2*montec::root.move.who - 3;   // -1 if we are white, 1 if black (root.move.who is the opponent)
	  std::cerr << "Green zone; komi = " << global::komi << ", perspective = " << perspective << ", ratchet = " << global::komi_ratchet << std::endl;
	  if (global::komi*perspective < global::komi_ratchet) {
	    std::cerr << "Changing komi from " << global::komi << " to ";
	    global::komi += (montec::root.move.who == 1) ? -2 : 2;
	    std::cerr << global::komi << std::endl;
	  }
	} else if (montec::root.t.value_sum > montec::root.t.playouts * (1-montec::decrease_komi_threshhold)) {  // red zone
	  int perspective = 2*montec::root.move.who - 3;   // -1 if we are white, 1 if black (root.move.who is the opponent)
	  std::cerr << "Red zone; komi = " << global::komi << ", perspective = " << perspective << ", ratchet = " << global::komi_ratchet << std::endl;
	  if (global::komi*perspective > 0) {
	    global::komi_ratchet = global::komi*perspective;
	  }
	  std::cerr << "New ratchet: " << global::komi_ratchet << ". Changing komi from " << global::komi << " to ";
	  global::komi -= (montec::root.move.who == 1) ? -2 : 2;
	  std::cerr << global::komi << std::endl;
	}
      }

    /*
      if (montec::iterations >= komi_change_at) {
	komi_change_at *= 4;
	if (montec::root.t.value_sum < montec::root.t.playouts * (1-montec::increase_komi_threshhold)) {
	  std::cerr << "Changing komi from " << global::komi << " to ";
	  global::komi += (montec::root.move.who == 1) ? -1 : 1;
	  std::cerr << global::komi << std::endl;
	}
      }
    */
    }
    unsigned seed = thread_no + threads_count * i;
    descend(alloc, &montec::root, seed);
    i++;  montec::iterations++;
    if (montec::iterations >= max_iter_count || montec::finish_sim) {
      break;
    }
  }
  montec::threads_to_be_finished--;
  if (montec::threads_to_be_finished == 0) {
    cv_finish_threads.notify_all();
  } else
  {
    // wait for other threads to finish their work
    std::unique_lock<std::mutex> ul(mutex_finish_threads);
    cv_finish_threads.wait(ul, [] { return montec::threads_to_be_finished == 0; });
  }

  // now the main function may read data, and the threads must wait (not to release memory in TreenodeAllocator)
  {
    std::unique_lock<std::mutex> ul(mutex_finish_threads);
    cv_finish_threads.wait(ul, [] { return montec::finish_threads; });
  }
  // the main function is done, we may exit
  return i;
}

std::string
MonteCarlo::findBestMoveMT(Game &pos, int threads, int iter_count, int msec)
{
  debug_previous_count = -1;
  montec::root = Treenode();
  montec::root.move = pos.getLastMove();
  montec::root.parent = &montec::root;
  montec::root.game_ptr = std::make_shared<Game>(pos);  
  std::cerr << "Descend starts, komi==" << global::komi << std::endl;
#ifdef DEBUG_SGF
  assert(0);  // SGF write is not thread-safe
#endif

  montec::iterations = 0;  montec::finish_sim = false;  montec::finish_threads = false;
  montec::generateMovesCount = 0;
  montec::threads_to_be_finished = threads;
  std::vector<std::future<int>> concurrent;
  concurrent.reserve(threads);
  for (int t=0; t<threads; t++) {
    concurrent.push_back( std::async(std::launch::async,  [=] { return runSimulations(iter_count, t, threads); }) );
  }
  auto time_begin = std::chrono::high_resolution_clock::now();
  if (msec > 0) {
    for (;;) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - time_begin).count();
      if (montec::iterations >= 100) {
	if (montec::root.children == nullptr || montec::root.children[0].isLast()) {
	  montec::finish_sim = true;
	  break;
	}
	if (4*duration > 3*msec) {
	  int best = montec::root.getBestChild()->t.playouts;
	  if (best > montec::iterations * (msec / (1.95 * duration))) {
	    montec::finish_sim = true;
	    break;
	  }
	}
	if (duration > msec) {
	  montec::finish_sim = true;
	  break;
	}
      }
      if (montec::threads_to_be_finished == 0) {
	montec::finish_sim = true;
	break;
      }
      // print performance
      if (duration > 0) {
	double speed = 1000.0 * montec::iterations / static_cast<double>(duration);
	std::cerr << "Speed: " << static_cast<unsigned>(speed) << " iter/s, for thread: " << static_cast<unsigned>(speed / threads) << std::endl;
      }
    }
  } else {
    while (montec::threads_to_be_finished > 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
#ifndef SPEED_TEST
      if (5*montec::iterations > 3*iter_count) {
	const Treenode *ch = montec::root.getBestChild();
	int best = (ch != nullptr) ? ch->t.playouts.load() : (iter_count + 21);
	if (best > 20 + iter_count/2) {
	  montec::finish_sim = true;
	  break;
	}
      }
      // print performance
      double duration = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - time_begin).count();
      if (duration > 0) {
	double speed =  montec::iterations / duration;
	std::cerr << "Speed: " << static_cast<unsigned>(speed) << " iter/s, for thread: " << static_cast<unsigned>(speed / threads) << std::endl;
      }
#endif
    }
  }
  // wait for threads to finish their work (we cannot kill them before reading best move,
  // because that would release the memory in TreenodeAllocators)
  {
    std::unique_lock<std::mutex> ul(mutex_finish_threads);
    cv_finish_threads.wait(ul, [] { return montec::threads_to_be_finished == 0; });
  }


  std::cerr << "Descend ends" << std::endl;
  assert(pos.checkRootListOfMovesCorrectness(montec::root.children));
  //std::sort(montec::root.children.rbegin(), montec::root.children.rend());  // note: reverse iterators to sort descending
  int n = TreenodeAllocator::getSize(montec::root.children);
  if (n>1) {
    montec::root.children[n-1].markAsNotLast();
    std::sort(montec::root.children.load(), montec::root.children.load() + n, [](Treenode &t1, Treenode &t2) { return t1.t.playouts - t1.prior.playouts > t2.t.playouts - t2.prior.playouts; });
    montec::root.children[n-1].markAsLast();
  }
  std::cerr << "Sort ends, root.children.size()==" << n << ", root value = " << montec::root.t.value_sum/montec::root.t.playouts << ", root playouts = " << montec::root.t.playouts << std::endl;
  constexpr int max_moves = 100;
  for (int i=0; i<max_moves && i<n; i++) {
    std::cerr << montec::root.children[i].show() << std::endl;
    if (i==0) {
      //std::sort(montec::root.children[i].children.rbegin(), montec::root.children[i].children.rend());  // note: reverse iterators to sort descending
      int nn = TreenodeAllocator::getSize(montec::root.children[i].children);
      if (nn > 1) {
	montec::root.children[i].children[nn-1].markAsNotLast();
	std::sort(montec::root.children[i].children.load(), montec::root.children[i].children.load()+nn, [](Treenode &t1, Treenode &t2) { return t1.t.playouts > t2.t.playouts; });
	montec::root.children[i].children[nn-1].markAsLast();
      }
      for (int j=0; j<max_moves && j<nn; j++) {
	std::cerr << "   " << montec::root.children[i].children[j].show() << std::endl;
      }
    }
  }
  if (n > max_moves) {
    std::cerr << "Other moves: ";
    for (int i=max_moves; i<n; ++i) {
      std::cerr << montec::root.children[i].move.show() << "  ";
    }
    std::cerr << std::endl;
  }
  {
    int real_playouts = 0;
    for (int i=0; i<n; i++) {
      real_playouts += montec::root.children[i].t.playouts - montec::root.children[i].prior.playouts;
    }
    std::cerr << "Real saved playouts: " << real_playouts << "; generateMovesCount: " << montec::generateMovesCount << std::endl;
  }

  std::string res = "";
  if (montec::root.children != nullptr) {
    res = montec::root.children[0].getMoveSgf();
  }

  // let the threads go to the end
  {
    std::lock_guard<std::mutex> lg(mutex_finish_threads);
    montec::finish_threads = true;
  }
  cv_finish_threads.notify_all();
  // and finish them completely
  for (int t=0; t<threads; t++) {
    int num = concurrent[t].get();
    std::cerr << "Thread " << t << ": sims = " << num << std::endl;
  }

  return res;
}


void
play_engine(Game &game, std::string &s, int threads_count, int iter_count, int msec)
{
  for (;;) {
    {
      MonteCarlo mc;
      start_time = std::chrono::high_resolution_clock::now();
      auto best_move = threads_count > 1 ?
	mc.findBestMoveMT(game, threads_count, iter_count, msec) :
	mc.findBestMove(game, iter_count);
      auto end_time = std::chrono::high_resolution_clock::now();
      std::cerr << std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count() << " mikros" << std::endl;
      std::cout << s.substr(0, s.rfind(")")) << best_move << ")" << std::endl;
    }

    std::string n(""), buf;
    do {
      std::getline(std::cin, buf);
      if (buf.length()>=3 && buf.substr(0,3) == std::string("END")) {
	return;
      }
      n += buf;
    } while (n.find(")") == std::string::npos);
    if (s.substr(0, s.length()-1) == n.substr(0, s.length()-1) and n.find(";", s.length()-1) != std::string::npos) {
      // the same beginning, add new moves
      game.show();
      std::string added = std::string("(") + n.substr(s.length()-1);
      std::cerr << "To sgf: " << s << std::endl;
      std::cerr << "I add moves: " << added << std::endl;
      SgfParser parser(added);
      auto seq = parser.parseMainVar();
      game.replaySgfSequence(seq, std::numeric_limits<int>::max());
    } else {
      std::cerr << "NEW GAME STARTED." << std::endl;
      SgfParser parser(n);
      auto seq = parser.parseMainVar();
      game = Game(seq, std::numeric_limits<int>::max());
    }
    s = n;
    //      while (s.back() == '\n') s.pop_back();
  }
}


void
findAndPrintBestMove(Game &game, int iter_count)
{
  MonteCarlo mc;
  start_time = std::chrono::high_resolution_clock::now();
  auto best_move = mc.findBestMove(game, iter_count);
  //auto best_move = mc.findBestMoveMT(game, threads_count, iter_count, 0);
  auto end_time = std::chrono::high_resolution_clock::now();
  std::cerr << std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count()
	    << " mikros" << std::endl;
  std::cerr << "debug: " << debug_nanos/1000 << " mikros" << std::endl;
  std::cerr << "debug2: " << debug_nanos2/1000 << " mikros" << std::endl;
  std::cerr << "debug3: " << debug_nanos3/1000 << " mikros" << std::endl;
  std::cerr << "All threats2m: " << debug_allt2m << ", skipped: " << debug_skippedt2m << ", small singular: " << debug_sing_smallt2m
            << ", large singular: " << debug_sing_larget2m
            << ", found before: " << debug_foundt2m << ". n= " << debug_n << ", N=" << debug_N <<  std::endl;
}

void
playInteractively(Game &game, int threads_count, int iter_count)
{
  for (;;) {
    // get input
    std::string buf, error_info = "";
    std::getline(std::cin, buf);
    CommandParser comm_parser;
    try {
      auto commands = comm_parser.parse(buf);
      if (commands[0] == "threads") {
	threads_count = std::stoi(commands[1]);
	if (threads_count <= 0) threads_count = 1;
	std::cerr << "set threads to " << threads_count << std::endl;
      } else if (commands[0] == "iters") {
	iter_count = std::stoi(commands[1]);
	if (iter_count <= 0) iter_count = 1;
	std::cerr << "set iterations to " << iter_count << std::endl;
      } else if (commands[0] == "first") {
      } else if (commands[0] == "second") {
      } else if (commands[0] == "help" or commands[0] == "?") {
	std::cerr << "Type 'show', 'quit' or 'bye', coordinate(s) to play at given point (and enclose next points)" << std::endl
		  << "'move' to make a move by AI, 'threads', 'iters' to change AI settings" << std::endl;
      } else if (commands[0] == "new") {
      } else if (commands[0] == "move") {
	{
	  MonteCarlo mc;
	  start_time = std::chrono::high_resolution_clock::now();
	  auto best_move = threads_count > 1 ?
	    mc.findBestMoveMT(game, threads_count, iter_count, 0) :
	    mc.findBestMove(game, iter_count);
	  auto end_time = std::chrono::high_resolution_clock::now();
	  std::cerr << std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count() << " mikros" << std::endl;
						  auto l = best_move.find_first_of('[');
						  auto r = best_move.find_last_of(']');
						  if (l < r) {
						    best_move = best_move.substr(l+1, r-l-1);
						  }
						  std::cerr << "best move: " << best_move << std::endl;
						  game.makeSgfMove(best_move, game.whoNowMoves());
						  }

	} else if (commands[0] == "quit" || commands[0] ==  "bye") {
	  break;
	} else if (commands[0] == "back") {

	} else if (commands[0] == "show") {
	  game.show();
	} else if (commands[0] == "_play") {
	  // get coordinates, in [1] point to play, in [2...] points to enclose (if any)
	  std::string s = commands[1];
	  for (unsigned i=2; i<commands.size(); ++i) {
	    s += "!" + commands[i];
	  }
	  game.makeSgfMove(s, game.whoNowMoves());

	}
      } catch (const std::runtime_error& e) {
	std::cerr << e.what() << std::endl;
      }

    }
}
