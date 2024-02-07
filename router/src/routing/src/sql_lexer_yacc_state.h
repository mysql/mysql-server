/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTING_SQL_LEXER_YACC_STATE_INCLUDED
#define ROUTING_SQL_LEXER_YACC_STATE_INCLUDED

#include <cstdlib>

#include "my_inttypes.h"                // uchar, uint, ...
#include "mysql/service_mysql_alloc.h"  // my_free

/**
  This class represents the character input stream consumed during lexical
  analysis.

  In addition to consuming the input stream, this class performs some comment
  pre processing, by filtering out out-of-bound special text from the query
  input stream.

  Two buffers, with pointers inside each, are maintained in parallel. The
  'raw' buffer is the original query text, which may contain out-of-bound
  comments. The 'cpp' (for comments pre processor) is the pre-processed buffer
  that contains only the query text that should be seen once out-of-bound data
  is removed.
*/

/*
  Important: if a new lock type is added, a matching lock description
             must be added to sql_test.cc's lock_descriptions array.
*/
enum thr_lock_type {
  TL_IGNORE = -1,
  TL_UNLOCK, /* UNLOCK ANY LOCK */
  /*
    Parser only! At open_tables() becomes TL_READ or
    TL_READ_NO_INSERT depending on the binary log format
    (SBR/RBR) and on the table category (log table).
    Used for tables that are read by statements which
    modify tables.
  */
  TL_READ_DEFAULT,
  TL_READ, /* Read lock */
  TL_READ_WITH_SHARED_LOCKS,
  /* High prior. than TL_WRITE. Allow concurrent insert */
  TL_READ_HIGH_PRIORITY,
  /* READ, Don't allow concurrent insert */
  TL_READ_NO_INSERT,
  /*
     Write lock, but allow other threads to read / write.
     Used by BDB tables in MySQL to mark that someone is
     reading/writing to the table.
   */
  TL_WRITE_ALLOW_WRITE,
  /*
    parser only! Late bound low_priority_flag.
    At open_tables() becomes thd->insert_lock_default.
  */
  TL_WRITE_CONCURRENT_DEFAULT,
  /*
    WRITE lock used by concurrent insert. Will allow
    READ, if one could use concurrent insert on table.
  */
  TL_WRITE_CONCURRENT_INSERT,
  /*
    parser only! Late bound low_priority flag.
    At open_tables() becomes thd->update_lock_default.
  */
  TL_WRITE_DEFAULT,
  /* WRITE lock that has lower priority than TL_READ */
  TL_WRITE_LOW_PRIORITY,
  /* Normal WRITE lock */
  TL_WRITE,
  /* Abort new lock request with an error */
  TL_WRITE_ONLY
};

/**
  Type of metadata lock request.

  @sa Comments for MDL_object_lock::can_grant_lock() and
      MDL_scoped_lock::can_grant_lock() for details.
*/

