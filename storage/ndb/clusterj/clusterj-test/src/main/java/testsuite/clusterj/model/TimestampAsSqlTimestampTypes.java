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

package testsuite.clusterj.model;

import com.mysql.clusterj.annotation.Column;
import com.mysql.clusterj.annotation.Index;
import com.mysql.clusterj.annotation.Indices;
import com.mysql.clusterj.annotation.PersistenceCapable;
import com.mysql.clusterj.annotation.PrimaryKey;
import java.sql.Timestamp;

/** Schema
 *
drop table if exists timestamptypes;
create table timestamptypes (
 id int not null primary key,

 timestamp_not_null_hash timestamp,
 timestamp_not_null_btree timestamp,
 timestamp_not_null_both timestamp,
 timestamp_not_null_none timestamp

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

create unique index idx_timestamp_not_null_hash using hash on timestamptypes(timestamp_not_null_hash);
create index idx_timestamp_not_null_btree on timestamptypes(timestamp_not_null_btree);
create unique index idx_timestamp_not_null_both on timestamptypes(timestamp_not_null_both);

 */
@Indices({
    @Index(name="idx_timestamp_not_null_both", columns=@Column(name="timestamp_not_null_both"))
})
@PersistenceCapable(table="timestamptypes")
@PrimaryKey(column="id")
public interface TimestampAsSqlTimestampTypes extends IdBase {

    int getId();
    void setId(int id);

    // Timestamp
    @Column(name="timestamp_not_null_hash")
    @Index(name="idx_timestamp_not_null_hash")
    Timestamp getTimestamp_not_null_hash();
    void setTimestamp_not_null_hash(Timestamp value);

    @Column(name="timestamp_not_null_btree")
    @Index(name="idx_timestamp_not_null_btree")
    Timestamp getTimestamp_not_null_btree();
    void setTimestamp_not_null_btree(Timestamp value);

    @Column(name="timestamp_not_null_both")
    Timestamp getTimestamp_not_null_both();
    void setTimestamp_not_null_both(Timestamp value);

    @Column(name="timestamp_not_null_none")
    Timestamp getTimestamp_not_null_none();
    void setTimestamp_not_null_none(Timestamp value);

}
