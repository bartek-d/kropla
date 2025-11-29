/********************************************************************************************************
 kropla -- a program to play Kropki; file patterns.cc.
    Copyright (C) 2015,2016,2017,2018,2019 Bartek Dyda,
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

#include "patterns.h"

#include <fstream>

/********************************************************************************************************
  Wildcards replacer -- class for substituting wildcards in patterns
*********************************************************************************************************/
class WildcardReplacer
{
    std::string orig_str, replaced;
    bool end;
    std::vector<unsigned> current;
    std::vector<std::string> possibilities;

   public:
    WildcardReplacer(std::string s, int length, bool with_atari);
    WildcardReplacer &operator++();    // prefix ++
    WildcardReplacer operator++(int);  // postfix ++
    bool isAtEnd() const { return end; };
    const std::string &toStr() const { return replaced; };
};

WildcardReplacer::WildcardReplacer(std::string s, int length, bool with_atari)
    : orig_str{s}, replaced{}, end{false}
{
    static const std::map<char, std::string> wildcards = {
        {'?', ".YQ#|@"}, {'*', ".YQ|@"}, {'x', ".Q#@"},  {'o', ".Y#|"},
        {'X', "Y|"},     {'O', "Q@"},    {'y', ".Q#|@"}, {'q', ".Y#|@"},
        {'=', ".YQ#@"},  {'0', ".YQ#|"}};
    current.assign(length, 0);
    for (int i = 0; i < length; ++i)
    {
        if (s.at(i) == 'H')
        {
            possibilities.push_back("H");
            replaced += 'H';
            continue;
        }
        auto wp = wildcards.find(s.at(i));
        if (wp != wildcards.end())
        {
            if (with_atari)
            {
                possibilities.push_back(wp->second);
            }
            else
            {
                std::string pos;
                for (const auto c : wp->second)
                {
                    if (c != '|' && c != '@') pos.push_back(c);
                }
                possibilities.push_back(std::move(pos));
            }
        }
        else
        {
            possibilities.push_back(std::string(1, s.at(i)));
        }
        replaced += possibilities.back().at(0);
    }
}

WildcardReplacer &WildcardReplacer::operator++()  // prefix ++
{
    if (end) return *this;
    for (int i = current.size() - 1; i >= 0; --i)
    {
        if (possibilities[i].size() > 1)
        {
            ++current[i];
            if (current[i] == possibilities[i].size()) current[i] = 0;
            replaced.at(i) = possibilities[i][current[i]];
            if (current[i]) return *this;
        }
    }
    end = true;
    return *this;
}

WildcardReplacer WildcardReplacer::operator++(
    int)  // postfix ++, not really needed
{
    WildcardReplacer tmp(*this);
    operator++();
    return tmp;
}

/********************************************************************************************************
  Pattern3 class for handling 3x3 patterns
*********************************************************************************************************/

Pattern3::Pattern3() = default;
Pattern3::Pattern3(const std::vector<std::string> &vs, int type)
{
    generate(vs, type);
}

Pattern3::Pattern3(const std::string &filename, pattern3_t v)
{
    readFromFile(filename);
    setEmptyValue(v);
}

/// Rotates clockwise.
pattern3_t Pattern3::rotate(pattern3_t p)
{
    return ((p & 0xfff) << 4) | ((p & 0xf000) >> 12) | ((p & 0x70000) << 1) |
           ((p & 0x80000) >> 3);
}

/// Reflects with respect to the horizontal axis.
pattern3_t Pattern3::reflect(pattern3_t p)
{
    int n[8], a[4];  // neighbours, atari
    pattern3_t mask = 3;
    int shift = 0;
    for (int i = 0; i < 8; i++)
    {
        n[i] = (p & mask) >> shift;
        mask <<= 2;
        shift += 2;
    }
    mask = 0x10000;
    shift = 16;
    for (int i = 0; i < 4; i++)
    {
        a[i] = (p & mask) >> shift;
        mask <<= 1;
        shift++;
    }
    std::swap(n[0], n[2]);
    std::swap(n[3], n[7]);
    std::swap(n[4], n[6]);
    std::swap(a[1], a[3]);
    p = 0;
    shift = 0;
    for (int i = 0; i < 8; i++)
    {
        p |= (n[i] << shift);
        shift += 2;
    }
    for (int i = 0; i < 4; i++)
    {
        p |= (a[i] << shift);
        shift++;
    }
    return p;
}

