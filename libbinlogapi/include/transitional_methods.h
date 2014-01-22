/*
Copyright (c) 2013, Oracle and/or its affiliates. All rights
reserved.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of
the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
02110-1301  USA
*/

/**
  @file transitional_methods.h

  @brief Contains functions which are used by the server independent of the
  binlogevent library.
*/

#ifndef _TRANSITIONAL_METHODS_H
#define _TRANSITIONAL_METHODS_H
#include <stdlib.h>
#include <string.h>
#ifdef min //definition of min() and max() in std and libmysqlclient
           //can be/are different
#undef min
#endif
#ifdef max
#undef max
#endif

/**
  In case the variable is updated,
  make sure to update it in $MYSQL_SOURCE_DIR/my_global.h.
*/
#ifndef FN_REFLEN
#define FN_REFLEN       512     /* Max length of full path-name */
#endif

/**
   Splits server 'version' string into three numeric pieces stored
   into 'split_versions':
   X.Y.Zabc (X,Y,Z numbers, a not a digit) -> {X,Y,Z}
   X.Yabc -> {X,Y,0}

   @param version        String representing server version
   @param split_versions Array with each element containing one split of the
                         input version string
*/
inline void do_server_version_split(const char *version, unsigned char split_versions[3])
{
  const char *p= version;
  char *r;
  unsigned long number;
  for (unsigned int i= 0; i<=2; i++)
  {
    number= strtoul(p, &r, 10);
    /*
      It is an invalid version if any version number greater than 255 or
      first number is not followed by '.'.
    */
    if (number < 256 && (*r == '.' || i != 0))
      split_versions[i]= (unsigned char)number;
    else
    {
      split_versions[0]= 0;
      split_versions[1]= 0;
      split_versions[2]= 0;
      break;
    }

    p= r;
    if (*r == '.')
      p++; // skip the dot
  }
}


/**
  Calculate the version product from the numeric pieces representing the server
  version:
  For a server version X.Y.Zabc (X,Y,Z numbers, a not a digit), the input is
  {X,Y,Z}. This is converted to XYZ in bit representation.

  @param  version_split Array containing the version information of the server
  @return               The version product of the server
*/
inline unsigned long version_product(const unsigned char* version_split)
{
  return ((version_split[0] * 256 + version_split[1]) * 256
          + version_split[2]);
}


/**
   Replication event checksum is introduced in the following "checksum-home"
   version. The checksum-aware servers extract FD's version to decide whether
   the FD event  carries checksum info.
*/
extern const unsigned char checksum_version_split[3];
extern const unsigned long checksum_version_product;
#endif
