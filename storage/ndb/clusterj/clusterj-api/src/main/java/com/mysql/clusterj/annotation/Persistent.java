/*
   Copyright (c) 2010, 2022, Oracle and/or its affiliates.
   Use is subject to license terms.

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

package com.mysql.clusterj.annotation;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Annotation for defining the persistence of a member.
 */
@Target({ElementType.FIELD, ElementType.METHOD}) 
@Retention(RetentionPolicy.RUNTIME)
public @interface Persistent
{

    /** Behavior when this member contains a null value. 
     * @return the behavior when this member contains a null value
     */
    NullValue nullValue() default NullValue.NONE;

    /** Whether this member is part of the primary key for
     * the table. This is equivalent to specifying @PrimaryKey as 
     * a separate annotation on the member.
     * @return whether this member is part of the primary key
     */
    String primaryKey() default "";

    /** Column name where the values are stored for this member. 
     * @return the name of the column
     */
    String column() default "";

    /** Non-standard extensions for this member. 
     * @return the non-standard extensions
     */
    Extension[] extensions() default {};
}
