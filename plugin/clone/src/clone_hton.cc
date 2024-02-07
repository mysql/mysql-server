/* Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

/**
@file clone/src/clone_hton.cc
Clone Plugin: Interface with SE handlerton

*/

#include "plugin/clone/include/clone_hton.h"

/* Namespace for all clone data types */
namespace myclone {
/** Structure to pass clone information to each storage plugin */
struct Hton {
  /** Clone locator vector */
  Storage_Vector *m_loc_vec;

  /** Clone task vector */
  Task_Vector *m_task_vec;

  /** Current locator index */
  uint m_cur_index;

  /** Error reported during clone */
  int m_err;

  /** Clone type */
  Ha_clone_type m_type;

  /** Clone begin mode */
  Ha_clone_mode m_mode;

  /** clone target data directory */
  const char *m_data_dir;
};

}  // namespace myclone

/** Begin clone operation for current storage engine plugin
@param[in,out]	thd	server thread handle
@param[in]	plugin	storage plugin
@param[in]	arg	clone parameters
@return true if failure */
static bool run_hton_clone_begin(THD *thd, plugin_ref plugin, void *arg) {
  auto clone_arg = static_cast<myclone::Hton *>(arg);

  auto hton = plugin_data<handlerton *>(plugin);

  if (hton->clone_interface.clone_begin != nullptr) {
    myclone::Locator loc = {hton, nullptr, 0};
    uint32_t task_id = 0;

    assert(clone_arg->m_mode == HA_CLONE_MODE_START);

    clone_arg->m_err = hton->clone_interface.clone_begin(
        hton, thd, loc.m_loc, loc.m_loc_len, task_id, clone_arg->m_type,
        clone_arg->m_mode);

    clone_arg->m_loc_vec->push_back(loc);
    clone_arg->m_task_vec->push_back(task_id);

    if (clone_arg->m_err != 0) {
      return (true);
    }
  }

  return (false);
}

int hton_clone_begin(THD *thd, Storage_Vector &clone_loc_vec,
                     Task_Vector &task_vec, Ha_clone_type clone_type,
                     Ha_clone_mode clone_mode) {
  assert(task_vec.empty());
  /* If Storage locators are empty, construct them here. */
  if (clone_loc_vec.empty()) {
    myclone::Hton clone_args;

    clone_args.m_loc_vec = &clone_loc_vec;
    clone_args.m_task_vec = &task_vec;
    clone_args.m_cur_index = 0;
    clone_args.m_err = 0;
    clone_args.m_type = clone_type;
    clone_args.m_mode = clone_mode;
    clone_args.m_data_dir = nullptr;

    plugin_foreach(thd, run_hton_clone_begin, MYSQL_STORAGE_ENGINE_PLUGIN,
                   &clone_args);

    return (clone_args.m_err);
  }

  for (auto &loc_iter : clone_loc_vec) {
    uint32_t task_id = 0;

#if !defined(NDEBUG)
    Ha_clone_flagset flags;

    loc_iter.m_hton->clone_interface.clone_capability(flags);

    /* TODO: Skip adding task if SE doesn't support */
    if (clone_mode == HA_CLONE_MODE_ADD_TASK) {
      assert(flags[HA_CLONE_MULTI_TASK]);
    }

    /* TODO: Stop and start if restart not supported */
    if (clone_mode == HA_CLONE_MODE_RESTART) {
      assert(flags[HA_CLONE_RESTART]);
    }
#endif
    auto err = loc_iter.m_hton->clone_interface.clone_begin(
        loc_iter.m_hton, thd, loc_iter.m_loc, loc_iter.m_loc_len, task_id,
        clone_type, clone_mode);

    if (err != 0) {
      return (err);
    }

    task_vec.push_back(task_id);
  }

  return (0);
}

int hton_clone_copy(THD *thd, Storage_Vector &clone_loc_vec,
                    Task_Vector &task_vec, Ha_clone_cbk *clone_cbk) {
  uint index = 0;

  for (auto &loc_iter : clone_loc_vec) {
    assert(index < task_vec.size());
    clone_cbk->set_loc_index(index);

    auto err = loc_iter.m_hton->clone_interface.clone_copy(
        loc_iter.m_hton, thd, loc_iter.m_loc, loc_iter.m_loc_len,
        task_vec[index], clone_cbk);

    if (err != 0) {
      return (err);
    }
    index++;
  }

  return (0);
}

int hton_clone_end(THD *thd, Storage_Vector &clone_loc_vec,
                   Task_Vector &task_vec, int in_err) {
  uint index = 0;

  for (auto &loc_iter : clone_loc_vec) {
    assert(index < task_vec.size());
    auto err = loc_iter.m_hton->clone_interface.clone_end(
        loc_iter.m_hton, thd, loc_iter.m_loc, loc_iter.m_loc_len,
        task_vec[index], in_err);

    if (err != 0) {
      return (err);
    }
    ++index;
  }

  return (0);
}

