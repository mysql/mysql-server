/*
   Copyright (c) 2017, 2022, Oracle and/or its affiliates.

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

package testsuite.clusterj.model;

import com.mysql.clusterj.annotation.PersistenceCapable;

/** Schema
 *
create table mediumunsignedtypes (
 id int not null primary key,

 medium_unsigned_null_hash mediumint unsigned,
 medium_unsigned_null_btree mediumint unsigned,
 medium_unsigned_null_both mediumint unsigned,
 medium_unsigned_null_none mediumint unsigned,

 medium_unsigned_not_null_hash mediumint unsigned not null,
 medium_unsigned_not_null_btree mediumint unsigned not null,
 medium_unsigned_not_null_both mediumint unsigned not null,
 medium_unsigned_not_null_none mediumint unsigned not null,

 unique key idx_medium_unsigned_null_hash (medium_unsigned_null_hash) using hash,
 key idx_medium_unsigned_null_btree (medium_unsigned_null_btree),
 unique key idx_medium_unsigned_null_both (medium_unsigned_null_both),

 unique key idx_medium_unsigned_not_null_hash (medium_unsigned_not_null_hash) using hash,
 key idx_medium_unsigned_not_null_btree (medium_unsigned_not_null_btree),
 unique key idx_medium_unsigned_not_null_both (medium_unsigned_not_null_both)

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

 */
@PersistenceCapable(table="mediumunsignedtypes")
public interface MediumUnsignedTypes extends IdBase {

    int getId();
    void setId(int id);

    public int getMedium_unsigned_null_hash();
    public void setMedium_unsigned_null_hash(int value);

    public int getMedium_unsigned_null_btree();
    public void setMedium_unsigned_null_btree(int value);

    public int getMedium_unsigned_null_both();
    public void setMedium_unsigned_null_both(int value);

    public int getMedium_unsigned_null_none();
    public void setMedium_unsigned_null_none(int value);

    public int getMedium_unsigned_not_null_hash();
    public void setMedium_unsigned_not_null_hash(int value);

    public int getMedium_unsigned_not_null_btree();
    public void setMedium_unsigned_not_null_btree(int value);

    public int getMedium_unsigned_not_null_both();
    public void setMedium_unsigned_not_null_both(int value);

    public int getMedium_unsigned_not_null_none();
    public void setMedium_unsigned_not_null_none(int value);

}

