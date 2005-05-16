/*	$NetBSD: readline.h,v 1.12 2004/09/08 18:15:37 christos Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jaromir Dolecek.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _READLINE_H_
#define _READLINE_H_

#include <sys/types.h>

/* list of readline stuff supported by editline library's readline wrapper */

/* typedefs */
typedef int	  Function(const char *, int);
typedef void	  VFunction(void);
typedef char	 *CPFunction(const char *, int);
typedef char	**CPPFunction(const char *, int, int);

typedef void *histdata_t;

typedef struct _hist_entry {
	const char	*line;
	histdata_t	*data;
} HIST_ENTRY;

typedef struct _keymap_entry {
	char type;
#define ISFUNC	0
#define ISKMAP	1
#define ISMACR	2
	Function *function;
} KEYMAP_ENTRY;

#define KEYMAP_SIZE	256

typedef KEYMAP_ENTRY KEYMAP_ENTRY_ARRAY[KEYMAP_SIZE];
typedef KEYMAP_ENTRY *Keymap;

#define control_character_threshold	0x20
#define control_character_bit		0x40

#ifndef CTRL
#include <sys/ioctl.h>
#if defined(__GLIBC__) || defined(__MWERKS__)
#include <sys/ttydefaults.h>
#endif
#ifndef CTRL
#define CTRL(c)		((c) & 037)
#endif
#endif
#ifndef UNCTRL
#define UNCTRL(c)	(((c) - 'a' + 'A')|control_character_bit)
#endif

#define RUBOUT		0x7f
#define ABORT_CHAR	CTRL('G')

/* global variables used by readline enabled applications */
#ifdef __cplusplus
extern "C" {
#endif
extern const char	*rl_library_version;
extern char		*rl_readline_name;
extern FILE		*rl_instream;
extern FILE		*rl_outstream;
extern char		*rl_line_buffer;
extern int		 rl_point, rl_end;
extern int		 history_base, history_length;
extern int		 max_input_history;
extern char		*rl_basic_word_break_characters;
extern char		*rl_completer_word_break_characters;
extern char		*rl_completer_quote_characters;
extern Function		*rl_completion_entry_function;
extern CPPFunction	*rl_attempted_completion_function;
extern int		rl_completion_type;
extern int		rl_completion_query_items;
extern char		*rl_special_prefixes;
extern int		rl_completion_append_character;
extern int		rl_inhibit_completion;
extern Function		*rl_pre_input_hook;
extern Function		*rl_startup_hook;
extern char		*rl_terminal_name;
extern int		rl_already_prompted;
extern char		*rl_prompt;
/*
 * The following is not implemented
 */
extern KEYMAP_ENTRY_ARRAY emacs_standard_keymap,
			emacs_meta_keymap,
			emacs_ctlx_keymap;
extern int		rl_filename_completion_desired;
extern int		rl_ignore_completion_duplicates;
extern Function		*rl_getc_function;
extern VFunction	*rl_redisplay_function;
extern VFunction	*rl_completion_display_matches_hook;
extern VFunction	*rl_prep_term_function;
extern VFunction	*rl_deprep_term_function;

/* supported functions */
char		*readline(const char *);
int		 rl_initialize(void);

void		 using_history(void);
int		 add_history(const char *);
void		 clear_history(void);
void		 stifle_history(int);
int		 unstifle_history(void);
int		 history_is_stifled(void);
int		 where_history(void);
HIST_ENTRY	*current_history(void);
HIST_ENTRY	*history_get(int);
int		 history_total_bytes(void);
int		 history_set_pos(int);
HIST_ENTRY	*previous_history(void);
HIST_ENTRY	*next_history(void);
int		 history_search(const char *, int);
int		 history_search_prefix(const char *, int);
int		 history_search_pos(const char *, int, int);
int		 read_history(const char *);
int		 write_history(const char *);
int		 history_expand(char *, char **);
char	       **history_tokenize(const char *);
const char	*get_history_event(const char *, int *, int);
char		*history_arg_extract(int, int, const char *);

char		*tilde_expand(char *);
char		*filename_completion_function(const char *, int);
char		*username_completion_function(const char *, int);
int		 rl_complete(int, int);
int		 rl_read_key(void);
char	       **completion_matches(const char *, CPFunction *);
void		 rl_display_match_list(char **, int, int);

int		 rl_insert(int, int);
void		 rl_reset_terminal(const char *);
int		 rl_bind_key(int, int (*)(int, int));
int		 rl_newline(int, int);
void		 rl_callback_read_char(void);
void		 rl_callback_handler_install(const char *, VFunction *);
void		 rl_callback_handler_remove(void);
void		 rl_redisplay(void);
int		 rl_get_previous_history(int, int);
void		 rl_prep_terminal(int);
void		 rl_deprep_terminal(void);
int		 rl_read_init_file(const char *);
int		 rl_parse_and_bind(const char *);
void		 rl_stuff_char(int);
int		 rl_add_defun(const char *, Function *, int);

/*
 * The following are not implemented
 */
Keymap		 rl_get_keymap(void);
Keymap		 rl_make_bare_keymap(void);
int		 rl_generic_bind(int, const char *, const char *, Keymap);
int		 rl_bind_key_in_map(int, Function *, Keymap);
#ifdef __cplusplus
}
#endif

#endif /* _READLINE_H_ */
