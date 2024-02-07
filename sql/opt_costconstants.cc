/*
   Copyright (c) 2014, 2024, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/opt_costconstants.h"

#include <assert.h>

#include "mysql/components/services/bits/psi_bits.h"
#include "mysql/strings/m_ctype.h"
#include "sql/handler.h"
#include "sql/sql_plugin_ref.h"
#include "sql/table.h"  // TABLE

/**
  The default value for storage device type. If device type information
  is added to the data dictionary or the storage engines start to
  provide this information, this default can be replaced.
*/
const unsigned int DEFAULT_STORAGE_CLASS = 0;

cost_constant_error Server_cost_constants::set(const LEX_CSTRING &name,
                                               double value) {
  assert(name.str != nullptr);
  assert(name.length > 0);

  if (name.str == nullptr || name.length == 0)
    return UNKNOWN_COST_NAME; /* purecov: inspected */

  /*
    The cost constant value must be a positive and non-zero.
  */
  if (value <= 0.0) return INVALID_COST_VALUE;

  // ROW_EVALUATE_COST
  if (my_strcasecmp(&my_charset_utf8mb3_general_ci, "ROW_EVALUATE_COST",
                    name.str) == 0) {
    m_row_evaluate_cost = value;
    return COST_CONSTANT_OK;
  }
  // KEY_COMPARE_COST
  if (my_strcasecmp(&my_charset_utf8mb3_general_ci, "KEY_COMPARE_COST",
                    name.str) == 0) {
    m_key_compare_cost = value;
    return COST_CONSTANT_OK;
  }
  // MEMORY_TEMPTABLE_CREATE_COST
  if (my_strcasecmp(&my_charset_utf8mb3_general_ci,
                    "MEMORY_TEMPTABLE_CREATE_COST", name.str) == 0) {
    m_memory_temptable_create_cost = value;
    return COST_CONSTANT_OK;
  }
  // MEMORY_TEMPTABLE_ROW_COST
  if (my_strcasecmp(&my_charset_utf8mb3_general_ci, "MEMORY_TEMPTABLE_ROW_COST",
                    name.str) == 0) {
    m_memory_temptable_row_cost = value;
    return COST_CONSTANT_OK;
  }
  // DISK_TEMPTABLE_CREATE_COST
  if (my_strcasecmp(&my_charset_utf8mb3_general_ci,
                    "DISK_TEMPTABLE_CREATE_COST", name.str) == 0) {
    m_disk_temptable_create_cost = value;
    return COST_CONSTANT_OK;
  }
  // DISK_TEMPTABLE_ROW_COST
  if (my_strcasecmp(&my_charset_utf8mb3_general_ci, "DISK_TEMPTABLE_ROW_COST",
                    name.str) == 0) {
    m_disk_temptable_row_cost = value;
    return COST_CONSTANT_OK;
  }

  return UNKNOWN_COST_NAME;  // Cost constant does not exist
}

cost_constant_error SE_cost_constants::set(const LEX_CSTRING &name,
                                           const double value,
                                           bool default_value) {
  assert(name.str != nullptr);
  assert(name.length > 0);

  if (name.str == nullptr || name.length == 0)
    return UNKNOWN_COST_NAME; /* purecov: inspected */

  /*
    The cost constant value must be a positive and non-zero number.
  */
  if (value <= 0.0) return INVALID_COST_VALUE;

  // MEMORY_BLOCK_READ_COST
  if (my_strcasecmp(&my_charset_utf8mb3_general_ci, "MEMORY_BLOCK_READ_COST",
                    name.str) == 0) {
    update_cost_value(&m_memory_block_read_cost,
                      &m_memory_block_read_cost_default, value, default_value);
    return COST_CONSTANT_OK;
  }
  // IO_BLOCK_READ_COST
  if (my_strcasecmp(&my_charset_utf8mb3_general_ci, "IO_BLOCK_READ_COST",
                    name.str) == 0) {
    update_cost_value(&m_io_block_read_cost, &m_io_block_read_cost_default,
                      value, default_value);
    return COST_CONSTANT_OK;
  }

  return UNKNOWN_COST_NAME;  // Cost constant does not exist
}

cost_constant_error SE_cost_constants::update(const LEX_CSTRING &name,
                                              const double value) {
  return set(name, value, false);
}

cost_constant_error SE_cost_constants::update_default(const LEX_CSTRING &name,
                                                      const double value) {
  return set(name, value, true);
}

void SE_cost_constants::update_cost_value(double *cost_constant,
                                          bool *cost_constant_is_default,
                                          double new_value,
                                          bool new_value_is_default) {
  // If this is not a new default value then do the update unconditionally
  if (!new_value_is_default) {
    *cost_constant = new_value;
    *cost_constant_is_default = false;  // No longer default value
  } else {
    /*
      This new value is a default value. Only update the cost constant if it
      currently has the default value.
    */
    if (*cost_constant_is_default) *cost_constant = new_value;
  }
}

Cost_model_se_info::Cost_model_se_info() {
  for (uint i = 0; i < MAX_STORAGE_CLASSES; ++i)
    m_se_cost_constants[i] = nullptr;
}

