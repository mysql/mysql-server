/* Copyright (c) 2000, 2019, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file

  @brief
  This file defines all performance schema native functions
*/

#include "sql/item_pfs_func.h"

#include <cmath>

#include "sql/derror.h"  // ER_THD
#include "sql/sql_lex.h"

extern bool pfs_enabled;

/** ps_current_thread_id() */

bool Item_func_pfs_current_thread_id::itemize(Parse_context *pc, Item **res) {
  if (skip_itemize(res)) return false;
  if (super::itemize(pc, res)) return true;
  pc->thd->lex->safe_to_cache_query = false; /* result can vary */
  return false;
}

bool Item_func_pfs_current_thread_id::resolve_type(THD *) {
  unsigned_flag = true;
  maybe_null = true;
  return false;
}

bool Item_func_pfs_current_thread_id::fix_fields(THD *thd, Item **ref) {
  if (super::fix_fields(thd, ref)) return true;
  thd->thread_specific_used = true; /* for binlog */
  return false;
}

longlong Item_func_pfs_current_thread_id::val_int() {
  DBUG_ASSERT(fixed);
  /* Verify Performance Schema available. */
  if (!pfs_enabled) {
    my_printf_error(ER_WRONG_PERFSCHEMA_USAGE,
                    "'%s': The Performance Schema is not enabled.", MYF(0),
                    func_name());
    return error_int();
  }
#ifdef HAVE_PSI_THREAD_INTERFACE
  /* Return the thread id for this connection. */
  m_thread_id = PSI_THREAD_CALL(get_current_thread_internal_id)();
#endif
  /* Valid thread id is > 0. */
  if (m_thread_id == 0) {
    return error_int();
  }
  return static_cast<longlong>(m_thread_id);
}

/** ps_thread_id() */

bool Item_func_pfs_thread_id::itemize(Parse_context *pc, Item **res) {
  if (skip_itemize(res)) return false;
  if (super::itemize(pc, res)) return true;
  pc->thd->lex->safe_to_cache_query = false; /* result can vary */
  return false;
}

bool Item_func_pfs_thread_id::resolve_type(THD *) {
  unsigned_flag = true;
  maybe_null = true;
  return false;
}

longlong Item_func_pfs_thread_id::val_int() {
  DBUG_ASSERT(fixed);
  /* If input is null, return null. */
  null_value = args[0]->null_value;
  if (null_value) {
    return error_int();
  }
  /* Verify Performance Schema available. */
  if (!pfs_enabled) {
    my_printf_error(ER_WRONG_PERFSCHEMA_USAGE,
                    "'%s': The Performance Schema is not enabled.", MYF(0),
                    func_name());
    return error_int();
  }
  /* Verify non-negative integer input. */
  if (!is_integer_type(args[0]->data_type()) || args[0]->val_int() < 0) {
    return error_int();
  }
#ifdef HAVE_PSI_THREAD_INTERFACE
  /* Get the thread id assigned to the processlist id. */
  m_processlist_id = args[0]->val_int();
  PSI_thread *psi = PSI_THREAD_CALL(get_thread_by_id)(m_processlist_id);
  if (psi) {
    m_thread_id = PSI_THREAD_CALL(get_thread_internal_id)(psi);
  }
#endif
  /* Valid thread id is > 0. */
  if (m_thread_id == 0) {
    return error_int();
  }
  return static_cast<longlong>(m_thread_id);
}

/** format_bytes() */

bool Item_func_pfs_format_bytes::resolve_type(THD *) {
  maybe_null = true;
  collation.set(&my_charset_utf8_general_ci);
  /* Format is 'AAAA.BB UUU' = 11 characters or 'AAAA bytes' = 10 characters. */
  fix_char_length(11);
  return false;
}

