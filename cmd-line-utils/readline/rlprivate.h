/* rlprivate.h -- functions and variables global to the readline library,
		  but not intended for use by applications. */

/* Copyright (C) 1999-2005 Free Software Foundation, Inc.

   This file is part of the GNU Readline Library, a library for
   reading lines of text with interactive input and history editing.

   The GNU Readline Library is free software; you can redistribute it
   and/or modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2, or
   (at your option) any later version.

   The GNU Readline Library is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   The GNU General Public License is often shipped with GNU software, and
   is generally kept in a file called COPYING or LICENSE.  If you do not
   have a copy of the license, write to the Free Software Foundation,
   59 Temple Place, Suite 330, Boston, MA 02111 USA. */

#if !defined (_RL_PRIVATE_H_)
#define _RL_PRIVATE_H_

#include "rlconf.h"	/* for VISIBLE_STATS */
#include "rlstdc.h"
#include "posixjmp.h"	/* defines procenv_t */

/*************************************************************************
 *									 *
 * Global structs undocumented in texinfo manual and not in readline.h   *
 *									 *
 *************************************************************************/
/* search types */
#define RL_SEARCH_ISEARCH	0x01		/* incremental search */
#define RL_SEARCH_NSEARCH	0x02		/* non-incremental search */
#define RL_SEARCH_CSEARCH	0x04		/* intra-line char search */

/* search flags */
#define SF_REVERSE		0x01
#define SF_FOUND		0x02
#define SF_FAILED		0x04

typedef struct  __rl_search_context
{
  int type;
  int sflags;

  char *search_string;
  int search_string_index;
  int search_string_size;

  char **lines;
  char *allocated_line;    
  int hlen;
  int hindex;

  int save_point;
  int save_mark;
  int save_line;
  int last_found_line;
  char *prev_line_found;

  UNDO_LIST *save_undo_list;

  int history_pos;
  int direction;

  int lastc;
#if defined (HANDLE_MULTIBYTE)
  char mb[MB_LEN_MAX];
#endif

  char *sline;
  int sline_len;
  int sline_index;

  const char  *search_terminators;
} _rl_search_cxt;

/* Callback data for reading numeric arguments */
#define NUM_SAWMINUS	0x01
#define NUM_SAWDIGITS	0x02
#define NUM_READONE	0x04

typedef int _rl_arg_cxt;

/* A context for reading key sequences longer than a single character when
   using the callback interface. */
#define KSEQ_DISPATCHED	0x01
#define KSEQ_SUBSEQ	0x02
#define KSEQ_RECURSIVE	0x04

typedef struct __rl_keyseq_context
{
  int flags;
  int subseq_arg;
  int subseq_retval;		/* XXX */
  Keymap dmap;

  Keymap oldmap;
  int okey;
  struct __rl_keyseq_context *ocxt;
  int childval;
} _rl_keyseq_cxt;

  /* fill in more as needed */
/* `Generic' callback data and functions */
typedef struct __rl_callback_generic_arg 
{
  int count;
  int i1, i2;
  /* add here as needed */
} _rl_callback_generic_arg;

typedef int _rl_callback_func_t PARAMS((_rl_callback_generic_arg *));

/*************************************************************************
 *									 *
 * Global functions undocumented in texinfo manual and not in readline.h *
 *									 *
 *************************************************************************/

/*************************************************************************
 *									 *
 * Global variables undocumented in texinfo manual and not in readline.h *
 *									 *
 *************************************************************************/

/* complete.c */
extern int rl_complete_with_tilde_expansion;
#if defined (VISIBLE_STATS)
extern int rl_visible_stats;
#endif /* VISIBLE_STATS */

/* readline.c */
extern int rl_line_buffer_len;
extern int rl_arg_sign;
extern int rl_visible_prompt_length;
extern int readline_echoing_p;
extern int rl_key_sequence_length;
extern int rl_byte_oriented;

extern _rl_keyseq_cxt *_rl_kscxt;

/* display.c */
extern int rl_display_fixed;

/* parens.c */
extern int rl_blink_matching_paren;

/*************************************************************************
 *									 *
 * Global functions and variables unsed and undocumented		 *
 *									 *
 *************************************************************************/

/* kill.c */
extern int rl_set_retained_kills PARAMS((int));

/* terminal.c */
extern void _rl_set_screen_size PARAMS((int, int));

/* undo.c */
extern int _rl_fix_last_undo_of_type PARAMS((enum undo_code, int, int));

/* util.c */
extern char *_rl_savestring PARAMS((const char *));

/*************************************************************************
 *									 *
 * Functions and variables private to the readline library		 *
 *									 *
 *************************************************************************/

/* NOTE: Functions and variables prefixed with `_rl_' are
   pseudo-global: they are global so they can be shared
   between files in the readline library, but are not intended
   to be visible to readline callers. */

