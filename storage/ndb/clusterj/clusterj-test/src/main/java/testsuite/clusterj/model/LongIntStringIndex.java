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
drop table if exists longintstringix;
create table longintstringix (
 id int primary key,
 longix bigint not null,
 stringix varchar(10) not null,
 intix int not null,
 stringvalue varchar(10)
) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

create index idx_long_int_string on longintstringix(longix, intix, stringix);

 */
@PersistenceCapable(table="longintstringix")
public interface LongIntStringIndex extends IdBase{

    @Column(name="id")
    int getId();
    void setId(int id);

    @Column(name="longix")
    long getLongix();
    void setLongix(long value);

    @Column(name="intix")
    int getIntix();
    void setIntix(int value);

    @Column(name="stringix")
    String getStringix();
    void setStringix(String value);

    @Column(name="stringvalue")
    String getStringvalue();
    void setStringvalue(String value);

}
