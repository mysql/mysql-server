/*****************************************************************************

Copyright (c) 2013, 2017, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/******************************************************************//**
@file fts/fts0plugin.cc
Full Text Search plugin support.

Created 2013/06/04 Shaohua Wang
***********************************************************************/

#include "ft_global.h"
#include "fts0ast.h"
#include "fts0plugin.h"
#include "fts0tokenize.h"
#include "my_inttypes.h"

/******************************************************************//**
FTS default parser init
@return 0 */
static
int
fts_default_parser_init(
/*====================*/
	MYSQL_FTPARSER_PARAM *param)	/*!< in: plugin parser param */
{
	return(0);
}

/******************************************************************//**
FTS default parser deinit
@return 0 */
static
int
fts_default_parser_deinit(
/*======================*/
	MYSQL_FTPARSER_PARAM *param)	/*!< in: plugin parser param */
{
        return(0);
}

/******************************************************************//**
FTS default parser parse from ft_static.c in MYISAM.
@return 0 if parse successfully, or return non-zero */
static
int
fts_default_parser_parse(
/*=====================*/
	MYSQL_FTPARSER_PARAM *param)	/*!< in: plugin parser param */
{
	return(param->mysql_parse(param, param->doc, param->length));
}

/* FTS default parser from ft_static.c in MYISAM. */
struct st_mysql_ftparser fts_default_parser =
{
	MYSQL_FTPARSER_INTERFACE_VERSION,
	fts_default_parser_parse,
	fts_default_parser_init,
	fts_default_parser_deinit
};

/******************************************************************//**
Get a operator node from token boolean info
@return node */
static
fts_ast_node_t*
fts_query_get_oper_node(
/*====================*/
	MYSQL_FTPARSER_BOOLEAN_INFO*	info,	/*!< in: token info */
	fts_ast_state_t*		state)	/*!< in/out: query parse state*/
{
	fts_ast_node_t*	oper_node = NULL;

	if (info->yesno > 0) {
		oper_node = fts_ast_create_node_oper(state, FTS_EXIST);
	} else if (info->yesno < 0) {
		oper_node = fts_ast_create_node_oper(state, FTS_IGNORE);
	} else if (info->weight_adjust > 0) {
		oper_node = fts_ast_create_node_oper(state, FTS_INCR_RATING);
	} else if (info->weight_adjust < 0) {
		oper_node = fts_ast_create_node_oper(state, FTS_DECR_RATING);
	} else if (info->wasign > 0) {
		oper_node = fts_ast_create_node_oper(state, FTS_NEGATE);
	}

	return(oper_node);
}

