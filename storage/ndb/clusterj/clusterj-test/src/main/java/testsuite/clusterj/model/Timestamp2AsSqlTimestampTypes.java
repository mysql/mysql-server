/*
   Copyright (c) 2012, 2023, Oracle and/or its affiliates.

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

import com.mysql.clusterj.annotation.Column;
import com.mysql.clusterj.annotation.PersistenceCapable;
import com.mysql.clusterj.annotation.PrimaryKey;
import java.sql.Timestamp;

/** Schema
*
drop table if exists timestamp2types;
create table timestamp2types (
id int not null primary key auto_increment,

timestampx timestamp    null,
timestamp0 timestamp(0) null,
timestamp1 timestamp(1) null,
timestamp2 timestamp(2) null,
timestamp3 timestamp(3) null,
timestamp4 timestamp(4) null,
timestamp5 timestamp(5) null,
timestamp6 timestamp(6) null

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

*/
@PersistenceCapable(table="timestamp2types")
@PrimaryKey(column="id")
public interface Timestamp2AsSqlTimestampTypes extends IdBase {

 int getId();
 void setId(int id);

 // Timestamp
 @Column(name="timestampx")
 Timestamp getTimestampx();
 void setTimestampx(Timestamp value);

 @Column(name="timestamp0")
 Timestamp getTimestamp0();
 void setTimestamp0(Timestamp value);

 @Column(name="timestamp1")
 Timestamp getTimestamp1();
 void setTimestamp1(Timestamp value);

 @Column(name="timestamp2")
 Timestamp getTimestamp2();
 void setTimestamp2(Timestamp value);

 @Column(name="timestamp3")
 Timestamp getTimestamp3();
 void setTimestamp3(Timestamp value);

 @Column(name="timestamp4")
 Timestamp getTimestamp4();
 void setTimestamp4(Timestamp value);

 @Column(name="timestamp5")
 Timestamp getTimestamp5();
 void setTimestamp5(Timestamp value);

 @Column(name="timestamp6")
 Timestamp getTimestamp6();
 void setTimestamp6(Timestamp value);

}
