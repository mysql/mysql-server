/*	$NetBSD: readline.h,v 1.1 2001/01/05 21:15:50 jdolecek Exp $	*/

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
#define	_READLINE_H_

#include <sys/types.h>

/* list of readline stuff supported by editline library's readline wrapper */

/* typedefs */
typedef int	  Function(const char *, int);
typedef void	  VFunction(void);
typedef char	 *CPFunction(const char *, int);
typedef char	**CPPFunction(const char *, int, int);

typedef struct _hist_entry {
	const char	*line;
	const char	*data;
} HIST_ENTRY;

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
extern CPFunction	*rl_completion_entry_function;
extern CPPFunction	*rl_attempted_completion_function;
extern int		rl_completion_type;
extern int		rl_completion_query_items;
extern char		*rl_special_prefixes;
extern int		rl_completion_append_character;

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
#ifdef __cplusplus
}
#endif

#endif /* _READLINE_H_ */
