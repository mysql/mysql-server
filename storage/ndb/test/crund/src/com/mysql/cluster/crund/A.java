/* -*- mode: java; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=4:tabstop=4:smarttab:
 *
 *  Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved.
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
import java.util.Collection;

/**
 * An Entity test class.
 */
@javax.persistence.Entity
@javax.persistence.Table(name="a")
public class A implements Serializable {
    // see B0.java for persistence annotations

    @javax.persistence.Id
    private int id;

    private int cint;

    private long clong;

    private float cfloat;

    private double cdouble;

    @javax.persistence.OneToMany(mappedBy="a")
    private Collection<B0> b0s;

    public A() {
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

    public Collection<B0> getB0s() {
        return b0s;
    }

    public void setB0s(Collection<B0> b0s) {
        this.b0s = b0s;
    }

    // while implementing Serializable...
    static private final long serialVersionUID = -3359921162347129079L;

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

