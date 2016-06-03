/*
 * Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef _TOKENIZER_H_
#define _TOKENIZER_H_

#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <stdexcept>


// Avoid warnings from includes of other project and protobuf
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#elif defined _MSC_VER
#pragma warning (push)
#pragma warning (disable : 4018 4996)
#endif

#include <boost/function.hpp>

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#pragma GCC diagnostic pop
#elif defined _MSC_VER
#pragma warning (pop)
#endif

namespace mysqlx
{
  class Token
  {
  public:
    enum TokenType
    {
      NOT = 1,
      AND = 2,
      OR = 3,
      XOR = 4,
      IS = 5,
      LPAREN = 6,
      RPAREN = 7,
      LSQBRACKET = 8,
      RSQBRACKET = 9,
      BETWEEN = 10,
      TRUE_ = 11,
      T_NULL = 12,
      FALSE_ = 13,
      IN_ = 14,
      LIKE = 15,
      INTERVAL = 16,
      REGEXP = 17,
      ESCAPE = 18,
      IDENT = 19,
      LSTRING = 20,
      LNUM = 21,
      DOT = 22,
      //AT = 23,
      COMMA = 24,
      EQ = 25,
      NE = 26,
      GT = 27,
      GE = 28,
      LT = 29,
      LE = 30,
      BITAND = 31,
      BITOR = 32,
      BITXOR = 33,
      LSHIFT = 34,
      RSHIFT = 35,
      PLUS = 36,
      MINUS = 37,
      MUL = 38,
      DIV = 39,
      HEX = 40,
      BIN = 41,
      NEG = 42,
      BANG = 43,
      MICROSECOND = 44,
      SECOND = 45,
      MINUTE = 46,
      HOUR = 47,
      DAY = 48,
      WEEK = 49,
      MONTH = 50,
      QUARTER = 51,
      YEAR = 52,
      PLACEHOLDER = 53,
      DOUBLESTAR = 54,
      MOD = 55,
      AS = 56,
      ASC = 57,
      DESC = 58,
      CAST = 59,
      CHARACTER = 60,
      SET = 61,
      CHARSET = 62,
      ASCII = 63,
      UNICODE = 64,
      BYTE = 65,
      BINARY = 66,
      CHAR = 67,
      NCHAR = 68,
      DATE = 69,
      DATETIME = 70,
      TIME = 71,
      DECIMAL = 72,
      SIGNED = 73,
      UNSIGNED = 74,
      INTEGER = 75,  // 'integer' keyword
      LINTEGER = 76, // integer number
      DOLLAR = 77,
      JSON = 78,
      COLON = 79,
      LCURLY = 80,
      RCURLY = 81,
      ARROW = 82,
      QUOTE = 83
    };

    Token(Token::TokenType type, const std::string& text, int cur_pos);

    const std::string& get_text() const { return _text; }
    TokenType get_type() const { return _type; }
    int get_pos() const { return _pos; }
  private:
    TokenType _type;
    std::string _text;
    int _pos;
  };

  class Tokenizer
  {
  public:
    Tokenizer(const std::string& input);

    typedef std::vector<Token> tokens_t;

    bool next_char_is(tokens_t::size_type i, int tok);
    void assert_cur_token(Token::TokenType type);
    bool cur_token_type_is(Token::TokenType type);
    bool next_token_type(Token::TokenType type);
    bool pos_token_type_is(tokens_t::size_type pos, Token::TokenType type);
    const std::string& consume_token(Token::TokenType type);
    const Token& peek_token();
    void unget_token();
    void inc_pos_token();
    int get_token_pos() { return _pos; }
    const Token& consume_any_token();
    void assert_tok_position();
    bool tokens_available();
    bool is_interval_units_type();
    bool is_type_within_set(const std::set<Token::TokenType>& types);

    std::vector<Token>::const_iterator begin() const { return _tokens.begin(); }
    std::vector<Token>::const_iterator end() const { return _tokens.end(); }

    void get_tokens();
    const std::string& get_input() { return _input; }

  protected:
    std::vector<Token> _tokens;
    std::string _input;
    tokens_t::size_type _pos;

  public:

    struct Cmp_icase
    {
      bool operator()(const std::string& lhs, const std::string& rhs) const;
    };

    struct Maps
    {
      typedef std::map<std::string, Token::TokenType, Cmp_icase> reserved_words_t;
      reserved_words_t reserved_words;
      std::set<Token::TokenType> interval_units;
      std::map<std::string, std::string, Cmp_icase> operator_names;
      std::map<std::string, std::string, Cmp_icase> unary_operator_names;

      Maps();
    };

  public:
    static Maps map;
  };

  class Parser_error : public std::runtime_error
  {
  public:
    Parser_error(const std::string& msg) : std::runtime_error(msg)
    {
    }
  };
}

#endif