Cost_model_se_info::~Cost_model_se_info() {
  for (uint i = 0; i < MAX_STORAGE_CLASSES; ++i) {
    delete m_se_cost_constants[i];
    m_se_cost_constants[i] = nullptr;
  }
}

Cost_model_constants::Cost_model_constants(Optimizer optimizer)
    : m_server_constants(optimizer),
      m_engines(PSI_NOT_INSTRUMENTED, num_hton2plugins()),
      m_ref_counter(0),
      m_optimizer(optimizer) {
  /**
    Create default cost constants for each storage engine.
  */
  for (size_t engine = 0; engine < m_engines.size(); ++engine) {
    const handlerton *ht = nullptr;

    // Check if the storage engine has been installed
    if (hton2plugin(engine)) {
      // Find the handlerton for the storage engine
      ht = static_cast<handlerton *>(hton2plugin(engine)->data);
    }

    for (uint storage = 0; storage < MAX_STORAGE_CLASSES; ++storage) {
      SE_cost_constants *se_cost = nullptr;

      /*
        If the storage engine has provided a function for creating
        storage engine specific cost constants, then ask the
        storage engine to create the cost constants.
      */
      if (ht && ht->get_cost_constants)
        se_cost = ht->get_cost_constants(storage); /* purecov: tested */

      /*
        If the storage engine did not provide cost constants, then the
        default cost constants will be used.
      */
      if (se_cost == nullptr) se_cost = new SE_cost_constants(optimizer);

      m_engines[engine].set_cost_constants(se_cost, storage);
    }
  }
}

Cost_model_constants::~Cost_model_constants() { assert(m_ref_counter == 0); }

const SE_cost_constants *Cost_model_constants::get_se_cost_constants(
    const TABLE *table) const {
  assert(table->file != nullptr);
  assert(table->file->ht != nullptr);

  static SE_cost_constants default_cost_original(Optimizer::kOriginal);
  static SE_cost_constants default_cost_hypergraph(Optimizer::kHypergraph);
  const SE_cost_constants *default_cost = m_optimizer == Optimizer::kOriginal
                                              ? &default_cost_original
                                              : &default_cost_hypergraph;

  /*
    We do not see data for new htons loaded by the current session,
    use default statistics.
  */
  const uint slot = table->file->ht->slot;
  const SE_cost_constants *se_cc =
      slot < m_engines.size()
          ? m_engines[slot].get_cost_constants(DEFAULT_STORAGE_CLASS)
          : default_cost;
  assert(se_cc != nullptr);

  return se_cc;
}

cost_constant_error Cost_model_constants::update_server_cost_constant(
    const LEX_CSTRING &name, double value) {
  return m_server_constants.set(name, value);
}

cost_constant_error Cost_model_constants::update_engine_cost_constant(
    THD *thd, const LEX_CSTRING &se_name, uint storage_category,
    const LEX_CSTRING &name, double value) {
  cost_constant_error retval = COST_CONSTANT_OK;

  // Validate the storage category.
  if (storage_category >= MAX_STORAGE_CLASSES) return INVALID_DEVICE_TYPE;

  // Check if this is a default value
  if (my_strcasecmp(&my_charset_utf8mb3_general_ci, "default", se_name.str) ==
      0) {
    retval = update_engine_default_cost(name, storage_category, value);
  } else {
    // Look up the handler's slot id by using the storage engine name
    const uint ht_slot_id = find_handler_slot_from_name(thd, se_name);
    if (ht_slot_id == HA_SLOT_UNDEF) return UNKNOWN_ENGINE_NAME;

    SE_cost_constants *se_cc =
        m_engines[ht_slot_id].get_cost_constants(storage_category);
    assert(se_cc != nullptr);

    retval = se_cc->update(name, value);
  }

  return retval;
}

uint Cost_model_constants::find_handler_slot_from_name(
    THD *thd, const LEX_CSTRING &name) const {
  // Look up the storage engine
  const plugin_ref plugin = ha_resolve_by_name(thd, &name, false);
  if (!plugin) return HA_SLOT_UNDEF;

  // Find the handlerton for this storage engine
  handlerton *ht = plugin_data<handlerton *>(plugin);
  assert(ht != nullptr);
  if (!ht) {
    assert(false); /* purecov: inspected */
    return HA_SLOT_UNDEF;
  }

  return ht->slot;
}

cost_constant_error Cost_model_constants::update_engine_default_cost(
    const LEX_CSTRING &name, uint storage_category, double value) {
  assert(storage_category < MAX_STORAGE_CLASSES);

  /*
    Return value: if at least one of the storage engines recognizes the
    cost constants name, then success is returned.
  */
  cost_constant_error retval = UNKNOWN_COST_NAME;

  /*
    Update all constants for engines that have their own cost constants
  */
  for (size_t i = 0; i < m_engines.size(); ++i) {
    SE_cost_constants *se_cc =
        m_engines[i].get_cost_constants(storage_category);
    if (se_cc) {
      const cost_constant_error err = se_cc->update_default(name, value);
      if (err != UNKNOWN_COST_NAME) retval = err;
    }
  }

  return retval;
}
