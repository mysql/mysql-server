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

package com.mysql.clusterj.jpatest.model;

import java.io.Serializable;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.IOException;
import java.util.Collection;
import java.util.HashSet;

/**
 * An Entity test class.
 */
@javax.persistence.Entity
@javax.persistence.Table(name="a")
public class A implements Serializable {
    // @Basic(fetch=EAGER) is the default mapping annotation
    // for primitive types, wrappers, String...

    // required for serialization
    static private final long serialVersionUID = -3359921162347129079L;

    @javax.persistence.Id
    private int id;

    private int cint;
    
    private long clong;
    
    private float cfloat;
    
    private double cdouble;
    
    private String cstring;
    
    @javax.persistence.OneToMany(mappedBy="a")
    private Collection<B0> b0s = new HashSet<B0>();
    
    public A() {
    }

    // XXX ....
    static public A create(int id) {
        A o = new A();
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
    
    public Collection<B0> getB0s() {
        return b0s;
    }
    
    public void setB0s(Collection<B0> b0s) {
        this.b0s = b0s;
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
        buffer.append("A id: ");
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
        buffer.append("; collection<b0>(");
        int size = b0s.size();
        buffer.append(size);
        buffer.append("): [");
        String separator = "";
        for (B0 b: b0s) {
            buffer.append(separator);
            buffer.append (b.getId());
            separator = "; ";
        }
        buffer.append("]");
        return buffer.toString();
    }

}

