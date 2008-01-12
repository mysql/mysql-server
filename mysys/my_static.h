/* Copyright (C) 2000 MySQL AB

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

/*
  Static variables for mysys library. All definied here for easy making of
  a shared library
*/

C_MODE_START
#include <signal.h>

#define MAX_SIGNALS	10		/* Max signals under a dont-allow */
#define MIN_KEYBLOCK	(min(IO_SIZE,1024))
#define MAX_KEYBLOCK	8192		/* Max keyblocklength == 8*IO_SIZE */
#define MAX_BLOCK_TYPES MAX_KEYBLOCK/MIN_KEYBLOCK

struct st_remember {
  int number;
  sig_handler (*func)(int number);
};

/*
  Structure that stores information of a allocated memory block
  The data is at &struct_adr+sizeof(ALIGN_SIZE(sizeof(struct irem)))
  The lspecialvalue is at the previous 4 bytes from this, which may not
  necessarily be in the struct if the struct size isn't aligned at a 8 byte
  boundary.
*/

struct st_irem
{
  struct st_irem *next;		/* Linked list of structures	   */
  struct st_irem *prev;		/* Other link			   */
  char *filename;		/* File in which memory was new'ed */
  size_t datasize;		/* Size requested		   */
  uint32 linenum;		/* Line number in above file	   */
  uint32 SpecialValue;		/* Underrun marker value	   */
};


extern char	NEAR curr_dir[FN_REFLEN],NEAR home_dir_buff[FN_REFLEN];

extern volatile int _my_signals;
extern struct st_remember _my_sig_remember[MAX_SIGNALS];

extern const char *soundex_map;

extern USED_MEM* my_once_root_block;
extern uint	 my_once_extra;

extern uchar	*sf_min_adress,*sf_max_adress;
extern uint	sf_malloc_count;
extern struct st_irem *sf_malloc_root;

extern struct st_my_file_info my_file_info_default[MY_NFILE];

extern ulonglong query_performance_frequency, query_performance_offset;

#if defined(THREAD) && !defined(__WIN__)
extern sigset_t my_signals;		/* signals blocked by mf_brkhant */
#endif
C_MODE_END
