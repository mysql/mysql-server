/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef ERROR_HANDLER_INCLUDED
#define ERROR_HANDLER_INCLUDED

#include "my_global.h"
#include "mysqld_error.h"  // ER_*
#include "sql_error.h"     // Sql_condition

class THD;
struct TABLE_LIST;

/**
  This class represents the interface for internal error handlers.
  Internal error handlers are exception handlers used by the server
  implementation.
*/
class Internal_error_handler
{
protected:
  Internal_error_handler() :
    m_prev_internal_handler(NULL)
  {}

  virtual ~Internal_error_handler() {}

public:
  /**
    Handle a sql condition.
    This method can be implemented by a subclass to achieve any of the
    following:
    - mask a warning/error internally, prevent exposing it to the user,
    - mask a warning/error and throw another one instead.
    When this method returns true, the sql condition is considered
    'handled', and will not be propagated to upper layers.
    It is the responsability of the code installing an internal handler
    to then check for trapped conditions, and implement logic to recover
    from the anticipated conditions trapped during runtime.

    This mechanism is similar to C++ try/throw/catch:
    - 'try' correspond to <code>THD::push_internal_handler()</code>,
    - 'throw' correspond to <code>my_error()</code>,
    which invokes <code>my_message_sql()</code>,
    - 'catch' correspond to checking how/if an internal handler was invoked,
    before removing it from the exception stack with
    <code>THD::pop_internal_handler()</code>.

    @param thd the calling thread
    @param cond the condition raised.
    @return true if the condition is handled
  */
  virtual bool handle_condition(THD *thd,
                                uint sql_errno,
                                const char* sqlstate,
                                Sql_condition::enum_severity_level *level,
                                const char* msg) = 0;

private:
  Internal_error_handler *m_prev_internal_handler;
  friend class THD;
};


/**
  Implements the trivial error handler which cancels all error states
  and prevents an SQLSTATE to be set.
*/

class Dummy_error_handler : public Internal_error_handler
{
public:
  virtual bool handle_condition(THD *thd,
                                uint sql_errno,
                                const char* sqlstate,
                                Sql_condition::enum_severity_level *level,
                                const char* msg)
  {
    /* Ignore error */
    return true;
  }
};


/**
  This class is an internal error handler implementation for
  DROP TABLE statements. The thing is that there may be warnings during
  execution of these statements, which should not be exposed to the user.
  This class is intended to silence such warnings.
*/

class Drop_table_error_handler : public Internal_error_handler
{
public:
  virtual bool handle_condition(THD *thd,
                                uint sql_errno,
                                const char* sqlstate,
                                Sql_condition::enum_severity_level *level,
                                const char* msg);
};


/**
  Internal error handler to process an error from MDL_context::upgrade_lock()
  and mysql_lock_tables(). Used by implementations of HANDLER READ and
  LOCK TABLES LOCAL.
*/

class MDL_deadlock_and_lock_abort_error_handler: public Internal_error_handler
{
public:
  virtual bool handle_condition(THD *thd,
                                uint sql_errno,
                                const char *sqlstate,
                                Sql_condition::enum_severity_level *level,
                                const char* msg)
  {
    if (sql_errno == ER_LOCK_ABORTED || sql_errno == ER_LOCK_DEADLOCK)
      m_need_reopen= true;

    return m_need_reopen;
  }

  bool need_reopen() const { return m_need_reopen; };
  void init() { m_need_reopen= false; };
private:
  bool m_need_reopen;
};


/**
   An Internal_error_handler that suppresses errors regarding views'
   underlying tables that occur during privilege checking. It hides errors which
   show view underlying table information.
   This happens in the cases when

   - A view's underlying table (e.g. referenced in its SELECT list) does not
     exist or columns of underlying table are altered. There should not be an
     error as no attempt was made to access it per se.

   - Access is denied for some table, column, function or stored procedure
     such as mentioned above. This error gets raised automatically, since we
     can't untangle its access checking from that of the view itself.

    There are currently two mechanisms at work that handle errors for views
    based on an Internal_error_handler. This one and another one is
    Show_create_error_handler. The latter handles errors encountered during
    execution of SHOW CREATE VIEW, while this mechanism using this method is
    handles SELECT from views. The two methods should not clash.

*/
class View_error_handler : public Internal_error_handler
{
  TABLE_LIST *m_top_view;

public:
  View_error_handler(TABLE_LIST *top_view) :
  m_top_view(top_view)
  {}
  virtual bool handle_condition(THD *thd, uint sql_errno, const char *,
                                Sql_condition::enum_severity_level *level,
                                const char *message);
};

/**
  This internal handler is used to trap ER_NO_SUCH_TABLE.
*/

class No_such_table_error_handler : public Internal_error_handler
{
public:
  No_such_table_error_handler()
    : m_handled_errors(0), m_unhandled_errors(0)
  {}

  virtual bool handle_condition(THD *thd,
                                uint sql_errno,
                                const char* sqlstate,
                                Sql_condition::enum_severity_level *level,
                                const char* msg)
  {
    if (sql_errno == ER_NO_SUCH_TABLE)
    {
      m_handled_errors++;
      return true;
    }

    m_unhandled_errors++;
    return false;
  }

  /**
    Returns true if one or more ER_NO_SUCH_TABLE errors have been
    trapped and no other errors have been seen. false otherwise.
  */
  bool safely_trapped_errors() const
  {
    /*
      If m_unhandled_errors != 0, something else, unanticipated, happened,
      so the error is not trapped but returned to the caller.
      Multiple ER_NO_SUCH_TABLE can be raised in case of views.
    */
    return ((m_handled_errors > 0) && (m_unhandled_errors == 0));
  }

private:
  int m_handled_errors;
  int m_unhandled_errors;
};


/**
  This internal handler implements downgrade from SL_ERROR to SL_WARNING
  for statements which support IGNORE.
*/

class Ignore_error_handler : public Internal_error_handler
{
public:
  virtual bool handle_condition(THD *thd,
                                uint sql_errno,
                                const char* sqlstate,
                                Sql_condition::enum_severity_level *level,
                                const char* msg);
};

/**
  This internal handler implements upgrade from SL_WARNING to SL_ERROR
  for the error codes affected by STRICT mode. Currently STRICT mode does
  not affect SELECT statements.
*/

class Strict_error_handler : public Internal_error_handler
{
public:
  enum enum_set_select_behavior
  {
    DISABLE_SET_SELECT_STRICT_ERROR_HANDLER,
    ENABLE_SET_SELECT_STRICT_ERROR_HANDLER
  };

  Strict_error_handler()
    : m_set_select_behavior(DISABLE_SET_SELECT_STRICT_ERROR_HANDLER)
  {}

  Strict_error_handler(enum_set_select_behavior param)
    : m_set_select_behavior(param)
  {}

  virtual bool handle_condition(THD *thd,
                                uint sql_errno,
                                const char* sqlstate,
                                Sql_condition::enum_severity_level *level,
                                const char* msg);

private:
  /*
    For SELECT and SET statement, we do not always give error in STRICT mode.
    For triggers, Strict_error_handler is pushed in the beginning of statement.
    If a SELECT or SET is executed from the Trigger, it should not always give
    error. We use this flag to choose when to give error and when warning.
  */
  enum_set_select_behavior m_set_select_behavior;
};


#endif // ERROR_HANDLER_INCLUDED
