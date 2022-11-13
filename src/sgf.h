/********************************************************************************************************
 kropla -- a program to play Kropki; file sgf.h
    Copyright (C) 2015,2016,2017,2018 Bartek Dyda,
    email: bartekdyda (at) protonmail (dot) com

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

#include <list>
#include <memory>   // unique pointer
#include <utility>  // std::pair
#include <vector>

/********************************************************************************************************
  SgfParser class and classes for keeping the sgf.
*********************************************************************************************************/
typedef std::pair<std::string, std::vector<std::string> > SgfProperty;

struct SgfNode
{
    std::vector<SgfProperty> props;
    std::list<std::shared_ptr<SgfNode> > children;
    std::weak_ptr<SgfNode> parent;
    SgfNode() : props(), children(), parent(){};
    std::string toString(bool mainVar = false) const;
    std::vector<SgfProperty>::iterator findProp(const std::string& pr);
};

typedef std::vector<SgfNode> SgfSequence;

class SgfParser
{
    std::string input;
    unsigned pos{0};
    const static int eof = -1;
    int eatChar();
    int checkChar() const;
    void eatWS();
    std::string propValue();
    std::string propIdent();
    SgfProperty property();
    SgfNode node();
    std::string getDebugInfo() const;

   public:
    SgfParser(const std::string& s) : input{s} {}
    SgfSequence parseMainVar();
};

/********************************************************************************************************
  Sgf tree
*********************************************************************************************************/
class SgfTree
{
    std::shared_ptr<SgfNode> root;
    std::weak_ptr<SgfNode> cursor;
    std::weak_ptr<SgfNode> saved_cursor;
    SgfProperty partial_move;
    std::string comment;

   public:
    SgfTree();
    void changeBoardSize(int x, int y);
    void addChild(std::vector<SgfProperty>&& prs, bool as_first = false);
    void addComment(const std::string& cmt);
    void addProperty(SgfProperty prop);
    void makeMove(SgfProperty move);
    void makePartialMove(SgfProperty move);
    void makePartialMove_addEncl(const std::string& sgf_encl);
    void finishPartialMove();
    void saveCursor();
    void restoreCursor();
    std::string toString(bool mainVar = false);
    std::string toString_debug(bool mainVar = false);
};
