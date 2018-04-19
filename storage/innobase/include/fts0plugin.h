/*****************************************************************************

Copyright (c) 2013, 2018, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file include/fts0plugin.h
 Full text search plugin header file

 Created 2013/06/04 Shaohua Wang
 ***********************************************************************/

#ifndef INNOBASE_FTS0PLUGIN_H
#define INNOBASE_FTS0PLUGIN_H

#include "ha_prototypes.h"

extern struct st_mysql_ftparser fts_default_parser;

struct fts_ast_state_t;

#define PARSER_INIT(parser, arg) \
  if (parser->init) {            \
    parser->init(arg);           \
  }
#define PARSER_DEINIT(parser, arg) \
  if (parser->deinit) {            \
    parser->deinit(arg);           \
  }

/** fts parse query by plugin parser.
 @return 0 if parse successfully, or return non-zero. */
int fts_parse_by_parser(ibool mode,   /*!< in: query boolean mode */
                        uchar *query, /*!< in: query string */
                        ulint len,    /*!< in: query string length */
                        st_mysql_ftparser *parse, /*!< in: fts plugin parser */
                        fts_ast_state_t *state);  /*!< in: query parser state */

#endif /* INNOBASE_FTS0PLUGIN_H */
