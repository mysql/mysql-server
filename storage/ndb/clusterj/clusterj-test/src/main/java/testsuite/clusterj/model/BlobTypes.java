/*
   Copyright (c) 2010, 2018, Oracle and/or its affiliates. All rights reserved.

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

import com.mysql.clusterj.annotation.Lob;
import com.mysql.clusterj.annotation.PersistenceCapable;
import com.mysql.clusterj.annotation.PrimaryKey;

/** Schema
 *
drop table if exists blobtypes;
create table blobtypes (
 id int not null primary key,
 id_null_none int,
 id_null_hash int,

 blobbytes blob,

 unique key idx_id_null_hash (id_null_hash) using hash

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

 */

/** Blob types.
 */
@PersistenceCapable(table="blobtypes")
@PrimaryKey(column="id")
public interface BlobTypes extends IdBase {

    int getId();
    void setId(int id);

    int getId_null_none();
    void setId_null_none(int id);

    int getId_null_hash();
    void setId_null_hash(int id);

    @Lob
    byte[] getBlobbytes();
    void setBlobbytes(byte[] value);

}
