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
create table mediumintegertypes (
 id int not null primary key,

 medium_null_hash mediumint,
 medium_null_btree mediumint,
 medium_null_both mediumint,
 medium_null_none mediumint,

 medium_not_null_hash mediumint not null,
 medium_not_null_btree mediumint not null,
 medium_not_null_both mediumint not null,
 medium_not_null_none mediumint not null,

 unique key idx_medium_null_hash (medium_null_hash) using hash,
 key idx_medium_null_btree (medium_null_btree),
 unique key idx_medium_null_both (medium_null_both),

 unique key idx_medium_not_null_hash (medium_not_null_hash) using hash,
 key idx_medium_not_null_btree (medium_not_null_btree),
 unique key idx_medium_not_null_both (medium_not_null_both)

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

 */
@PersistenceCapable(table="mediumintegertypes")
public interface MediumIntegerTypes extends IdBase {

    int getId();
    void setId(int id);

    public int getMedium_null_hash();
    public void setMedium_null_hash(int value);

    public int getMedium_null_btree();
    public void setMedium_null_btree(int value);

    public int getMedium_null_both();
    public void setMedium_null_both(int value);

    public int getMedium_null_none();
    public void setMedium_null_none(int value);

    public int getMedium_not_null_hash();
    public void setMedium_not_null_hash(int value);

    public int getMedium_not_null_btree();
    public void setMedium_not_null_btree(int value);

    public int getMedium_not_null_both();
    public void setMedium_not_null_both(int value);

    public int getMedium_not_null_none();
    public void setMedium_not_null_none(int value);

}

