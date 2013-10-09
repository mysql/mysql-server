/*
 *  Copyright (c) 2011, 2012, Oracle and/or its affiliates. All rights reserved.
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
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

package jdbctest;

import java.math.BigDecimal;

public class DecimalTypesTest extends testsuite.clusterj.DecimalTypesTest {

    /** Test all DecimalTypes columns.
drop table if exists decimaltypes;
create table decimaltypes (
 id int not null primary key,

 decimal_null_hash decimal(10,5),
 decimal_null_btree decimal(10,5),
 decimal_null_both decimal(10,5),
 decimal_null_none decimal(10,5)

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

create unique index idx_decimal_null_hash using hash on decimaltypes(decimal_null_hash);
create index idx_decimal_null_btree on decimaltypes(decimal_null_btree);
create unique index idx_decimal_null_both on decimaltypes(decimal_null_both);

     */

    /** One of two tests in the superclass that we don't want to run */
    @Override
    public void testWriteJDBCReadNDB() {
    }

    /** One of two tests in the superclass that we don't want to run */
    @Override
    public void testWriteNDBReadJDBC() {
   }

    /** The test we want */
    public void testWriteJDBCReadJDBC() {
        writeJDBCreadJDBC();
        failOnError();
    }

    /** Query tests */
    public void testQuery() {
        generateInstances(getColumnDescriptors());
        writeToJDBC(columnDescriptors, instances);
        queryAndVerifyResults("id greaterThan", columnDescriptors,
                "id > ?", new Object[] {5}, 6, 7, 8, 9);
        queryAndVerifyResults("id greaterEqual", columnDescriptors,
                "id >= ?", new Object[] {5}, 5, 6, 7, 8, 9);
        queryAndVerifyResults("id lessThan", columnDescriptors,
                "id < ?", new Object[] {3}, 0, 1, 2);
        queryAndVerifyResults("id lessEqual", columnDescriptors,
                "id <= ?", new Object[] {3}, 0, 1, 2, 3);

        queryAndVerifyResults("decimal_null_btree equal", columnDescriptors,
                "decimal_null_btree = ?", new BigDecimal[] {BigDecimal.valueOf(3.00001)}, 3);
        queryAndVerifyResults("decimal_null_btree lessEqual", columnDescriptors,
                "decimal_null_btree <= ?", new BigDecimal[] {BigDecimal.valueOf(3.00001)}, 0, 1, 2, 3);
        queryAndVerifyResults("decimal_null_btree lessThan", columnDescriptors,
                "decimal_null_btree < ?", new BigDecimal[] {BigDecimal.valueOf(3.00001)}, 0, 1, 2);
        queryAndVerifyResults("decimal_null_btree greaterEqual", columnDescriptors,
                "decimal_null_btree >= ?", new BigDecimal[] {BigDecimal.valueOf(6.00001)}, 6, 7, 8, 9);
        queryAndVerifyResults("decimal_null_btree greaterThan", columnDescriptors,
                "decimal_null_btree > ?", new BigDecimal[] {BigDecimal.valueOf(6.00001)}, 7, 8, 9);
        
        queryAndVerifyResults("decimal_null_hash equal", columnDescriptors,
                "decimal_null_hash = ?", new BigDecimal[] {BigDecimal.valueOf(3.0)}, 3);
        queryAndVerifyResults("decimal_null_hash lessEqual", columnDescriptors,
                "decimal_null_hash <= ?", new BigDecimal[] {BigDecimal.valueOf(3.0)}, 0, 1, 2, 3);
        queryAndVerifyResults("decimal_null_hash lessThan", columnDescriptors,
                "decimal_null_hash < ?", new BigDecimal[] {BigDecimal.valueOf(3.0)}, 0, 1, 2);
        queryAndVerifyResults("decimal_null_hash greaterEqual", columnDescriptors,
                "decimal_null_hash >= ?", new BigDecimal[] {BigDecimal.valueOf(6.0)}, 6, 7, 8, 9);
        queryAndVerifyResults("decimal_null_hash greaterThan", columnDescriptors,
                "decimal_null_hash > ?", new BigDecimal[] {BigDecimal.valueOf(6.0)}, 7, 8, 9);
        
        queryAndVerifyResults("decimal_null_both equal", columnDescriptors,
                "decimal_null_both = ?", new BigDecimal[] {BigDecimal.valueOf(3.00002)}, 3);
        queryAndVerifyResults("decimal_null_both lessEqual", columnDescriptors,
                "decimal_null_both <= ?", new BigDecimal[] {BigDecimal.valueOf(3.00002)}, 0, 1, 2, 3);
        queryAndVerifyResults("decimal_null_both lessThan", columnDescriptors,
                "decimal_null_both < ?", new BigDecimal[] {BigDecimal.valueOf(3.00002)}, 0, 1, 2);
        queryAndVerifyResults("decimal_null_both greaterEqual", columnDescriptors,
                "decimal_null_both >= ?", new BigDecimal[] {BigDecimal.valueOf(6.00002)}, 6, 7, 8, 9);
        queryAndVerifyResults("decimal_null_both greaterThan", columnDescriptors,
                "decimal_null_both > ?", new BigDecimal[] {BigDecimal.valueOf(6.00002)}, 7, 8, 9);
        
        queryAndVerifyResults("decimal_null_none equal", columnDescriptors,
                "decimal_null_none = ?", new BigDecimal[] {BigDecimal.valueOf(3.00003)}, 3);
        queryAndVerifyResults("decimal_null_none lessEqual", columnDescriptors,
                "decimal_null_none <= ?", new BigDecimal[] {BigDecimal.valueOf(3.00003)}, 0, 1, 2, 3);
        queryAndVerifyResults("decimal_null_none lessThan", columnDescriptors,
                "decimal_null_none < ?", new BigDecimal[] {BigDecimal.valueOf(3.00003)}, 0, 1, 2);
        queryAndVerifyResults("decimal_null_none greaterEqual", columnDescriptors,
                "decimal_null_none >= ?", new BigDecimal[] {BigDecimal.valueOf(6.00003)}, 6, 7, 8, 9);
        queryAndVerifyResults("decimal_null_none greaterThan", columnDescriptors,
                "decimal_null_none > ?", new BigDecimal[] {BigDecimal.valueOf(6.00003)}, 7, 8, 9);
       
        // query multiple terms
        queryAndVerifyResults("decimal_null_btree greaterThan and lessEqual", columnDescriptors,
                "decimal_null_btree > ? and decimal_null_btree <= ?",
                new BigDecimal[] {BigDecimal.valueOf(3.00001), BigDecimal.valueOf(6.00001)},
                4, 5, 6);
        queryAndVerifyResults("decimal_null_btree lessThan", columnDescriptors,
                "decimal_null_btree < ?",
                new BigDecimal[] {BigDecimal.valueOf(3.00001)},
                0, 1, 2);
        queryAndVerifyResults("decimal_null_btree greaterEqual", columnDescriptors,
                "decimal_null_btree >= ?",
                new BigDecimal[] {BigDecimal.valueOf(6.00001)},
                6, 7, 8, 9);
        queryAndVerifyResults("decimal_null_btree greaterThan", columnDescriptors,
                "decimal_null_btree > ?",
                new BigDecimal[] {BigDecimal.valueOf(6.00001)},
                7, 8, 9);

        queryAndVerifyResults("decimal_null_btree greaterThan and lessEqual and decimal_null_none lessThan " +
                "or decimal_null_none greaterThan", columnDescriptors,
                "decimal_null_btree > ? and decimal_null_btree <= ?" +
                " and (decimal_null_none < ? or decimal_null_none > ?)",
                new BigDecimal[] {BigDecimal.valueOf(0.00001), BigDecimal.valueOf(8.00001),
                        BigDecimal.valueOf(3.00003), BigDecimal.valueOf(6.00003)},
                        1, 2, 7, 8);
        
        queryAndVerifyResults("decimal_null_btree greaterThan and lessEqual" +
                " and not (decimal_null_none lessThan or decimal_null_none greaterThan)", columnDescriptors,
                "decimal_null_btree > ? and decimal_null_btree <= ?" +
                " and not (decimal_null_none < ? or decimal_null_none > ?)",
                new BigDecimal[] {BigDecimal.valueOf(0.00001), BigDecimal.valueOf(8.00001),
                        BigDecimal.valueOf(3.00003), BigDecimal.valueOf(6.00003)},
                        3, 4, 5, 6);
        
        queryAndVerifyResults("decimal_null_btree greaterThan and lessEqual" +
                " and (decimal_null_none between)", columnDescriptors,
                "decimal_null_btree > ? and decimal_null_btree <= ?" +
                " and decimal_null_none between ? and ?",
                new BigDecimal[] {BigDecimal.valueOf(0.00001), BigDecimal.valueOf(8.00001),
                        BigDecimal.valueOf(3.00003), BigDecimal.valueOf(6.00003)},
                        3, 4, 5, 6);
        
        queryAndVerifyResults("decimal_null_btree greaterThan and lessEqual" +
                " and not (decimal_null_none between)", columnDescriptors,
                "decimal_null_btree > ? and decimal_null_btree <= ?" +
                " and not decimal_null_none between ? and ?",
                new BigDecimal[] {BigDecimal.valueOf(0.00001), BigDecimal.valueOf(8.00001),
                        BigDecimal.valueOf(3.00003), BigDecimal.valueOf(6.00003)},
                        1, 2, 7, 8);
        
        queryAndVerifyResults("decimal_null_btree greaterThan and lessEqual" +
                " and not (decimal_null_none between)", columnDescriptors,
                "decimal_null_btree > ? and decimal_null_btree <= ?" +
                " and decimal_null_none not between ? and ?",
                new BigDecimal[] {BigDecimal.valueOf(0.00001), BigDecimal.valueOf(8.00001),
                        BigDecimal.valueOf(3.00003), BigDecimal.valueOf(6.00003)},
                        1, 2, 7, 8);
        
        queryAndVerifyResults("decimal_null_none between " +
                "and decimal_null_btree greaterThan and lessEqual", columnDescriptors,
                "decimal_null_none between ? and ?" +
                " and decimal_null_btree > ? and decimal_null_btree <= ?",
                new BigDecimal[] {
                        BigDecimal.valueOf(3.00003), BigDecimal.valueOf(6.00003),
                        BigDecimal.valueOf(0.00001), BigDecimal.valueOf(8.00001)},
                        3, 4, 5, 6);
        
        queryAndVerifyResults("not decimal_null_none between " +
                " and decimal_null_btree greaterThan and lessEqual", columnDescriptors,
                "not decimal_null_none between ? and ?" +
                " and decimal_null_btree > ? and decimal_null_btree <= ?",
                new BigDecimal[] {
                        BigDecimal.valueOf(3.00003), BigDecimal.valueOf(6.00003),
                        BigDecimal.valueOf(0.00001), BigDecimal.valueOf(8.00001)},
                        1, 2, 7, 8);
        
        queryAndVerifyResults("decimal_null_none not between " +
                "and decimal_null_btree greaterThan and lessEqual", columnDescriptors,
                "decimal_null_none not between ? and ?" +
                " and decimal_null_btree > ? and decimal_null_btree <= ?",
                new BigDecimal[] {
                        BigDecimal.valueOf(3.00003), BigDecimal.valueOf(6.00003),
                        BigDecimal.valueOf(0.00001), BigDecimal.valueOf(8.00001)},
                        1, 2, 7, 8);
        
        queryAndVerifyResults("decimal_null_none between", columnDescriptors,
                "decimal_null_none between ? and ?",
                new BigDecimal[] {BigDecimal.valueOf(4.00003), BigDecimal.valueOf(7.00003)},
                        4, 5, 6, 7);
        
        queryAndVerifyResults("decimal_null_none not between", columnDescriptors,
                "decimal_null_none not between ? and ?",
                new BigDecimal[] {BigDecimal.valueOf(4.00003), BigDecimal.valueOf(7.00003)},
                        0, 1, 2, 3, 8, 9);
        
        queryAndVerifyResults("not decimal_null_none between", columnDescriptors,
                "not decimal_null_none between ? and ?",
                new BigDecimal[] {BigDecimal.valueOf(4.00003), BigDecimal.valueOf(7.00003)},
                        0, 1, 2, 3, 8, 9);
        
        failOnError();
    }

}