pattern3_t Pattern3::reverseColour(pattern3_t p)
{
    /* Reverse color assignment - achieved by swapping odd and even bits (from
     * Pachi) */
    return ((p >> 1) & 0x5555) | ((p & 0x5555) << 1) | (p & 0xf0000);
}

/// Adds up to 8 patterns, p and its rotations and reflections.
void Pattern3::addPatterns(pattern3_t p, pattern3_val value)
{
    for (int j = 0; j < 2; j++)
    {
        for (int i = 0; i < 4; i++)
        {
            // std::cerr << show(p) << "  value = " << value << std::endl;
            if (add_type == TYPE_REPLACE)
            {
                if (value > 0)
                {
                    if (values[p] == 0) values[p] = value;
                }
                else if (value < 0)
                {
                    values[p] = value;
                }
            }
            else
            {
                values[p] = std::max(value, values[p]);
            }
            //  if (values[p]>=0)  // old code (v103-)
            //    values[p] = (value < 0) ? value : std::max(value, values[p]);
            //    // TODO: maybe sum would be better?
            p = rotate(p);
        }
        p = reflect(p);
    }
}

/// s should have only atoms!
pattern3_t Pattern3::getCodeOfPattern(const std::string &s)
{
    pattern3_t p = 0;
    const int field_bit[9] = {12, 14, 0, 10, -1, 2, 8, 6, 4};
    const int atari_bit[9] = {-1, 19, -1, 18, -1, 16, -1, 17, -1};
    for (int i = 0; i < 9; i++)
    {
        if (field_bit[i] < 0) continue;
        switch (s.at(i))
        {
            case '.':
                // 0 value, nothing to do
                break;
            case '|':
                // dot of 1 in atari
                if (atari_bit[i] >= 0) p |= (1 << atari_bit[i]);
                [[fallthrough]];
            case 'Y':
                // dot of 1 not in atari
                p |= (1 << field_bit[i]);
                break;
            case '@':
                // dot of 2 in atari
                if (atari_bit[i] >= 0) p |= (1 << atari_bit[i]);
                [[fallthrough]];
            case 'Q':
                // dot of 2 not in atari
                p |= (2 << field_bit[i]);
                break;
            case '#':
                // outside
                p |= (3 << field_bit[i]);
                break;
            default:
                assert(0);  // unexpected character!
        }
    }
    return p;
}

void Pattern3::generateFromStr(const std::string &sarg, pattern3_val value)
{
    for (WildcardReplacer wr(sarg, 9, true); !wr.isAtEnd(); ++wr)
    {
        const std::string &s = wr.toStr();
        // std::cerr << sarg << "-->" << s << " value=" << value << std::endl;
        // here s has only atoms
        pattern3_t p = getCodeOfPattern(s);
        addPatterns(p, value);
        if (sarg.length() < 10 || sarg.at(9) != 'X')
        {
            addPatterns(reverseColour(p), value);
        }
    }
}

pattern3_val Pattern3::getValue(pattern3_t p, int who) const
{
    assert(p >= 0 && p < PATTERN3_SIZE);
    if (who == 1)
        return values[p];
    else
        return values[reverseColour(p)];
}

void Pattern3::generate(const std::vector<std::string> &vs, int type)
{
    add_type = type;
    for (unsigned i = 0; i < vs.size() - 1; i += 2)
    {
        generateFromStr(vs[i], stoi(vs[i + 1]));
    }
}

void Pattern3::readFromFile(const std::string &filename)
{
    std::ifstream file(filename, std::ifstream::binary);
    file.read(reinterpret_cast<char *>(values.data()), sizeof(values));
    if (not file)
        throw std::runtime_error("Reading patterns from " + filename +
                                 " failed");
}

std::string Pattern3::show(pattern3_t p)
{
    const int field_bit[9] = {12, 14, 0, 10, -1, 2, 8, 6, 4};
    const int atari_bit[9] = {-1, 19, -1, 18, -1, 16, -1, 17, -1};
    std::stringstream out;
    for (int i = 0; i < 9; i++)
    {
        int val = -1;
        if (field_bit[i] >= 0)
        {
            val = (p >> field_bit[i]) & 3;
            if (atari_bit[i] >= 0) val |= (p >> (atari_bit[i] - 2)) & 4;
        }
        const char symbols[9] = {
            '-',                  // centre point
            '.', 'Y', 'Q', '#',   // no atari values
            'E', '|', '@', 'F'};  // atari values; E,F denote invalid values
        out << std::setw(2) << symbols[val + 1];
        if (i % 3 == 2) out << std::endl;
    }
    return out.str();
}

