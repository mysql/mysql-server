// Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
//
// This program is free software; you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free Software
// Foundation; version 2 of the License.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, 51 Franklin
// Street, Suite 500, Boston, MA 02110-1335 USA.

/// @file
///
/// This file declares the interface of class Sql_cmd_create_srs, which
/// handles the CREATE SPATIAL REFERENCE SYSTEM statement.

#ifndef SQL_SQL_CMD_SRS_H_INCLUDED
#define SQL_SQL_CMD_SRS_H_INCLUDED

#include "my_sqlcommand.h"           // SQLCOM_CREATE_SRS
#include "mysql/mysql_lex_string.h"  // MYSQL_LEX_STRING
#include "sql/dd/types/spatial_reference_system.h"
#include "sql/gis/srid.h"  // gis::srid_t
#include "sql/sql_cmd.h"

class THD;

struct Sql_cmd_srs_attributes
{
  MYSQL_LEX_STRING srs_name;
  MYSQL_LEX_STRING definition;
  MYSQL_LEX_STRING organization;
  unsigned long long organization_coordsys_id;
  MYSQL_LEX_STRING description;

  Sql_cmd_srs_attributes() : srs_name({nullptr, 0}), definition({nullptr, 0}),
    organization({nullptr, 0}), organization_coordsys_id(0),
    description({nullptr, 0})
  {}
};

class Sql_cmd_create_srs final : public Sql_cmd
{
public:
  Sql_cmd_create_srs() {}
  void init(bool or_replace, bool if_not_exists, gis::srid_t srid,
            MYSQL_LEX_STRING srs_name, MYSQL_LEX_STRING definition,
            MYSQL_LEX_STRING organization, gis::srid_t organization_coordsys_id,
            MYSQL_LEX_STRING description)
  {
    m_or_replace= or_replace;
    m_if_not_exists= if_not_exists;
    m_srid= srid;
    m_srs_name= srs_name;
    m_definition= definition;
    m_organization= organization;
    m_organization_coordsys_id= organization_coordsys_id;
    m_description= description;
  }
  enum_sql_command sql_command_code() const override
  { return SQLCOM_CREATE_SRS; }
  bool execute(THD *thd) override;

  /// Fill an SRS with information from this CREATE statement (except the ID).
  ///
  /// @param[in,out] srs The SRS.
  ///
  /// @retval false Success.
  /// @retval true An error occurred (i.e., invalid SRS definition). The error
  /// has been reported with my_error.
  bool fill_srs(dd::Spatial_reference_system *srs);

private:
  /// Whether OR REPLACE was specified.
  bool m_or_replace= false;
  /// Whether IF NOT EXISTS was specified
  bool m_if_not_exists= false;
  /// The SRID of the new SRS.
  gis::srid_t m_srid= 0;
  /// The name of the new SRS.
  ///
  /// The value is always a valid name (verified by PT_create_srs), but it may
  /// be a duplicate of an existing one.
  MYSQL_LEX_STRING m_srs_name;
  /// The definition of the new SRS.
  ///
  /// The definition is not parsed and validated until the SRS is created.
  MYSQL_LEX_STRING m_definition;
  /// Organization that is the source of the SRS definition.
  MYSQL_LEX_STRING m_organization;
  /// Source organization's SRS ID.
  gis::srid_t m_organization_coordsys_id= 0;
  /// Description of the new SRS.
  MYSQL_LEX_STRING m_description;
};

#endif  // SQL_SQL_CMD_SRS_H_INCLUDED
