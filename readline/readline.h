/* Readline.h -- the names of functions callable from within readline. */

/* Copyright (C) 1987, 1989, 1992 Free Software Foundation, Inc.

   This file is part of the GNU Readline Library, a library for
   reading lines of text with interactive input and history editing.

   The GNU Readline Library is free software; you can redistribute it
   and/or modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 1, or
   (at your option) any later version.

   The GNU Readline Library is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   The GNU General Public License is often shipped with GNU software, and
   is generally kept in a file called COPYING or LICENSE.  If you do not
   have a copy of the license, write to the Free Software Foundation,
   675 Mass Ave, Cambridge, MA 02139, USA. */

#if !defined (_READLINE_H_)
#define _READLINE_H_

#ifdef __cplusplus
extern "C" {
#endif

#if defined (READLINE_LIBRARY)
#  include "rlstdc.h"
#  include "keymaps.h"
#  include "tilde.h"
#else
#  include <readline/rlstdc.h>
#  include <readline/keymaps.h>
#  include <readline/tilde.h>
#endif

/* Readline data structures. */

/* Maintaining the state of undo.  We remember individual deletes and inserts
   on a chain of things to do. */

/* The actions that undo knows how to undo.  Notice that UNDO_DELETE means
   to insert some text, and UNDO_INSERT means to delete some text.   I.e.,
   the code tells undo what to undo, not how to undo it. */
enum undo_code { UNDO_DELETE, UNDO_INSERT, UNDO_BEGIN, UNDO_END };

/* What an element of THE_UNDO_LIST looks like. */
typedef struct undo_list {
  struct undo_list *next;
  int start, end;		/* Where the change took place. */
  char *text;			/* The text to insert, if undoing a delete. */
  enum undo_code what;		/* Delete, Insert, Begin, End. */
} UNDO_LIST;

/* The current undo list for RL_LINE_BUFFER. */
extern UNDO_LIST *rl_undo_list;

/* The data structure for mapping textual names to code addresses. */
typedef struct _funmap {
  char *name;
  Function *function;
} FUNMAP;

extern FUNMAP **funmap;

/* **************************************************************** */
/*								    */
/*	     Functions available to bind to key sequences	    */
/*								    */
/* **************************************************************** */

/* Bindable commands for numeric arguments. */
extern int rl_digit_argument __P((int, int));
extern int rl_universal_argument __P((int, int));

/* Bindable commands for moving the cursor. */
extern int rl_forward __P((int, int));
extern int rl_backward __P((int, int));
extern int rl_beg_of_line __P((int, int));
extern int rl_end_of_line __P((int, int));
extern int rl_forward_word __P((int, int));
extern int rl_backward_word __P((int, int));
extern int rl_refresh_line __P((int, int));
extern int rl_clear_screen __P((int, int));
extern int rl_arrow_keys __P((int, int));

/* Bindable commands for inserting and deleting text. */
extern int rl_insert __P((int, int));
extern int rl_quoted_insert __P((int, int));
extern int rl_tab_insert __P((int, int));
extern int rl_newline __P((int, int));
extern int rl_do_lowercase_version __P((int, int));
extern int rl_rubout __P((int, int));
extern int rl_delete __P((int, int));
extern int rl_rubout_or_delete __P((int, int));
extern int rl_delete_horizontal_space __P((int, int));
extern int rl_delete_or_show_completions __P((int, int));
extern int rl_insert_comment __P((int, int));

/* Bindable commands for changing case. */
extern int rl_upcase_word __P((int, int));
extern int rl_downcase_word __P((int, int));
extern int rl_capitalize_word __P((int, int));

/* Bindable commands for transposing characters and words. */
extern int rl_transpose_words __P((int, int));
extern int rl_transpose_chars __P((int, int));

/* Bindable commands for searching within a line. */
extern int rl_char_search __P((int, int));
extern int rl_backward_char_search __P((int, int));

/* Bindable commands for readline's interface to the command history. */
extern int rl_beginning_of_history __P((int, int));
extern int rl_end_of_history __P((int, int));
extern int rl_get_next_history __P((int, int));
extern int rl_get_previous_history __P((int, int));

/* Bindable commands for managing the mark and region. */
extern int rl_set_mark __P((int, int));
extern int rl_exchange_point_and_mark __P((int, int));

/* Bindable commands to set the editing mode (emacs or vi). */
extern int rl_vi_editing_mode __P((int, int));
extern int rl_emacs_editing_mode __P((int, int));

/* Bindable commands for managing key bindings. */
extern int rl_re_read_init_file __P((int, int));
extern int rl_dump_functions __P((int, int));
extern int rl_dump_macros __P((int, int));
extern int rl_dump_variables __P((int, int));

/* Bindable commands for word completion. */
extern int rl_complete __P((int, int));
extern int rl_possible_completions __P((int, int));
extern int rl_insert_completions __P((int, int));
extern int rl_menu_complete __P((int, int));

/* Bindable commands for killing and yanking text, and managing the kill ring. */
extern int rl_kill_word __P((int, int));
extern int rl_backward_kill_word __P((int, int));
extern int rl_kill_line __P((int, int));
extern int rl_backward_kill_line __P((int, int));
extern int rl_kill_full_line __P((int, int));
extern int rl_unix_word_rubout __P((int, int));
extern int rl_unix_line_discard __P((int, int));
extern int rl_copy_region_to_kill __P((int, int));
extern int rl_kill_region __P((int, int));
extern int rl_copy_forward_word __P((int, int));
extern int rl_copy_backward_word __P((int, int));
extern int rl_yank __P((int, int));
extern int rl_yank_pop __P((int, int));
extern int rl_yank_nth_arg __P((int, int));
extern int rl_yank_last_arg __P((int, int));
/* Not available unless __CYGWIN32__ is defined. */
#ifdef __CYGWIN32__
extern int rl_paste_from_clipboard __P((int, int));
#endif

/* Bindable commands for incremental searching. */
extern int rl_reverse_search_history __P((int, int));
extern int rl_forward_search_history __P((int, int));

/* Bindable keyboard macro commands. */
extern int rl_start_kbd_macro __P((int, int));
extern int rl_end_kbd_macro __P((int, int));
extern int rl_call_last_kbd_macro __P((int, int));

/* Bindable undo commands. */
extern int rl_revert_line __P((int, int));
extern int rl_undo_command __P((int, int));

/* Bindable tilde expansion commands. */
extern int rl_tilde_expand __P((int, int));

/* Bindable terminal control commands. */
extern int rl_restart_output __P((int, int));
extern int rl_stop_output __P((int, int));

/* Miscellaneous bindable commands. */
extern int rl_abort __P((int, int));
extern int rl_tty_status __P((int, int));

/* Bindable commands for incremental and non-incremental history searching. */
extern int rl_history_search_forward __P((int, int));
extern int rl_history_search_backward __P((int, int));
extern int rl_noninc_forward_search __P((int, int));
extern int rl_noninc_reverse_search __P((int, int));
extern int rl_noninc_forward_search_again __P((int, int));
extern int rl_noninc_reverse_search_again __P((int, int));

/* Not available unless readline is compiled -DPAREN_MATCHING. */
extern int rl_insert_close __P((int, int));

/* Not available unless READLINE_CALLBACKS is defined. */
extern void rl_callback_handler_install __P((char *, VFunction *));
extern void rl_callback_read_char __P((void));
extern void rl_callback_handler_remove __P((void));

/* Things for vi mode. Not available unless readline is compiled -DVI_MODE. */
/* VI-mode bindable commands. */
extern int rl_vi_redo __P((int, int));
extern int rl_vi_undo __P((int, int));
extern int rl_vi_yank_arg __P((int, int));
extern int rl_vi_fetch_history __P((int, int));
extern int rl_vi_search_again __P((int, int));
extern int rl_vi_search __P((int, int));
extern int rl_vi_complete __P((int, int));
extern int rl_vi_tilde_expand __P((int, int));
extern int rl_vi_prev_word __P((int, int));
extern int rl_vi_next_word __P((int, int));
extern int rl_vi_end_word __P((int, int));
extern int rl_vi_insert_beg __P((int, int));
extern int rl_vi_append_mode __P((int, int));
extern int rl_vi_append_eol __P((int, int));
extern int rl_vi_eof_maybe __P((int, int));
extern int rl_vi_insertion_mode __P((int, int));
extern int rl_vi_movement_mode __P((int, int));
extern int rl_vi_arg_digit __P((int, int));
extern int rl_vi_change_case __P((int, int));
extern int rl_vi_put __P((int, int));
extern int rl_vi_column __P((int, int));
extern int rl_vi_delete_to __P((int, int));
extern int rl_vi_change_to __P((int, int));
extern int rl_vi_yank_to __P((int, int));
extern int rl_vi_delete __P((int, int));
extern int rl_vi_back_to_indent __P((int, int));
extern int rl_vi_first_print __P((int, int));
extern int rl_vi_char_search __P((int, int));
extern int rl_vi_match __P((int, int));
extern int rl_vi_change_char __P((int, int));
extern int rl_vi_subst __P((int, int));
extern int rl_vi_overstrike __P((int, int));
extern int rl_vi_overstrike_delete __P((int, int));
extern int rl_vi_replace __P((int, int));
extern int rl_vi_set_mark __P((int, int));
extern int rl_vi_goto_mark __P((int, int));

/* VI-mode utility functions. */
extern int rl_vi_check __P((void));
extern int rl_vi_domove __P((int, int *));
extern int rl_vi_bracktype __P((int));

/* VI-mode pseudo-bindable commands, used as utility functions. */
extern int rl_vi_fWord __P((int, int));
extern int rl_vi_bWord __P((int, int));
extern int rl_vi_eWord __P((int, int));
extern int rl_vi_fword __P((int, int));
extern int rl_vi_bword __P((int, int));
extern int rl_vi_eword __P((int, int));

/* **************************************************************** */
/*								    */
/*			Well Published Functions		    */
/*								    */
/* **************************************************************** */

/* Readline functions. */
/* Read a line of input.  Prompt with PROMPT.  A NULL PROMPT means none. */
extern char *readline __P((char *));

extern int rl_initialize __P((void));

extern int rl_discard_argument __P((void));

/* Utility functions to bind keys to readline commands. */
extern int rl_add_defun __P((char *, Function *, int));
extern int rl_bind_key __P((int, Function *));
extern int rl_bind_key_in_map __P((int, Function *, Keymap));
extern int rl_unbind_key __P((int));
extern int rl_unbind_key_in_map __P((int, Keymap));
extern int rl_unbind_function_in_map __P((Function *, Keymap));
extern int rl_unbind_command_in_map __P((char *, Keymap));
extern int rl_set_key __P((char *, Function *, Keymap));
extern int rl_generic_bind __P((int, char *, char *, Keymap));
extern int rl_variable_bind __P((char *, char *));

/* Backwards compatibility, use rl_generic_bind instead. */
extern int rl_macro_bind __P((char *, char *, Keymap));

/* Undocumented in the texinfo manual; not really useful to programs. */
extern int rl_translate_keyseq __P((char *, char *, int *));
extern char *rl_untranslate_keyseq __P((int));

extern Function *rl_named_function __P((char *));
extern Function *rl_function_of_keyseq __P((char *, Keymap, int *));

extern void rl_list_funmap_names __P((void));
extern char **rl_invoking_keyseqs_in_map __P((Function *, Keymap));
extern char **rl_invoking_keyseqs __P((Function *));
 
extern void rl_function_dumper __P((int));
extern void rl_macro_dumper __P((int));
extern void rl_variable_dumper __P((int));

extern int rl_read_init_file __P((char *));
extern int rl_parse_and_bind __P((char *));

/* Functions for manipulating keymaps. */
extern Keymap rl_make_bare_keymap __P((void));
extern Keymap rl_copy_keymap __P((Keymap));
extern Keymap rl_make_keymap __P((void));
extern void rl_discard_keymap __P((Keymap));

extern Keymap rl_get_keymap_by_name __P((char *));
extern char *rl_get_keymap_name __P((Keymap));
extern void rl_set_keymap __P((Keymap));
extern Keymap rl_get_keymap __P((void));
extern void rl_set_keymap_from_edit_mode __P((void));
extern char *rl_get_keymap_name_from_edit_mode __P((void));

/* Functions for manipulating the funmap, which maps command names to functions. */
extern int rl_add_funmap_entry __P((char *, Function *));
extern void rl_initialize_funmap __P((void));
extern char **rl_funmap_names __P((void));

/* Utility functions for managing keyboard macros. */
extern void rl_push_macro_input __P((char *));

/* Functions for undoing, from undo.c */
extern void rl_add_undo __P((enum undo_code, int, int, char *));
extern void free_undo_list __P((void));
extern int rl_do_undo __P((void));
extern int rl_begin_undo_group __P((void));
extern int rl_end_undo_group __P((void));
extern int rl_modifying __P((int, int));

/* Functions for redisplay. */
extern void rl_redisplay __P((void));
extern int rl_on_new_line __P((void));
extern int rl_forced_update_display __P((void));
extern int rl_clear_message __P((void));
extern int rl_reset_line_state __P((void));

#if defined (__STDC__) && defined (USE_VARARGS) && defined (PREFER_STDARG)
extern int rl_message (const char *, ...);
#else
extern int rl_message ();
#endif

/* Undocumented in texinfo manual. */
extern int rl_show_char __P((int));
extern int rl_character_len __P((int, int));
extern int crlf __P((void));

/* Save and restore internal prompt redisplay information. */
extern void rl_save_prompt __P((void));
extern void rl_restore_prompt __P((void));

/* Modifying text. */
extern int rl_insert_text __P((char *));
extern int rl_delete_text __P((int, int));
extern int rl_kill_text __P((int, int));
extern char *rl_copy_text __P((int, int));

/* Terminal and tty mode management. */
extern void rl_prep_terminal __P((int));
extern void rl_deprep_terminal __P((void));
extern void rltty_set_default_bindings __P((Keymap));

extern int rl_reset_terminal __P((char *));
extern void rl_resize_terminal __P((void));

/* `Public' utility functions . */
extern void rl_extend_line_buffer __P((int));
extern int ding __P((void));

/* Functions for character input. */
extern int rl_stuff_char __P((int));
extern int rl_execute_next __P((int));
extern int rl_read_key __P((void));
extern int rl_getc __P((FILE *));

/* Readline signal handling, from signals.c */
extern int rl_set_signals __P((void));
extern int rl_clear_signals __P((void));
extern void rl_cleanup_after_signal __P((void));
extern void rl_reset_after_signal __P((void));
extern void rl_free_line_state __P((void));
 
/* Undocumented. */
extern int rl_expand_prompt __P((char *));

extern int maybe_save_line __P((void));
extern int maybe_unsave_line __P((void));
extern int maybe_replace_line __P((void));

/* Completion functions. */
extern int rl_complete_internal __P((int));
extern void rl_display_match_list __P((char **, int, int));

extern char **completion_matches __P((char *, CPFunction *));
extern char *username_completion_function __P((char *, int));
extern char *filename_completion_function __P((char *, int));

/* **************************************************************** */
/*								    */
/*			Well Published Variables		    */
/*								    */
/* **************************************************************** */

/* The version of this incarnation of the readline library. */
extern char *rl_library_version;

/* The name of the calling program.  You should initialize this to
   whatever was in argv[0].  It is used when parsing conditionals. */
extern char *rl_readline_name;

/* The prompt readline uses.  This is set from the argument to
   readline (), and should not be assigned to directly. */
extern char *rl_prompt;

/* The line buffer that is in use. */
extern char *rl_line_buffer;

/* The location of point, and end. */
extern int rl_point, rl_end;

/* The mark, or saved cursor position. */
extern int rl_mark;

/* Flag to indicate that readline has finished with the current input
   line and should return it. */
extern int rl_done;

/* If set to a character value, that will be the next keystroke read. */
extern int rl_pending_input;

/* Non-zero if we called this function from _rl_dispatch().  It's present
   so functions can find out whether they were called from a key binding
   or directly from an application. */
extern int rl_dispatching;

/* The name of the terminal to use. */
extern char *rl_terminal_name;

/* The input and output streams. */
extern FILE *rl_instream, *rl_outstream;

/* If non-zero, then this is the address of a function to call just
   before readline_internal () prints the first prompt. */
extern Function *rl_startup_hook;

/* If non-zero, this is the address of a function to call just before
   readline_internal_setup () returns and readline_internal starts
   reading input characters. */
extern Function *rl_pre_input_hook;
      
/* The address of a function to call periodically while Readline is
   awaiting character input, or NULL, for no event handling. */
extern Function *rl_event_hook;

extern Function *rl_getc_function;
extern VFunction *rl_redisplay_function;
extern VFunction *rl_prep_term_function;
extern VFunction *rl_deprep_term_function;

/* Dispatch variables. */
extern Keymap rl_executing_keymap;
extern Keymap rl_binding_keymap;

/* Display variables. */
/* If non-zero, readline will erase the entire line, including any prompt,
   if the only thing typed on an otherwise-blank line is something bound to
   rl_newline. */
extern int rl_erase_empty_line;

/* Variables to control readline signal handling. */
/* If non-zero, readline will install its own signal handlers for
   SIGINT, SIGTERM, SIGQUIT, SIGALRM, SIGTSTP, SIGTTIN, and SIGTTOU. */
extern int rl_catch_signals;

/* If non-zero, readline will install a signal handler for SIGWINCH
   that also attempts to call any calling application's SIGWINCH signal
   handler.  Note that the terminal is not cleaned up before the
   application's signal handler is called; use rl_cleanup_after_signal()
   to do that. */
extern int rl_catch_sigwinch;

/* Completion variables. */
/* Pointer to the generator function for completion_matches ().
   NULL means to use filename_entry_function (), the default filename
   completer. */
extern Function *rl_completion_entry_function;

/* If rl_ignore_some_completions_function is non-NULL it is the address
   of a function to call after all of the possible matches have been
   generated, but before the actual completion is done to the input line.
   The function is called with one argument; a NULL terminated array
   of (char *).  If your function removes any of the elements, they
   must be free()'ed. */
extern Function *rl_ignore_some_completions_function;

/* Pointer to alternative function to create matches.
   Function is called with TEXT, START, and END.
   START and END are indices in RL_LINE_BUFFER saying what the boundaries
   of TEXT are.
   If this function exists and returns NULL then call the value of
   rl_completion_entry_function to try to match, otherwise use the
   array of strings returned. */
extern CPPFunction *rl_attempted_completion_function;

/* The basic list of characters that signal a break between words for the
   completer routine.  The initial contents of this variable is what
   breaks words in the shell, i.e. "n\"\\'`@$>". */
extern char *rl_basic_word_break_characters;

/* The list of characters that signal a break between words for
   rl_complete_internal.  The default list is the contents of
   rl_basic_word_break_characters.  */
extern char *rl_completer_word_break_characters;

/* List of characters which can be used to quote a substring of the line.
   Completion occurs on the entire substring, and within the substring   
   rl_completer_word_break_characters are treated as any other character,
   unless they also appear within this list. */
extern char *rl_completer_quote_characters;

/* List of quote characters which cause a word break. */
extern char *rl_basic_quote_characters;

/* List of characters that need to be quoted in filenames by the completer. */
extern char *rl_filename_quote_characters;

/* List of characters that are word break characters, but should be left
   in TEXT when it is passed to the completion function.  The shell uses
   this to help determine what kind of completing to do. */
extern char *rl_special_prefixes;

/* If non-zero, then this is the address of a function to call when
   completing on a directory name.  The function is called with
   the address of a string (the current directory name) as an arg. */
extern Function *rl_directory_completion_hook;

/* Backwards compatibility with previous versions of readline. */
#define rl_symbolic_link_hook rl_directory_completion_hook

/* If non-zero, then this is the address of a function to call when
   completing a word would normally display the list of possible matches.
   This function is called instead of actually doing the display.
   It takes three arguments: (char **matches, int num_matches, int max_length)
   where MATCHES is the array of strings that matched, NUM_MATCHES is the
   number of strings in that array, and MAX_LENGTH is the length of the
   longest string in that array. */
extern VFunction *rl_completion_display_matches_hook;

/* Non-zero means that the results of the matches are to be treated
   as filenames.  This is ALWAYS zero on entry, and can only be changed
   within a completion entry finder function. */
extern int rl_filename_completion_desired;

/* Non-zero means that the results of the matches are to be quoted using
   double quotes (or an application-specific quoting mechanism) if the
   filename contains any characters in rl_word_break_chars.  This is
   ALWAYS non-zero on entry, and can only be changed within a completion
   entry finder function. */
extern int rl_filename_quoting_desired;

/* Set to a function to quote a filename in an application-specific fashion.
   Called with the text to quote, the type of match found (single or multiple)
   and a pointer to the quoting character to be used, which the function can
   reset if desired. */
extern CPFunction *rl_filename_quoting_function;

/* Function to call to remove quoting characters from a filename.  Called
   before completion is attempted, so the embedded quotes do not interfere
   with matching names in the file system. */
extern CPFunction *rl_filename_dequoting_function;

/* Function to call to decide whether or not a word break character is
   quoted.  If a character is quoted, it does not break words for the
   completer. */
extern Function *rl_char_is_quoted_p;

/* Non-zero means to suppress normal filename completion after the
   user-specified completion function has been called. */
extern int rl_attempted_completion_over;

/* Set to a character describing the type of completion being attempted by
   rl_complete_internal; available for use by application completion
   functions. */
extern int rl_completion_type;

/* Character appended to completed words when at the end of the line.  The
   default is a space.  Nothing is added if this is '\0'. */
extern int rl_completion_append_character;

/* Up to this many items will be displayed in response to a
   possible-completions call.  After that, we ask the user if she
   is sure she wants to see them all.  The default value is 100. */
extern int rl_completion_query_items;

/* If non-zero, then disallow duplicates in the matches. */
extern int rl_ignore_completion_duplicates;

/* If this is non-zero, completion is (temporarily) inhibited, and the
   completion character will be inserted as any other. */
extern int rl_inhibit_completion;
   
/* Definitions available for use by readline clients. */
#define RL_PROMPT_START_IGNORE	'\001'
#define RL_PROMPT_END_IGNORE	'\002'

/* Possible values for do_replace argument to rl_filename_quoting_function,
   called by rl_complete_internal. */
#define NO_MATCH        0
#define SINGLE_MATCH    1
#define MULT_MATCH      2

#if !defined (savestring)
extern char *savestring ();	/* XXX backwards compatibility */
#endif

#ifdef __cplusplus
}
#endif

#endif /* _READLINE_H_ */
