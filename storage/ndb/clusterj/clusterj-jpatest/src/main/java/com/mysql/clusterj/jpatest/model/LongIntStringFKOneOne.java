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
import javax.persistence.OneToOne;

import org.apache.openjpa.persistence.jdbc.Index;

/** Schema
 *
drop table if exists longintstringfk;
create table longintstringfk (
 longpk bigint not null,
 intpk int not null,
 stringpk varchar(10) not null,
 longfk bigint,
 intfk int,
 stringfk varchar(10),
 stringvalue varchar(10),
        KEY FK_longfkintfkstringfk (longfk, intfk, stringfk),
        CONSTRAINT PK_longintstringfk PRIMARY KEY (longpk, intpk, stringpk)

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

 */
@javax.persistence.Entity
@javax.persistence.Table(name="longintstringfk")
@javax.persistence.IdClass(value=LongIntStringOid.class)
public class LongIntStringFKOneOne extends LongIntStringConstants implements Serializable {

    @javax.persistence.Id
    @javax.persistence.Column(name="longpk")
    private Long longpk;

    @javax.persistence.Id
    @javax.persistence.Column(name="intpk")
    private int intpk;

    @javax.persistence.Id
    @javax.persistence.Column(name="stringpk")
    private String stringpk;

    @OneToOne
    @JoinColumns({
        @JoinColumn(name="longfk", referencedColumnName="longpk"),
        @JoinColumn(name="intfk", referencedColumnName="intpk"),
        @JoinColumn(name="stringfk", referencedColumnName="stringpk")
        })
    @Index(name="FK_longfkintfkstringfk")
    private LongIntStringPKOneOne longIntStringPKOneOne;

    @javax.persistence.Column(name="stringvalue")
    private String stringvalue;

    public LongIntStringFKOneOne() {
    }

    public Long getLongpk() {
        return longpk;
    }

    public void setLongpk(Long value) {
        longpk = value;
    }

    public int getIntpk() {
        return intpk;
    }

    public void setIntpk(int value) {
        intpk = value;
    }

    public String getStringpk() {
        return stringpk;
    }

    public void setStringpk(String value) {
        stringpk = value;
    }

    public LongIntStringPKOneOne getLongIntStringPKOneOne() {
        return longIntStringPKOneOne;
    }

    public void setLongIntStringPKOneOne(LongIntStringPKOneOne value) {
        longIntStringPKOneOne = value;
    }

    static public LongIntStringFKOneOne create(int id) {
        LongIntStringFKOneOne o = new LongIntStringFKOneOne();
        o.longpk = getPK1(id);
        o.intpk = getPK2(id);
        o.stringpk = getPK3(id);
        o.stringvalue = getValue(id);
        return o;
    }

    static public LongIntStringOid createOid(int id) {
        LongIntStringOid oid = new LongIntStringOid(id);
        return oid;
    }

    @Override
    public String toString() {
        StringBuffer result = new StringBuffer();
        result.append("LongIntStringFK[");
        result.append(longpk);
        result.append(",");
        result.append(intpk);
        result.append(",\"");
        result.append(stringpk);
        result.append("\"]: ");
        result.append(stringvalue);
        result.append(").");
        return result.toString();
    }

}

