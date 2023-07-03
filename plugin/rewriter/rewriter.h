#ifndef REWRITER_INCLUDED
#define REWRITER_INCLUDED
/*  Copyright (c) 2015, 2022, Oracle and/or its affiliates.

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
    GNU General Public License, version 2.0, for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "my_config.h"

#include <memory>
#include <string>

#include "map_helpers.h"
#include "my_inttypes.h"
#include "plugin/rewriter/rule.h"

/**
  @file rewriter.h

  Facade for the query rewriter. Provides the Rewriter class which performs
  all of the functionality. A query rewriter plugin needs only include this
  file.
*/

class Persisted_rule;

/**
  Implementation of the post parse query rewriter. The public interface
  consists of two operations: refresh(), which loads the rules from the disk
  table, and rewrite_query(), which rewrites a query if applicable.
*/
class Rewriter {
 public:
  Rewriter();

  /**
    The number of rules currently loaded in the hash table. In case of rules
    that fail to load, this number will be lower than the number of rows in
    the database.
  */
  int get_number_loaded_rules() const { return m_digests.size(); }

  ~Rewriter();

  /**
    Attempts to rewrite thd's current query with digest in 'key'.

    @return A Rewrite_result object.
  */
  Rewrite_result rewrite_query(MYSQL_THD thd, const uchar *key);

  /// Empty the hashtable and reload all rules from disk table.
  longlong refresh(MYSQL_THD thd);

  /**
    Implementation of the loading procedure. The server doesn't handle
    different sessions in the same thread, so we load the rules into the hash
    table in this function, intended to be run in a new thread. The main
    thread will do join().

    @param session_thd The session to be used for loading rules.
  */
  void do_refresh(MYSQL_THD session_thd);

 private:
  longlong m_refresh_status;

  /// The in-memory rules hash table.
  malloc_unordered_multimap<std::string, std::unique_ptr<Rule>> m_digests{
      PSI_INSTRUMENT_ME};

  /// Loads the rule retrieved from the database in the hash table.
  bool load_rule(MYSQL_THD thd, Persisted_rule *diskrule);
};

#endif /* REWRITER_INCLUDED */
