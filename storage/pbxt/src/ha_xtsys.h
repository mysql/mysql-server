/* Copyright (c) 2008 PrimeBase Technologies GmbH, Germany
 *
 * PrimeBase XT
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Paul McCullagh
 *
 * 2007-05-20
 *
 * H&G2JCtL
 *
 * PBXT System Table handler.
 *
 */
#ifndef __HA_XTSYS_H__
#define __HA_XTSYS_H__

#ifdef DRIZZLED
#include <drizzled/common.h>
#include <drizzled/handler_structs.h>
#include <drizzled/current_session.h>
#include <drizzled/cursor.h>
#else
#include "mysql_priv.h"
#endif

#include "xt_defs.h"

#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class implementation */
#endif

#if MYSQL_VERSION_ID >= 50120
#define byte uchar
#endif

class XTOpenSystemTable;

class ha_xtsys: public handler
{
	THR_LOCK_DATA		ha_lock;			///< MySQL lock
	XTOpenSystemTable	*ha_open_tab;

public:
	ha_xtsys(handlerton *hton, TABLE_SHARE *table_arg);
	~ha_xtsys() { }

	const char *table_type() const { return "PBXT"; }

	const char *index_type(uint XT_UNUSED(inx)) {
		return "NONE";
	}

	const char **bas_ext() const;

	MX_TABLE_TYPES_T table_flags() const {
		return HA_BINLOG_ROW_CAPABLE | HA_BINLOG_STMT_CAPABLE;
	}

	MX_ULONG_T index_flags(uint XT_UNUSED(inx), uint XT_UNUSED(part), bool XT_UNUSED(all_parts)) const {
		return (HA_READ_NEXT | HA_READ_PREV | HA_READ_RANGE | HA_KEYREAD_ONLY);
	}
	uint	max_supported_keys()			const { return 512; }
	uint	max_supported_key_part_length() const { return 1024; }

	int		open(const char *name, int mode, uint test_if_locked);
	int		close(void);
	int		rnd_init(bool scan);
	int		rnd_next(byte *buf);
	int		rnd_pos(byte * buf, byte *pos);
	void	position(const byte *record);
	int		info(uint);

	int		external_lock(THD *thd, int lock_type);
	int		delete_table(const char *from);
	int		create(const char *name, TABLE *form, HA_CREATE_INFO *create_info);

	THR_LOCK_DATA **store_lock(THD *thd, THR_LOCK_DATA **to, enum thr_lock_type lock_type);
	bool get_error_message(int error, String *buf);
};

#endif

