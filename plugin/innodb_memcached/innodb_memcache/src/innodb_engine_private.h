/***********************************************************************

Copyright (c) 2012, 2015, Oracle and/or its affiliates. All rights reserved.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

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
	const char*	config_str);	/*!< in: configure string */

/*******************************************************************//**
Destroy and Free InnoDB Memcached engine */
static
void
innodb_destroy(
/*===========*/
	ENGINE_HANDLE*	handle,		/*!< in: Destroy the engine instance */
	bool		force);		/*!< in: Force to destroy */

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
	const rel_time_t exptime);	/*!< in: expiration time */

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
	uint16_t	vbucket);	/*!< in: bucket, used by default
					engine only */

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
        const size_t	name_len);	/*!< in: name length */

/*******************************************************************//**
release */
static
void
innodb_release(
/*===========*/
	ENGINE_HANDLE*	handle,		/*!< in: Engine handle */
	const void*	cookie,		/*!< in: connection cookie */
	item*		item);		/*!< in: item to free */

/*******************************************************************//**
release */
static
void
innodb_clean_engine(
/*================*/
	ENGINE_HANDLE*	handle,		/*!< in: Engine handle */
	const void*	cookie,		/*!< in: connection cookie */
	void*		conn);		/*!< in: item to free */
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
	uint16_t	vbucket);	/*!< in: bucket, used by default
					engine only */

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
	ADD_STAT	add_stat);	/*!< out: stats to fill */

/*******************************************************************//**
reset statistics
@return ENGINE_SUCCESS if successfully, otherwise error code */
static
void
innodb_reset_stats(
/*===============*/
	ENGINE_HANDLE*	handle,		/*!< in: Engine Handle */
	const void*	cookie);	/*!< in: connection cookie */

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
	uint16_t	vbucket);	/*!< in: bucket, used by default
					engine only */

/*******************************************************************//**
Support memcached "INCR" and "DECR" command, add or subtract a "delta"
value from an integer key value
@return ENGINE_SUCCESS if successfully, otherwise error code */
static
ENGINE_ERROR_CODE
innodb_arithmetic(
/*==============*/
	ENGINE_HANDLE*	handle,		/*!< in: Engine Handle */
	const void*	cookie,		/*!< in: connection cookie */
	const void*	key,		/*!< in: key for the value to add */
	const int	nkey,		/*!< in: key length */
	const bool	increment,	/*!< in: whether to increment
					or decrement */
	const bool	create,		/*!< in: whether to create the key
					value pair if can't find */
	const uint64_t	delta,		/*!< in: value to add/substract */
	const uint64_t	initial,	/*!< in: initial */
	const rel_time_t exptime,	/*!< in: expiration time */
	uint64_t*	cas,		/*!< out: new cas value */
	uint64_t*	result,		/*!< out: result out */
	uint16_t	vbucket);	/*!< in: bucket, used by default
					engine only */

/*******************************************************************//**
Support memcached "FLUSH_ALL" command, clean up storage (trunate InnoDB Table)
@return ENGINE_SUCCESS if successfully, otherwise error code */
static
ENGINE_ERROR_CODE
innodb_flush(
/*=========*/
	ENGINE_HANDLE*	handle,		/*!< in: Engine Handle */
	const void*	cookie,		/*!< in: connection cookie */
	time_t		when);		/*!< in: when to flush, not used by
					InnoDB */

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
	ADD_RESPONSE	response);	/*!< out: respondse */

/*******************************************************************//**
Callback functions used by Memcached's process_command() function
to get the result key/value information
@return TRUE if info fetched */
static
bool
innodb_get_item_info(
/*=================*/
	ENGINE_HANDLE*	handle,		/*!< in: Engine Handle */
	const void*	cookie,		/*!< in: connection cookie */
	const item*	item,		/*!< in: item in question */
	item_info*	item_info);	/*!< out: item info got */

#endif /* innodb_engine_private_h */
