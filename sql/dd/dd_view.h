/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD_VIEW_INCLUDED
#define DD_VIEW_INCLUDED


class THD;
struct TABLE_LIST;

typedef struct st_mem_root MEM_ROOT;

namespace dd {
class Schema;
class View;

/**
  Store view metadata in the data-dictionary.

  @param   thd                Thread handle.
  @param   schema             Schema where the view should be created.
  @param   view               TABLE_LIST element describing the view.

  @note The caller must rollback both statement and transaction on failure,
        before any further accesses to DD. This is because such a failure
        might be caused by a deadlock, which requires rollback before any
        other operations on SE (including reads using attachable transactions)
        can be done.

  @retval  false        On Success.
  @retval  true         On Failure.
*/
bool create_view(THD *thd, const dd::Schema &schema, TABLE_LIST *view);

/**
  Update view metadata in dd.views.

  @param thd                Thread handle.
  @param new_view           View object that should be updated.
  @param view               TABLE_LIST element describing the new view.

  @note The caller must rollback both statement and transaction on failure,
        before any further accesses to DD. This is because such a failure
        might be caused by a deadlock, which requires rollback before any
        other operations on SE (including reads using attachable transactions)
        can be done.

  @retval false       On success.
  @retval true        On failure.
*/
bool update_view(THD *thd, dd::View *new_view, TABLE_LIST *view);

/** Read view metadata from dd.views into TABLE_LIST */
bool read_view(TABLE_LIST *view, const dd::View &view_ref,
               MEM_ROOT *mem_root);

/**
  Update view status(valid/invalid) value in dd.views.options.

  @param   thd          Thread handle.
  @param   schema_name  Schema name.
  @param   view_name    View name.
  @param   status       View status(valid/invalid).
  @param   commit_dd_changes  Indicates whether changes to DD need to be
                              committed.

  @note In case when commit_dd_changes is false, the caller must rollback
        both statement and transaction on failure, before any further
        accesses to DD. This is because such a failure might be caused by
        a deadlock, which requires rollback before any other operations on
        SE (including reads using attachable transactions) can be done.
        If case when commit_dd_changes is true this function will handle
        transaction rollback itself.

  @retval  false        On Success.
  @retval  true         On Failure.
*/
bool update_view_status(THD *thd, const char *schema_name,
                        const char *view_name, bool status,
                        bool commit_dd_changes);

} // namespace dd
#endif // DD_VIEW_INCLUDED
