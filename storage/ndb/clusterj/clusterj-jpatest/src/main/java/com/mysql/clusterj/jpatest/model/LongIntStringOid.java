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
 * longpk, intpk, and stringpk.
 */
public class LongIntStringOid extends LongIntStringConstants implements Serializable {

    public Long longpk;

    public int intpk;

    public String stringpk;

    /** Needed for persistence oid contract. */
    public LongIntStringOid() {

    }

    /** The normal constructor. */
    public LongIntStringOid(int i) {
        longpk = getPK1(i);
        intpk = getPK2(i);
        stringpk = getPK3(i);
    }

    @Override
    public boolean equals(Object obj) {
        if (obj == null || !this.getClass().equals(obj.getClass()))
            return false;
        LongIntStringOid o = (LongIntStringOid)obj;
        return (this.longpk.equals(o.longpk)
                && this.intpk == o.intpk
                && this.stringpk.equals(o.stringpk));
    }

    @Override
    public int hashCode() {
        return stringpk.hashCode() + (int)intpk + longpk.intValue();
    }

    @Override
    public String toString() {
    StringBuffer result = new StringBuffer();
    result.append("LongIntStringOid[");
    result.append(longpk);
    result.append(",");
    result.append(intpk);
    result.append(",\"");
    result.append(stringpk);
    result.append("\"]");
    return result.toString();
    }

}

