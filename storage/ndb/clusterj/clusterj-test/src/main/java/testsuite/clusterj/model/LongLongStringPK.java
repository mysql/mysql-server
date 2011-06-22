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
import com.mysql.clusterj.annotation.PersistenceCapable;
import com.mysql.clusterj.annotation.PrimaryKey;

/** Schema
 *
create table longlongstringpk (
 longpk1 bigint not null,
 longpk2 bigint not null,
 stringpk varchar(10) not null,
 stringvalue varchar(10),
        CONSTRAINT PK_longlongstringpk PRIMARY KEY (longpk1, longpk2, stringpk)

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;
 */
@PersistenceCapable(table="longlongstringpk")
@PrimaryKey(columns={
    @Column(name="longpk1"),
    @Column(name="longpk2"),
    @Column(name="stringpk")
})
public interface LongLongStringPK {

    @Column(name="longpk1")
    long getLongpk1();
    void setLongpk1(long value);

    @Column(name="longpk2")
    long getLongpk2();
    void setLongpk2(long value);

    @Column(name="stringpk")
    String getStringpk();
    void setStringpk(String value);

    @Column(name="stringvalue")
    String getStringvalue();
    void setStringvalue(String value);

}
