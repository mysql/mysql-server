/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <common/common.hpp>

// Initialize Sqlstate records statically.
// They are not used by other static initializers.

#define make_Sqlstate(state, code) \
	const Sqlstate Sqlstate::_##state(#state, code)

make_Sqlstate(00000, SQL_SUCCESS);
make_Sqlstate(01004, SQL_SUCCESS_WITH_INFO);
make_Sqlstate(01S02, SQL_SUCCESS_WITH_INFO);
make_Sqlstate(07009, SQL_ERROR);
make_Sqlstate(08003, SQL_ERROR);
make_Sqlstate(21S01, SQL_ERROR);
make_Sqlstate(22001, SQL_ERROR);
make_Sqlstate(22002, SQL_ERROR);
make_Sqlstate(22003, SQL_ERROR);
make_Sqlstate(22005, SQL_ERROR);
make_Sqlstate(22008, SQL_ERROR);
make_Sqlstate(22012, SQL_ERROR);
make_Sqlstate(24000, SQL_ERROR);
make_Sqlstate(25000, SQL_ERROR);
make_Sqlstate(42000, SQL_ERROR);
make_Sqlstate(42S02, SQL_ERROR);
make_Sqlstate(42S22, SQL_ERROR);
make_Sqlstate(HY004, SQL_ERROR);
make_Sqlstate(HY009, SQL_ERROR);
make_Sqlstate(HY010, SQL_ERROR);
make_Sqlstate(HY011, SQL_ERROR);
make_Sqlstate(HY012, SQL_ERROR);
make_Sqlstate(HY014, SQL_ERROR);
make_Sqlstate(HY019, SQL_ERROR);
make_Sqlstate(HY024, SQL_ERROR);
make_Sqlstate(HY090, SQL_ERROR);
make_Sqlstate(HY091, SQL_ERROR);
make_Sqlstate(HY092, SQL_ERROR);
make_Sqlstate(HY095, SQL_ERROR);
make_Sqlstate(HY096, SQL_ERROR);
make_Sqlstate(HYC00, SQL_ERROR);
make_Sqlstate(HYT00, SQL_ERROR);
make_Sqlstate(IM000, SQL_ERROR);	// consider all to be errors for now
make_Sqlstate(IM001, SQL_ERROR);
make_Sqlstate(IM999, SQL_ERROR);

SQLRETURN
Sqlstate::getCode(SQLRETURN code) const
{
    int codes[2];
    int ranks[2];
    codes[0] = code;
    codes[1] = m_code;
    for (int i = 0; i < 2; i++) {
	switch (codes[i]) {
	case SQL_SUCCESS:
	    ranks[i] = 0;
	    break;
	case SQL_SUCCESS_WITH_INFO:
	    ranks[i] = 1;
	    break;
	case SQL_NO_DATA:
	    ranks[i] = 2;
	    break;
	case SQL_NEED_DATA:
	    ranks[i] = 3;
	    break;
	case SQL_ERROR:
	    ranks[i] = 9;
	    break;
	default:
	    ctx_assert(false);
	    ranks[i] = 9;
	}
    }
    if (ranks[0] < ranks[1])
	code = m_code;
    return code;
}