/*************************************************************************
 * Undocumented private functions					 *
 *************************************************************************/

#if defined(READLINE_CALLBACKS)

/* readline.c */
extern void readline_internal_setup PARAMS((void));
extern char *readline_internal_teardown PARAMS((int));
extern int readline_internal_char PARAMS((void));

extern _rl_keyseq_cxt *_rl_keyseq_cxt_alloc PARAMS((void));
extern void _rl_keyseq_cxt_dispose PARAMS((_rl_keyseq_cxt *));
extern void _rl_keyseq_chain_dispose PARAMS((void));

extern int _rl_dispatch_callback PARAMS((_rl_keyseq_cxt *));
     
/* callback.c */
extern _rl_callback_generic_arg *_rl_callback_data_alloc PARAMS((int));
extern void _rl_callback_data_dispose PARAMS((_rl_callback_generic_arg *));

#endif /* READLINE_CALLBACKS */

/* bind.c */

/* complete.c */
extern char _rl_find_completion_word PARAMS((int *, int *));
extern void _rl_free_match_list PARAMS((char **));

/* display.c */
extern char *_rl_strip_prompt PARAMS((char *));
extern void _rl_move_cursor_relative PARAMS((int, const char *));
extern void _rl_move_vert PARAMS((int));
extern void _rl_save_prompt PARAMS((void));
extern void _rl_restore_prompt PARAMS((void));
extern char *_rl_make_prompt_for_search PARAMS((int));
extern void _rl_erase_at_end_of_line PARAMS((int));
extern void _rl_clear_to_eol PARAMS((int));
extern void _rl_clear_screen PARAMS((void));
extern void _rl_update_final PARAMS((void));
extern void _rl_redisplay_after_sigwinch PARAMS((void));
extern void _rl_clean_up_for_exit PARAMS((void));
extern void _rl_erase_entire_line PARAMS((void));
extern int _rl_current_display_line PARAMS((void));

/* input.c */
extern int _rl_any_typein PARAMS((void));
extern int _rl_input_available PARAMS((void));
extern int _rl_input_queued PARAMS((int));
extern void _rl_insert_typein PARAMS((int));
extern int _rl_unget_char PARAMS((int));
extern int _rl_pushed_input_available PARAMS((void));

/* isearch.c */
extern _rl_search_cxt *_rl_scxt_alloc PARAMS((int, int));
extern void _rl_scxt_dispose PARAMS((_rl_search_cxt *, int));

extern int _rl_isearch_dispatch PARAMS((_rl_search_cxt *, int));
extern int _rl_isearch_callback PARAMS((_rl_search_cxt *));

extern int _rl_search_getchar PARAMS((_rl_search_cxt *));

/* macro.c */
extern void _rl_with_macro_input PARAMS((char *));
extern int _rl_next_macro_key PARAMS((void));
extern void _rl_push_executing_macro PARAMS((void));
extern void _rl_pop_executing_macro PARAMS((void));
extern void _rl_add_macro_char PARAMS((int));
extern void _rl_kill_kbd_macro PARAMS((void));

/* misc.c */
extern int _rl_arg_overflow PARAMS((void));
extern void _rl_arg_init PARAMS((void));
extern int _rl_arg_getchar PARAMS((void));
extern int _rl_arg_callback PARAMS((_rl_arg_cxt));
extern void _rl_reset_argument PARAMS((void));

extern void _rl_start_using_history PARAMS((void));
extern int _rl_free_saved_history_line PARAMS((void));
extern void _rl_set_insert_mode PARAMS((int, int));

/* nls.c */
extern int _rl_init_eightbit PARAMS((void));

/* parens.c */
extern void _rl_enable_paren_matching PARAMS((int));

/* readline.c */
extern void _rl_init_line_state PARAMS((void));
extern void _rl_set_the_line PARAMS((void));
extern int _rl_dispatch PARAMS((int, Keymap));
extern int _rl_dispatch_subseq PARAMS((int, Keymap, int));
extern void _rl_internal_char_cleanup PARAMS((void));

/* rltty.c */
extern int _rl_disable_tty_signals PARAMS((void));
extern int _rl_restore_tty_signals PARAMS((void));

/* search.c */
extern int _rl_nsearch_callback PARAMS((_rl_search_cxt *));

/* terminal.c */
extern void _rl_get_screen_size PARAMS((int, int));
extern int _rl_init_terminal_io PARAMS((const char *));
#ifdef _MINIX
extern void _rl_output_character_function PARAMS((int));
#else
extern int _rl_output_character_function PARAMS((int));
#endif
extern void _rl_output_some_chars PARAMS((const char *, int));
extern int _rl_backspace PARAMS((int));
extern void _rl_enable_meta_key PARAMS((void));
extern void _rl_control_keypad PARAMS((int));
extern void _rl_set_cursor PARAMS((int, int));

