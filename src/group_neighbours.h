/********************************************************************************************************
 kropla -- a program to play Kropki; file group_neighbours.h.
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
#pragma once

#include "game.h"

struct GroupNeighbours {
  GroupNeighbours() = delete;
  GroupNeighbours(Game& game, std::vector<uint8_t>& neighbours, pti group_id, pti forbidden_point, uint8_t mask, int who);
  bool isGroupClose(pti group_id) const;
  std::vector<pti> neighbours_list{};
  std::vector<pti> neighbour_groups{};
private:
  void addPointIfItIsNeighbour(const std::array<pti, 4>& groups, pti group_id, std::vector<uint8_t>& neighbours, uint8_t mask, pti i);
  void addNeighbourGroups(const std::array<pti, 4>& groups, pti group_id);
};
