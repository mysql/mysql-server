/*****************************************************************************

Copyright (c) 1997, 2022, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file row/row0vers.cc
 Row versions

 Created 2/6/1997 Heikki Tuuri
 *******************************************************/

#include <stddef.h>

#include "btr0btr.h"
#include "current_thd.h"
#include "dict0boot.h"
#include "dict0dict.h"
#include "ha_prototypes.h"
#include "lock0lock.h"
#include "mach0data.h"
#include "que0que.h"
#include "read0read.h"
#include "rem0cmp.h"
#include "row0ext.h"
#include "row0mysql.h"
#include "row0row.h"
#include "row0upd.h"
#include "row0vers.h"
#include "trx0purge.h"
#include "trx0rec.h"
#include "trx0roll.h"
#include "trx0rseg.h"
#include "trx0trx.h"
#include "trx0undo.h"

#include "my_dbug.h"

/** Check whether all non-virtual columns in a index entries match
@param[in]      index           the secondary index
@param[in]      ientry1         first index entry to compare
@param[in]      ientry2         second index entry to compare
@param[in,out]  n_non_v_col             number of non-virtual columns
in the index
@return true if all matches, false otherwise */
static bool row_vers_non_vc_index_entry_match(dict_index_t *index,
                                              const dtuple_t *ientry1,
                                              const dtuple_t *ientry2,
                                              ulint *n_non_v_col);

/** Checks if a particular version of a record from clustered index matches
the secondary index record. The match occurs if and only if two condition hold:
1) the clust_rec exists and is not delete marked
2) the values in columns in clust_rec match those in sec_rec
Please note that the delete marker on sec_rec does not play any role in this
definition!
@param[in]    clust_index       the clustered index
@param[in]    clust_rec         the clustered index record, can be null or
                                delete marked
@param[in]    clust_vrow        the values of virtual columns, can be NULL if
                                the clust_rec was stored in undo log by
                                operation that did not change any secondary
                                index column (and was not a DELETE operation)
@param[in]    clust_offsets     the offsets for clust_rec,
                                rec_get_offsets(clust_rec, clust_index)
@param[in]    sec_index         the secondary index
@param[in]    sec_rec           the secondary index record
@param[in]    sec_offsets       the offsets for secondary index record,
                                rec_get_offsets(sec_rec, sec_index)
@param[in]    comp              the compression flag for both the clustered and
                                the secondary index, as both are assumed equal
@param[in]    looking_for_match are we looking for match?
                                false means that we are looking for non-match
@param[in]    heap              the heap to be used for all allocations
@return true iff the clust_rec matches sec_rec
*/
static bool row_clust_vers_matches_sec(
    const dict_index_t *const clust_index, const rec_t *const clust_rec,
    const dtuple_t *const clust_vrow, const ulint *const clust_offsets,
    const dict_index_t *const sec_index, const rec_t *const sec_rec,
    const ulint *const sec_offsets, const bool comp,
    const bool looking_for_match, mem_heap_t *const heap) {
  /** If we could not find a clust_rec version, it means it either never existed
  or was garbage collected, in either case we can interpret it as the row not
  being present at that point in time. Similarly, if it is delete marked.
  In all this cases, we report that there is no match. */
  if (!clust_rec || rec_get_deleted_flag(clust_rec, comp)) {
    return false;
  }

  /** If the index involves virtual columns, then we can relay on the assumption
  that `trx_undo_prev_version_build` will try to retrieve clust_vrow, and the
  only reason it can not retrieve it is because there was no change to any of
  the indexed columns. In particular this should mean, that the answer to the
  question "does this clust_rec match sec_row?" did not change, and we can
  return the same value as before, which was !looking_for_match. We know it was
  not looking_for_match because in such case the loop would stop.

  There are some difficulties we should take into consideration here:

  1. It could be the case that there was no "previous iteration".
  Indeed, it can happen, that this is the first call.
  If we got to this line, it means that there are at least two versions of the
  clustered index row: the most recent, which we don't see, and the one passed
  here as `clust_rec`, which we know has all the important columns equal to the
  most recent one.
  Moreover, we know that `clust_rec` is not delete marked.
  We also know, that the most recent version is also not delete marked,
  because, if it was delete marked, then the most recent change would be a
  DELETE operation, and in such cases we always undo log the values of columns,
  yet `clust_vrow` is null.
  So, the most recent version, and the version just before it, not only have
  the same values of indexed columns, but also the same delete mark.
  If so, then it is impossible, that this particular change created, or
  removed a secondary index entry.
  Therefore we need to continue the loop, and to do so we have to return the
  opposite of what the loop is searching for, thus !looking_for_match.

  Here's a bit different argument, perhaps more persuasive in case we want to
  prove that the returned value correctly answers the question "does clust_rec
  match the sec_rec?".
  Consider two cases, depending on sec_rec delete mark:

  A) sec_rec is delete marked
  In this case, looking_for_match is set to true, thus we are about to return
  false. So, our claim is that clust_rec does not match sec_rec.
  For consider for a moment the opposite, that clust_rec does match sec_rec -
  it would follow, that also the most recent version matches sec_rec, as it has
  the same values of columns, and delete mark. But then, we have that two most
  recent versions of the clustered index record are not delete marked and match
  the secondary index record, yet for some reason the change was not
  synchronized to the secondary index, which is still delete marked!
  This contradicts the assumption that at most one most recent change is not
  synchronized to the secondary index.

  B) sec_rec is not delete marked
  In this case, looking_for_match is set to false, thus we are about to return
  true. So, our claim is that clust_rec does match sec_rec.
  For consider for a moment the opposite, that clust_rec doesn't match sec_rec -
  it would follow, that also the most recent version doesn't match sec_rec, as
  it has the same values of columns, and delete mark. But then, we have that two
  most recent versions of the clustered index record do not match the secondary
  index record, yet for some reason the change was not synchronized to the
  secondary index, which is still not delete marked!
  This contradicts the assumption that at most one most recent change is not
  synchronized to the secondary index.

  2. It could (hypothetically) be the case that in previous iteration the answer
  was different, because the two versions differ in delete mark

  Again, before getting here we've already established that `clust_rec` is not
  delete marked, and if `clust_vrow` is missing, then it must mean that the
  later version is also not delete marked, as otherwise we would have to log all
  columns to the undo log */
  if (dict_index_has_virtual(sec_index) && !clust_vrow) {
    return !looking_for_match;
  }

  /** Reconstruct all the columns */
  row_ext_t *ext;
  dtuple_t *row =
      row_build(ROW_COPY_POINTERS, clust_index, clust_rec, clust_offsets,
                nullptr, nullptr, nullptr, &ext, heap);
  if (dict_index_has_virtual(sec_index)) {
    ut_ad(row->n_v_fields == clust_vrow->n_v_fields);
    dtuple_copy_v_fields(row, clust_vrow);
  }
  dtuple_t *entry = row_build_index_entry(row, ext, sec_index, heap);

  /** If the reconstructed values do not match the secondary index then we know
  we should report no match. We compare the strings in binary mode to make it
  more robust, because a thread which has changed "a" to "A" should prevent
  concurrent transactions from peeking into the new binary representation,
  say via CONVERT(column_name, binary). */
  dtuple_set_types_binary(entry, dtuple_get_n_fields(entry));
  return (0 == cmp_dtuple_rec(entry, sec_rec, sec_index, sec_offsets));
}

