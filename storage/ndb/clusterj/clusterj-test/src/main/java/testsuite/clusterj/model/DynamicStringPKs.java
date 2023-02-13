/*
 *  Copyright (c) 2023, Oracle and/or its affiliates.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License, version 2.0,
 *  as published by the Free Software Foundation.
 *
 *  This program is also distributed with certain software (including
 *  but not limited to OpenSSL) that is licensed under separate terms,
 *  as designated in a particular file or component or in included license
 *  documentation.  The authors of MySQL hereby grant you an additional
 *  permission to link the program and your derivative works with the
 *  separately licensed software that they have included with MySQL.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License, version 2.0, for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

package testsuite.clusterj.model;

import com.mysql.clusterj.DynamicObject;

/** Schema
 *
drop table if exists dynamicstringpks;
create table dynamicstringpks (
 key1 varchar(85) collate utf8_unicode_ci not null,
 key2 varchar(85) collate utf8_unicode_ci not null,
 key3 varchar(85) collate utf8_unicode_ci not null,
 key4 bigint not null,
 key5 varchar(85) collate utf8_unicode_ci not null,
 key6 bigint not null,
 key7 varchar(85) collate utf8_unicode_ci not null,
 number int not null,
 name varchar(10) not null,
 primary key (key1, key2, key3, key4, key5, key6, key7)
) ENGINE=ndbcluster DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci;;

 */
public abstract class DynamicStringPKs extends DynamicObject {

    public String getKey1() {
        return (String)get(0);
    }
    public String getKey2() {
        return (String)get(1);
    }
    public String getKey3() {
        return (String)get(2);
    }
    public Integer getKey4() {
        return (Integer)get(3);
    }
    public String getKey5() {
        return (String)get(4);
    }
    public Integer getKey6() {
        return (Integer)get(5);
    }
    public String getKey7() {
        return (String)get(6);
    }
    public void setKey1(String str) {
        set(0, str);
    }
    public void setKey2(String str) {
        set(1, str);
    }
    public void setKey3(String str) {
        set(2, str);
    }
    public void setKey4(int val) {
        set(3, val);
    }
    public void setKey5(String str) {
        set(4, str);
    }
    public void setKey6(int val) {
        set(5, val);
    }
    public void setKey7(String str) {
        set(6, str);
    }

    public int getNumber() {
        return (Integer)get(7);
    }
    public void setNumber(int value) {
        set(7, value);
    }

    public String getName() {
        return (String)get(8);
    }
    public void setName(String value) {
        set(8, value);
    }
}
