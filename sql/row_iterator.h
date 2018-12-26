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

class Item;
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

  TODO: Convert the joins themselves into RowIterator.

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

 protected:
  THD *thd() const { return m_thd; }

 private:
  THD *const m_thd;
};

class TableRowIterator : public RowIterator {
 public:
  TableRowIterator(THD *thd, TABLE *table) : RowIterator(thd), m_table(table) {}

  void UnlockRow() override;

 protected:
  int HandleError(int error);
  void PrintError(int error);
  void PushDownCondition(Item *condition);
  TABLE *table() const { return m_table; }

 private:
  TABLE *const m_table;

  friend class AlternativeIterator;
};

#endif  // SQL_ROW_ITERATOR_H_