String *Item_func_pfs_format_bytes::val_str(String *) {
  /* If input is null, return null. */
  null_value = args[0]->null_value;
  if (null_value) {
    return error_str();
  }

  /* Check for numeric input. Negative values are ok. */
  if (!is_numeric_type(args[0]->data_type())) {
    my_error(ER_DATA_OUT_OF_RANGE, MYF(0), "Input", func_name());
    return error_str();
  }

  /* Declaring 'volatile' as workaround for 32-bit optimization bug. */
  volatile double bytes = args[0]->val_real();
  volatile double bytes_abs = std::abs(bytes);

  volatile const double kib = 1024ULL;
  volatile const double mib = static_cast<double>(1024ULL * kib);
  volatile const double gib = static_cast<double>(1024ULL * mib);
  volatile const double tib = static_cast<double>(1024ULL * gib);
  volatile const double pib = static_cast<double>(1024ULL * tib);
  volatile const double eib = static_cast<double>(1024ULL * pib);

  volatile double divisor;
  int len;
  const char *unit;

  if (bytes_abs >= eib) {
    divisor = eib;
    unit = "EiB";
  } else if (bytes_abs >= pib) {
    divisor = pib;
    unit = "PiB";
  } else if (bytes_abs >= tib) {
    divisor = tib;
    unit = "TiB";
  } else if (bytes_abs >= gib) {
    divisor = gib;
    unit = "GiB";
  } else if (bytes_abs >= mib) {
    divisor = mib;
    unit = "MiB";
  } else if (bytes_abs >= kib) {
    divisor = kib;
    unit = "KiB";
  } else {
    divisor = 1;
    unit = "bytes";
  }

  if (divisor == 1) {
    len = sprintf(m_value_buffer, "%4d %s", (int)bytes, unit);
  } else {
    double value = bytes / divisor;
    if (std::abs(value) >= 100000.0) {
      len = sprintf(m_value_buffer, "%4.2e %s", value, unit);
    } else {
      len = sprintf(m_value_buffer, "%4.2f %s", value, unit);
    }
  }

  m_value.set(m_value_buffer, len, &my_charset_utf8_general_ci);
  return &m_value;
}

/** format_pico_time() */

bool Item_func_pfs_format_pico_time::resolve_type(THD *) {
  maybe_null = true;
  collation.set(&my_charset_utf8_general_ci);
  /* Format is 'AAAA.BB UUU' = 11 characters or 'AAA ps' = 6 characters. */
  fix_char_length(11);
  return false;
}

String *Item_func_pfs_format_pico_time::val_str(String *) {
  /* If input is null, return null. */
  null_value = args[0]->null_value;
  if (null_value) {
    return error_str();
  }

  /* Check for numeric input. Negative values are ok. */
  if (!is_numeric_type(args[0]->data_type())) {
    my_error(ER_DATA_OUT_OF_RANGE, MYF(0), "Input", func_name());
    return error_str();
  }

  /* Declaring 'volatile' as workaround for 32-bit optimization bug. */
  volatile const double nano = 1000ull;
  volatile const double micro = static_cast<double>(1000ull * nano);
  volatile const double milli = static_cast<double>(1000ull * micro);
  volatile const double sec = static_cast<double>(1000ull * milli);
  volatile const double min = static_cast<double>(60ull * sec);
  volatile const double hour = static_cast<double>(60ull * min);
  volatile const double day = static_cast<double>(24ull * hour);

  volatile double time_val = args[0]->val_real();
  volatile double time_abs = std::abs(time_val);

  volatile double divisor;
  int len;
  const char *unit;

  /* SI-approved time units. */
  if (time_abs >= day) {
    divisor = day;
    unit = "d";
  } else if (time_abs >= hour) {
    divisor = hour;
    unit = "h";
  } else if (time_abs >= min) {
    divisor = min;
    unit = "min";
  } else if (time_abs >= sec) {
    divisor = sec;
    unit = "s";
  } else if (time_abs >= milli) {
    divisor = milli;
    unit = "ms";
  } else if (time_abs >= micro) {
    divisor = micro;
    unit = "us";
  } else if (time_abs >= nano) {
    divisor = nano;
    unit = "ns";
  } else {
    divisor = 1;
    unit = "ps";
  }

  if (divisor == 1) {
    len = sprintf(m_value_buffer, "%3d %s", (int)time_val, unit);
  } else {
    double value = time_val / divisor;
    if (std::abs(value) >= 100000.0) {
      len = sprintf(m_value_buffer, "%4.2e %s", value, unit);
    } else {
      len = sprintf(m_value_buffer, "%4.2f %s", value, unit);
    }
  }

  m_value.set(m_value_buffer, len, &my_charset_utf8_general_ci);
  return &m_value;
}
