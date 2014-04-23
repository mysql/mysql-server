/*
   Copyright (c) 2012, 2014, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/
#include <fstream>
#include <stdint.h>
#include "auth_utils.h"

using namespace std;
/**
  Lazy whitespace trimmer
*/
void trim(string *s)
{
  stringstream trimmer;
  trimmer << *s;
  s->clear();
  trimmer >> *s;
}

int parse_cnf_file(istream &sin, map<string, string > *options,
                     const string &header)
{
  string option_name;
  string option_value;
  string token_header;
  token_header.append("[").append(header).append("]");
  try{
  while(true)
  {
    string row;
    getline(sin, row);
    trim(&row);
    if (row == token_header)
      break;
    else if (sin.eof())
      return ERR_SYNTAX;
  }

  while (!getline(sin, option_name, '=').eof())
  {
    trim(&option_name);
    if (option_name[0] == '[')
      break;
    getline(sin, option_value);
    trim(&option_value);
    if (option_name.length() > 0)
      options->insert(make_pair<string, string >(option_name, option_value));
  }
  return ALL_OK;
  } catch(...)
  {
    return ERR_SYNTAX;
  }
}

int decrypt_login_cnf_file(istream &fin, ostream &sout)
{
  try {
  fin.seekg(4, fin.beg);
  char rkey[20];
  fin.read(rkey, 20);
  while(true)
  {
    uint32_t len;
    fin.read((char*)&len,4);
    if (len == 0 || fin.eof())
      break;
    char *cipher= new char[len];
    fin.read(cipher, len);
    char plain[1024];
    int aes_length;
    aes_length= my_aes_decrypt((const unsigned char *) cipher, len,
                               (unsigned char *) plain,
                               (const unsigned char *) rkey,
                               20, my_aes_128_ecb, NULL);
    plain[aes_length]= 0;
    sout << plain;

  }
  return ALL_OK;

  } catch(...)
  {
    return ERR_ENCRYPTION;
  }
}

const string g_allowed_pwd_chars("qwertyuiopasdfghjklzxcvbnm,.-1234567890+*"
                                 "QWERTYUIOPASDFGHJKLZXCVBNM;:_!#%&/()=?><");
const string get_allowed_pwd_chars() { return g_allowed_pwd_chars; }

void generate_password(string *password, int size)
{
  stringstream ss;
  rand_struct srnd;
  while(size > 0)
  {
    int ch= ((int)(my_rnd_ssl(&srnd)*100))%get_allowed_pwd_chars().size();
    ss << get_allowed_pwd_chars()[ch];
    --size;
  }
  password->assign(ss.str());
}
