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
import com.mysql.clusterj.annotation.Index;
import com.mysql.clusterj.annotation.PersistenceCapable;
import com.mysql.clusterj.annotation.PrimaryKey;

/** OrderLine represents a line item in an order
 * 
CREATE TABLE b0 (
        id              INT NOT NULL,   // id generated number
        cint            INT,            // line item number
        clong           BIGINT,         // quantity
        cfloat          FLOAT,          // unit price
        cdouble         DOUBLE,         // value of this line item
        a_id            INT,            // order number associated with this order line
        cstring         VARCHAR(100),   // description
        cvarchar_ascii  VARCHAR(100) CHARACTER SET ASCII,
        ctext_ascii     TEXT(100) CHARACTER SET ASCII,
        cvarchar_ucs2   VARCHAR(100) CHARACTER SET ASCII,
        ctext_ucs2      TEXT(100) CHARACTER SET ASCII,
        KEY FK_a_id (a_id),
        CONSTRAINT PK_B0_0 PRIMARY KEY (id)
) ENGINE=ndbcluster DEFAULT CHARSET=latin1;
 *
 */
@PersistenceCapable(table="b0")
public interface OrderLine extends IdBase {
    
    @PrimaryKey
    int getId();
    void setId(int id);

    @Column(name="cint")
    int getLineNumber();
    void setLineNumber(int lineNumber);

    @Column(name="clong")
    long getQuantity();
    void setQuantity(long quantity);

    @Column(name="cfloat")
    float getUnitPrice();
    void setUnitPrice(float unitPrice);

    @Column(name="cdouble")
    double getTotalValue();
    void setTotalValue(double totalValue);

    @Column(name="a_id")
    @Index(name="FK_a_id")
    int getOrderId();
    void setOrderId(int orderId);

}
