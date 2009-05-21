/* Copyright (c) 2005 PrimeBase Technologies GmbH
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
 * 2006-06-07	Paul McCullagh
 *
 * H&G2JCtL
 *
 * This file contains PBXT streaming interface.
 */

#ifndef __streaming_xt_h__
#define __streaming_xt_h__

#include "xt_defs.h"
#define PBMS_API	pbms_api_PBXT
#include "pbms.h"

xtBool xt_init_streaming(void);
void xt_exit_streaming(void);

void	xt_pbms_close_all_tables(const char *table_url);
xtBool	xt_pbms_close_connection(void *thd, XTExceptionPtr e);
xtBool	xt_pbms_open_table(void **open_table, char *table_path);
void	xt_pbms_close_table(void *open_table);
xtBool	xt_pbms_use_blob(void *open_table, char **ret_blob_url, char *blob_url, unsigned short col_index);
xtBool	xt_pbms_retain_blobs(void *open_table, PBMSEngineRefPtr eng_ref);
void	xt_pbms_release_blob(void *open_table, char *blob_url, unsigned short col_index, PBMSEngineRefPtr eng_ref);
void	xt_pbms_drop_table(const char *table_path);
void	xt_pbms_rename_table(const char *from_table, const char *to_table);

#endif
