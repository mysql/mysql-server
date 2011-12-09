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

import java.math.BigInteger;

import javax.persistence.Entity;
import javax.persistence.Id;
import javax.persistence.Table;

/** Schema
 *
drop table if exists bigintegertypes;
create table bigintegertypes (
 id int not null primary key,

 decimal_null_hash decimal(10),
 decimal_null_btree decimal(10),
 decimal_null_both decimal(10),
 decimal_null_none decimal(10)

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

create unique index idx_decimal_null_hash using hash on bigintegertypes(decimal_null_hash);
create index idx_decimal_null_btree on bigintegertypes(decimal_null_btree);
create unique index idx_decimal_null_both on bigintegertypes(decimal_null_both);

 */
@Entity
@Table(name="bigintegertypes")
public class BigIntegerTypes implements IdBase {

    @Id
    int id;
    BigInteger decimal_null_hash;
    BigInteger decimal_null_btree;
    BigInteger decimal_null_both;
    BigInteger decimal_null_none;

    public int getId() {
        return id;
    }
    public void setId(int id) {
        this.id = id;
    }
    public BigInteger getDecimal_null_hash() {
        return decimal_null_hash;
    }
    public void setDecimal_null_hash(BigInteger decimalNullHash) {
        decimal_null_hash = decimalNullHash;
    }
    public BigInteger getDecimal_null_btree() {
        return decimal_null_btree;
    }
    public void setDecimal_null_btree(BigInteger decimalNullBtree) {
        decimal_null_btree = decimalNullBtree;
    }
    public BigInteger getDecimal_null_both() {
        return decimal_null_both;
    }
    public void setDecimal_null_both(BigInteger decimalNullBoth) {
        decimal_null_both = decimalNullBoth;
    }
    public BigInteger getDecimal_null_none() {
        return decimal_null_none;
    }
    public void setDecimal_null_none(BigInteger decimalNullNone) {
        decimal_null_none = decimalNullNone;
    }

}