/** Begin clone apply for current storage engine plugin
@param[in,out]	thd	server thread handle
@param[in]	plugin	storage plugin
@param[in]	arg	clone parameters
@return true if failure */
static bool run_hton_clone_apply_begin(THD *thd, plugin_ref plugin, void *arg) {
  auto clone_arg = static_cast<myclone::Hton *>(arg);

  auto hton = plugin_data<handlerton *>(plugin);

  if (hton->clone_interface.clone_apply_begin != nullptr) {
    myclone::Locator loc = {hton, nullptr, 0};
    uint32_t task_id = 0;

    assert(clone_arg->m_mode == HA_CLONE_MODE_VERSION);

    clone_arg->m_err = hton->clone_interface.clone_apply_begin(
        hton, thd, loc.m_loc, loc.m_loc_len, task_id, clone_arg->m_mode,
        clone_arg->m_data_dir);

    clone_arg->m_loc_vec->push_back(loc);

    if (clone_arg->m_err != 0) {
      return (true);
    }
  }

  return (false);
}

int hton_clone_apply_begin(THD *thd, const char *clone_data_dir,
                           Storage_Vector &clone_loc_vec, Task_Vector &task_vec,
                           Ha_clone_mode clone_mode) {
  /* If Storage locators are empty, construct them here. */
  auto add_task = task_vec.empty();

  assert(clone_mode == HA_CLONE_MODE_RESTART || task_vec.empty());

  if (clone_loc_vec.empty()) {
    myclone::Hton clone_args;

    clone_args.m_loc_vec = &clone_loc_vec;
    clone_args.m_task_vec = &task_vec;
    clone_args.m_cur_index = 0;
    clone_args.m_err = 0;
    clone_args.m_type = HA_CLONE_HYBRID;
    clone_args.m_mode = clone_mode;
    clone_args.m_data_dir = clone_data_dir;

    plugin_foreach(thd, run_hton_clone_apply_begin, MYSQL_STORAGE_ENGINE_PLUGIN,
                   &clone_args);

    return (clone_args.m_err);
  }

  uint32_t loop_index [[maybe_unused]] = 0;

  for (auto &loc_iter : clone_loc_vec) {
    uint32_t task_id = 0;

#if !defined(NDEBUG)
    Ha_clone_flagset flags;

    loc_iter.m_hton->clone_interface.clone_capability(flags);

    /* TODO: Skip adding task if SE doesn't support */
    if (clone_mode == HA_CLONE_MODE_ADD_TASK) {
      assert(flags[HA_CLONE_MULTI_TASK]);
    }

    /* TODO: Stop and start if restart no supported */
    if (clone_mode == HA_CLONE_MODE_RESTART) {
      assert(flags[HA_CLONE_RESTART]);
    }
#endif
    auto err = loc_iter.m_hton->clone_interface.clone_apply_begin(
        loc_iter.m_hton, thd, loc_iter.m_loc, loc_iter.m_loc_len, task_id,
        clone_mode, clone_data_dir);

    if (err != 0) {
      return (err);
    }

    if (add_task) {
      task_vec.push_back(task_id);
    }

    assert(task_vec[loop_index] == task_id);
    ++loop_index;
  }

  return (0);
}

int hton_clone_apply_error(THD *thd, Storage_Vector &clone_loc_vec,
                           Task_Vector &task_vec, int in_err) {
  assert(in_err != 0);

  uint index = 0;
  for (auto &loc_iter : clone_loc_vec) {
    assert(index < task_vec.size());
    auto err = loc_iter.m_hton->clone_interface.clone_apply(
        loc_iter.m_hton, thd, loc_iter.m_loc, loc_iter.m_loc_len,
        task_vec[index], in_err, nullptr);

    if (err != 0) {
      return (err);
    }
    ++index;
  }

  return (0);
}

int hton_clone_apply_end(THD *thd, Storage_Vector &clone_loc_vec,
                         Task_Vector &task_vec, int in_err) {
  uint index = 0;
  for (auto &loc_iter : clone_loc_vec) {
    /* Task vector could be empty if we are exiting immediately
    after initialization */
    uint32_t task_id = 0;
    if (!task_vec.empty()) {
      assert(index < task_vec.size());
      task_id = task_vec[index];
    }
    auto err = loc_iter.m_hton->clone_interface.clone_apply_end(
        loc_iter.m_hton, thd, loc_iter.m_loc, loc_iter.m_loc_len, task_id,
        in_err);

    if (err != 0) {
      return (err);
    }
    ++index;
  }

  return (0);
}
