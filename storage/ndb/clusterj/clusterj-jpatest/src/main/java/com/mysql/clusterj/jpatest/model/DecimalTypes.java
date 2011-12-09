/*
   Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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

import java.math.BigDecimal;

import javax.persistence.Column;
import javax.persistence.Entity;
import javax.persistence.Id;
import javax.persistence.Table;

/** Schema
 *
drop table if exists decimaltypes;
create table decimaltypes (
 id int not null primary key,

 decimal_null_hash decimal(10,5),
 decimal_null_btree decimal(10,5),
 decimal_null_both decimal(10,5),
 decimal_null_none decimal(10,5)

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

create unique index idx_decimal_null_hash using hash on decimaltypes(decimal_null_hash);
create index idx_decimal_null_btree on decimaltypes(decimal_null_btree);
create unique index idx_decimal_null_both on decimaltypes(decimal_null_both);

 */
@Entity
@Table(name="decimaltypes")
public class DecimalTypes implements IdBase {

    @Id
    int id;
    @Column(precision=10,scale=5)
    BigDecimal decimal_null_hash;
    @Column(precision=10,scale=5)
    BigDecimal decimal_null_btree;
    @Column(precision=10,scale=5)
    BigDecimal decimal_null_both;
    @Column(precision=10,scale=5)
    BigDecimal decimal_null_none;

    public int getId() {
        return id;
    }
    public void setId(int id) {
        this.id = id;
    }
    public BigDecimal getDecimal_null_hash() {
        return decimal_null_hash;
    }
    public void setDecimal_null_hash(BigDecimal decimalNullHash) {
        decimal_null_hash = decimalNullHash;
    }
    public BigDecimal getDecimal_null_btree() {
        return decimal_null_btree;
    }
    public void setDecimal_null_btree(BigDecimal decimalNullBtree) {
        decimal_null_btree = decimalNullBtree;
    }
    public BigDecimal getDecimal_null_both() {
        return decimal_null_both;
    }
    public void setDecimal_null_both(BigDecimal decimalNullBoth) {
        decimal_null_both = decimalNullBoth;
    }
    public BigDecimal getDecimal_null_none() {
        return decimal_null_none;
    }
    public void setDecimal_null_none(BigDecimal decimalNullNone) {
        decimal_null_none = decimalNullNone;
    }

}
