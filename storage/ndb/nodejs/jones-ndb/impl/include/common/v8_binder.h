/*
 Copyright (c) 2012, 2016, Oracle and/or its affiliates. All rights reserved.
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is also distributed with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have included with MySQL.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */


/*****************************************************************
 This V8 Binder layer helps separate v8 dependencies from 
 dependencies on a higher-level application framework such as Node.

 Completing this task will require doing something about libuv: 
 either making libuv common to all frameworks, or creating an 
 abstraction over it.
 ******************************************************************/

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
