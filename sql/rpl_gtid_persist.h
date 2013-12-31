/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; version 2 of the
   License.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
   02110-1301 USA */

#ifndef RPL_GTID_PERSIST_H_
#define RPL_GTID_PERSIST_H_

#include <string>
using std::string;

class Open_tables_backup;

class Gtid_table_persistor
{

public:
  static const LEX_STRING DB_NAME;
  static const LEX_STRING TABLE_NAME;
  static const uint number_fields= 3;

  Gtid_table_persistor() : need_commit(false), m_count(0) { };
  virtual ~Gtid_table_persistor() { };

  /**
    Insert the gtid into table.

    @param gtid  holds the sidno and the gno.

    @retval
      0    OK
    @retval
      1    The table was not found.
    @retval
      -1   Error
  */
  int save(Gtid *gtid);
  /**
    Insert the gtid set into table.

    @param gtid_executed  contains a set of gtid, which holds
                          the sidno and the gno.

    @retval
      0    OK
    @retval
      1    The table was not found.
    @retval
      -1   Error
  */
  int save(Gtid_set *gtid_executed);
  /**
    Compress gtid_executed table, execute the following
    within a single transaction.
      - Read each row by the PK in increasing order, delete
        consecutive rows from the gtid_executed table and
        fetch these deleted gtids at the same time.
      - Store compressed intervals of these deleted gtids
        into the gtid_executed table.

    @retval
      0    OK
    @retval
      1    The table was not found.
    @retval
      -1   Error
  */
  int compress();
  /**
    Reset the table.

    @retval
      0    OK
    @retval
      1    The table was not found.
    @retval
      -1   Error
  */
  int reset();

  /**
    Fetch gtids from gtid_executed table and store them
    into gtid_executed set.

    @param[out]  gtid_executed store gtids fetched
                 from gtid_executed table

    @retval
      0    OK
    @retval
      1    The table was not found.
    @retval
      -1   Error
  */
  int fetch_gtids_from_table(Gtid_set *gtid_executed);

private:
  /* Need to commit current transaction if it is true */
  bool need_commit;
  /* Count the append size of the table */
  ulong m_count;
  /* The number of threads, which are saving gtid into table */
  ulong saving_threads;
  /* Indicate if the table is being reset */
  bool m_is_resetting;

  /**
    Creates a new thread in the bootstrap process or in the mysqld startup,
    a thread is created in order to be able to access a table.

    @return
      @retval THD* Pointer to thread structure
  */
  THD *create_thd();
  /**
    Destroys the created thread and restores the
    system_thread information.

    @param thd Thread requesting to be destroyed
  */
  void drop_thd(THD* thd);
  /**
    Opens and locks a table.

    It's assumed that the caller knows what they are doing:
    - whether it was necessary to reset-and-backup the open tables state
    - whether the requested lock does not lead to a deadlock
    - whether this open mode would work under LOCK TABLES, or inside a
    stored function or trigger.

    Note that if the table can't be locked successfully this operation will
    close it. Therefore it provides guarantee that it either opens and locks
    table or fails without leaving any tables open.

    @param      thd           Thread requesting to open the table
    @param      lock_type     How to lock the table
    @param[out] table         We will store the open table here
    @param[out] backup        Save the lock info. here

    @return
      @retval TRUE open and lock failed - an error message is pushed into the
                                          stack
      @retval FALSE success
  */
  bool open_table(THD* thd, enum thr_lock_type lock_type,
                  TABLE **table, Open_tables_backup* backup);
  /**
    Commits the changes, unlocks the table and closes it. This method
    needs to be called even if the open_table fails, in order to ensure
    the lock info is properly restored.

    @param thd    Thread requesting to close the table
    @param table  Table to be closed
    @param backup Restore the lock info from here
    @param error  If there was an error while updating
                    the table

    If there is an error, rolls back the current statement. Otherwise,
    commits it. However, if a new thread was created and there is an
    error, the transaction must be rolled back. Otherwise, it must be
    committed. In this case, the changes were not done on behalf of
    any user transaction and if not finished, there would be pending
    changes.

    @return
      @retval FALSE No error
      @retval TRUE  Failure
  */
  bool close_table(THD* thd, TABLE* table, Open_tables_backup* backup,
                   bool error);
  /**
    Write a gtid interval into the gtid_executed table.

    @param  table    Reference to a table object.
    @param  sid      The source id of the gtid interval.
    @param  gno_star The first GNO of the gtid interval.
    @param  gno_end  The last GNO of the gtid interval.

    @return
      @retval 0    OK.
      @retval -1   Error.
  */
  int write_row(TABLE* table, char *sid,
                rpl_gno gno_start, rpl_gno gno_end);
  /**
    Delete all rows in the gtid_executed table.

    @param  table Reference to a table object.

    @return
      @retval 0    OK.
      @retval -1   Error.
  */
  int delete_all(TABLE* table);
  /**
    Read each row by the PK in increasing order, delete
    consecutive rows from the gtid_executed table and
    fetch these deleted gtids at the same time.

    @param  table Reference to a table object.
    @param  gtid_deleted Gtid set deleted.

    @return
      @retval 0    OK.
      @retval -1   Error.
  */
  int delete_consecutive_rows(TABLE* table, Gtid_set *gtid_deleted);
  /**
    Encode the current row fetched from the table into gtid text.

    @param  table Reference to a table object.
    @retval Return the encoded gtid text.
  */
  string encode_gtid_text(TABLE* table);
  /**
    Insert the gtid set into table.

    @param table          The gtid_executed table.
    @param gtid_executed  Contains a set of gtid, which holds
                          the sidno and the gno.

    @retval
      0    OK
    @retval
      1    The table was not found.
    @retval
      -1   Error
  */
  int save(TABLE* table, Gtid_set *gtid_set);
  /**
    Check if previous gtid_interval and current gtid_interval are consecutive.
    They are consecutive if prev_gtid_interval.gno_end + 1 ==
    gtid_interval.gno_start.

    @param prev_gtid_interval A string with sid, gno_start and gno_end.
    @param gtid_interval      A string with sid, gno_start and gno_end.

    @return
      @retval true   prev_gtid_interval and gtid_interval are consecutive.
      @retval false  prev_gtid_interval and gtid_interval are not consecutive.
  */
  bool is_consecutive(string prev_gtid_interval, string gtid_interval);

  /* Prevent user from invoking default assignment function. */
  Gtid_table_persistor& operator=(const Gtid_table_persistor& info);
  /* Prevent user from invoking default constructor function. */
  Gtid_table_persistor(const Gtid_table_persistor& info);
};

extern Gtid_table_persistor *gtid_table_persistor;


#endif /* RPL_GTID_PERSIST_H_ */
