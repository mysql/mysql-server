/* -*- mode: java; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=4:tabstop=4:smarttab:
 *
 *  Copyright 2010 Sun Microsystems, Inc.
 *   All rights reserved. Use is subject to license terms.
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
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

package com.mysql.cluster.crund;

import java.io.Serializable;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.IOException;

/**
 * An Entity test class.
 */
@javax.persistence.Entity
@javax.persistence.Table(name="b0")
public class B0 implements Serializable {
    // @javax.persistence.Basic  default for primitive types, wrappers, String
    //   Defaults:
    //   fetch=EAGER
    //   optional=true  (value of all nonprimitive field/property may be null)
    // @javax.persistence.Lob specifies for the @Basic mapping that a
    //   persistent property or field should be persisted as a large object
    //   to a database-supported large object type.  A Lob may be either a
    //   binary or character type.
    // @javax.persistence.Transient specify a field or property of an entity
    //   that is not persistent
    // @javax.persistence.ManyToOne for a single-valued association to another
    //   entity class that has many-to-one multiplicity. Defaults:
    //   cascade=new javax.persistence.CascadeType[]{}
    //   fetch=javax.persistence.FetchType.EAGER
    //   optional=true  (value of all field/property may be null)
    // @javax.persistence.ManyToOne for a many-valued association to another
    //   entity class that has one-to-many multiplicity. Defaults:
    //   [mappedBy=field_name (relationship is bidirectional)]
    //   cascade=new javax.persistence.CascadeType[]{}
    //   fetch=javax.persistence.FetchType.LAZY
    //   optional=true  (value of all field/property may be null)

    @javax.persistence.Id
    private int id;

    private int cint;

    private long clong;

    private float cfloat;

    private double cdouble;

    @javax.persistence.Basic(fetch=javax.persistence.FetchType.LAZY)
    private byte[] cvarbinary_def;

    @javax.persistence.Basic(fetch=javax.persistence.FetchType.LAZY)
    private String cvarchar_def;

    @javax.persistence.ManyToOne(fetch=javax.persistence.FetchType.LAZY)
    @javax.persistence.Column(name="a_id")
    @org.apache.openjpa.persistence.jdbc.Index(name="I_B0_FK")
    private A a;

    public B0() {
    }

    public int getId() {
        return id;
    }

    public void setId(int id) {
        this.id = id;
    }

    public int getCint() {
        return cint;
    }

    public void setCint(int cint) {
        this.cint = cint;
    }

    public long getClong() {
        return clong;
    }

    public void setClong(long clong) {
        this.clong = clong;
    }

    public float getCfloat() {
        return cfloat;
    }

    public void setCfloat(float cfloat) {
        this.cfloat = cfloat;
    }

    public double getCdouble() {
        return cdouble;
    }

    public void setCdouble(double cdouble) {
        this.cdouble = cdouble;
    }

    public byte[] getCvarbinary_def() {
        return cvarbinary_def;
    }

    public void setCvarbinary_def(byte[] cvarbinary_def) {
        this.cvarbinary_def = cvarbinary_def;
    }

    public String getCvarchar_def() {
        return cvarchar_def;
    }

    public void setCvarchar_def(String cvarchar_def) {
        this.cvarchar_def = cvarchar_def;
    }

    public A getA() {
        return a;
    }

    public void setA(A a) {
        this.a = a;
    }

    // while implementing Serializable...
    static private final long serialVersionUID = 4644052765183330073L;

    // while implementing Serializable...
    private void readObject(ObjectInputStream in) throws IOException, ClassNotFoundException {
        in.defaultReadObject();
    }

    // while implementing Serializable...
    private void writeObject(ObjectOutputStream out) throws IOException {
        out.defaultWriteObject();
    }

/*
    static public class Oid implements Serializable {

        public int id;

        public Oid() {
        }

        public boolean equals(Object obj) {
            if (obj == null || !this.getClass().equals(obj.getClass()))
                return false;
            Oid o = (Oid)obj;
            return (this.id == o.id);
        }

        public int hashCode() {
            return id;
        }
    }
*/
}