/**
Loops through the history of clustered index record in the undo log, stopping
after the first version which was not created by the given active transaction,
and reports if it found a version which satisfies criterion specified by
looking_for_match. If looking_for_match is true, it searches for a version which
matches the secondary index record. Otherwise it searches for a version which
does not match.
@param[in]      looking_for_match are we looking for match?
                                  false means that we are looking for non-match
@param[in]      clust_index       the clustered index
@param[in]      clust_rec         the clustered index record, can be null or
                                  delete marked
@param[in]      clust_offsets     the offsets for clust_rec,
                                  rec_get_offsets(clust_rec, clust_index)
@param[in]      sec_index         the secondary index
@param[in]      sec_rec           the secondary index record
@param[in]      sec_offsets       the offsets for secondary index record,
                                  rec_get_offsets(sec_rec, sec_index)
@param[in]      comp              the compression flag for both the clustered
                                  and the secondary index, as both are assumed
                                  equal
@param[in]      trx_id            the active transaction which created the most
                                  recent version of clustered index record
@param[in]      mtr               the mtr inside which we are operating
@param[in,out]  heap              the heap to be used for all allocations. This
                                  heap might get deallocated, and a newly
                                  allocated one will be returned, along with its
                                  ownership
@return true iff a version of the clust_rec which is in relation specified by
looking_for_match to the given sec_rec is found among versions created by trx_id
or the one version before them
*/
static bool row_vers_find_matching(
    bool looking_for_match, const dict_index_t *const clust_index,
    const rec_t *const clust_rec, ulint *&clust_offsets,
    const dict_index_t *const sec_index, const rec_t *const sec_rec,
    const ulint *const sec_offsets, const bool comp, const trx_id_t trx_id,
    mtr_t *const mtr, mem_heap_t *&heap) {
  const rec_t *version = clust_rec;
  trx_id_t version_trx_id = trx_id;

  while (version_trx_id == trx_id) {
    mem_heap_t *old_heap = heap;
    const dtuple_t *clust_vrow = nullptr;
    rec_t *prev_version = nullptr;

    /* We keep the semaphore in mtr on the clust_rec page, so
    that no other transaction can update it and get an
    implicit x-lock on rec until mtr_commit(mtr). */

    heap = mem_heap_create(1024, UT_LOCATION_HERE);

    trx_undo_prev_version_build(
        clust_rec, mtr, version, clust_index, clust_offsets, heap,
        &prev_version, nullptr,
        dict_index_has_virtual(sec_index) ? &clust_vrow : nullptr, 0, nullptr);

    /* The oldest visible clustered index version must not be
    delete-marked, because we never start a transaction by
    inserting a delete-marked record. */
    ut_ad(prev_version || !rec_get_deleted_flag(version, comp) ||
          !trx_rw_is_active(trx_id, false));

    /* Free version and clust_offsets. */
    mem_heap_free(old_heap);

    version = prev_version;

    if (version == nullptr) {
      version_trx_id = 0;
    } else {
      clust_offsets = rec_get_offsets(version, clust_index, nullptr,
                                      ULINT_UNDEFINED, UT_LOCATION_HERE, &heap);
      version_trx_id = row_get_rec_trx_id(version, clust_index, clust_offsets);
    }

    if (row_clust_vers_matches_sec(
            clust_index, version, clust_vrow, clust_offsets, sec_index, sec_rec,
            sec_offsets, comp, looking_for_match, heap) == looking_for_match) {
      return true;
    }
  }

  return false;
}

/** Finds out if an active transaction has inserted or modified a secondary
 index record.
 @param[in]       clust_rec     Clustered index record
 @param[in]       clust_index   The clustered index
 @param[in]       sec_rec       Secondary index record
 @param[in]       sec_index     The secondary index
 @param[in]       sec_offsets   Rec_get_offsets(sec_rec, sec_index)
 @param[in,out]   mtr           Mini-transaction
 @return 0 if committed, else the active transaction id;
 NOTE that this function can return false positives but never false
 negatives. The caller must confirm all positive results by calling checking if
 the trx is still active.*/
