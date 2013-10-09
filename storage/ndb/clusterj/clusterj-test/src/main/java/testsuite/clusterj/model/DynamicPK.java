/*
 *  Copyright (c) 2011, 2012, Oracle and/or its affiliates. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

package testsuite.clusterj.model;

import com.mysql.clusterj.DynamicObject;

/** Schema
 *
drop table if exists xxx;
create table xxx (
 id binary(xxx) primary key not null,
 number int not null,
 name varchar(10) not null
) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

 */
public abstract class DynamicPK extends DynamicObject {

    public byte[] getId() {
        return (byte[])get(0);
    }
    public void setId(byte[] value) {
        set(0, value);
    }

    public int getNumber() {
        return (Integer)get(1);
    }
    public void setNumber(int value) {
        set(1, value);
    }

    public String getName() {
        return (String)get(2);
    }
    public void setName(String value) {
        set(2, value);
    }

}
