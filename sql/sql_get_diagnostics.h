/* Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef SQL_GET_DIAGNOSTICS_H
#define SQL_GET_DIAGNOSTICS_H

/** Diagnostics information forward reference. */
class Diagnostics_information;


/**
  Sql_cmd_get_diagnostics represents a GET DIAGNOSTICS statement.

  The GET DIAGNOSTICS statement retrieves exception or completion
  condition information from a diagnostics area, usually pertaining
  to the last non-diagnostic SQL statement that was executed.
*/
class Sql_cmd_get_diagnostics : public Sql_cmd
{
public:
  /**
    Constructor, used to represent a GET DIAGNOSTICS statement.

    @param info Diagnostics information to be obtained.
  */
  Sql_cmd_get_diagnostics(Diagnostics_information *info)
    : m_info(info)
  {}

  virtual enum_sql_command sql_command_code() const
  {
    return SQLCOM_GET_DIAGNOSTICS;
  }

  virtual bool execute(THD *thd);

private:
  /** The information to be obtained. */
  Diagnostics_information *m_info;
};


/**
  Represents the diagnostics information to be obtained.

  Diagnostic information is made available through statement
  information and condition information items.
*/
class Diagnostics_information : public Sql_alloc
{
public:
  /**
    Which diagnostics area to access.
    Only CURRENT is supported for now.
  */
  enum Which_area
  {
    /** Access the first diagnostics area. */
    CURRENT_AREA
  };

  /** Set which diagnostics area to access. */
  void set_which_da(Which_area area)
  { m_area= area; }

  /** Get which diagnostics area to access. */
  Which_area get_which_da(void) const
  { return m_area; }

  /**
    Aggregate diagnostics information.

    @param thd  The current thread.
    @param da   The diagnostics area.

    @retval false on success.
    @retval true on error
  */
  virtual bool aggregate(THD *thd, const Diagnostics_area *da) = 0;

protected:
  /**
    Diagnostics_information objects are allocated in thd->mem_root.
    Do not rely on the destructor for any cleanup.
  */
  virtual ~Diagnostics_information()
  {
    DBUG_ASSERT(false);
  }

  /**
    Evaluate a diagnostics information item in a specific context.

    @param thd        The current thread.
    @param diag_item  The diagnostics information item.
    @param ctx        The context to evaluate the item.

    @retval false on success.
    @retval true on error.
  */
  template <typename Diag_item, typename Context>
  bool evaluate(THD *thd, Diag_item *diag_item, Context ctx)
  {
    Item *value;

    /* Get this item's value. */
    if (! (value= diag_item->get_value(thd, ctx)))
      return true;

    /* Set variable/parameter value. */
    return diag_item->set_value(thd, &value);
  }

private:
  /** Which diagnostics area to access. */
  Which_area m_area;
};


/**
  A diagnostics information item. Used to associate a specific
  diagnostics information item to a target variable.
*/
class Diagnostics_information_item : public Sql_alloc
{
public:
  /**
    Set a value for this item.

    @param thd    The current thread.
    @param value  The obtained value.

    @retval false on success.
    @retval true on error.
  */
  bool set_value(THD *thd, Item **value);

protected:
  /**
    Constructor, used to represent a diagnostics information item.

    @param target A target that gets the value of this item.
  */
  Diagnostics_information_item(Item *target)
    : m_target(target)
  {}

  /**
    Diagnostics_information_item objects are allocated in thd->mem_root.
    Do not rely on the destructor for any cleanup.
  */
  virtual ~Diagnostics_information_item()
  {
    DBUG_ASSERT(false);
  }

private:
  /** The target variable that will receive the value of this item. */
  Item *m_target;
};


/**
  A statement information item.
*/
class Statement_information_item : public Diagnostics_information_item
{
public:
  /** The name of a statement information item. */
  enum Name
  {
    NUMBER,
    ROW_COUNT
  };

  /**
    Constructor, used to represent a statement information item.

    @param name   The name of this item.
    @param target A target that gets the value of this item.
  */
  Statement_information_item(Name name, Item *target)
    : Diagnostics_information_item(target), m_name(name)
  {}

  /** Obtain value of this statement information item. */
  Item *get_value(THD *thd, const Diagnostics_area *da);

private:
  /** The name of this statement information item. */
  Name m_name;
};


/**
  Statement information.

  @remark Provides information about the execution of a statement.
*/
class Statement_information : public Diagnostics_information
{
public:
  /**
    Constructor, used to represent the statement information of a
    GET DIAGNOSTICS statement.

    @param items  List of requested statement information items.
  */
  Statement_information(List<Statement_information_item> *items)
    : m_items(items)
  {}

  /** Obtain statement information in the context of a diagnostics area. */
  bool aggregate(THD *thd, const Diagnostics_area *da);

private:
  /* List of statement information items. */
  List<Statement_information_item> *m_items;
};

#endif