static inline trx_t *row_vers_impl_x_locked_low(
    const rec_t *const clust_rec, const dict_index_t *const clust_index,
    const rec_t *const sec_rec, const dict_index_t *const sec_index,
    const ulint *const sec_offsets, mtr_t *const mtr) {
  trx_id_t trx_id;

  ulint *clust_offsets;
  mem_heap_t *heap;

  /** Here's my best understanding of what this code is doing.

  When we call this function we already have `sec_rec` - a row from secondary
  index `sec_index`, which includes:
  - obviously the values of columns mentioned in secondary index definition,
    in particular materialized values of virtual columns
  - primary key columns not mentioned explicitly in secondary index definition,
  - information about row format (`comp`)
  - information if the row is delete marked or not (`rec_del`: 32 or 0)

  We assume that this `sec_rec` really is a record in the secondary index, as
  opposed to some artificially "made up" sequence of bytes. Moreover we
  assume that this secondary index row is currently latched (not to be confused
  with "locked"), so that `sec_rec` is the most current state of this row.

  Also, we assume, that rows in secondary index are either added, or removed,
  (or delete marked, or delete un-marked) but never modified.
  Moreover, we assume, that each of these secondary index operations is done
  after the primary (clustered) index was modified, to reflect the new state of
  affairs.

  We assume that `clust_rec` is the current version of the clustered index
  record to which the secondary record `sec_rec` points to.

  To be more precise:

  Let S[f] mean value of field f in the secondary index record S.
  Let C[t][f] mean value of field f in version t of clustered record C, where we
  use consecutive natural numbers to denote versions: t=0,1,...,current_version.

  Note: secondary index is not versioned

  Let S.deleted and C[t].deleted be delete markers of these records.

  Definition 1.
  We say that secondary index row S `points-to` a clustered index row C if and
  only if:
    S[pkey] = C[t][pkey] for each primary key column pkey (for any version t)

  Note: it does not matter which version t we pick, as for our purposes primary
  key fields may be thought as immutable (say, we emulate their modification by
  combination of delete + insert).

  Definition 2.
  We say that secondary index row S `matches` a clustered index row C in
  version t if and only if:
    (S[f] = C[t][f] for each column f) and not (C[t].deleted)

  Note: In the above definition f might be a virtual column.
  Note: There might be multiple versions which a single S `matches`, for
  example when a transaction modifies a row back and forth, or changes columns
  which are not indexed by secondary index.
  Note: The definition of `matches` does not depend on S.deleted

  Definition 3.
  We say that secondary index row S `corresponds-to` a clustered index row C in
  version t if and only if:
    (not(S.deleted) and (S `matches` C[t]))
    or
    (S.deleted and not (S `matches` C[t]))

  In other words, S `corresponds-to` C[t] means that the state of secondary
  index row S is synchronized with the state of the row in clustered index in
  version t.

  Assumption 1.
    (S `corresponds-to` C[current_version]) or
    (S `corresponds-to` C[current_version-1])
  In other words, `sec_rec` `corresponds-to` either the most current_version of
  the primary record it `points-to` (i.e. the changes in the clustered index
  were synchronized to the secondary index), or the current_version-1 - (i.e.
  the changes in the clustered index was not synchronized to the secondary index
  yet). This belief is supported by reading the source code and observation that
  to modify secondary index, one has to modify clustered index first, and
  modifying clustered index and later secondary index requires holding (implicit
  or explicit) lock on the clustered index record, so there is at most one
  transaction operating on any given clustered index row, and thus at most one
  change "unsynchronized" to secondary index yet.

  An equivalent formulation of Assumption 1 in terms of `matches` is:
  (not(S.deleted) =>
    ((S `matches` C[current_version]) or (S `matches` C[current_version-1]))
  ) and (
    S.deleted =>
    not((S `matches` C[current_version]) and (S `matches` C[current_version-1]))
  )
  So, a non-deleted S implies that one of the two most recent versions
  `matches` it, and a deleted S, means that at least one of the two most recent
  versions does not `match` it.

  Definition 4.
  We say that S `could-be-authored-by` a clustered index row C in
  version t if and only if:
    (S `corresponds-to` C[t]) and !(S `corresponds-to` C[t-1])

  This can be equivalently expressed using `matches` relation as:
    (not(S.deleted) and (S `matches` C[t]) and not(S `matches` C[t-1]))
    or
    (S.deleted and not(S `matches` C[t]) and (S `matches` C[t-1]))


  Definition 5.
  We say that secondary index row S `was-authored-by` a clustered index row C in
  version t if and only if:
    (S `could-be-authored-by` C[t]) and
    (for each v > t. not(S `could-be-authored-by` C[v]))
  So, t is the latest version in which S `could-be-authored-by` C[t].

  Equivalently, one can define `was-authored-by` in terms of `matches`, by
  identifying the most recent version t for which `matches` relation between
  S and C[t] has changed in the right direction, that is,
  in case S.deleted we search for the first change from (S `matches` C[t-1]) to
  not(S `matches` C[t]), while in case of not(S.deleted) we search for the first
  change from not(S `matches` C[t-1]) to (S `matches` C[t]).

  We are now ready to explain precisely what the call to
  row_vers_impl_x_locked_low(C=`clust_rec`,...,S=`sec_rec`,...)
  tries to achieve.

  Post-condition of row_vers_impl_x_locked_low:
  =============================================

  If there is t, such that S `was-authored-by` C[t], and C[t].trx_id is active
  then the return value is C[t].trx_id.
  Otherwise the return value is 0.

  Explanation of the algorithm in row_vers_impl_x_locked_low:
  ===========================================================

  The implementation is tricky, as it tries hard to avoid ever looking at the
  C[current_version], instead looking only at older versions.
  (One reason for this effort, IMHO, is that virtual columns might be expensive
  to materialize, and are not stored in clustered index at all. Another reason,
  I guess, might be to have only one way of reading data - from undo log).
  Take a moment to realize that this is wonderful that it's even possible, as
  this is not apparent from the Def 5! After all it might well be the case that
  the `t` we are looking for is equal to `current_version` in which case the
  definition of `was-authored-by` used naively would require us to check if S
  `matches` C[current_version], which in turn done naively forces us to
  look at fields of C[current_version]!

  So, how can we do that without ever looking at C[current_version] fields?

  We start by reading C[current_version].trx_id, and this is the only piece of
  information we read from current_version.
  We store that in `trx_id` variable.

  We check if `trx_id` is active.

  If `trx_id` is not active, then we know that we can return 0.
  Why? Because it is impossible for any other C[t].trx_id to be still active, if
  the most recent trx to modify the record is already inactive.

  From now on we assume that `trx_id` is active.

  We observe that the definition of S `was-authored-by` C[t] requires
  not(S `corresponds-to` C[t-1]).
  So, one thing we can use to filter interesting versions, is to proceed through
  most recent versions t=current_version, current_version-1, ...
  until we find the first t, such that not(S `corresponds-to` C[t-1]).
  Surprisingly this is the only condition we have to check! Why?
  Observe, that it must also be the case that (S `corresponds-to` C[t]),
  because we either have tested that explicitly in the previous step of the loop
  or in case of first iteration, it follows from Assumption 1.
  This means, that (S `could-be-authored-by` C[t]), and since the t is maximal,
  we have (S `was-authored-by` C[t]).

  Therefore our algorithm is to simply loop over versions t, as long as
  C[t].trx_id = trx_id, and stop as soon as not(S `corresponds-to` C[t-1]) in
  which case the answer is yes, or if we can't find such a version, the answer
  is no.

  The reality is however much more complicated, as it needs to deal with:
  A) incomplete history of versions (we remove old undo log entries from tail)
  B) missing information about virtual columns (we don't log values of virtual
  columns to undo log if they had not changed)

  I'll explain our approach to these two problems in comments at the place they
  are handled.*/

  DBUG_TRACE;

  ut_ad(rec_offs_validate(sec_rec, sec_index, sec_offsets));

  heap = mem_heap_create(1024, UT_LOCATION_HERE);

  clust_offsets = rec_get_offsets(clust_rec, clust_index, nullptr,
                                  ULINT_UNDEFINED, UT_LOCATION_HERE, &heap);

  trx_id = row_get_rec_trx_id(clust_rec, clust_index, clust_offsets);

  trx_t *trx = trx_rw_is_active(trx_id, true);

  if (trx == nullptr) {
    /* The transaction that modified or inserted clust_rec is no
    longer active, or it is corrupt: no implicit lock on rec */
    lock_check_trx_id_sanity(trx_id, clust_rec, clust_index, clust_offsets);
    mem_heap_free(heap);
    return nullptr;
  }

  auto comp = page_rec_is_comp(sec_rec);
  ut_ad(sec_index->table == clust_index->table);
  ut_ad(comp == dict_table_is_comp(sec_index->table));
  ut_ad(!comp == !page_rec_is_comp(clust_rec));

  bool looking_for_match = rec_get_deleted_flag(sec_rec, comp);

  if (!row_vers_find_matching(looking_for_match, clust_index, clust_rec,
                              clust_offsets, sec_index, sec_rec, sec_offsets,
                              comp, trx_id, mtr, heap)) {
    trx_release_reference(trx);
    trx = nullptr;
  }

  DBUG_PRINT("info", ("Implicit lock is held by trx:" TRX_ID_FMT, trx_id));

  mem_heap_free(heap);
  return trx;
}

