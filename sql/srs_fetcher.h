#ifndef SRS_FETCHER_H_INCLUDED
#define SRS_FETCHER_H_INCLUDED

/*
  Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA
*/

#include "sql/gis/geometries.h"
#include "sql/gis/srid.h"
#include "sql/mdl.h"

namespace dd {
  class Spatial_reference_system;
}

class Srs_fetcher
{
private:
  THD *m_thd;

  /**
    Take an MDL lock on an SRID.

    @param[in] srid Spatial reference system ID
    @param[in] lock_type Type of lock to take

    @retval false Success.
    @retval true Locking failed. An error has already been flagged.
  */
  bool lock(gis::srid_t srid, enum_mdl_type lock_type);

public:
  Srs_fetcher(THD *thd)
    :m_thd(thd)
  {}

  /**
    Acquire an SRS from the data dictionary. Take a shared read lock on the
    SRID.

    @param[in] srid Spatial reference system ID
    @param[out] srs The spatial reference system

    @retval false Success.
    @retval true Locking failed. An error has already been flagged.
  */
  bool acquire(gis::srid_t srid, const dd::Spatial_reference_system **srs);

  /**
    Acquire an SRS from the data dictionary with the intent of modifying
    it. Take an exclusive lock on the SRID.

    @param[in] srid Spatial reference system ID
    @param[out] srs The spatial reference system

    @retval false Success.
    @retval true Locking failed. An error has already been flagged.
  */
  bool acquire_for_modification(gis::srid_t srid,
                                dd::Spatial_reference_system **srs);

  static bool srs_exists(THD *thd, gis::srid_t srid, bool *exists);
};

#endif // SRS_FETCHER_H_INCLUDED
