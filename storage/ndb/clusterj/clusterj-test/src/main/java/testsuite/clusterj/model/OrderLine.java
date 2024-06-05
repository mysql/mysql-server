/*
   Copyright (c) 2010, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

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