void Pattern3::showCode() const
{
    std::cerr << " = { " << std::endl;
    int i = 0;
    for (auto p : values)
    {
        std::cerr << "  " << p << ", ";
        if ((++i & 7) == 0) std::cerr << std::endl;
    }
    std::cerr << " };" << std::endl;
}

/********************************************************************************************************
  Pattern52 class for handling 5xx patterns
*********************************************************************************************************/
typedef uint32_t pattern52_t;

pattern52_t Pattern52::reflect(pattern52_t p) const
{
    return ((p & 0x4c00) >> 10) | ((p & 0xff) << 10) | (p & 0x300);
}

pattern52_t Pattern52::reverseColour(pattern52_t p) const
{
    /* Reverse color assignment - achieved by swapping odd and even bits */
    return ((p >> 1) & 0x15555) | ((p & 0x15555) << 1);
}

/// Adds up to 2 patterns, p and its reflection.
void Pattern52::addPatterns(pattern52_t p, real_t value)
{
    if (values[p] >= 0)
        values[p] =
            (value < 0)
                ? value
                : std::max(value,
                           values[p]);  // TODO: maybe sum would be better?
    p = reflect(p);
    if (values[p] >= 0)
        values[p] =
            (value < 0)
                ? value
                : std::max(value,
                           values[p]);  // TODO: maybe sum would be better?
}

void Pattern52::generateFromStr(const std::string &sarg, real_t value)
{
    for (WildcardReplacer wr(sarg, 10, false); !wr.isAtEnd(); ++wr)
    {
        const std::string &s = wr.toStr();
        // std::cerr << sarg << "-->" << s << " value=" << value << std::endl;
        // here s has only atoms
        pattern52_t p = 0;
        // note: field_bit[2]==field_bit[7]==8, because bits 8-9 may be encoded
        // either in s[2] or s[7]
        const int field_bit[10] = {0, 4, 8, 10, 14, 2, 6, 8, 12, 16};
        for (int i = 0; i < 10; i++)
        {
            if (s.at(i) == 'H') continue;
            switch (s.at(i))
            {
                case '.':
                    // 0 value, nothing to do
                    break;
                case 'Y':
                    // dot of 1 not in atari
                    p |= (1 << field_bit[i]);
                    break;
                case 'Q':
                    // dot of 2 not in atari
                    p |= (2 << field_bit[i]);
                    break;
                case '#':
                    // outside
                    p |= (3 << field_bit[i]);
                    break;
            }
        }
        addPatterns(p, value);
        if (sarg.length() < 11 || sarg.at(10) != 'X')
        {
            addPatterns(reverseColour(p), value);
        }
    }
}

real_t Pattern52::getValue(pattern52_t p, int who) const
{
    if (who == 1)
        return values[p];
    else
        return values[reverseColour(p)];
}

void Pattern52::generate(const std::vector<std::string> &vs)
{
    for (unsigned i = 0; i < vs.size() - 1; i += 2)
    {
        generateFromStr(vs[i], stof(vs[i + 1]));
    }
}

std::string Pattern52::show(pattern52_t p) const
{
    const int field_bit[10] = {0, 4, 8, 10, 14, 2, 6, 8, 12, 16};
    std::stringstream out;
    for (int i = 0; i < 10; i++)
    {
        int val = -1;
        if (field_bit[i] >= 0)
        {
            val = (p >> field_bit[i]) & 3;
        }
        char symbols[9] = {
            '-',                  // centre point
            '.', 'Y', 'Q', '#',   // no atari values
            'E', '|', '@', 'F'};  // atari values; E,F denote invalid values
        out << std::setw(2) << symbols[val + 1];
        if (i % 5 == 4) out << std::endl;
    }
    return out.str();
}

void Pattern52::showCode() const
{
    std::cerr << " = { " << std::endl;
    int i = 0;
    for (auto p : values)
    {
        std::cerr << "  " << p << ", ";
        if ((++i & 7) == 0) std::cerr << std::endl;
    }
    std::cerr << " };" << std::endl;
}
