/*
 Copyright (c) 2012, Oracle and/or its affiliates. All rights
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


/*****************************************************************
 This V8 Binder layer helps separate v8 dependencies from 
 dependencies on a higher-level application framework such as Node.

 Completing this task will require doing something about libuv: 
 either making libuv common to all frameworks, or creating an 
 abstraction over it.
 ******************************************************************/

#pragma once

/* Choose a binder framework:
*/
#define V8BINDER_FOR_NODE


/***** Node.JS Binder ******/
#ifdef V8BINDER_FOR_NODE

#define BUILDING_NODE_EXTENSION 1
#include "node.h"
#include "node_buffer.h"

#define V8BINDER_LOADABLE_MODULE(A,B)  NODE_MODULE(A,B)
#define V8BINDER_UNWRAP_BUFFER(JSVAL) node::Buffer::Data(JSVAL->ToObject())


#endif
