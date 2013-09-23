/*
Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

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

package testsuite.clusterj;

import com.mysql.clusterj.annotation.PersistenceCapable;

import testsuite.clusterj.model.IdBase;

public class QueryHashPKScanTest extends AbstractQueryTest {

    @Override
    public Class<?> getInstanceType() {
        return HashPK.class;
    }

    @Override
    void createInstances(int number) {
        for (int i = 0; i < 10; ++i) {
            HashPK instance = session.newInstance(HashPK.class);
            instance.setId(i);
            instance.setName("Instance " + i);
            instances.add(instance);
        }
    }

    /** Test all single-predicate queries using HashPK, which has a
     * hash only primary key.
     *
     */
    public void testHashPKScan() {
        equalQuery("id", "PRIMARY", 8, 8);
        greaterEqualQuery("id", "none", 7, 7, 8, 9);
        greaterThanQuery("id", "none", 6, 7, 8, 9);
        lessEqualQuery("id", "none", 4, 4, 3, 2, 1, 0);
        lessThanQuery("id", "none", 4, 3, 2, 1, 0);
        betweenQuery("id", "none", 4, 6, 4, 5, 6);
        greaterEqualAndLessEqualQuery("id", "none", 4, 6, 4, 5, 6);
        greaterThanAndLessEqualQuery("id", "none", 4, 6, 5, 6);
        greaterEqualAndLessThanQuery("id", "none", 4, 6, 4, 5);
        greaterThanAndLessThanQuery("id", "none", 4, 6, 5);

        failOnError();
    }

    /**
CREATE TABLE IF NOT EXISTS hashpk (
  id int not null,
  name varchar(30),
    CONSTRAINT PK_hashpk PRIMARY KEY (id) USING HASH  
) ENGINE = ndbcluster;
     */
    @PersistenceCapable(table="hashpk")
    public interface HashPK extends IdBase {
        int getId();
        void setId(int value);
        String getName();
        void setName(String value);
    }
}