trx_t *row_vers_impl_x_locked(const rec_t *rec, const dict_index_t *index,
                              const ulint *offsets) {
  mtr_t mtr;
  trx_t *trx;
  const rec_t *clust_rec;
  dict_index_t *clust_index;

  ut_ad(!locksys::owns_exclusive_global_latch());
  ut_ad(!trx_sys_mutex_own());

  mtr_start(&mtr);

  /* Search for the clustered index record. The latch on the
  page of clust_rec locks the top of the stack of versions. The
  bottom of the version stack is not locked; oldest versions may
  disappear by the fact that transactions may be committed and
  collected by the purge. This is not a problem, because we are
  only interested in active transactions. */

  clust_rec =
      row_get_clust_rec(BTR_SEARCH_LEAF, rec, index, &clust_index, &mtr);

  if (!clust_rec) {
    /* In a rare case it is possible that no clust rec is found
    for a secondary index record: if in row0umod.cc
    row_undo_mod_remove_clust_low() we have already removed the
    clust rec, while purge is still cleaning and removing
    secondary index records associated with earlier versions of
    the clustered index record. In that case there cannot be
    any implicit lock on the secondary index record, because
    an active transaction which has modified the secondary index
    record has also modified the clustered index record. And in
    a rollback we always undo the modifications to secondary index
    records before the clustered index record. */

    trx = nullptr;
  } else {
    trx = row_vers_impl_x_locked_low(clust_rec, clust_index, rec, index,
                                     offsets, &mtr);

    ut_ad(trx == nullptr || trx_is_referenced(trx));
  }

  mtr_commit(&mtr);

  return (trx);
}

/** Finds out if we must preserve a delete marked earlier version of a clustered
 index record, because it is >= the purge view.
 @param[in]     trx_id          Transaction id in the version
 @param[in]     name            Table name
 @param[in,out] mtr             Mini-transaction holding the latch on the
                                 clustered index record; it will also hold
                                 the latch on purge_view
 @return true if earlier version should be preserved */
bool row_vers_must_preserve_del_marked(trx_id_t trx_id,
                                       const table_name_t &name, mtr_t *mtr) {
  ut_ad(!rw_lock_own(&(purge_sys->latch), RW_LOCK_S));

  mtr_s_lock(&purge_sys->latch, mtr, UT_LOCATION_HERE);

  return (!purge_sys->view.changes_visible(trx_id, name));
}

/** Check whether all non-virtual columns in a index entries match
@param[in]      index           the secondary index
@param[in]      ientry1         first index entry to compare
@param[in]      ientry2         second index entry to compare
@param[in,out]  n_non_v_col             number of non-virtual columns
in the index
@return true if all matches, false otherwise */
static bool row_vers_non_vc_index_entry_match(dict_index_t *index,
                                              const dtuple_t *ientry1,
                                              const dtuple_t *ientry2,
                                              ulint *n_non_v_col) {
  ulint n_fields = dtuple_get_n_fields(ientry1);
  ulint ret = true;

  *n_non_v_col = 0;

  ut_ad(n_fields == dtuple_get_n_fields(ientry2));

  for (ulint i = 0; i < n_fields; i++) {
    const dict_field_t *ind_field = index->get_field(i);

    const dict_col_t *col = ind_field->col;

    /* Only check non-virtual columns */
    if (col->is_virtual()) {
      continue;
    }

    if (ret) {
      const dfield_t *field1 = dtuple_get_nth_field(ientry1, i);
      const dfield_t *field2 = dtuple_get_nth_field(ientry2, i);

      if (cmp_dfield_dfield(field1, field2, ind_field->is_ascending) != 0) {
        ret = false;
      }
    }

    (*n_non_v_col)++;
  }

  return (ret);
}

/** build virtual column value from current cluster index record data
@param[in,out]  row             the cluster index row in dtuple form
@param[in]      clust_index     clustered index
@param[in]      index           the secondary index
@param[in]      heap            heap used to build virtual dtuple */
static void row_vers_build_clust_v_col(dtuple_t *row, dict_index_t *clust_index,
                                       dict_index_t *index, mem_heap_t *heap) {
  mem_heap_t *local_heap = nullptr;
  for (ulint i = 0; i < dict_index_get_n_fields(index); i++) {
    const dict_field_t *ind_field = index->get_field(i);

    if (ind_field->col->is_virtual()) {
      const dict_v_col_t *col;

      col = reinterpret_cast<const dict_v_col_t *>(ind_field->col);

      innobase_get_computed_value(row, col, clust_index, &local_heap, heap,
                                  nullptr, current_thd, nullptr, nullptr,
                                  nullptr, nullptr);
    }
  }

  if (local_heap) {
    mem_heap_free(local_heap);
  }
}

/** Build latest virtual column data from undo log
@param[in]      in_purge        whether this is the purge thread
@param[in]      rec             clustered index record
@param[in]      clust_index     clustered index
@param[in,out]  clust_offsets   offsets on the clustered index record
@param[in]      index           the secondary index
@param[in]      roll_ptr        the rollback pointer for the purging record
@param[in]      trx_id          trx id for the purging record
@param[in,out]  v_heap          heap used to build vrow
@param[out]     vrow            dtuple holding the virtual rows
@param[in,out]  mtr             mtr holding the latch on rec */
static void row_vers_build_cur_vrow_low(
    bool in_purge, const rec_t *rec, dict_index_t *clust_index,
    ulint *clust_offsets, dict_index_t *index, roll_ptr_t roll_ptr,
    trx_id_t trx_id, mem_heap_t *v_heap, const dtuple_t **vrow, mtr_t *mtr) {
  const rec_t *version;
  rec_t *prev_version;
  mem_heap_t *heap = nullptr;
  ulint num_v = dict_table_get_n_v_cols(index->table);
  const dfield_t *field;
  ulint i;
  bool all_filled = false;

  *vrow = dtuple_create_with_vcol(v_heap, 0, num_v);
  dtuple_init_v_fld(*vrow);

  for (i = 0; i < num_v; i++) {
    dfield_get_type(dtuple_get_nth_v_field(*vrow, i))->mtype = DATA_MISSING;
  }

  version = rec;

  /* If this is called by purge thread, set TRX_UNDO_PREV_IN_PURGE
  bit to search the undo log until we hit the current undo log with
  roll_ptr */
  const ulint status = in_purge
                           ? TRX_UNDO_PREV_IN_PURGE | TRX_UNDO_GET_OLD_V_VALUE
                           : TRX_UNDO_GET_OLD_V_VALUE;

  while (!all_filled) {
    mem_heap_t *heap2 = heap;
    heap = mem_heap_create(1024, UT_LOCATION_HERE);
    roll_ptr_t cur_roll_ptr =
        row_get_rec_roll_ptr(version, clust_index, clust_offsets);

    trx_undo_prev_version_build(rec, mtr, version, clust_index, clust_offsets,
                                heap, &prev_version, nullptr, vrow, status,
                                nullptr);

    if (heap2) {
      mem_heap_free(heap2);
    }

    if (!prev_version) {
      /* Versions end here */
      break;
    }

    clust_offsets = rec_get_offsets(prev_version, clust_index, nullptr,
                                    ULINT_UNDEFINED, UT_LOCATION_HERE, &heap);

    ulint entry_len = dict_index_get_n_fields(index);

    all_filled = true;

    for (i = 0; i < entry_len; i++) {
      const dict_field_t *ind_field = index->get_field(i);
      const dict_col_t *col = ind_field->col;

      if (!col->is_virtual()) {
        continue;
      }

      const dict_v_col_t *v_col = reinterpret_cast<const dict_v_col_t *>(col);
      field = dtuple_get_nth_v_field(*vrow, v_col->v_pos);

      if (dfield_get_type(field)->mtype == DATA_MISSING) {
        all_filled = false;
        break;
      }
    }

    trx_id_t rec_trx_id =
        row_get_rec_trx_id(prev_version, clust_index, clust_offsets);

    if (rec_trx_id < trx_id || roll_ptr == cur_roll_ptr) {
      break;
    }

    version = prev_version;
  }

  mem_heap_free(heap);
}

