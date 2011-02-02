/*
   Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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

package com.mysql.clusterj.jpatest.model;

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
    // @Basic(fetch=EAGER) is the default mapping annotation
    // for primitive types, wrappers, String...

    // for serializable
    static private final long serialVersionUID = 4644052765183330073L;

    @javax.persistence.Id
    private int id;
    
    private int cint;
    
    private long clong;
    
    private float cfloat;
    
    private double cdouble;
    
    private String cstring;

    private byte[] bytes;

    @javax.persistence.ManyToOne
    @javax.persistence.Column(name="a_id")
    @org.apache.openjpa.persistence.jdbc.Index(name="FK_a_id")
    private A a;
    
    public B0() {
    }
    
    static public B0 create(int id) {
        B0 o = new B0();
        o.setId(id);
        o.setCint((int)id);
        o.setClong((long)id);
        o.setCfloat((float)id);
        o.setCdouble((double)id);
        o.setCstring(String.valueOf(id));
        return o;
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
    
    public void setBytes(byte[] value) {
        this.bytes = value;
    }
    
    public byte[] getBytes() {
        return bytes;
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
    
    public String getCstring() {
        return cstring;
    }
    
    public void setCstring(String cstring) {
        this.cstring = cstring;
    }
    
    public A getA() {
        return a;
    }
    
    public void setA(A a) {
        this.a = a;
    }
    
    private void writeObject(ObjectOutputStream out) throws IOException {
        out.defaultWriteObject();
    }

    private void readObject(ObjectInputStream in) throws IOException, ClassNotFoundException {
        in.defaultReadObject();
    }

    static public class Oid implements Serializable {
        
        public int id;
        
        public Oid() {
        }
        
        @Override
        public boolean equals(Object obj) {
            if (obj == null || !this.getClass().equals(obj.getClass()))
                return false;
            Oid o = (Oid)obj;
            return (this.id == o.id);
        }
        
        @Override
        public int hashCode() {
            return id;
        }        
    }   

    @Override
    public String toString() {
        StringBuffer buffer = new StringBuffer();
        buffer.append("B0 id: ");
        buffer.append(id);
        buffer.append("; cint: ");
        buffer.append(cint);
        buffer.append("; clong: ");
        buffer.append(clong);
        buffer.append("; cfloat: ");
        buffer.append(cfloat);
        buffer.append("; cdouble: ");
        buffer.append(cdouble);
        buffer.append("; cstring: ");
        buffer.append(cstring);
        buffer.append("; a: ");
        buffer.append(a);
        return buffer.toString();
    }

}
