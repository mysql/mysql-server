/*
   Copyright (c) 2012, 2016, Oracle and/or its affiliates. All rights reserved.

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
#include "client_priv.h"
#include "my_rnd.h"
#include "my_aes.h"
#include <sstream>
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
      return ERR_NO_SUCH_CATEGORY;
  }

  while (!getline(sin, option_name, '=').eof())
  {
    trim(&option_name);
    if (option_name[0] == '[')
      break;
    getline(sin, option_value);
    trim(&option_value);
    if (option_name.length() > 0)
      options->insert(make_pair(option_name, option_value));
  }
  return ALL_OK;
  } catch(...)
  {
    return ERR_SYNTAX;
  }
}

#define MAX_CIPHER_LEN 4096
#define MAX_CIPHER_STORE_LEN 4U
#define LOGIN_KEY_LEN 20U

int decrypt_login_cnf_file(istream &fin, ostream &sout)
{
  try {
  fin.seekg(MAX_CIPHER_STORE_LEN, fin.beg);
  char rkey[LOGIN_KEY_LEN];
  fin.read(rkey, LOGIN_KEY_LEN);
  while(true)
  {
    int len;
    char len_buf[MAX_CIPHER_STORE_LEN];
    char cipher[MAX_CIPHER_LEN];
    fin.read(len_buf, MAX_CIPHER_STORE_LEN);
    len= sint4korr(len_buf);
    if (len == 0 || fin.eof())
      break;
    if (len > MAX_CIPHER_LEN)
      return ERR_ENCRYPTION;
    fin.read(cipher, len);
    char plain[MAX_CIPHER_LEN+1];
    int aes_length;
    aes_length= my_aes_decrypt((const unsigned char *) cipher, len,
                               (unsigned char *) plain,
                               (const unsigned char *) rkey,
                               LOGIN_KEY_LEN, my_aes_128_ecb, NULL);
    if ((aes_length > MAX_CIPHER_LEN) || (aes_length <= 0))
      return ERR_ENCRYPTION;
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
