/* Copyright (C) 2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef PARAMINFO_H
#define PARAMINFO_H

#define DB_TOKEN "DB"
#define MGM_TOKEN "MGM"
#define API_TOKEN "API"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * The Configuration parameter type and status
 */

enum ParameterType        { CI_BOOL, CI_INT, CI_INT64, CI_STRING, CI_SECTION };
enum ParameterStatus      { CI_USED,            ///< Active
		     CI_DEPRICATED,      ///< Can be, but shouldn't
		     CI_NOTIMPLEMENTED,  ///< Is ignored.
		     CI_INTERNAL         ///< Not configurable by the user
};

/**
 *   Entry for one configuration parameter
 */
typedef struct m_ParamInfo {
  Uint32         _paramId;
  const char*    _fname;   
  const char*    _section;
  const char*    _description;
  ParameterStatus         _status;
  bool           _updateable;    
  ParameterType           _type;          
  const char*    _default;
  const char*    _min;
  const char*    _max;
}ParamInfo;

#ifdef __cplusplus
}
#endif

#endif
