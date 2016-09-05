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


#ifndef _NGS_SMART_PTR_H_
#define _NGS_SMART_PTR_H_

#include <boost/enable_shared_from_this.hpp>
#include <boost/move/unique_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>

namespace ngs
{
using boost::dynamic_pointer_cast;
using boost::enable_shared_from_this;
using boost::make_shared;
using boost::move;
using boost::movelib::unique_ptr;
using boost::shared_ptr;
using boost::static_pointer_cast;
using boost::weak_ptr;

namespace detail
{
using boost::allocate_shared;
} // namespace detail
} // namespace ngs

#endif // _NGS_SMART_PTR_H_
