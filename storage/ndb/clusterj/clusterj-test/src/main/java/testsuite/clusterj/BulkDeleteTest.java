/*
   Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

package testsuite.clusterj;

import com.mysql.clusterj.Query;
import com.mysql.clusterj.query.Predicate;
import com.mysql.clusterj.query.PredicateOperand;
import com.mysql.clusterj.query.QueryBuilder;
import com.mysql.clusterj.query.QueryDefinition;
import com.mysql.clusterj.query.QueryDomainType;
import com.mysql.clusterj.Session;
import java.util.ArrayList;
import java.util.List;
import testsuite.clusterj.model.Employee;

public class BulkDeleteTest extends AbstractClusterJModelTest {

    static private final int TotalRecords = 12000;
    static private final int QueryMaxId = 10505;
    static private final int InsertBatchSize = 100;
    static private final int DeleteBatchSize = 1000;
    static private final int InsertBatches = TotalRecords / InsertBatchSize;

    @Override
    public void localSetUp() {
        createSessionFactory();
        createSession();
        createEmployeeInstances(TotalRecords);
        addTearDownClasses(Employee.class);

        /* Delete whatever exists in the table. This uses session.removeAll(),
           so it will be limited by MaxNoOfConcurrentOperations. */
        removeAll(Employee.class);

        /* Insert in batches */
        int empId = 0;
        for(int i = 0 ; i < InsertBatches ; i++) {
            createBatch(empId);
            empId += InsertBatchSize;
        }
    }

    private void createBatch(int start) {
        List<Employee> batch = new ArrayList<Employee>();
        for(int i = 0 ; i < InsertBatchSize ; i++)
          batch.add(employees.get(start +i));
        tx.begin();
        session.makePersistentAll(batch);
        tx.commit();
    }

    public void test() {
        QueryBuilder builder = session.getQueryBuilder();
        QueryDomainType<Employee> defn = builder.createQueryDefinition(Employee.class);
        PredicateOperand propertyId = defn.get("id");
        PredicateOperand maximum = defn.param("maxId");
        defn.where(propertyId.lessThan(maximum));

        /* Delete in batches */
        Query<Employee> query = session.createQuery(defn);
        query.setParameter("maxId", QueryMaxId);
        query.setLimits(0, DeleteBatchSize);
        int result = 0;
        do {
            result = query.deletePersistentAll();
            System.out.println("Batch result: " + result);
        } while(result == DeleteBatchSize);
    }
}
