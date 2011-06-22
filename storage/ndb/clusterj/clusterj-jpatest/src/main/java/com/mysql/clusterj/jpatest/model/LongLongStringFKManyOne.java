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
import javax.persistence.JoinColumn;
import javax.persistence.JoinColumns;
import javax.persistence.ManyToOne;

/** Schema
 *
create table longlongstringfk (
 longpk1 bigint not null,
 longpk2 bigint not null,
 stringpk varchar(10) not null,
 longfk1 bigint not null,
 longfk2 bigint not null,
 stringfk varchar(10) not null,
 stringvalue varchar(10),
        KEY FK_longfk1longfk2stringfk (longfk1, longfk2, stringfk),
        CONSTRAINT PK_longlongstringfk PRIMARY KEY (longpk1, longpk2, stringpk)

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;
 */
@javax.persistence.Entity
@javax.persistence.Table(name="longlongstringfk")
@javax.persistence.IdClass(value=LongLongStringOid.class)
public class LongLongStringFKManyOne extends LongLongStringConstants implements Serializable {

    @javax.persistence.Id
    @javax.persistence.Column(name="longpk1")
    private long longpk1;

    @javax.persistence.Id
    @javax.persistence.Column(name="longpk2")
    private long longpk2;

    @javax.persistence.Id
    @javax.persistence.Column(name="stringpk")
    private String stringpk;

    @ManyToOne
    @JoinColumns({
        @JoinColumn(name="longfk1", referencedColumnName="longpk1"),
        @JoinColumn(name="longfk2", referencedColumnName="longpk2"),
        @JoinColumn(name="stringfk", referencedColumnName="stringpk")
        })
        @org.apache.openjpa.persistence.jdbc.Index(name="FK_longfk1longfk2stringfk")
    private LongLongStringPKOneMany longLongStringPKOneMany;

    @javax.persistence.Column(name="stringvalue")
    private String stringvalue;

    public LongLongStringFKManyOne() {
    }

    public long getLongpk1() {
        return longpk1;
    }

    public void setLongpk1(long value) {
        longpk1 = value;
    }

    public long getLongpk2() {
        return longpk2;
    }

    public void setLongpk2(long value) {
        longpk2 = value;
    }

    public String getStringpk() {
        return stringpk;
    }

    public void setStringpk(String value) {
        stringpk = value;
    }

    public LongLongStringPKOneMany getLongLongStringPKOneMany() {
        return longLongStringPKOneMany;
    }

    public void setLongLongStringPKRelationship(LongLongStringPKOneMany value) {
        longLongStringPKOneMany = value;
    }

    static public LongLongStringFKManyOne create(int id) {
        LongLongStringFKManyOne o = new LongLongStringFKManyOne();
        o.longpk1 = getPK1(id);
        o.longpk2 = getPK2(id);
        o.stringpk = getPK3(id);
        o.stringvalue = getValue(id);
        return o;
    }

    static public LongLongStringOid createOid(int id) {
        LongLongStringOid oid = new LongLongStringOid(id);
        return oid;
    }

    @Override
    public String toString() {
        StringBuffer result = new StringBuffer();
        result.append("LongLongStringFK[");
        result.append(longpk1);
        result.append(",");
        result.append(longpk2);
        result.append(",\"");
        result.append(stringpk);
        result.append("\"]: ");
        result.append(stringvalue);
        result.append(" -> (");
        result.append((longLongStringPKOneMany==null)?"null":longLongStringPKOneMany.toString());
        result.append(").");
        return result.toString();
    }

}