/** Check a virtual column value index secondary virtual index matches
that of current cluster index record, which is recreated from information
stored in undo log
@param[in]      in_purge        called by purge thread
@param[in]      rec             record in the clustered index
@param[in]      icentry         the index entry built from a cluster row
@param[in]      clust_index     cluster index
@param[in]      clust_offsets   offsets on the cluster record
@param[in]      index           the secondary index
@param[in]      ientry          the secondary index entry
@param[in]      roll_ptr        the rollback pointer for the purging record
@param[in]      trx_id          trx id for the purging record
@param[in,out]  v_heap          heap used to build virtual dtuple
@param[in,out]  vrow            dtuple holding the virtual rows (if needed)
@param[in]      mtr             mtr holding the latch on rec
@return true if matches, false otherwise */
static bool row_vers_vc_matches_cluster(
    bool in_purge, const rec_t *rec, const dtuple_t *icentry,
    dict_index_t *clust_index, ulint *clust_offsets, dict_index_t *index,
    const dtuple_t *ientry, roll_ptr_t roll_ptr, trx_id_t trx_id,
    mem_heap_t *v_heap, const dtuple_t **vrow, mtr_t *mtr) {
  const rec_t *version;
  rec_t *prev_version;
  mem_heap_t *heap2;
  mem_heap_t *heap = nullptr;
  mem_heap_t *tuple_heap;
  ulint num_v = dict_table_get_n_v_cols(index->table);
  bool compare[REC_MAX_N_FIELDS];
  ulint n_fields = dtuple_get_n_fields(ientry);
  ulint n_non_v_col = 0;
  ulint n_cmp_v_col = 0;
  const dfield_t *field1;
  dfield_t *field2;
  ulint i;

  /* First compare non-virtual columns (primary keys) */
  if (!row_vers_non_vc_index_entry_match(index, ientry, icentry,
                                         &n_non_v_col)) {
    return (false);
  }

  tuple_heap = mem_heap_create(1024, UT_LOCATION_HERE);

  ut_ad(n_fields > n_non_v_col);

  *vrow = dtuple_create_with_vcol(v_heap ? v_heap : tuple_heap, 0, num_v);
  dtuple_init_v_fld(*vrow);

  for (i = 0; i < num_v; i++) {
    dfield_get_type(dtuple_get_nth_v_field(*vrow, i))->mtype = DATA_MISSING;
    compare[i] = false;
  }

  version = rec;

  /* If this is called by purge thread, set TRX_UNDO_PREV_IN_PURGE
  bit to search the undo log until we hit the current undo log with
  roll_ptr */
  ulint status =
      (in_purge ? TRX_UNDO_PREV_IN_PURGE : 0) | TRX_UNDO_GET_OLD_V_VALUE;

  while (n_cmp_v_col < n_fields - n_non_v_col) {
    heap2 = heap;
    heap = mem_heap_create(1024, UT_LOCATION_HERE);
    roll_ptr_t cur_roll_ptr =
        row_get_rec_roll_ptr(version, clust_index, clust_offsets);

    ut_ad(cur_roll_ptr != 0);
    ut_ad(in_purge == (roll_ptr != 0));

    trx_undo_prev_version_build(rec, mtr, version, clust_index, clust_offsets,
                                heap, &prev_version, nullptr, vrow, status,
                                nullptr);

    if (heap2) {
      mem_heap_free(heap2);
    }

    if (!prev_version) {
      /* Versions end here */
      goto func_exit;
    }

    clust_offsets = rec_get_offsets(prev_version, clust_index, nullptr,
                                    ULINT_UNDEFINED, UT_LOCATION_HERE, &heap);

    ulint entry_len = dict_index_get_n_fields(index);

    for (i = 0; i < entry_len; i++) {
      const dict_field_t *ind_field = index->get_field(i);
      const dict_col_t *col = ind_field->col;
      field1 = dtuple_get_nth_field(ientry, i);

      if (!col->is_virtual()) {
        continue;
      }

      const dict_v_col_t *v_col = reinterpret_cast<const dict_v_col_t *>(col);
      field2 = dtuple_get_nth_v_field(*vrow, v_col->v_pos);

      if ((dfield_get_type(field2)->mtype != DATA_MISSING) &&
          (!compare[v_col->v_pos])) {
        if (ind_field->prefix_len != 0 && !dfield_is_null(field2) &&
            field2->len > ind_field->prefix_len) {
          field2->len = ind_field->prefix_len;
        }

        /* For multi-byte character sets (like utf8mb4)
        and index on prefix of varchar vcol, we log
        prefix_len * mbmaxlen bytes but the actual
        secondary index record size can be less than
        that. For comparison, use actual length of
        secondary index record */
        ulint mbmax_len = DATA_MBMAXLEN(field2->type.mbminmaxlen);
        if (ind_field->prefix_len != 0 && !dfield_is_null(field2) &&
            mbmax_len > 1) {
          field2->len = field1->len;
        }

        /* The index field mismatch */
        if (v_heap ||
            (dfield_is_multi_value(field2) &&
             cmp_multi_value_dfield_dfield(field2, field1) != 0) ||
            (!dfield_is_multi_value(field2) &&
             cmp_dfield_dfield(field2, field1, ind_field->is_ascending) != 0)) {
          if (v_heap) {
            dtuple_dup_v_fld(*vrow, v_heap);
          }

          mem_heap_free(tuple_heap);
          mem_heap_free(heap);
          return (false);
        }

        compare[v_col->v_pos] = true;
        n_cmp_v_col++;
      }
    }

    trx_id_t rec_trx_id =
        row_get_rec_trx_id(prev_version, clust_index, clust_offsets);

    if (rec_trx_id < trx_id || roll_ptr == cur_roll_ptr) {
      break;
    }

    version = prev_version;
  }

func_exit:
  if (n_cmp_v_col == 0) {
    *vrow = nullptr;
  }

  mem_heap_free(tuple_heap);
  mem_heap_free(heap);

  /* FIXME: In the case of n_cmp_v_col is not the same as
  n_fields - n_non_v_col, callback is needed to compare the rest
  columns. At the timebeing, we will need to return true */
  return (true);
}

