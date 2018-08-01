/*
   Copyright (c) 2010, 2018, Oracle and/or its affiliates. All rights reserved.

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

 blobbytes blob

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

    @Lob
    byte[] getBlobbytes();
    void setBlobbytes(byte[] value);

}