/******************************************************************//**
FTS plugin parser 'myql_add_word' callback function for query parse.
Refer to 'st_mysql_ftparser_param' for more detail.
Note:
a. Parse logic refers to 'ftb_query_add_word' from ft_boolean_search.c in MYISAM;
b. Parse node or tree refers to fts0pars.y.
@return 0 if add successfully, or return non-zero. */
static
int
fts_query_add_word_for_parser(
/*==========================*/
	MYSQL_FTPARSER_PARAM*	param,		/*!< in: parser param */
	char*			word,		/*!< in: token */
	int			word_len,	/*!< in: token length */
	MYSQL_FTPARSER_BOOLEAN_INFO*	info)	/*!< in: token info */
{
	fts_ast_state_t* state =
		static_cast<fts_ast_state_t*>(param->mysql_ftparam);
	fts_ast_node_t*	cur_node = state->cur_node;
	fts_ast_node_t*	oper_node = NULL;
	fts_ast_node_t*	term_node = NULL;
	fts_ast_node_t*	node = NULL;

	switch (info->type) {
	case FT_TOKEN_STOPWORD:
		/* We only handler stopword in phrase */
		if (cur_node->type != FTS_AST_PARSER_PHRASE_LIST) {
			break;
		}
		// Fall through.

	case FT_TOKEN_WORD:
		term_node = fts_ast_create_node_term_for_parser(
			state, word, word_len);

		if (info->trunc) {
			fts_ast_term_set_wildcard(term_node);
		}

		if (cur_node->type == FTS_AST_PARSER_PHRASE_LIST) {
			/* Ignore operator inside phrase */
			fts_ast_add_node(cur_node, term_node);
		} else {
			ut_ad(cur_node->type == FTS_AST_LIST
			      || cur_node->type == FTS_AST_SUBEXP_LIST);
			oper_node = fts_query_get_oper_node(info, state);

			if (oper_node) {
				node = fts_ast_create_node_list(state, oper_node);
				fts_ast_add_node(node, term_node);
				fts_ast_add_node(cur_node, node);
			} else {
				fts_ast_add_node(cur_node, term_node);
			}
		}

		break;

	case FT_TOKEN_LEFT_PAREN:
		/* Check parse error */
		if (cur_node->type != FTS_AST_LIST
		    && cur_node->type != FTS_AST_SUBEXP_LIST) {
			return(1);
		}

		/* Set operator */
                oper_node = fts_query_get_oper_node(info, state);
		if (oper_node != NULL) {
			node = fts_ast_create_node_list(state, oper_node);
			fts_ast_add_node(cur_node, node);
			node->go_up = true;
			node->up_node = cur_node;
			cur_node = node;
		}

		if (info->quot) {
			/* Phrase node */
			node = fts_ast_create_node_phrase_list(state);
		} else {
			/* Subexp list node */
			node = fts_ast_create_node_subexp_list(state, NULL);
		}

		fts_ast_add_node(cur_node, node);

		node->up_node = cur_node;
		state->cur_node = node;
		state->depth += 1;

		break;

	case FT_TOKEN_RIGHT_PAREN:
		info->quot = 0;

		if (cur_node->up_node != NULL) {
			cur_node = cur_node->up_node;

			if (cur_node->go_up) {
				ut_a(cur_node->up_node
				     && !(cur_node->up_node->go_up));
				cur_node = cur_node->up_node;
			}
		}

		state->cur_node = cur_node;

		if (state->depth > 0) {
			state->depth--;
		} else {
			/* Parentheses mismatch */
			return(1);
		}

		break;

	case FT_TOKEN_EOF:
	default:
		break;
	}

	return(0);
}

/******************************************************************//**
FTS plugin parser 'myql_parser' callback function for query parse.
Refer to 'st_mysql_ftparser_param' for more detail.
@return 0 if parse successfully */
static
int
fts_parse_query_internal(
/*=====================*/
	MYSQL_FTPARSER_PARAM*	param,	/*!< in: parser param */
	char*			query,	/*!< in: query string */
	int			len)	/*!< in: query length */
{
	MYSQL_FTPARSER_BOOLEAN_INFO	info;
	const CHARSET_INFO*		cs = param->cs;
	uchar**	start = reinterpret_cast<uchar**>(&query);
	uchar*	end = reinterpret_cast<uchar*>(query + len);
	FT_WORD	w = {NULL, 0, 0};

	info.prev = ' ';
	info.quot = 0;
	memset(&w, 0, sizeof(w));
	/* Note: We don't handle simple parser mode here,
	but user supplied plugin parser should handler it. */
	while (fts_get_word(cs, start, end, &w, &info)) {
		int ret = param->mysql_add_word(
				param,
				reinterpret_cast<char*>(w.pos),
				w.len, &info);
		if (ret) {
			return(ret);
		}
	}

	return(0);
}

/******************************************************************//**
fts parse query by plugin parser.
@return 0 if parse successfully, or return non-zero. */
int
fts_parse_by_parser(
/*================*/
	ibool			mode,		/*!< in: parse boolean mode */
	uchar*			query_str,	/*!< in: query string */
	ulint			query_len,	/*!< in: query string length */
	st_mysql_ftparser*	parser,		/*!< in: fts plugin parser */
	fts_ast_state_t*	state)		/*!< in/out: parser state */
{
	MYSQL_FTPARSER_PARAM	param;
	int	ret;

	ut_ad(parser);

	/* Initial parser param */
	param.mysql_parse = fts_parse_query_internal;
	param.mysql_add_word = fts_query_add_word_for_parser;
	param.mysql_ftparam = static_cast<void*>(state);
	param.cs = state->charset;
	param.doc = reinterpret_cast<char*>(query_str);
	param.length = static_cast<int>(query_len);
	param.flags = 0;
	param.mode = mode ?
		     MYSQL_FTPARSER_FULL_BOOLEAN_INFO :
		     MYSQL_FTPARSER_SIMPLE_MODE;

	PARSER_INIT(parser, &param);
	ret = parser->parse(&param);
	PARSER_DEINIT(parser, &param);

	return(ret | state->depth);
}
