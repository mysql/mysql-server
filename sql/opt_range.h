/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


/* classes to use when handling where clause */

#ifndef _opt_range_h
#define _opt_range_h

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

#define NO_MIN_RANGE	1
#define NO_MAX_RANGE	2
#define NEAR_MIN	4
#define NEAR_MAX	8
#define UNIQUE_RANGE	16
#define EQ_RANGE	32

typedef struct st_key_part {
  uint16 key,part,part_length;
  uint8  null_bit;
  Field *field;
} KEY_PART;

class QUICK_RANGE :public Sql_alloc {
 public:
  char *min_key,*max_key;
  uint16 min_length,max_length,flag;
  QUICK_RANGE();				/* Full range */
  QUICK_RANGE(const char *min_key_arg,uint min_length_arg,
	      const char *max_key_arg,uint max_length_arg,
	      uint flag_arg)
    : min_key((char*) sql_memdup(min_key_arg,min_length_arg+1)),
      max_key((char*) sql_memdup(max_key_arg,max_length_arg+1)),
      min_length(min_length_arg),
      max_length(max_length_arg),
      flag(flag_arg)
    {}
};

class QUICK_SELECT {
public:
  bool next;
  int error;
  uint index,max_used_key_length;
  TABLE *head;
  handler *file;
  byte    *record;
  List<QUICK_RANGE> ranges;
  List_iterator<QUICK_RANGE> it;
  QUICK_RANGE *range;
  MEM_ROOT alloc;

  KEY_PART *key_parts;
  ha_rows records;
  double read_time;

  QUICK_SELECT(TABLE *table,uint index_arg,bool no_alloc=0);
  virtual ~QUICK_SELECT();
  void reset(void) { next=0; it.rewind(); }
  virtual int init();
  virtual int get_next();
  int cmp_next(QUICK_RANGE *range);
  bool unique_key_range();
};

class SQL_SELECT :public Sql_alloc {
 public:
  QUICK_SELECT *quick;		// If quick-select used
  COND		*cond;		// where condition
  TABLE	*head;
  IO_CACHE file;		// Positions to used records
  ha_rows records;		// Records in use if read from file
  double read_time;		// Time to read rows
  key_map quick_keys;		// Possible quick keys
  key_map needed_reg;		// Possible quick keys after prev tables.
  table_map const_tables,read_tables;
  bool	free_cond;

  SQL_SELECT();
  ~SQL_SELECT();
  bool check_quick(bool force_quick_range=0, ha_rows limit = HA_POS_ERROR)
  { return test_quick_select(~0L,0,limit, force_quick_range) < 0; }
  inline bool skipp_record() { return cond ? cond->val_int() == 0 : 0; }
  int test_quick_select(key_map keys,table_map prev_tables,ha_rows limit,
			bool force_quick_range=0);
};

QUICK_SELECT *get_quick_select_for_ref(TABLE *table, struct st_table_ref *ref);

#endif