/** Build a dtuple contains virtual column data for current cluster index
@param[in]      in_purge        called by purge thread
@param[in]      rec             cluster index rec
@param[in]      clust_index     cluster index
@param[in]      clust_offsets   cluster rec offset
@param[in]      index           secondary index
@param[in]      roll_ptr        roll_ptr for the purge record
@param[in]      trx_id          transaction ID on the purging record
@param[in,out]  heap            heap memory
@param[in,out]  v_heap          heap memory to keep virtual column dtuple
@param[in]      mtr             mtr holding the latch on rec
@return dtuple contains virtual column data */
static const dtuple_t *row_vers_build_cur_vrow(
    bool in_purge, const rec_t *rec, dict_index_t *clust_index,
    ulint **clust_offsets, dict_index_t *index, roll_ptr_t roll_ptr,
    trx_id_t trx_id, mem_heap_t *heap, mem_heap_t *v_heap, mtr_t *mtr) {
  const dtuple_t *cur_vrow = nullptr;

  roll_ptr_t t_roll_ptr =
      row_get_rec_roll_ptr(rec, clust_index, *clust_offsets);

  /* if the row is newly inserted, then the virtual
  columns need to be computed */
  if (trx_undo_roll_ptr_is_insert(t_roll_ptr)) {
    ut_ad(!rec_get_deleted_flag(rec, page_rec_is_comp(rec)));

    /* This is a newly inserted record and cannot
    be deleted, So the externally stored field
    cannot be freed yet. */
    dtuple_t *row =
        row_build(ROW_COPY_POINTERS, clust_index, rec, *clust_offsets, nullptr,
                  nullptr, nullptr, nullptr, heap);

    row_vers_build_clust_v_col(row, clust_index, index, heap);
    cur_vrow = dtuple_copy(row, v_heap);
    dtuple_dup_v_fld(cur_vrow, v_heap);
  } else {
    /* Try to fetch virtual column data from undo log */
    row_vers_build_cur_vrow_low(in_purge, rec, clust_index, *clust_offsets,
                                index, roll_ptr, trx_id, v_heap, &cur_vrow,
                                mtr);
  }

  *clust_offsets = rec_get_offsets(rec, clust_index, nullptr, ULINT_UNDEFINED,
                                   UT_LOCATION_HERE, &heap);
  return (cur_vrow);
}

/** Finds out if a version of the record, where the version >= the current
 purge view, should have ientry as its secondary index entry. We check
 if there is any not delete marked version of the record where the trx
 id >= purge view, and the secondary index entry and ientry are identified in
 the alphabetical ordering; exactly in this case we return true.
 @return true if earlier version should have */
