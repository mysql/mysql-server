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

package com.mysql.clusterj.jpatest.model;

import java.io.Serializable;

/** This class implements the object id for classes that have
 * three primary keys: long, long, String. The key fields in the persistent
 * class must be named the same as the oid class:
 * longpk1, longpk2, and stringpk.
 */
public class LongLongStringOid extends LongLongStringConstants implements Serializable {

    public long longpk1;

    public long longpk2;

    public String stringpk;

    /** Needed for persistence oid contract. */
    public LongLongStringOid() {

    }

    /** The normal constructor. */
    public LongLongStringOid(int i) {
        longpk1 = getPK1(i);
        longpk2 = getPK2(i);
        stringpk = getPK3(i);
    }

    @Override
    public boolean equals(Object obj) {
        if (obj == null || !this.getClass().equals(obj.getClass()))
            return false;
        LongLongStringOid o = (LongLongStringOid)obj;
        return (this.longpk1 == o.longpk1
                && this.longpk2 == o.longpk2
                && this.stringpk.equals(o.stringpk));
    }

    @Override
    public int hashCode() {
        return stringpk.hashCode() + (int)longpk1 + (int)longpk2;
    }

    @Override
    public String toString() {
    StringBuffer result = new StringBuffer();
    result.append("LongLongStringOid[");
    result.append(longpk1);
    result.append(",");
    result.append(longpk2);
    result.append(",\"");
    result.append(stringpk);
    result.append("\"]");
    return result.toString();
    }

}

