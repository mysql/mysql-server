/* Copyright (c) 2009 PrimeBase Technologies GmbH, Germany
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
 * Barry Leslie
 *
 * 2009-07-16
 *
 * H&G2JCtL
 *
 * PBMS interface used to enable engines for use with the PBMS engine.
 *
 * For an example on how to build this into an engine have a look at the PBXT engine
 * in file ha_pbxt.cc. Search for 'PBMS_ENABLED'.
 *
 */


#ifndef __PBMS_ENABLED_H__
#define __PBMS_ENABLED_H__

#include "pbms.h"

/*
 * pbms_initialize() should be called from the engines plugIn's 'init()' function.
 * The engine_name is the name of your engine, "PBXT" or "InnoDB" for example.
 *
 * The isServer flag indicates if this entire server is being enabled. This is only
 * true if this is being built into the server's handler code above the engine level
 * calls. 
 */
extern bool pbms_initialize(const char *engine_name, bool isServer, PBMSResultPtr result);

/*
 * pbms_finalize() should be called from the engines plugIn's 'deinit()' function.
 */
extern void pbms_finalize();

/*
 * pbms_write_row_blobs() should be called from the engine's 'write_row' function.
 * It can alter the row data so it must be called before any other function using the row data.
 * It should also be called from engine's 'update_row' function for the new row.
 *
 * pbms_completed() must be called after calling pbms_write_row_blobs() and just before
 * returning from write_row() to indicate if the operation completed successfully.
 */
extern int pbms_write_row_blobs(TABLE *table, uchar *buf, PBMSResultPtr result);

/*
 * pbms_delete_row_blobs() should be called from the engine's 'delete_row' function.
 * It should also be called from engine's 'update_row' function for the old row.
 *
 * pbms_completed() must be called after calling pbms_delete_row_blobs() and just before
 * returning from delete_row() to indicate if the operation completed successfully.
 */
extern int pbms_delete_row_blobs(TABLE *table, const uchar *buf, PBMSResultPtr result);

/*
 * pbms_rename_table_with_blobs() should be called from the engine's 'rename_table' function.
 *
 * NOTE: Renaming tables across databases is not supported.
 *
 * pbms_completed() must be called after calling pbms_rename_table_with_blobs() and just before
 * returning from rename_table() to indicate if the operation completed successfully.
 */
extern int pbms_rename_table_with_blobs(const char *old_table_path, const char *new_table_path, PBMSResultPtr result);

/*
 * pbms_delete_table_with_blobs() should be called from the engine's 'delete_table' function.
 *
 * NOTE: Currently pbms_delete_table_with_blobs() cannot be undone so it should only
 * be called after the host engine has performed successfully drop it's table.
 *
 * pbms_completed() must be called after calling pbms_delete_table_with_blobs() and just before
 * returning from delete_table() to indicate if the operation completed successfully.
 */
extern int pbms_delete_table_with_blobs(const char *table_path, PBMSResultPtr result);

/*
 * pbms_completed() must be called to indicate success or failure of a an operation after having
 * called  pbms_write_row_blobs(), pbms_delete_row_blobs(), pbms_rename_table_with_blobs(), or
 * pbms_delete_table_with_blobs().
 *
 * pbms_completed() has the effect of committing or rolling back the changes made if the session
 * is in 'autocommit' mode.
 */
extern void pbms_completed(TABLE *table, bool ok);

#endif
