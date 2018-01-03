/***********************************************************************

Copyright (c) 2012, 2017, Oracle and/or its affiliates. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is also distributed with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have included with MySQL.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

***********************************************************************/

/**************************************************//**
@file innodb_engine_private.h
Callback functions to support memcached commands

Extracted and modified from NDB memcached project
04/12/2011 Jimmy Yang
*******************************************************/


#ifndef innodb_engine_private_h
#define innodb_engine_private_h


/** Declarations of functions that implement the engine interface */

/*******************************************************************//**
Get engine info.
@return engine info */
static
const engine_info*
innodb_get_info(
/*============*/
	ENGINE_HANDLE*	handle);	/*!< in: Engine handle */


/*******************************************************************//**
Initialize InnoDB Memcached Engine.
@return ENGINE_SUCCESS if successful */
static
ENGINE_ERROR_CODE
innodb_initialize(
/*==============*/
	ENGINE_HANDLE*	handle,		/*!< in/out: InnoDB memcached
					engine */
	const char*	config_str	/*!< in: configure string */
);

/*******************************************************************//**
Allocate gets a struct item from the slab allocator, and fills in
everything but the value.  It seems like we can just pass this on to
the default engine; we'll intercept it later in store().
This is also called directly from finalize_read() in the commit thread. */
static
ENGINE_ERROR_CODE
innodb_allocate(
/*============*/
	ENGINE_HANDLE*	handle,		/*!< in: Engine handle */
	const void*	cookie,		/*!< in: connection cookie */
	item**		item,		/*!< out: item to allocate */
	const void*	key,		/*!< in: key value(s) */
	const size_t	nkey,		/*!< in: key length */
	const size_t	nbytes,		/*!< in: value length */
	const int	flags,		/*!< in: flag */
	const rel_time_t exptime	/*!< in: expiration time */
);

/*******************************************************************//**
Cleanup connections
@return number of connection cleaned */
static
ENGINE_ERROR_CODE
innodb_remove(
/*==========*/
	ENGINE_HANDLE*	handle,		/*!< in: Engine handle */
	const void*	cookie,		/*!< in: connection cookie */
	const void*	key,		/*!< in: key value */
	const size_t	nkey,		/*!< in: key length */
	uint64_t	cas,		/*!< in: cas */
	uint16_t	vbucket 	/*!< in: bucket, used by default
					engine only */
);

/*******************************************************************//**
bind table
@return number of connection cleaned */
static
ENGINE_ERROR_CODE
innodb_bind(
/*========*/
	ENGINE_HANDLE*	handle,		/*!< in: Engine handle */
	const void*	cookie,		/*!< in: connection cookie */
	const void*	name,		/*!< in: table ID name */
        const size_t	name_len	/*!< in: name length */
);

/*******************************************************************//**
release */
static
void
innodb_release(
/*===========*/
	ENGINE_HANDLE*	handle,		/*!< in: Engine handle */
	const void*	cookie,		/*!< in: connection cookie */
	item*		item		/*!< in: item to free */
);

/*******************************************************************//**
release */
static
void
innodb_clean_engine(
/*================*/
	ENGINE_HANDLE*	handle,		/*!< in: Engine handle */
	const void*	cookie,		/*!< in: connection cookie */
	void*		conn		/*!< in: item to free */
);
/*******************************************************************//**
Free value assocaited with key */
static
void
innodb_free_item(
/*=====================*/
        void* item);     /*!< in: Item to be freed */
/*******************************************************************//**
Support memcached "GET" command, fetch the value according to key
@return ENGINE_SUCCESS if successfully, otherwise error code */
static
ENGINE_ERROR_CODE
innodb_get(
/*=======*/
	ENGINE_HANDLE*	handle,		/*!< in: Engine Handle */
	const void*	cookie,		/*!< in: connection cookie */
	item**		item,		/*!< out: item to fill */
	const void*	key,		/*!< in: search key */
	const int	nkey,		/*!< in: key length */
	uint16_t	vbucket 	/*!< in: bucket, used by default
					engine only */
);

/*******************************************************************//**
Get statistics info
@return ENGINE_SUCCESS if successfully, otherwise error code */
static
ENGINE_ERROR_CODE
innodb_get_stats(
/*=============*/
	ENGINE_HANDLE*	handle,		/*!< in: Engine Handle */
	const void*	cookie,		/*!< in: connection cookie */
	const char*	stat_key,	/*!< in: statistics key */
	int		nkey,		/*!< in: key length */
	ADD_STAT	add_stat	/*!< out: stats to fill */
);

/*******************************************************************//**
reset statistics
@return ENGINE_SUCCESS if successfully, otherwise error code */
static
void
innodb_reset_stats(
/*===============*/
	ENGINE_HANDLE*	handle,		/*!< in: Engine Handle */
	const void*	cookie  	/*!< in: connection cookie */
);

/*******************************************************************//**
API interface for memcached's "SET", "ADD", "REPLACE", "APPEND"
"PREPENT" and "CAS" commands
@return ENGINE_SUCCESS if successfully, otherwise error code */
static
ENGINE_ERROR_CODE
innodb_store(
/*=========*/
	ENGINE_HANDLE*	handle,		/*!< in: Engine Handle */
	const void*	cookie,		/*!< in: connection cookie */
	item*		item,		/*!< out: result to fill */
	uint64_t*	cas,		/*!< in: cas value */
	ENGINE_STORE_OPERATION  op,	/*!< in: type of operation */
	uint16_t	vbucket 	/*!< in: bucket, used by default
					engine only */
);

/*******************************************************************//**
Support memcached "FLUSH_ALL" command, clean up storage (trunate InnoDB Table)
@return ENGINE_SUCCESS if successfully, otherwise error code */
static
ENGINE_ERROR_CODE
innodb_flush(
/*=========*/
	ENGINE_HANDLE*	handle,		/*!< in: Engine Handle */
	const void*	cookie,		/*!< in: connection cookie */
	time_t		when		/*!< in: when to flush, not used by
					InnoDB */
);

/*******************************************************************//**
Deal with unknown command. Currently not used
@return ENGINE_SUCCESS if successfully processed, otherwise error code */
static
ENGINE_ERROR_CODE
innodb_unknown_command(
/*===================*/
	ENGINE_HANDLE*	handle,		/*!< in: Engine Handle */
	const void*	cookie,		/*!< in: connection cookie */
	protocol_binary_request_header *request, /*!< in: request */
	ADD_RESPONSE	response	/*!< out: respondse */
);

#endif /* innodb_engine_private_h */
