/*
 * Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */


#ifndef _NGS_POSIX_TIME_H_
#define _NGS_POSIX_TIME_H_

#include <boost/date_time/posix_time/posix_time.hpp>


namespace ngs
{

using boost::posix_time::microsec_clock;
using boost::posix_time::ptime;
using boost::posix_time::time_duration;
using boost::posix_time::milliseconds;
using boost::posix_time::seconds;
using boost::date_time::not_a_date_time;

} // namespcae ngs

#endif // _NGS_POSIX_TIME_H_
