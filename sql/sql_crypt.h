/* Copyright (C) 2000-2001, 2005 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


#ifdef USE_PRAGMA_INTERFACE 
#pragma interface			/* gcc class implementation */
#endif

class SQL_CRYPT :public Sql_alloc
{
  struct rand_struct rand,org_rand;
  char decode_buff[256],encode_buff[256];
  uint shift;
 public:
  SQL_CRYPT() {}
  SQL_CRYPT(ulong *seed)
  {
    init(seed);
  }
  ~SQL_CRYPT() {}
  void init(ulong *seed);
  void reinit() { shift=0; rand=org_rand; }
  void encode(char *str, uint length);
  void decode(char *str, uint length);
};
