/*****************************************************************************

Copyright (c) 2017, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

#ifndef DICT_SDI_DECOMPRESS_H
#define DICT_SDI_DECOMPRESS_H

#include <zlib.h>

/** Decompress SDI record */
class Sdi_Decompressor {
public:
	Sdi_Decompressor(byte* uncomp_sdi, uint32_t uncomp_len, byte* comp_sdi, uint32_t comp_len) :
		m_uncomp_sdi(uncomp_sdi),
		m_uncomp_len(uncomp_len),
		m_comp_sdi(comp_sdi),
		m_comp_len(comp_len) {

		ut_ad(m_uncomp_sdi != nullptr);
		ut_ad(m_comp_sdi != nullptr);
	}

	~Sdi_Decompressor() {
	}

	/** Decompress the SDI and store in the buffer passed. */
	inline void decompress()
	{
		int ret;
		uLongf dest_len = m_uncomp_len;
		ret = uncompress(m_uncomp_sdi, &dest_len, m_comp_sdi, m_comp_len);

		if (ret != Z_OK) {
			ib::error()
				<< "ZLIB uncompress() failed:"
				<< " compressed len: " << m_comp_len
				<< ", original_len: " << m_uncomp_len;

			switch(ret) {
			case Z_BUF_ERROR:

				ib::fatal() << "retval = Z_BUF_ERROR";

			case Z_MEM_ERROR:

				ib::fatal() << "retval = Z_MEM_ERROR";

			case Z_DATA_ERROR:

				ib::fatal() << "retval = Z_DATA_ERROR";
			default:
				ut_error;
			}
		}
	}

	/** @return the uncompressed sdi */
	byte* get_data() const {
		return(m_uncomp_sdi);
	}

private:
	/** Buffer to hold uncompressed SDI. memory allocated by caller */
	byte*		m_uncomp_sdi;
	/** Length of Outbuf Buffer */
	uint32_t	m_uncomp_len;
	/** Input Compressed SDI */
	byte*		m_comp_sdi;
	/** Length of Compressed SDI */
	uint32_t	m_comp_len;
};
#endif
