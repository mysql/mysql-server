/*
   Copyright (c) 2010, 2022, Oracle and/or its affiliates.
   Use is subject to license terms.

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
import com.mysql.clusterj.annotation.Index;
import com.mysql.clusterj.annotation.NullValue;
import com.mysql.clusterj.annotation.PersistenceCapable;
import com.mysql.clusterj.annotation.Persistent;
import com.mysql.clusterj.annotation.PrimaryKey;

@PersistenceCapable(table="dn2id")
@PrimaryKey(columns={
    @Column(name="a0"),
    @Column(name="a1"),
    @Column(name="a2"),
    @Column(name="a3"),
    @Column(name="a4"),
    @Column(name="a5"),
    @Column(name="a6"),
    @Column(name="a7"),
    @Column(name="a8"),
    @Column(name="a9"),
    @Column(name="a10"),
    @Column(name="a11"),
    @Column(name="a12"),
    @Column(name="a13"),
    @Column(name="a14"),
    @Column(name="a15")})
public interface Dn2id {
    
    @Index(name="idx_unique_hash_eid")
    long getEid();
    void setEid(long id);

    @Persistent(nullValue=NullValue.EXCEPTION)
    @Column(name="object_classes")
    String getObjectClasses();
    void setObjectClasses(String name);

    @Persistent(nullValue=NullValue.DEFAULT)
    @Column(name="x_object_classes")
    String getXObjectClasses();
    void setXObjectClasses(String name);

    @Column(name="a0")
    String getA0();
    void setA0(String value);

    @Column(name="a1")
    String getA1();
    void setA1(String value);

    @Column(name="a2")
    String getA2();
    void setA2(String value);

    @Column(name="a3")
    String getA3();
    void setA3(String value);

    @Column(name="a4")
    String getA4();
    void setA4(String value);

    @Column(name="a5")
    String getA5();
    void setA5(String value);

    @Column(name="a6")
    String getA6();
    void setA6(String value);

    @Column(name="a7")
    String getA7();
    void setA7(String value);

    @Column(name="a8")
    String getA8();
    void setA8(String value);

    @Column(name="a9")
    String getA9();
    void setA9(String value);

    @Column(name="a10")
    String getA10();
    void setA10(String value);

    @Column(name="a11")
    String getA11();
    void setA11(String value);

    @Column(name="a12")
    String getA12();
    void setA12(String value);

    @Column(name="a13")
    String getA13();
    void setA13(String value);

    @Column(name="a14")
    String getA14();
    void setA14(String value);

    @Column(name="a15")
    String getA15();
    void setA15(String value);

}
