#pragma once

#include "board.h"
#include "game.h"
#include <string>

std::string constructSgfFromGameBoard(const std::string& board);
std::string applyIsometry(const std::string sgfCoord, unsigned isometry, Coord &coord);
Game constructGameFromSgfWithIsometry(const std::string& sgf, unsigned isometry);