bool row_vers_old_has_index_entry(
    bool also_curr,         /*!< in: true if also rec is included in the
                           versions to search; otherwise only versions
                           prior to it are searched */
    const rec_t *rec,       /*!< in: record in the clustered index; the
                            caller must have a latch on the page */
    mtr_t *mtr,             /*!< in: mtr holding the latch on rec; it will
                            also hold the latch on purge_view */
    dict_index_t *index,    /*!< in: the secondary index */
    const dtuple_t *ientry, /*!< in: the secondary index entry */
    roll_ptr_t roll_ptr,    /*!< in: roll_ptr for the purge record */
    trx_id_t trx_id)        /*!< in: transaction ID on the purging record */
{
  const rec_t *version;
  rec_t *prev_version;
  dict_index_t *clust_index;
  ulint *clust_offsets;
  mem_heap_t *heap;
  mem_heap_t *heap2;
  dtuple_t *row;
  const dtuple_t *entry;
  ulint comp;
  const dtuple_t *vrow = nullptr;
  mem_heap_t *v_heap = nullptr;
  const dtuple_t *cur_vrow = nullptr;

  ut_ad(mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_X_FIX) ||
        mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_S_FIX));
  ut_ad(!rw_lock_own(&(purge_sys->latch), RW_LOCK_S));

  clust_index = index->table->first_index();

  comp = page_rec_is_comp(rec);
  ut_ad(!dict_table_is_comp(index->table) == !comp);
  heap = mem_heap_create(1024, UT_LOCATION_HERE);
  clust_offsets = rec_get_offsets(rec, clust_index, nullptr, ULINT_UNDEFINED,
                                  UT_LOCATION_HERE, &heap);

  DBUG_EXECUTE_IF("ib_purge_virtual_index_crash", DBUG_SUICIDE(););

  DBUG_EXECUTE_IF("ib_purge_virtual_index_crash", DBUG_SUICIDE(););

  DBUG_EXECUTE_IF("ib_purge_virtual_index_crash", DBUG_SUICIDE(););

  DBUG_EXECUTE_IF("ib_purge_virtual_index_crash", DBUG_SUICIDE(););

  if (dict_index_has_virtual(index)) {
    v_heap = mem_heap_create(100, UT_LOCATION_HERE);
  }

  if (also_curr && !rec_get_deleted_flag(rec, comp)) {
    row_ext_t *ext;

    /* The top of the stack of versions is locked by the
    mtr holding a latch on the page containing the
    clustered index record. The bottom of the stack is
    locked by the fact that the purge_sys->view must
    'overtake' any read view of an active transaction.
    Thus, it is safe to fetch the prefixes for
    externally stored columns. */
    row = row_build(ROW_COPY_POINTERS, clust_index, rec, clust_offsets, nullptr,
                    nullptr, nullptr, &ext, heap);

    if (dict_index_has_virtual(index)) {
#ifndef UNIV_DEBUG
#define dbug_v_purge false
#else  /* UNIV_DEBUG */
      bool dbug_v_purge = false;
#endif /* UNIV_DEBUG */

      DBUG_EXECUTE_IF("ib_purge_virtual_index_callback", dbug_v_purge = true;);

      roll_ptr_t t_roll_ptr =
          row_get_rec_roll_ptr(rec, clust_index, clust_offsets);

      /* if the row is newly inserted, then the virtual
      columns need to be computed */
      if (trx_undo_roll_ptr_is_insert(t_roll_ptr) || dbug_v_purge) {
#ifdef INNODB_DD_VC_SUPPORT
        row_vers_build_clust_v_col(row, clust_index, index, heap);

        entry = row_build_index_entry(row, ext, index, heap);
        if (entry && dtuple_coll_eq(entry, ientry)) {
          mem_heap_free(heap);

          if (v_heap) {
            mem_heap_free(v_heap);
          }

          return true;
        }
#else
        mem_heap_free(heap);

        if (v_heap) {
          mem_heap_free(v_heap);
        }

        return true;
#endif /* INNODB_DD_VC_SUPPORT */
      } else {
        /* Build index entry out of row */
        entry = row_build_index_entry(row, ext, index, heap);

        /* If entry == NULL, the record contains unset
        BLOB pointers. The record may be safely removed,
        see below for full explanation */

        if (entry &&
            row_vers_vc_matches_cluster(also_curr, rec, entry, clust_index,
                                        clust_offsets, index, ientry, roll_ptr,
                                        trx_id, nullptr, &vrow, mtr)) {
          mem_heap_free(heap);

          if (v_heap) {
            mem_heap_free(v_heap);
          }

          return true;
        }
      }
      clust_offsets = rec_get_offsets(rec, clust_index, nullptr,
                                      ULINT_UNDEFINED, UT_LOCATION_HERE, &heap);
    } else {
      entry = row_build_index_entry(row, ext, index, heap);

      /* If entry == NULL, the record contains unset BLOB
      pointers.  This must be a freshly inserted record.  If
      this is called from
      row_purge_remove_sec_if_poss_low(), the thread will
      hold latches on the clustered index and the secondary
      index.  Because the insert works in three steps:

              (1) insert the record to clustered index
              (2) store the BLOBs and update BLOB pointers
              (3) insert records to secondary indexes

      the purge thread can safely ignore freshly inserted
      records and delete the secondary index record.  The
      thread that inserted the new record will be inserting
      the secondary index records. */

      /* NOTE that we cannot do the comparison as binary
      fields because the row is maybe being modified so that
      the clustered index record has already been updated to
      a different binary value in a char field, but the
      collation identifies the old and new value anyway! */
      if (entry && dtuple_coll_eq(entry, ientry)) {
        mem_heap_free(heap);

        if (v_heap) {
          mem_heap_free(v_heap);
        }
        return true;
      }
    }
  } else if (dict_index_has_virtual(index)) {
    /* The current cluster index record could be
    deleted, but the previous version of it might not. We will
    need to get the virtual column data from undo record
    associated with current cluster index */
    cur_vrow =
        row_vers_build_cur_vrow(also_curr, rec, clust_index, &clust_offsets,
                                index, roll_ptr, trx_id, heap, v_heap, mtr);
  }

  version = rec;

  for (;;) {
    heap2 = heap;
    heap = mem_heap_create(1024, UT_LOCATION_HERE);
    vrow = nullptr;

    trx_undo_prev_version_build(
        rec, mtr, version, clust_index, clust_offsets, heap, &prev_version,
        nullptr, dict_index_has_virtual(index) ? &vrow : nullptr, 0, nullptr);
    mem_heap_free(heap2); /* free version and clust_offsets */

    if (!prev_version) {
      /* Versions end here */

      mem_heap_free(heap);

      if (v_heap) {
        mem_heap_free(v_heap);
      }

      return false;
    }

    clust_offsets = rec_get_offsets(prev_version, clust_index, nullptr,
                                    ULINT_UNDEFINED, UT_LOCATION_HERE, &heap);

    if (dict_index_has_virtual(index)) {
      if (vrow) {
        /* Keep the virtual row info for the next
        version, unless it is changed */
        mem_heap_empty(v_heap);
        cur_vrow = dtuple_copy(vrow, v_heap);
        dtuple_dup_v_fld(cur_vrow, v_heap);
      }

      if (!cur_vrow) {
        /* Nothing for this index has changed,
        continue */
        version = prev_version;
        continue;
      }
    }

    if (!rec_get_deleted_flag(prev_version, comp)) {
      row_ext_t *ext;

      /* The stack of versions is locked by mtr.
      Thus, it is safe to fetch the prefixes for
      externally stored columns. */
      row = row_build(ROW_COPY_POINTERS, clust_index, prev_version,
                      clust_offsets, nullptr, nullptr, nullptr, &ext, heap);

      if (dict_index_has_virtual(index)) {
        ut_ad(cur_vrow);
        ut_ad(row->n_v_fields == cur_vrow->n_v_fields);
        dtuple_copy_v_fields(row, cur_vrow);
      }

      entry = row_build_index_entry(row, ext, index, heap);

      /* If entry == NULL, the record contains unset
      BLOB pointers.  This must be a freshly
      inserted record that we can safely ignore.
      For the justification, see the comments after
      the previous row_build_index_entry() call. */

      /* NOTE that we cannot do the comparison as binary
      fields because maybe the secondary index record has
      already been updated to a different binary value in
      a char field, but the collation identifies the old
      and new value anyway! */

      if (entry && dtuple_coll_eq(entry, ientry)) {
        mem_heap_free(heap);
        if (v_heap) {
          mem_heap_free(v_heap);
        }

        return true;
      }
    }

    version = prev_version;
  }
}

/** Constructs the version of a clustered index record which a consistent
 read should see. We assume that the trx id stored in rec is such that
 the consistent read should not see rec in its present version.
 @param[in]   rec   record in a clustered index; the caller must have a latch
                    on the page; this latch locks the top of the stack of
                    versions of this records
 @param[in]   mtr   mtr holding the latch on rec; it will also hold the latch
                    on purge_view
 @param[in]   index   the clustered index
 @param[in]   offsets   offsets returned by rec_get_offsets(rec, index)
 @param[in]   view   the consistent read view
 @param[in,out]   offset_heap   memory heap from which the offsets are
                                allocated
 @param[in]   in_heap   memory heap from which the memory for *old_vers is
                        allocated; memory for possible intermediate versions
                        is allocated and freed locally within the function
 @param[out]   old_vers   old version, or NULL if the history is missing or
                          the record does not exist in the view, that is, it
                          was freshly inserted afterwards.
 @param[out]   vrow   reports virtual column info if any
 @param[in]   lob_undo   undo log to be applied to blobs.
 @return DB_SUCCESS or DB_MISSING_HISTORY */
