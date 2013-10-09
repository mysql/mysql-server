/*
 Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/
/*
 * ArrayWrapper.java
 */

package com.mysql.jtie;

/**
 * An interface to Java peer classes representing a C/C++ object array type.
 * 
 * Please, note that
 * <ol>
 * <li> no length information is associated with this wrapper class for
 *      arrays and, hence,
 * <li> no bound-checking whatsoever is performed at index access.
 * </ol>
 */
// add warning: access to all memory possible
public interface ArrayWrapper< T > {
    /**
     * Returns the element of this array at a given index.
     * Index may be negative (rendering a true, 1-1 mapping of C/C++ pointers).
     */
    T at(int i);
}
