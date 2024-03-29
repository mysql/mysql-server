/*
  Copyright (c) 2022, 2023, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTING_SQL_LEXER_PARSER_INFO_INCLUDED
#define ROUTING_SQL_LEXER_PARSER_INFO_INCLUDED

/**
  Input parameters to the parser.
*/
struct Parser_input {
  /**
    True if the text parsed corresponds to an actual query,
    and not another text artifact.
    This flag is used to disable digest parsing of nested:
    - view definitions
    - table trigger definitions
    - table partition definitions
    - event scheduler event definitions
  */
  bool m_has_digest;
  /**
    True if the caller needs to compute a digest.
    This flag is used to request explicitly a digest computation,
    independently of the performance schema configuration.
  */
  bool m_compute_digest;

  Parser_input() : m_has_digest(false), m_compute_digest(false) {}
};

#endif
