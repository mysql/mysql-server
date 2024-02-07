/*
   Copyright (c) 2010, 2024, Oracle and/or its affiliates.
   Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

package testsuite.clusterj.model;

import com.mysql.clusterj.annotation.Column;
import com.mysql.clusterj.annotation.Index;
import com.mysql.clusterj.annotation.Indices;
import com.mysql.clusterj.annotation.PersistenceCapable;
import com.mysql.clusterj.annotation.PrimaryKey;
import java.math.BigInteger;

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
@Indices({
    @Index(name="idx_decimal_null_both", columns=@Column(name="decimal_null_both"))
})
@PersistenceCapable(table="bigintegertypes")
@PrimaryKey(column="id")
public interface BigIntegerTypes extends IdBase {

    int getId();
    void setId(int id);

    // Decimal
    @Column(name="decimal_null_hash")
    @Index(name="idx_decimal_null_hash")
    BigInteger getDecimal_null_hash();
    void setDecimal_null_hash(BigInteger value);

    @Column(name="decimal_null_btree")
    @Index(name="idx_decimal_null_btree")
    BigInteger getDecimal_null_btree();
    void setDecimal_null_btree(BigInteger value);

    @Column(name="decimal_null_both")
    BigInteger getDecimal_null_both();
    void setDecimal_null_both(BigInteger value);

    @Column(name="decimal_null_none")
    BigInteger getDecimal_null_none();
    void setDecimal_null_none(BigInteger value);

}
