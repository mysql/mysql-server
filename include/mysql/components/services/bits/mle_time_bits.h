/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef COMPONENTS_SERVICES_BITS_MLE_TIME_BITS_H
#define COMPONENTS_SERVICES_BITS_MLE_TIME_BITS_H

#define MYSQL_TIMESTAMP_TYPE_NONE -2
#define MYSQL_TIMESTAMP_TYPE_ERROR -1
#define MYSQL_TIMESTAMP_TYPE_DATE 0
#define MYSQL_TIMESTAMP_TYPE_DATETIME 1
#define MYSQL_TIMESTAMP_TYPE_TIME 2
#define MYSQL_TIMESTAMP_TYPE_DATETIME_TZ 3

/*
  This struct should be convertible to/from MYSQL_TIME
*/
typedef struct mle_time {
  unsigned int year, month, day, hour, minute, second;
  unsigned long second_part; /**< microseconds */
  bool neg;
  int time_type;
  /// The time zone displacement, specified in seconds.
  int time_zone_displacement;
} mle_time;

#endif /* COMPONENTS_SERVICES_BITS_MLE_TIME_BITS_H */
