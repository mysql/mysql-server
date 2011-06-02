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

import com.mysql.clusterj.annotation.PersistenceCapable;

/**
 * An Entity test interface for use with ClusterJ.
 */
@PersistenceCapable(table="a")
public interface IA {
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
}
