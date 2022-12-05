/*****************************************************************************

Copyright (c) 2016, 2022, Oracle and/or its affiliates.

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
#ifndef zlob0read_h
#define zlob0read_h

#include "zlob0index.h"

namespace lob {

/** Read one data chunk associated with one index entry.
@param[in]      index   The clustered index containing the LOB.
@param[in]      entry   Pointer to the index entry
@param[in]      offset  The offset from which to read the chunk.
@param[in,out]  len     The length of the output buffer. This length can
                        be greater than the chunk size.
@param[in,out]  buf     The output buffer.
@param[in]      mtr     Mini-transaction context.
@return number of bytes copied into the output buffer. */
ulint z_read_chunk(dict_index_t *index, z_index_entry_t &entry, ulint offset,
                   ulint &len, byte *&buf, mtr_t *mtr);

/** Read one zlib stream fully, given its index entry.
@param[in]      index      The index dictionary object.
@param[in]      entry      The index entry (memory copy).
@param[in,out]  zbuf       The output buffer
@param[in]      zbuf_size  The size of the output buffer.
@param[in,out]  mtr        Mini-transaction.
@return the size of the zlib stream.*/
ulint z_read_strm(dict_index_t *index, z_index_entry_t &entry, byte *zbuf,
                  ulint zbuf_size, mtr_t *mtr);

/** Fetch a compressed large object (ZLOB) from the system.
@param[in] ctx    the read context information.
@param[in] trx    the transaction that is doing the read.
@param[in] ref    the LOB reference identifying the LOB.
@param[in] offset read the LOB from the given offset.
@param[in] len    the length of LOB data that needs to be fetched.
@param[out] buf   the output buffer (owned by caller) of minimum len bytes.
@return the amount of data (in bytes) that was actually read. */
ulint z_read(ReadContext *ctx, trx_t *trx, lob::ref_t ref, ulint offset,
             ulint len, byte *buf);

#ifdef UNIV_DEBUG
/** Validate one zlib stream, given its index entry.
@param[in]      index      The index dictionary object.
@param[in]      entry      The index entry (memory copy).
@param[in]      mtr        Mini-transaction.
@return true if validation passed.
@return does not return if validation failed.*/
bool z_validate_strm(dict_index_t *index, z_index_entry_t &entry, mtr_t *mtr);
#endif /* UNIV_DEBUG */

} /* namespace lob */

#endif /* zlob0read_h */
