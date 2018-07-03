#ifndef SQL_ROW_ITERATOR_H_
#define SQL_ROW_ITERATOR_H_

/* Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <string>
#include <vector>

class Item;
class JOIN;
class THD;
struct TABLE;

/**
  A context for reading through a single table using a chosen access method:
  index read, scan, etc, use of cache, etc.. It is mostly meant as an interface,
  but also contains some private member functions that are useful for many
  implementations, such as error handling.

  A RowIterator is a simple iterator; you initialize it, and then read one
  record at a time until Read() returns EOF. A RowIterator can read from
  other Iterators if you want to, e.g., SortingIterator, which takes in records
  from another RowIterator and sorts them.

  The abstraction is not completely tight. In particular, it still leaves some
  specifics to TABLE, such as which columns to read (the read_set). This means
  it would probably be hard as-is to e.g. sort a join of two tables.

  Use by:
@code
  unique_ptr<RowIterator> iterator(new ...);
  if (iterator->Init())
    return true;
  while (iterator->Read() == 0) {
    ...
  }
@endcode
 */
class RowIterator {
 public:
  RowIterator(THD *thd) : m_thd(thd) {}
  virtual ~RowIterator() {}

  /**
    Initialize or reinitialize the iterator. You must always call Init()
    before trying a Read() (but Init() does not imply Read()).

    You can call Init() multiple times; subsequent calls will rewind the
    iterator (or reposition it, depending on whether the iterator takes in
    e.g. a TABLE_REF) and allow you to read the records anew.
   */
  virtual bool Init() = 0;

  /**
    Read a single row. The row data is not actually returned from the function;
    it is put in the table's (or tables', in case of a join) record buffer, ie.,
    table->records[0].

    @retval
      0   OK
    @retval
      -1   End of records
    @retval
      1   Error
   */
  virtual int Read() = 0;

  /**
    Mark the current row buffer as containing a NULL row or not, so that if you
    read from it and the flag is true, you'll get only NULLs no matter what is
    actually in the buffer (typically some old leftover row). This is used
    for outer joins, when an iterator hasn't produced any rows and we need to
    produce a NULL-complemented row. Init() or Read() won't necessarily
    reset this flag, so if you ever set is to true, make sure to also set it
    to false when needed.

    TODO: We shouldn't need this. See the comments on AggregateIterator for
    a bit more discussion on abstracting out a row interface.
   */
  virtual void SetNullRowFlag(bool is_null_row) = 0;

  // In certain queries, such as SELECT FOR UPDATE, UPDATE or DELETE queries,
  // reading rows will automatically take locks on them. (This means that the
  // set of locks taken will depend on whether e.g. the optimizer chose a table
  // scan or used an index, due to InnoDB's row locking scheme with “gap locks”
  // for B-trees instead of full predicate locks.)
  //
  // However, under some transaction isolation levels (READ COMMITTED or
  // less strict), it is possible to release such locks if and only if the row
  // failed a WHERE predicate, as only the returned rows are protected,
  // not _which_ rows are returned. Thus, if Read() returned a row that you did
  // not actually use, you should call UnlockRow() afterwards, which allows the
  // storage engine to release the row lock in such situations.
  //
  // TableRowIterator has a default implementation of this; other iterators
  // should usually either forward the call to their source iterator (if any)
  // or just ignore it. The right behavior depends on the iterator.
  virtual void UnlockRow() = 0;

  struct Child {
    RowIterator *iterator;

    // Normally blank. If not blank, a heading for this iterator
    // saying what kind of role it has to the parent if it is not
    // obvious. E.g., FilterIterator can print iterators that are
    // children because they come out of subselect conditions.
    std::string description;
  };

  virtual std::vector<Child> children() const { return std::vector<Child>(); }

  virtual std::vector<std::string> DebugString() const = 0;

  // If this is the root iterator of a join, points back to the join object.
  // This has one single purpose: EXPLAIN uses it to be able to get the SELECT
  // list and print out any subselects in it; they are not children of
  // the iterator per se, but need to be printed with it.
  //
  // We could have stored the list of these extra subselect iterators directly
  // on the iterator (it breaks the abstraction a bit to refer to JOIN here),
  // but setting a single pointer is cheaper, especially considering that most
  // queries are not EXPLAIN queries and we don't want the overhead for them.
  JOIN *join() const { return m_join; }

  // Should be called by JOIN::create_iterators() only.
  void set_join(JOIN *join) { m_join = join; }

  /**
    Start performance schema batch mode, if supported (otherwise ignored).

    PFS batch mode is a hack to reduce the overhead of performance schema,
    typically applied at the innermost table of the entire join. If you start
    it before scanning the table and then end it afterwards, the entire set
    of handler calls will be timed only once, as a group, and the costs will
    be distributed evenly out. This reduces timer overhead.

    If you start PFS batch mode, you must also take care to end it at the
    end of the scan, one way or the other. Do note that this is true even
    if the query ends abruptly (LIMIT is reached, or an error happens).
    The easiest workaround for this is to simply go through all the open
    handlers and call end_psi_batch_mode_if_started(). See the PFSBatchMode
    class for a useful helper.
   */
  virtual void StartPSIBatchMode() {}

  /**
    Ends performance schema batch mode, if started. It's always safe to
    call this.
   */
  virtual void EndPSIBatchModeIfStarted() {}

 protected:
  THD *thd() const { return m_thd; }

 private:
  THD *const m_thd;
  JOIN *m_join = nullptr;
};

class TableRowIterator : public RowIterator {
 public:
  TableRowIterator(THD *thd, TABLE *table) : RowIterator(thd), m_table(table) {}

  void UnlockRow() override;
  void SetNullRowFlag(bool is_null_row) override;
  void StartPSIBatchMode() override;
  void EndPSIBatchModeIfStarted() override;

 protected:
  int HandleError(int error);
  void PrintError(int error);
  TABLE *table() const { return m_table; }

 private:
  TABLE *const m_table;

  friend class AlternativeIterator;
};

#endif  // SQL_ROW_ITERATOR_H_
