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
 *  Created by Leslie on 8/27/08.
 *
 */

#ifndef __DISCOVER_XT_H__
#define __DISCOVER_XT_H__

#ifdef DRIZZLED
#include <drizzled/common.h>
#else
#include "mysql_priv.h"
#endif

/*
 * ---------------------------------------------------------------
 * TABLE DISCOVERY HANDLER
 */

typedef struct dt_field_info {
	/** 
	This is used as column name. 
	*/
	const char* field_name;
	/**
	For string-type columns, this is the maximum number of
	characters. For numeric data this can be NULL.
	*/
	uint field_length;

	/**
	For decimal  columns, this is the maximum number of
	digits after the decimal. For other data this can be NULL.
	*/
	char* field_decimal_length;
	/**
	This denotes data type for the column. For the most part, there seems to
	be one entry in the enum for each SQL data type, although there seem to
	be a number of additional entries in the enum.
	*/
	enum enum_field_types field_type;

	/**
	This is the charater set for non numeric data types including blob data.
	*/
	CHARSET_INFO *field_charset;

	uint field_flags;        // Field atributes(maybe_null, signed, unsigned etc.)
	const char* comment;
} DT_FIELD_INFO;

typedef struct dt_key_info
{
	const char*	key_name;
	uint		key_type; /* PRI_KEY_FLAG, UNIQUE_KEY_FLAG, MULTIPLE_KEY_FLAG */
	const char*	key_columns[8]; // The size of this can be set to what ever you need.
} DT_KEY_INFO;

int xt_create_table_frm(handlerton *hton, THD* thd, const char *db, const char *name, DT_FIELD_INFO *info, DT_KEY_INFO *keys, xtBool skip_existing);

#endif

