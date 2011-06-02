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

import com.mysql.clusterj.annotation.Column;
import com.mysql.clusterj.annotation.PersistenceCapable;

/**
 * An Entity test interface for ClusterJ.
 */
@PersistenceCapable(table="b0")
public interface IB0 {

    public int getId();
    public void setId(int id);

    public int getCint();
    public void setCint(int cint);

    public long getClong();
    public void setClong(long clong);

    public float getCfloat();
    public void setCfloat(float cfloat);

    public double getCdouble();
    public void setCdouble(double cdouble);

/*
   // XXX NPE despite allowsNull="true" annotation, must set to non-null
   o.setCvarbinary_def(new byte[0]);

     [java] SEVERE: Error executing getInsertOperation on table b0.
     [java] caught com.mysql.clusterj.ClusterJException: Error executing getInsertOperation on table b0. Caused by java.lang.NullPointerException:null
     [java] com.mysql.clusterj.ClusterJException: Error executing getInsertOperation on table b0. Caused by java.lang.NullPointerException:null
     [java] at com.mysql.clusterj.core.SessionImpl.insert(SessionImpl.java:283)
*/
    // XXX @javax.persistence.Basic(fetch=javax.persistence.FetchType.LAZY)
    @Column(name="cvarbinary_def",allowsNull="true")
    public byte[] getCvarbinary_def();
    public void setCvarbinary_def(byte[] cvarbinary_def);

    // XXX @javax.persistence.Basic(fetch=javax.persistence.FetchType.LAZY)
    @Column(name="cvarchar_def")
    public String getCvarchar_def();
    public void setCvarchar_def(String cvarchar_def);

    @Column(name="a_id") // or change name of attr
    public int getAid();
    public void setAid(int aid);
}