/* text.c */
extern void _rl_fix_point PARAMS((int));
extern int _rl_replace_text PARAMS((const char *, int, int));
extern int _rl_insert_char PARAMS((int, int));
extern int _rl_overwrite_char PARAMS((int, int));
extern int _rl_overwrite_rubout PARAMS((int, int));
extern int _rl_rubout_char PARAMS((int, int));
#if defined (HANDLE_MULTIBYTE)
extern int _rl_char_search_internal PARAMS((int, int, char *, int));
#else
extern int _rl_char_search_internal PARAMS((int, int, int));
#endif
extern int _rl_set_mark_at_pos PARAMS((int));

/* undo.c */
extern UNDO_LIST *_rl_copy_undo_entry PARAMS((UNDO_LIST *));
extern UNDO_LIST *_rl_copy_undo_list PARAMS((UNDO_LIST *));

/* util.c */
extern int _rl_abort_internal PARAMS((void));
extern char *_rl_strindex PARAMS((const char *, const char *));
extern int _rl_qsort_string_compare PARAMS((char **, char **));
extern int (_rl_uppercase_p) PARAMS((int));
extern int (_rl_lowercase_p) PARAMS((int));
extern int (_rl_pure_alphabetic) PARAMS((int));
extern int (_rl_digit_p) PARAMS((int));
extern int (_rl_to_lower) PARAMS((int));
extern int (_rl_to_upper) PARAMS((int));
extern int (_rl_digit_value) PARAMS((int));

/* vi_mode.c */
extern void _rl_vi_initialize_line PARAMS((void));
extern void _rl_vi_reset_last PARAMS((void));
extern void _rl_vi_set_last PARAMS((int, int, int));
extern int _rl_vi_textmod_command PARAMS((int));
extern void _rl_vi_done_inserting PARAMS((void));

/*************************************************************************
 * Undocumented private variables					 *
 *************************************************************************/

/* bind.c */
extern const char *_rl_possible_control_prefixes[];
extern const char *_rl_possible_meta_prefixes[];

/* callback.c */
extern _rl_callback_func_t *_rl_callback_func;
extern _rl_callback_generic_arg *_rl_callback_data;

/* complete.c */
extern int _rl_complete_show_all;
extern int _rl_complete_show_unmodified;
extern int _rl_complete_mark_directories;
extern int _rl_complete_mark_symlink_dirs;
extern int _rl_print_completions_horizontally;
extern int _rl_completion_case_fold;
extern int _rl_match_hidden_files;
extern int _rl_page_completions;

/* display.c */
extern int _rl_vis_botlin;
extern int _rl_last_c_pos;
extern int _rl_suppress_redisplay;
extern int _rl_want_redisplay;
extern const char *rl_display_prompt;

/* isearch.c */
extern char *_rl_isearch_terminators;

extern _rl_search_cxt *_rl_iscxt;

/* macro.c */
extern char *_rl_executing_macro;

/* misc.c */
extern int _rl_history_preserve_point;
extern int _rl_history_saved_point;

extern _rl_arg_cxt _rl_argcxt;

/* readline.c */
extern int _rl_horizontal_scroll_mode;
extern int _rl_mark_modified_lines;
extern int _rl_bell_preference;
extern int _rl_meta_flag;
extern int _rl_convert_meta_chars_to_ascii;
extern int _rl_output_meta_chars;
extern int _rl_bind_stty_chars;
extern char *_rl_comment_begin;
extern unsigned char _rl_parsing_conditionalized_out;
extern Keymap _rl_keymap;
extern FILE *_rl_in_stream;
extern FILE *_rl_out_stream;
extern int _rl_last_command_was_kill;
extern int _rl_eof_char;
extern procenv_t readline_top_level;

/* search.c */
extern _rl_search_cxt *_rl_nscxt;

/* terminal.c */
extern int _rl_enable_keypad;
extern int _rl_enable_meta;
extern const char *_rl_term_clreol;
extern const char *_rl_term_clrpag;
extern const char *_rl_term_im;
extern const char *_rl_term_ic;
extern const char *_rl_term_ei;
extern const char *_rl_term_DC;
extern const char *_rl_term_up;
extern const char *_rl_term_dc;
extern const char *_rl_term_cr;
extern const char *_rl_term_IC;
extern const char *_rl_term_forward_char;
extern int _rl_screenheight;
extern int _rl_screenwidth;
extern int _rl_screenchars;
extern int _rl_terminal_can_insert;
extern int _rl_term_autowrap;

/* undo.c */
extern int _rl_doing_an_undo;
extern int _rl_undo_group_level;

/* vi_mode.c */
extern int _rl_vi_last_command;

#endif /* _RL_PRIVATE_H_ */
