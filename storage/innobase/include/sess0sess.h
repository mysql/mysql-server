/*****************************************************************************

Copyright (c) 2013, 2014, Oracle and/or its affiliates. All Rights Reserved.

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

/**************************************************//**
@file include/sess0sess.h
InnoDB session state tracker.
Multi file, shared, system tablespace implementation.

Created 2014-04-30 by Krunal Bauskar
*******************************************************/

#ifndef sess0sess_h
#define sess0sess_h

#include "univ.i"
#include "dict0mem.h"
#include <map>

class dict_intrinsic_table_t {

public:
	/** Constructor
	@param[in,out]	handler		table handler. */
	dict_intrinsic_table_t(dict_table_t*	handler)
		:
		m_handler(handler),
		m_sess_row_id(),
		m_sess_trx_id()
	{
		/* Do nothing. */
	}

	/** Destructor */
	~dict_intrinsic_table_t()
	{
		m_handler = NULL;
		m_sess_row_id = m_sess_trx_id = 0;
	}

public:

	/* Table Handler holding other metadata information commonly needed
	for any table. */
	dict_table_t*				m_handler;

	/** row-id counter for use by intrinsic table for getting row-id.
	Given intrinsic table semantics, row-id can be locally maintained
	instead of getting it from central generator which involves mutex
	locking. */
	ib_uint64_t				m_sess_row_id;

	/** trx_id counter for use by intrinsic table for getting trx-id.
	Intrinsic table are not shared so don't need a central trx-id
	but just need a increased counter to track consistent view while
	proceeding SELECT as part of UPDATE. */
	ib_uint64_t				m_sess_trx_id;
};

/** InnoDB private data that is cached in THD */
typedef std::map<std::string, dict_intrinsic_table_t*>    table_cache_t;
class innodb_session_t {
public:
	/** Constructor */
	innodb_session_t()
		: m_trx(),
		  m_open_tables()
	{
		/* Do nothing. */
	}

	/** Destructor */
	~innodb_session_t()
	{
		m_trx = NULL;

		for (table_cache_t::iterator it = m_open_tables.begin();
		     it != m_open_tables.end();
		     ++it) {
			delete(it->second);
		}
	}

	/** Cache table handler.
	@param[in]	table_name	name of the table
	@param[in,out]	table		table handler to register */
	void register_table_handler(
		const char*	table_name,
		dict_table_t*	table)
	{
		ut_ad(lookup_table_handler(table_name) == NULL);
		m_open_tables.insert(table_cache_t::value_type(
			table_name, new dict_intrinsic_table_t(table)));
	}

	/** Lookup for table handler given table_name.
	@param[in]	table_name	name of the table to lookup */
	dict_table_t* lookup_table_handler(
		const char*	table_name)
	{
		table_cache_t::iterator it = m_open_tables.find(table_name);
		return((it == m_open_tables.end())
		       ? NULL : it->second->m_handler);
	}

	/** Get table session row-id and increment the row-id counter for
	next use.
	@param[in]	table_name	name of the table to lookup
	@return next table local row-id. */
	row_id_t get_next_table_sess_row_id(
			const char*	table_name)
	{
		table_cache_t::iterator it = m_open_tables.find(table_name);
		ut_ad(it != m_open_tables.end());
		return(++it->second->m_sess_row_id);
	}

	/** Get table session trx-id and increment the trx-id counter for
	next use.
	@param[in]	table_name	name of the table to lookup
	@return next table local trx-id. */
	trx_id_t get_next_table_sess_trx_id(
			const char*	table_name)
	{
		table_cache_t::iterator it = m_open_tables.find(table_name);
		ut_ad(it != m_open_tables.end());
		return(++it->second->m_sess_trx_id);
	}

	/** Get table session trx-id.
	@param[in]	table_name	name of the table to lookup
	@return next table local trx-id. */
	trx_id_t get_table_curr_sess_trx_id(
			const char*	table_name)
	{
		table_cache_t::iterator it = m_open_tables.find(table_name);
		ut_ad(it != m_open_tables.end());
		return(it->second->m_sess_trx_id);
	}

	/** Remove table handler entry.
	@param[in]	table_name	name of the table to remove */
	void unregister_table_handler(
		const char*	table_name)
	{
		table_cache_t::iterator it = m_open_tables.find(table_name);
		if (it == m_open_tables.end()) {
			return;
		}

		delete(it->second);
		m_open_tables.erase(table_name);
	}

	/** Count of register table handler.
	@return number of register table handlers */
	uint count_register_table_handler() const
	{
		return(static_cast<uint>(m_open_tables.size()));
	}

public:

	/** transaction handler. */
	trx_t*		m_trx;

	/** Handler of tables that are created or open but not added
	to InnoDB dictionary as they are session specific.
	Currently, limited to intrinsic temporary tables only. */
	table_cache_t	m_open_tables;
};


#endif /* sess0sess_h */
