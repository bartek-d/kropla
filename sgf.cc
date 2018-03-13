/********************************************************************************************************
 kropla -- a program to play Kropki; file sgf.cc
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


#include <utility>   // std::pair
#include <vector>
#include <string>
#include <memory>   // unique pointer
#include <list> 
#include <sstream>
#include <algorithm>
#include <cassert>

#include "sgf.h"

/********************************************************************************************************
  SgfParser class and classes for keeping the sgf.
*********************************************************************************************************/

std::string
SgfNode::toString(bool mainVar) const
{
  std::stringstream out;
  out << ";";
  for (auto &p : props) {
    out << p.first;
    for (auto &e : p.second) {
      out << "[" << e << "]";
    }
  }
  if (mainVar) {
    return out.str() + (!children.empty() ? children.front()->toString(mainVar) : "");
  } else {
    std::stringstream ch_out;
    int children_count = 0;
    for (auto &ch : children) {
      if (children_count >= 1) ch_out << ")(";
      ch_out << ch->toString(mainVar);
      ++children_count;
    }
    if (children_count >= 2) {
      return out.str() + "(" + ch_out.str() + ")";
    } else {
      return out.str() + ch_out.str();
    }
  }
}

std::vector<SgfProperty>::iterator
SgfNode::findProp(const std::string pr)
{
  return std::find_if(props.begin(), props.end(),
		      [pr](SgfProperty &p) { return p.first == pr; });
}


int
SgfParser::eatChar()
{
  if (pos < input.length())
    return input[pos++];
  throw std::runtime_error("eatChar: Unexpected end of string");
}

int
SgfParser::checkChar() const
{
  if (pos < input.length())
    return input[pos];
  return eof;
}

void
SgfParser::eatWS()
{
  while (pos < input.length()) {
    if (std::isspace(input[pos]))
      pos++;
    else
      break;
  }
}

std::string
SgfParser::propValue()
// "[" + string + "]", escape character: backslash
{
  std::string s;
  eatWS();
  int c = eatChar();
  if (c!='[')  throw std::runtime_error("propValue: ']' expected");
  for (;;) {
    c = eatChar();
    switch (c) {
    case ']' :
      return s;
    case '\\':
      s += eatChar();
      break;
    default:
      s += c;
      break;
    }
  }
}

std::string
SgfParser::propIdent()
// sequence of UPPERCASE letters
{
  std::string s;
  eatWS();
  while (checkChar()!= eof && std::isupper(checkChar())) {
    s += eatChar();
  }
  if (!s.empty())
    return s;
  throw std::runtime_error("propIdent: uppercase letter expected");
}

SgfProperty
SgfParser::property()
{
  std::string id = propIdent();
  std::vector<std::string> values;
  values.push_back(propValue());
  for (;;) {
    eatWS();
    if (checkChar() == '[') 
      values.push_back(propValue());
    else break;
  }
  return SgfProperty(id, values);
}


SgfNode
SgfParser::node()
{
  SgfNode n;
  eatWS();
  int c = eatChar();
  if (c!=';')  throw std::runtime_error("node: ';' expected");
  for (;;) {
    eatWS();
    if (checkChar()!= eof && std::isupper(checkChar())) {
      n.props.push_back(property());
    } else break;
  }
  return n;
}

SgfSequence
SgfParser::parseMainVar()
{
  SgfSequence seq;
  eatWS();
  int c = eatChar();
  if (c!='(')  throw std::runtime_error("parseMainVar: '(' expected");
  for (;;) {
    seq.push_back(node());
    eatWS();
    switch (checkChar()) {
    case ';':
      break;
    case '(':
      eatChar();
      break;
    case ')':
      return seq;
    default:
      throw std::runtime_error("parseMainVar: unexpected character");
    }
  }
}
  

/********************************************************************************************************
  Sgf tree
*********************************************************************************************************/

SgfTree::SgfTree()
{
  SgfNode node;
  node.props.push_back( {"FF", {"4"}});
  node.props.push_back( {"GM", {"40"}});
  node.props.push_back( {"CA", {"UTF-8"}});
  node.props.push_back( {"AP", {"board.cc:SgfTree"}});
  node.props.push_back( {"RU", {"Punish=0,Holes=1,AddTurn=0,MustSurr=0,MinArea=0,Pass=0,Stop=0,LastSafe=0,ScoreTerr=0,InstantWin=0"}});
  root = std::make_shared<SgfNode>(node);
  root->parent = root;  // not quite correct...
  cursor = root;
  partial_move.first.clear();
  //assert(partial_move.first.empty());
}

void
SgfTree::changeBoardSize(int x, int y)
{
  root->props.push_back({"SZ", {(x==y) ? std::to_string(x) : (std::to_string(x) + ":" + std::to_string(y))}});
}

void
SgfTree::addChild(std::vector<SgfProperty> &&prs, bool as_first)
{
  SgfNode node;
  node.props = std::move(prs);
  node.parent = cursor;
  if (as_first)
    cursor.lock()->children.push_front(std::make_shared<SgfNode>(node));
  else
    cursor.lock()->children.push_back(std::make_shared<SgfNode>(node));
}

void
SgfTree::addComment(std::string cmt)
{
  comment = cmt;
}

void
SgfTree::addProperty(SgfProperty prop)
{
  cursor.lock()->props.push_back(prop);
}


void
SgfTree::makeMove(SgfProperty move)
{
  auto pos = std::find_if(cursor.lock()->children.begin(), cursor.lock()->children.end(),
			  [move](std::shared_ptr<SgfNode> &s) { auto it = s->findProp(move.first); return (it != s->props.end() && it->second[0] == move.second[0]); } );
  if (pos == cursor.lock()->children.end()) {
    addChild({move}, true);
    cursor = cursor.lock()->children.front();
  } else {
    std::rotate(cursor.lock()->children.begin(), pos, std::next(pos));
    cursor = *cursor.lock()->children.begin();
  }
  if (!comment.empty()) {
    cursor.lock()->props.push_back({ "C", {comment}});
    comment.clear();
  }
}

void
SgfTree::makePartialMove(SgfProperty move)
{
  if (!partial_move.first.empty()) {
    makeMove(partial_move);
  }
  partial_move = move;
}

void
SgfTree::makePartialMove_addEncl(std::string sgf_encl)
{
  assert(!partial_move.first.empty());
  partial_move.second[0] += sgf_encl;
}

void
SgfTree::finishPartialMove()
{
  if (!partial_move.first.empty()) {
    makeMove(partial_move);
    partial_move.first.clear();
  }
}

void
SgfTree::saveCursor()
{
  saved_cursor = cursor;
}

void
SgfTree::restoreCursor()
{
  cursor = saved_cursor;
}


std::string
SgfTree::toString(bool mainVar)
{
  finishPartialMove();
  return "(" + root->toString(mainVar) + ")";
}

std::string
SgfTree::toString_debug(bool mainVar)
{
  // finishPartialMove();  in debug version
  return "(" + root->toString(mainVar) + ")";
}