dberr_t row_vers_build_for_consistent_read(
    const rec_t *rec, mtr_t *mtr, dict_index_t *index, ulint **offsets,
    ReadView *view, mem_heap_t **offset_heap, mem_heap_t *in_heap,
    rec_t **old_vers, const dtuple_t **vrow, lob::undo_vers_t *lob_undo) {
  DBUG_TRACE;
  const rec_t *version;
  rec_t *prev_version;
  trx_id_t trx_id;
  mem_heap_t *heap = nullptr;
  byte *buf;
  dberr_t err;

  ut_ad(index->is_clustered());
  ut_ad(mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_X_FIX) ||
        mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_S_FIX));
  ut_ad(!rw_lock_own(&(purge_sys->latch), RW_LOCK_S));

  ut_ad(rec_offs_validate(rec, index, *offsets));

  trx_id = row_get_rec_trx_id(rec, index, *offsets);

  /* Reset the collected LOB undo information. */
  if (lob_undo != nullptr) {
    lob_undo->reset();
  }

  ut_ad(!view->changes_visible(trx_id, index->table->name));

  ut_ad(!vrow || !(*vrow));

  version = rec;

  for (;;) {
    mem_heap_t *prev_heap = heap;

    heap = mem_heap_create(1024, UT_LOCATION_HERE);

    if (vrow) {
      *vrow = nullptr;
    }

    /* If purge can't see the record then we can't rely on
    the UNDO log record. */

    bool purge_sees =
        trx_undo_prev_version_build(rec, mtr, version, index, *offsets, heap,
                                    &prev_version, nullptr, vrow, 0, lob_undo);

    err = (purge_sees) ? DB_SUCCESS : DB_MISSING_HISTORY;

    if (prev_heap != nullptr) {
      mem_heap_free(prev_heap);
    }

    if (prev_version == nullptr) {
      /* It was a freshly inserted version */
      *old_vers = nullptr;
      ut_ad(!vrow || !(*vrow));
      break;
    }

    *offsets = rec_get_offsets(prev_version, index, *offsets, ULINT_UNDEFINED,
                               UT_LOCATION_HERE, offset_heap);

#if defined UNIV_DEBUG || defined UNIV_BLOB_LIGHT_DEBUG
    ut_a(!rec_offs_any_null_extern(index, prev_version, *offsets));
#endif /* UNIV_DEBUG || UNIV_BLOB_LIGHT_DEBUG */

    trx_id = row_get_rec_trx_id(prev_version, index, *offsets);

    if (view->changes_visible(trx_id, index->table->name)) {
      /* The view already sees this version: we can copy
      it to in_heap and return */

      buf =
          static_cast<byte *>(mem_heap_alloc(in_heap, rec_offs_size(*offsets)));

      *old_vers = rec_copy(buf, prev_version, *offsets);
      rec_offs_make_valid(*old_vers, index, *offsets);

      if (vrow && *vrow) {
        *vrow = dtuple_copy(*vrow, in_heap);
        dtuple_dup_v_fld(*vrow, in_heap);
      }
      break;
    }

    version = prev_version;
  }

  mem_heap_free(heap);

  return err;
}

/** Constructs the last committed version of a clustered index record,
 which should be seen by a semi-consistent read.
@param[in] rec Record in a clustered index; the caller must have a latch on the
page; this latch locks the top of the stack of versions of this records
@param[in] mtr Mini-transaction holding the latch on rec
@param[in] index The clustered index
@param[in,out] offsets Offsets returned by rec_get_offsets(rec, index)
@param[in,out] offset_heap Memory heap from which the offsets are allocated
@param[in] in_heap Memory heap from which the memory for *old_vers is allocated;
memory for possible intermediate versions is allocated and freed locally within
the function
@param[out] old_vers Rec, old version, or null if the record does not exist in
the view, that is, it was freshly inserted afterwards
@param[out] vrow Virtual row, old version, or null if it is not updated in the
view */
void row_vers_build_for_semi_consistent_read(
    const rec_t *rec, mtr_t *mtr, dict_index_t *index, ulint **offsets,
    mem_heap_t **offset_heap, mem_heap_t *in_heap, const rec_t **old_vers,
    const dtuple_t **vrow) {
  const rec_t *version;
  mem_heap_t *heap = nullptr;
  byte *buf;
  trx_id_t rec_trx_id = 0;

  ut_ad(index->is_clustered());
  ut_ad(mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_X_FIX) ||
        mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_S_FIX));
  ut_ad(!rw_lock_own(&(purge_sys->latch), RW_LOCK_S));

  ut_ad(rec_offs_validate(rec, index, *offsets));

  version = rec;
  ut_ad(!vrow || !(*vrow));

  for (;;) {
    mem_heap_t *heap2;
    rec_t *prev_version;
    trx_id_t version_trx_id;

    version_trx_id = row_get_rec_trx_id(version, index, *offsets);
    if (rec == version) {
      rec_trx_id = version_trx_id;
    }
    if (!trx_rw_is_active(version_trx_id, false)) {
    committed_version_trx:
      /* We found a version that belongs to a
      committed transaction: return it. */

#if defined UNIV_DEBUG || defined UNIV_BLOB_LIGHT_DEBUG
      ut_a(!rec_offs_any_null_extern(index, version, *offsets));
#endif /* UNIV_DEBUG || UNIV_BLOB_LIGHT_DEBUG */

      if (rec == version) {
        *old_vers = rec;
        if (vrow) {
          *vrow = nullptr;
        }
        break;
      }

      /* We assume that a rolled-back transaction stays in
      TRX_STATE_ACTIVE state until all the changes have been
      rolled back and the transaction is removed from
      the global list of transactions. */

      if (rec_trx_id == version_trx_id) {
        /* The transaction was committed while
        we searched for earlier versions.
        Return the current version as a
        semi-consistent read. */

        version = rec;
        *offsets = rec_get_offsets(version, index, *offsets, ULINT_UNDEFINED,
                                   UT_LOCATION_HERE, offset_heap);
      }

      buf =
          static_cast<byte *>(mem_heap_alloc(in_heap, rec_offs_size(*offsets)));

      *old_vers = rec_copy(buf, version, *offsets);
      rec_offs_make_valid(*old_vers, index, *offsets);
      if (vrow && *vrow) {
        *vrow = dtuple_copy(*vrow, in_heap);
        dtuple_dup_v_fld(*vrow, in_heap);
      }
      break;
    }

    DEBUG_SYNC_C("after_row_vers_check_trx_active");

    heap2 = heap;
    heap = mem_heap_create(1024, UT_LOCATION_HERE);

    if (!trx_undo_prev_version_build(rec, mtr, version, index, *offsets, heap,
                                     &prev_version, in_heap, vrow, 0,
                                     nullptr)) {
      mem_heap_free(heap);
      heap = heap2;
      heap2 = nullptr;
      goto committed_version_trx;
    }

    if (heap2) {
      mem_heap_free(heap2); /* free version */
    }

    if (prev_version == nullptr) {
      /* It was a freshly inserted version */
      *old_vers = nullptr;
      ut_ad(!vrow || !(*vrow));
      break;
    }

    version = prev_version;
    *offsets = rec_get_offsets(version, index, *offsets, ULINT_UNDEFINED,
                               UT_LOCATION_HERE, offset_heap);
#if defined UNIV_DEBUG || defined UNIV_BLOB_LIGHT_DEBUG
    ut_a(!rec_offs_any_null_extern(index, version, *offsets));
#endif /* UNIV_DEBUG || UNIV_BLOB_LIGHT_DEBUG */
  }    /* for (;;) */

  if (heap) {
    mem_heap_free(heap);
  }
}
