/*****************************************************************************

Copyright (c) 2016, Oracle and/or its affiliates. All Rights Reserved.

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
@file handler/p_s.h
InnoDB performance_schema tables interface to MySQL.

*******************************************************/

#ifndef p_s_h
#define p_s_h

#include "mysql/psi/psi_data_lock.h"

/** Inspect data locks in innodb.
This class is used by the performance schema to extract lock data.
*/
class Innodb_data_lock_inspector : public PSI_engine_data_lock_inspector
{
public:
	Innodb_data_lock_inspector();
	~Innodb_data_lock_inspector();

	virtual PSI_engine_data_lock_iterator*
		create_data_lock_iterator();
	virtual PSI_engine_data_lock_wait_iterator*
		create_data_lock_wait_iterator();
	virtual void destroy_data_lock_iterator(
		PSI_engine_data_lock_iterator *it);
	virtual void destroy_data_lock_wait_iterator(
		PSI_engine_data_lock_wait_iterator *it);
};

#endif /* p_s_h */
