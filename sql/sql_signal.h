/* Copyright (c) 2008 MySQL AB, 2009 Sun Microsystems, Inc.
   Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef SQL_SIGNAL_H
#define SQL_SIGNAL_H

/**
  Signal_common represents the common properties of the SIGNAL and RESIGNAL
  statements.
*/
class Signal_common : public Sql_statement
{
protected:
  /**
    Constructor.
    @param lex the LEX structure for this statement.
    @param cond the condition signaled if any, or NULL.
    @param set collection of signal condition item assignments.
  */
  Signal_common(LEX *lex,
                const sp_cond_type_t *cond,
                const Set_signal_information& set)
    : Sql_statement(lex),
      m_cond(cond),
      m_set_signal_information(set)
  {}

  virtual ~Signal_common()
  {}

  /**
    Assign the condition items 'MYSQL_ERRNO', 'level' and 'MESSAGE_TEXT'
    default values of a condition.
    @param cond the condition to update.
    @param set_level_code true if 'level' and 'MYSQL_ERRNO' needs to be overwritten
    @param level the level to assign
    @param sqlcode the sql code to assign
  */
  static void assign_defaults(MYSQL_ERROR *cond,
                              bool set_level_code,
                              MYSQL_ERROR::enum_warning_level level,
                              int sqlcode);

  /**
    Evaluate the condition items 'SQLSTATE', 'MYSQL_ERRNO', 'level' and 'MESSAGE_TEXT'
    default values for this statement.
    @param thd the current thread.
    @param cond the condition to update.
  */
  void eval_defaults(THD *thd, MYSQL_ERROR *cond);

  /**
    Evaluate each signal condition items for this statement.
    @param thd the current thread.
    @param cond the condition to update.
    @return 0 on success.
  */
  int eval_signal_informations(THD *thd, MYSQL_ERROR *cond);

  /**
    Raise a SQL condition.
    @param thd the current thread.
    @param cond the condition to raise.
    @return false on success.
  */
  bool raise_condition(THD *thd, MYSQL_ERROR *cond);

  /**
    The condition to signal or resignal.
    This member is optional and can be NULL (RESIGNAL).
  */
  const sp_cond_type_t *m_cond;

  /**
    Collection of 'SET item = value' assignments in the
    SIGNAL/RESIGNAL statement.
  */
  Set_signal_information m_set_signal_information;
};

/**
  Signal_statement represents a SIGNAL statement.
*/
class Signal_statement : public Signal_common
{
public:
  /**
    Constructor, used to represent a SIGNAL statement.
    @param lex the LEX structure for this statement.
    @param cond the SQL condition to signal (required).
    @param set the collection of signal informations to signal.
  */
  Signal_statement(LEX *lex,
                const sp_cond_type_t *cond,
                const Set_signal_information& set)
    : Signal_common(lex, cond, set)
  {}

  virtual ~Signal_statement()
  {}

  /**
    Execute a SIGNAL statement at runtime.
    @param thd the current thread.
    @return false on success.
  */
  virtual bool execute(THD *thd);
};

/**
  Resignal_statement represents a RESIGNAL statement.
*/
class Resignal_statement : public Signal_common
{
public:
  /**
    Constructor, used to represent a RESIGNAL statement.
    @param lex the LEX structure for this statement.
    @param cond the SQL condition to resignal (optional, may be NULL).
    @param set the collection of signal informations to resignal.
  */
  Resignal_statement(LEX *lex,
                     const sp_cond_type_t *cond,
                     const Set_signal_information& set)
    : Signal_common(lex, cond, set)
  {}

  virtual ~Resignal_statement()
  {}

  /**
    Execute a RESIGNAL statement at runtime.
    @param thd the current thread.
    @return 0 on success.
  */
  virtual bool execute(THD *thd);
};

#endif

