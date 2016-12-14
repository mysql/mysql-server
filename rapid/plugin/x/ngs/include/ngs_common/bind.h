/*
 * Copyright (c) 2016 Oracle and/or its affiliates. All rights reserved.
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

#ifndef _NGS_BIND_H_
#define _NGS_BIND_H_

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/ref.hpp>

namespace ngs
{
namespace placeholders
{
using ::_1;
using ::_2;
using ::_3;
using ::_4;
} // namespace placeholders

using boost::bind;
using boost::function;
using boost::ref;
} // namespace ngs

#endif // _NGS_BIND_H_
