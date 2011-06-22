/*
   Copyright 2010 Sun Microsystems, Inc.
   All rights reserved. Use is subject to license terms.

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

package com.mysql.clusterj.annotation;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Annotation on a member to define it as a primary key member of a class or
 * persistent interface.
 */
@Target({ElementType.TYPE, ElementType.FIELD, ElementType.METHOD}) 
@Retention(RetentionPolicy.RUNTIME)
public @interface PrimaryKey
{
    /**
     * Name of the primary key constraint
     * @return the name of the primary key constraint
     */
    String name() default "";

    /**
     * Name of the column to use for the primary key
     * @return the name of the column to use for the primary key
     */
    String column() default "";

    /**
     * The column(s) for the primary key
     * @return the column(s) for the primary key
     */
    Column[] columns() default {};
}
