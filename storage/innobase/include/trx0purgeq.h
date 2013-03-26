/*****************************************************************************

Copyright (c) 2013, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/srv0purgeq.h
Purge Queue implementation.

Created 2013-03-26 by Krunal Bauskar.
*******************************************************/

#ifndef trx0purgeq_h 
#define trx0purgeq_h

#include "trx0trx.h"
#include <queue>
#include <vector>

/** Purge Element is used by query processing thread for submitting purge-request. */
class PurgeElem {

public:
	PurgeElem()
		:
		m_trx_no(),
		m_next_rseg_pos()
	{
		 /* No op */
	}

	/**
	Set transaction number
	@param trx_no - transaction number to set. */
	void set_trx_no(trx_id_t trx_no)
	{
		m_trx_no = trx_no;
	}

	/**
	Get transaction number
	@return trx_id_t - get transaction number. */
	trx_id_t get_trx_no() const
	{
		return(m_trx_no);
	}

	/**
	Add rollback segment to central array.
	@param rseg - rollback segment to add. */
	void add(trx_rseg_t* rseg)
	{
		m_rsegs.push_back(rseg);
	}

	/**
	Reset next rseg position back to 0. */
	void reset()
	{
		m_next_rseg_pos = 0;
	}

	/**
	Clear existing central array. */
	void clear()
	{
		m_rsegs.clear();
		reset();
	}

	/**
	Get next rollback segment.
	@return trx_rseg_t - if next pos rseg valid return else return NULL. */
	trx_rseg_t* get_next_rseg()
	{
		if (m_next_rseg_pos >= m_rsegs.size()) {
			return(NULL);
		}

		return(m_rsegs[m_next_rseg_pos++]);
	}

	/**
	Compare two PurgeElem based on trx_no.
	@param - elem1 - first element to compare
	@param - elem2 - second element to compare
	@return true if elem1 > elem2 else false.*/
	bool operator()(
		const PurgeElem& elem1,
		const PurgeElem& elem2)
	{
		return(elem1.m_trx_no > elem2.m_trx_no);
	}

private:
	/** Transaction number of a transaction of which rollback segments
	are part off. */
	trx_id_t			m_trx_no;

	/** Rollback segments of a transaction, scheduled for purge. */
	std::vector<trx_rseg_t*>	m_rsegs;

	/** Return next rseg from this position. */
	ulint				m_next_rseg_pos;
};

typedef	std::priority_queue<PurgeElem, std::vector<PurgeElem>, PurgeElem>
		purge_queue_t;


#endif /* trx0purgeq_h */
