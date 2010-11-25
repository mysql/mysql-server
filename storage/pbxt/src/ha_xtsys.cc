/* Copyright (c) 2008 PrimeBase Technologies GmbH, Germany
 *
 * PrimeBase Media Stream for MySQL
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
 * Table handler.
 *
 */

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include "xt_config.h"

#include <stdlib.h>
#include <time.h>

#ifdef DRIZZLED
#include <drizzled/server_includes.h>
#endif

#include "ha_xtsys.h"
#include "ha_pbxt.h"

#include "strutil_xt.h"
#include "database_xt.h"
#include "discover_xt.h"
#include "systab_xt.h"
#include "xt_defs.h"

/* Note: mysql_priv.h messes with new, which caused a crash. */
#ifdef new
#undef new
#endif

/*
 * ---------------------------------------------------------------
 * HANDLER INTERFACE
 */

ha_xtsys::ha_xtsys(handlerton *hton, TABLE_SHARE *table_arg):
handler(hton, table_arg),
ha_open_tab(NULL)
{
	init();
}

static const char *ha_pbms_exts[] = {
	"",
	NullS
};

const char **ha_xtsys::bas_ext() const
{
	return ha_pbms_exts;
}

int ha_xtsys::open(const char *table_path, int XT_UNUSED(mode), uint XT_UNUSED(test_if_locked))
{
	THD				*thd = current_thd;
	XTExceptionRec	e;
	XTThreadPtr		self;
	int				err = 0;

	if (!(self = xt_ha_set_current_thread(thd, &e)))
		return xt_ha_pbxt_to_mysql_error(e.e_xt_err);

	try_(a) {
		xt_ha_open_database_of_table(self, (XTPathStrPtr) table_path);

		ha_open_tab = XTSystemTableShare::openSystemTable(self, table_path, table);
		thr_lock_data_init(ha_open_tab->ost_share->sts_my_lock, &ha_lock, NULL);
		ref_length = ha_open_tab->getRefLen();
	}
	catch_(a) {
		err = xt_ha_pbxt_thread_error_for_mysql(thd, self, FALSE);
		if (ha_open_tab) {
			ha_open_tab->release(self);
			ha_open_tab = NULL;
		}
	}
	cont_(a);

	return err;
}

int ha_xtsys::close(void)
{
	THD						*thd = current_thd;
	XTExceptionRec			e;
	volatile XTThreadPtr	self = NULL;
	int						err = 0;

	if (thd)
		self = xt_ha_set_current_thread(thd, &e);
	else {
		if (!(self = xt_create_thread("TempForClose", FALSE, TRUE, &e))) {
			xt_log_exception(NULL, &e, XT_LOG_DEFAULT);
			return 0;
		}
	}

	if (self) {
		try_(a) {
			if (ha_open_tab) {
				ha_open_tab->release(self);
				ha_open_tab = NULL;
			}
		}
		catch_(a) {
			err = xt_ha_pbxt_thread_error_for_mysql(thd, self, FALSE);
		}
		cont_(a);

		if (!thd)
			xt_free_thread(self);
	}
	else
		xt_log(XT_NS_CONTEXT, XT_LOG_WARNING, "Unable to release table reference\n");

	return err;
}

int ha_xtsys::rnd_init(bool XT_UNUSED(scan))
{
	int err = 0;

	if (!ha_open_tab->seqScanInit())
		err = xt_ha_pbxt_thread_error_for_mysql(current_thd, xt_get_self(), FALSE);

	return err;
}

int ha_xtsys::rnd_next(byte *buf)
{
	bool	eof;
	int		err = 0;

	if (!ha_open_tab->seqScanNext((char *) buf, &eof)) {
		if (eof)
			err = HA_ERR_END_OF_FILE;
		else
			err = xt_ha_pbxt_thread_error_for_mysql(current_thd, xt_get_self(), FALSE);
	}

	return err;
}

void ha_xtsys::position(const byte *record)
{
	xtWord4 rec_id;
	rec_id = ha_open_tab->seqScanPos((xtWord1 *) record);
	mi_int4store((xtWord1 *) ref, rec_id);
}

int ha_xtsys::rnd_pos(byte * buf, byte *pos)
{
	int		err = 0;
	xtWord4	rec_id;

	rec_id = mi_uint4korr((xtWord1 *) pos);
	if (!ha_open_tab->seqScanRead(rec_id, (char *) buf))
		err = xt_ha_pbxt_thread_error_for_mysql(current_thd, xt_get_self(), FALSE);

	return err;
}

int ha_xtsys::info(uint XT_UNUSED(flag))
{
	return 0;
}

int ha_xtsys::external_lock(THD *thd, int lock_type)
{
	XTExceptionRec	e;
	XTThreadPtr		self;
	int				err = 0;
	bool			ok;

	if (!(self = xt_ha_set_current_thread(thd, &e)))
		return xt_ha_pbxt_to_mysql_error(e.e_xt_err);

	if (lock_type == F_UNLCK)
		ok = ha_open_tab->unuse();
	else
		ok = ha_open_tab->use();

	if (!ok)
		err = xt_ha_pbxt_thread_error_for_mysql(current_thd, xt_get_self(), FALSE);

	return err;
}

THR_LOCK_DATA **ha_xtsys::store_lock(THD *XT_UNUSED(thd), THR_LOCK_DATA **to, enum thr_lock_type lock_type)
{
	if (lock_type != TL_IGNORE && ha_lock.type == TL_UNLOCK)
		ha_lock.type = lock_type;
	*to++ = &ha_lock;
	return to;
}

/* Note: ha_pbxt::delete_system_table is called instead. */
int ha_xtsys::delete_table(const char *XT_UNUSED(table_path))
{
	/* Should never be called */
	return 0;
}

int ha_xtsys::create(const char *XT_UNUSED(name), TABLE *XT_UNUSED(table_arg), HA_CREATE_INFO *XT_UNUSED(create_info))
{
	/* Allow the table to be created.
	 * This is required after a dump is restored.
	 */
	return 0;
}

bool ha_xtsys::get_error_message(int XT_UNUSED(error), String *buf)
{
	THD				*thd = current_thd;
	XTExceptionRec	e;
	XTThreadPtr		self;

	if (!(self = xt_ha_set_current_thread(thd, &e)))
		return FALSE;

	if (!self->t_exception.e_xt_err)
		return FALSE;

	buf->copy(self->t_exception.e_err_msg, strlen(self->t_exception.e_err_msg), system_charset_info);
	return TRUE;
}

