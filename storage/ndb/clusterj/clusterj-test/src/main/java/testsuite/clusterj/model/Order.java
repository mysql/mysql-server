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
import com.mysql.clusterj.annotation.PersistenceCapable;
import com.mysql.clusterj.annotation.PrimaryKey;

/** Order represents an order in a CRM system.
 * 
CREATE TABLE a (
        id              INT NOT NULL, // order id
        cint            INT,          // customer id
        clong           BIGINT,
        cfloat          FLOAT,
        cdouble         DOUBLE,       // total value
        cstring         VARCHAR(100), // description
        CONSTRAINT PK_A_0 PRIMARY KEY (id)
) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

 *
 */
@PersistenceCapable(table="a")
public interface Order extends IdBase {
    
    @PrimaryKey
    int getId();
    void setId(int id);

    @Column(name="cint")
    int getCustomerId();
    void setCustomerId(int name);

    @Column(name="cstring")
    String getDescription();
    void setDescription(String name);

    @Column(name="cdouble")
    double getValue();
    void setValue(double value);
}