enum enum_mdl_type {
  /*
    An intention exclusive metadata lock. Used only for scoped locks.
    Owner of this type of lock can acquire upgradable exclusive locks on
    individual objects.
    This lock type is also used when doing lookups in the dictionary
    cache. When acquiring objects in a schema, we lock the schema with IX
    to prevent the schema from being deleted. This should conceptually
    be an IS lock, but it would have the same behavior as the current IX.
    Compatible with other IX locks, but is incompatible with scoped S and
    X locks.
  */
  MDL_INTENTION_EXCLUSIVE = 0,
  /*
    A shared metadata lock.
    To be used in cases when we are interested in object metadata only
    and there is no intention to access object data (e.g. for stored
    routines or during preparing prepared statements).
    We also mis-use this type of lock for open HANDLERs, since lock
    acquired by this statement has to be compatible with lock acquired
    by LOCK TABLES ... WRITE statement, i.e. SNRW (We can't get by by
    acquiring S lock at HANDLER ... OPEN time and upgrading it to SR
    lock for HANDLER ... READ as it doesn't solve problem with need
    to abort DML statements which wait on table level lock while having
    open HANDLER in the same connection).
    To avoid deadlock which may occur when SNRW lock is being upgraded to
    X lock for table on which there is an active S lock which is owned by
    thread which waits in its turn for table-level lock owned by thread
    performing upgrade we have to use thr_abort_locks_for_thread()
    facility in such situation.
    This problem does not arise for locks on stored routines as we don't
    use SNRW locks for them. It also does not arise when S locks are used
    during PREPARE calls as table-level locks are not acquired in this
    case.
  */
  MDL_SHARED,
  /*
    A high priority shared metadata lock.
    Used for cases when there is no intention to access object data (i.e.
    data in the table).
    "High priority" means that, unlike other shared locks, it is granted
    ignoring pending requests for exclusive locks. Intended for use in
    cases when we only need to access metadata and not data, e.g. when
    filling an INFORMATION_SCHEMA table.
    Since SH lock is compatible with SNRW lock, the connection that
    holds SH lock lock should not try to acquire any kind of table-level
    or row-level lock, as this can lead to a deadlock. Moreover, after
    acquiring SH lock, the connection should not wait for any other
    resource, as it might cause starvation for X locks and a potential
    deadlock during upgrade of SNW or SNRW to X lock (e.g. if the
    upgrading connection holds the resource that is being waited for).
  */
  MDL_SHARED_HIGH_PRIO,
  /*
    A shared metadata lock for cases when there is an intention to read data
    from table.
    A connection holding this kind of lock can read table metadata and read
    table data (after acquiring appropriate table and row-level locks).
    This means that one can only acquire TL_READ, TL_READ_NO_INSERT, and
    similar table-level locks on table if one holds SR MDL lock on it.
    To be used for tables in SELECTs, subqueries, and LOCK TABLE ...  READ
    statements.
  */
  MDL_SHARED_READ,
  /*
    A shared metadata lock for cases when there is an intention to modify
    (and not just read) data in the table.
    A connection holding SW lock can read table metadata and modify or read
    table data (after acquiring appropriate table and row-level locks).
    To be used for tables to be modified by INSERT, UPDATE, DELETE
    statements, but not LOCK TABLE ... WRITE or DDL). Also taken by
    SELECT ... FOR UPDATE.
  */
  MDL_SHARED_WRITE,
  /*
    A version of MDL_SHARED_WRITE lock which has lower priority than
    MDL_SHARED_READ_ONLY locks. Used by DML statements modifying
    tables and using the LOW_PRIORITY clause.
  */
  MDL_SHARED_WRITE_LOW_PRIO,
  /*
    An upgradable shared metadata lock which allows concurrent updates and
    reads of table data.
    A connection holding this kind of lock can read table metadata and read
    table data. It should not modify data as this lock is compatible with
    SRO locks.
    Can be upgraded to SNW, SNRW and X locks. Once SU lock is upgraded to X
    or SNRW lock data modification can happen freely.
    To be used for the first phase of ALTER TABLE.
  */
  MDL_SHARED_UPGRADABLE,
  /*
    A shared metadata lock for cases when we need to read data from table
    and block all concurrent modifications to it (for both data and metadata).
    Used by LOCK TABLES READ statement.
  */
  MDL_SHARED_READ_ONLY,
  /*
    An upgradable shared metadata lock which blocks all attempts to update
    table data, allowing reads.
    A connection holding this kind of lock can read table metadata and read
    table data.
    Can be upgraded to X metadata lock.
    Note, that since this type of lock is not compatible with SNRW or SW
    lock types, acquiring appropriate engine-level locks for reading
    (TL_READ* for MyISAM, shared row locks in InnoDB) should be
    contention-free.
    To be used for the first phase of ALTER TABLE, when copying data between
    tables, to allow concurrent SELECTs from the table, but not UPDATEs.
  */
  MDL_SHARED_NO_WRITE,
  /*
    An upgradable shared metadata lock which allows other connections
    to access table metadata, but not data.
    It blocks all attempts to read or update table data, while allowing
    INFORMATION_SCHEMA and SHOW queries.
    A connection holding this kind of lock can read table metadata modify and
    read table data.
    Can be upgraded to X metadata lock.
    To be used for LOCK TABLES WRITE statement.
    Not compatible with any other lock type except S and SH.
  */
  MDL_SHARED_NO_READ_WRITE,
  /*
    An exclusive metadata lock.
    A connection holding this lock can modify both table's metadata and data.
    No other type of metadata lock can be granted while this lock is held.
    To be used for CREATE/DROP/RENAME TABLE statements and for execution of
    certain phases of other DDL statements.
  */
  MDL_EXCLUSIVE,
  /* This should be the last !!! */
  MDL_TYPE_END
};

/**
  The internal state of the syntax parser.
  This object is only available during parsing,
  and is private to the syntax parser implementation (sql_yacc.yy).
*/
class Yacc_state {
 public:
  Yacc_state() : yacc_yyss(nullptr), yacc_yyvs(nullptr), yacc_yyls(nullptr) {
    reset();
  }

  void reset() {
    if (yacc_yyss != nullptr) {
      my_free(yacc_yyss);
      yacc_yyss = nullptr;
    }
    if (yacc_yyvs != nullptr) {
      my_free(yacc_yyvs);
      yacc_yyvs = nullptr;
    }
    if (yacc_yyls != nullptr) {
      my_free(yacc_yyls);
      yacc_yyls = nullptr;
    }
    m_lock_type = TL_READ_DEFAULT;
    m_mdl_type = MDL_SHARED_READ;
  }

  ~Yacc_state() {
    if (yacc_yyss) {
      my_free(yacc_yyss);
      my_free(yacc_yyvs);
      my_free(yacc_yyls);
    }
  }

  /**
    Reset part of the state which needs resetting before parsing
    substatement.
  */
  void reset_before_substatement() {
    m_lock_type = TL_READ_DEFAULT;
    m_mdl_type = MDL_SHARED_READ;
  }

  /**
    Bison internal state stack, yyss, when dynamically allocated using
    my_yyoverflow().
  */
  uchar *yacc_yyss;

  /**
    Bison internal semantic value stack, yyvs, when dynamically allocated using
    my_yyoverflow().
  */
  uchar *yacc_yyvs;

  /**
    Bison internal location value stack, yyls, when dynamically allocated using
    my_yyoverflow().
  */
  uchar *yacc_yyls;

  /**
    Type of lock to be used for tables being added to the statement's
    table list in table_factor, table_alias_ref, single_multi and
    table_wild_one rules.
    Statements which use these rules but require lock type different
    from one specified by this member have to override it by using
    Query_block::set_lock_for_tables() method.

    The default value of this member is TL_READ_DEFAULT. The only two
    cases in which we change it are:
    - When parsing SELECT HIGH_PRIORITY.
    - Rule for DELETE. In which we use this member to pass information
      about type of lock from delete to single_multi part of rule.

    We should try to avoid introducing new use cases as we would like
    to get rid of this member eventually.
  */
  thr_lock_type m_lock_type;

  /**
    The type of requested metadata lock for tables added to
    the statement table list.
  */
  enum_mdl_type m_mdl_type;

  /*
    TODO: move more attributes from the LEX structure here.
  */
};

#endif
