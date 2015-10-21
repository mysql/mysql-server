/*
   Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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

import com.mysql.clusterj.DynamicObject;
import com.mysql.clusterj.ClusterJUserException;
import com.mysql.clusterj.Query;

import com.mysql.clusterj.query.Predicate;
import com.mysql.clusterj.query.PredicateOperand;
import com.mysql.clusterj.query.QueryBuilder;
import com.mysql.clusterj.query.QueryDomainType;
import java.util.ArrayList;
import java.util.List;
import testsuite.clusterj.model.Employee;

/** Test session.release(Object)
 * 
 */
public class ReleaseTest extends AbstractClusterJModelTest {

    public void test() {
        testReleaseStatic();
        testReleaseDynamic();
        testReleaseArray();
        testReleaseIterable();
        testReleaseFoundStatic();
        testReleaseFoundDynamic();
        testReleaseQueryStatic();
        testReleaseQueryDynamic();
        testReleaseNonPersistent();
        failOnError();
    }
    Employee emp0;
    Employee emp1;
    Employee emp2;
    Employee emp3;
    Employee emp4;
    Employee emp5;
    Employee emp6;
    Employee emp7;

    Employee  setEmployeeFields(Employee emp) {
        int id = emp.getId();
        emp.setName("Employee " + id);
        emp.setMagic(id);
        emp.setAge(id);
        return emp;
    }
    
    @Override
    public void localSetUp() {
        createSessionFactory();
        session = sessionFactory.getSession();
        session.deletePersistentAll(Employee.class);
        emp0 =  setEmployeeFields(session.newInstance(Employee.class, 0));
        emp1 =  setEmployeeFields(session.newInstance(DynamicEmployee.class, 1));
        emp2 =  setEmployeeFields(session.newInstance(Employee.class, 2));
        emp3 =  setEmployeeFields(session.newInstance(DynamicEmployee.class, 3));
        emp4 =  setEmployeeFields(session.newInstance(Employee.class, 4));
        emp5 =  setEmployeeFields(session.newInstance(DynamicEmployee.class, 5));
        emp6 =  setEmployeeFields(session.newInstance(Employee.class, 6));
        emp7 =  setEmployeeFields(session.newInstance(DynamicEmployee.class, 7));
        // make some instances persistent
        session.makePersistent(new Employee[] {emp0, emp1, emp2, emp5, emp6, emp7});
        // emp6 and emp7 are going to be found later
        emp6 = null;
        emp7 = null;
        addTearDownClasses(Employee.class, DynamicEmployee.class);
    }

    /** Test that getting a persistent field of a released object throws the proper exception
     */
    void testGetId(Employee emp, String where) {
        // access getId of emp and verify that it throws the proper exception
        try {
            emp.getId();
            error("getId for " + where + " failed to throw ClusterJUserException");
        } catch (ClusterJUserException ex) {
            // good catch: Cannot access object after release
            String message = ex.getMessage();
            errorIfNotEqual("wrong error message for " + where + ":" + message, true, message.contains("elease")); 
            // make sure that another release does not throw an exception
            try {
            session.release(emp);
            } catch (Throwable t) {
                error("release for " + where + " incorrectly threw exception " + t.getMessage());
            }
        } catch (Throwable t) {
            t.printStackTrace();
            error("getId for " + where + " threw wrong exception " + t.getMessage());
        }
    }

    /** Test that setting a persistent field of a released object throws the proper exception
     */
    void testSetId(Employee emp, String where) {
        // access getId of emp and verify that it throws the proper exception
        try {
            emp.setId(0);
            error("setId for " + where + " failed to throw ClusterJUserException");
        } catch (ClusterJUserException ex) {
            // good catch: Cannot access object after release
            String message = ex.getMessage();
            errorIfNotEqual("wrong error message for " + where + ":" + message, true, message.contains("elease")); 
            // make sure that another release does not throw an exception
            try {
            session.release(emp);
            } catch (Throwable t) {
                error("release for " + where + " incorrectly threw exception " + t.getMessage());
            }
        } catch (Throwable t) {
            t.printStackTrace();
            error("setId for " + where + " threw wrong exception " + t.getMessage());
        }
    }

    /** Test that makePersistent of a released object throws the proper exception
     */
    void testMakePersistent(Employee emp, String where) {
        // makePersistent emp and verify that it throws the proper exception
        try {
            session.makePersistent(emp);
            error("makePersistent for " + where + " failed to throw ClusterJUserException");
        } catch (ClusterJUserException ex) {
            // good catch: Cannot access object after release
            String message = ex.getMessage();
            errorIfNotEqual("wrong error message for " + where + ":" + message, true, message.contains("elease")); 
            // make sure that another release does not throw an exception
            try {
            session.release(emp);
            } catch (Throwable t) {
                error("release for " + where + " incorrectly threw exception " + t.getMessage());
            }
        } catch (Throwable t) {
            error("makePersistent for " + where + " threw wrong exception " + t.getMessage());
        }
    }

    /** Test that SavePersistent of a released object throws the proper exception
     */
    void testSavePersistent(Employee emp, String where) {
        // savePersistent emp and verify that it throws the proper exception
        try {
            session.savePersistent(emp);
            error("savePersistent for " + where + " failed to throw ClusterJUserException");
        } catch (ClusterJUserException ex) {
            // good catch: Cannot access object after release
            String message = ex.getMessage();
            errorIfNotEqual("wrong error message: " + message, true, message.contains("elease")); 
            // make sure that another release does not throw an exception
            try {
            session.release(emp);
            } catch (Throwable t) {
                error("release for " + where + " incorrectly threw exception " + t.getMessage());
            }
        } catch (Throwable t) {
            error("savePersistent for " + where + " threw wrong exception " + t.getMessage());
        }
    }

    /** Test that updatePersistent of a released object throws the proper exception
     */
    void testUpdatePersistent(Employee emp, String where) {
        // updatePersistent emp and verify that it throws the proper exception
        try {
            session.updatePersistent(emp);
            error("getId for " + where + " failed to throw ClusterJUserException");
        } catch (ClusterJUserException ex) {
            // good catch: Cannot access object after release
            String message = ex.getMessage();
            errorIfNotEqual("wrong error message: " + message, true, message.contains("elease")); 
            // make sure that another release does not throw an exception
            try {
            session.release(emp);
            } catch (Throwable t) {
                error("release for " + where + " incorrectly threw exception " + t.getMessage());
            }
        } catch (Throwable t) {
            error("updatePersistent for " + where + " threw wrong exception " + t.getMessage());
        }
    }

    /** Test that deletePersistent of a released object throws the proper exception
     */
    void testDeletePersistent(Employee emp, String where) {
        // deletePersistent emp and verify that it throws the proper exception
        try {
            session.deletePersistent(emp);
            error("deletePersistent for " + where + " failed to throw ClusterJUserException");
        } catch (ClusterJUserException ex) {
            // good catch: Cannot access object after release
            String message = ex.getMessage();
            errorIfNotEqual("wrong error message: " + message, true, message.contains("elease")); 
            try {
            session.release(emp);
            } catch (Throwable t) {
                error("release for " + where + " incorrectly threw exception " + t.getMessage());
            }
        } catch (Throwable t) {
            error("deletePersistent for " + where + " threw wrong exception " + t.getMessage());
        }
    }

    /** Test that release of non-persistent types throws the proper exception
     */
    void testNonPersistentRelease(Object object, String where) {
        try {
            session.release(object);
            error("release for " +  where + " failed to throw ClusterJUserException");
        } catch (ClusterJUserException ex) {
            // good catch: release argument must be a persistent type
            String message = ex.getMessage();
            errorIfNotEqual("wrong error message: " + message, true, message.contains("elease")); 
        } catch (Throwable t) {
            error("release for Integer threw the wrong exception: " + t.getMessage());
        }        
    }

    /** Test releasing resources for static class Employee. */
    protected void testReleaseStatic() {
        // release employee 0 and make sure that accessing it throws an exception
        Employee result = session.release(emp0);
        if (result != emp0) {
            error("for static object, result of release does not equal parameter.");
        }
        testGetId(emp0, "static object");
        testSetId(emp0, "static object");
        testMakePersistent(emp0, "static object");
        testSavePersistent(emp0, "static object");
        testUpdatePersistent(emp0, "static object");
        testDeletePersistent(emp0, "static object");
    }

    protected void testReleaseDynamic() {
        // release employee 1 and make sure that accessing it throws an exception
        Employee result = session.release(emp1);
        if (result != emp1) {
            error("for dynamic object, result of release does not equal parameter.");
        }
        testGetId(emp1, "dynamic object");
        testSetId(emp1, "dynamic object");
        testMakePersistent(emp1, "dynamic object");
        testSavePersistent(emp1, "dynamic object");
        testUpdatePersistent(emp1, "dynamic object");
        testDeletePersistent(emp1, "dynamic object");
    }

    /** Test releasing resources for Iterable<Employee>. */
    protected void testReleaseIterable() {
        // release employee 2 and 3 list and make sure that accessing them throws an exception
        List<Object> list = new ArrayList<Object>();
        list.add(emp2);
        list.add(emp3);
        List<Object> result = null;
        try {
            result = session.release(list);
        } catch (Throwable t) {
            t.printStackTrace();
        }
        if (list != result) error("session.release list did not return argument");
        testGetId(emp2, "static object in list");
        testSetId(emp2, "static object in list");
        testMakePersistent(emp2, "static object in list");
        testSavePersistent(emp2, "static object in list");
        testUpdatePersistent(emp2, "static object in list");
        testDeletePersistent(emp2, "static object in list");
        testGetId(emp3, "dynamic object in list");
        testSetId(emp3, "dynamic object in list");
        testMakePersistent(emp3, "dynamic object in list");
        testSavePersistent(emp3, "dynamic object in list");
        testUpdatePersistent(emp3, "dynamic object in list");
        testDeletePersistent(emp3, "dynamic object in list");
    }

    /** Test releasing resources for Employee[]. */
    protected void testReleaseArray() {
        // release employee array 4 and 5 and make sure that accessing them throws an exception
        Employee[] array = new Employee[] {emp4, emp5};
        Employee[] result = null;
        try {
            result = session.release(array);
        } catch (Throwable t) {
            t.printStackTrace();
        }
        if (array != result) error("session.release array did not return argument");
        testGetId(emp4, "static object in array");
        testSetId(emp4, "static object in array");
        testMakePersistent(emp4, "static object in array");
        testSavePersistent(emp4, "static object in array");
        testUpdatePersistent(emp4, "static object in array");
        testDeletePersistent(emp4, "static object in array");
        testGetId(emp5, "dynamic object in array");
        testSetId(emp5, "dynamic object in array");
        testMakePersistent(emp5, "dynamic object in array");
        testSavePersistent(emp5, "dynamic object in array");
        testUpdatePersistent(emp5, "dynamic object in array");
        testDeletePersistent(emp5, "dynamic object in array");
    }

    /** Test releasing object of non-persistent type */
    protected void testReleaseNonPersistent() {
        testNonPersistentRelease(new Integer(1), "new Integer(1)");
        testNonPersistentRelease(new Integer[] {1, 2}, "new Integer[] {1, 2}");
    }

    /** Test releasing static object acquired via find */
    protected void testReleaseFoundStatic() {
        emp6 = session.find(Employee.class, 6);
        session.release(emp6);
        testGetId(emp6, "static found object");
        testSetId(emp6, "static found object");
        testMakePersistent(emp6, "static found object");
        testSavePersistent(emp6, "static found object");
        testUpdatePersistent(emp6, "static found object");
        testDeletePersistent(emp6, "static found object");
    }

    /** Test releasing dynamic object acquired via find */
    protected void testReleaseFoundDynamic() {
        emp7 = session.find(DynamicEmployee.class, 7);
        session.release(emp7);
        testGetId(emp7, "dynamic found object");
        testSetId(emp7, "dynamic found object");
        testMakePersistent(emp7, "dynamic found object");
        testSavePersistent(emp7, "dynamic found object");
        testUpdatePersistent(emp7, "dynamic found object");
        testDeletePersistent(emp7, "dynamic found object");
    }

    /** Test releasing static objects acquired via query */
    protected void testReleaseQueryStatic() {
        QueryBuilder builder = session.getQueryBuilder();
        QueryDomainType dobj = builder.createQueryDefinition(Employee.class);
        // parameter name
        PredicateOperand id_low = dobj.param("id_low");
        PredicateOperand id_high = dobj.param("id_high");
        // property name
        PredicateOperand column = dobj.get("id");
        // compare the column with the parameters
        Predicate compare = column.between(id_low, id_high);
        // set the where clause into the query
        dobj.where(compare);
        // create a query instance
        Query query = session.createQuery(dobj);

        // set the parameter values
        query.setParameter("id_low", 0);
        query.setParameter("id_high", 2);
        // get the results
        List<Employee> results = query.getResultList();
        errorIfNotEqual("Static query wrong result size", 3, results.size());
        // release the results
        session.release(results);
        for (Employee emp: results) {
            testGetId(emp, "static query object");
            testSetId(emp, "static query object");
            testMakePersistent(emp, "static query object");
            testSavePersistent(emp, "static query object");
            testUpdatePersistent(emp, "static query object");
            testDeletePersistent(emp, "static query object");
        }
    }

    /** Test releasing dynamic objects acquired via query */
    protected void testReleaseQueryDynamic() {
        QueryBuilder builder = session.getQueryBuilder();
        QueryDomainType dobj = builder.createQueryDefinition(DynamicEmployee.class);
        // parameter name
        PredicateOperand id_low = dobj.param("id_low");
        PredicateOperand id_high = dobj.param("id_high");
        // property name
        PredicateOperand column = dobj.get("id");
        // compare the column with the parameters
        Predicate compare = column.between(id_low, id_high);
        // set the where clause into the query
        dobj.where(compare);
        // create a query instance
        Query query = session.createQuery(dobj);

        // set the parameter values
        query.setParameter("id_low", 0);
        query.setParameter("id_high", 2);
        // get the results
        List<Employee> results = query.getResultList();
        errorIfNotEqual("Dynamic query wrong result size", 3, results.size());
        // release the results
        session.release(results);
        for (Employee emp: results) {
            testGetId(emp, "dynamic query object");
            testSetId(emp, "dynamic query object");
            testMakePersistent(emp, "dynamic query object");
            testSavePersistent(emp, "dynamic query object");
            testUpdatePersistent(emp, "dynamic query object");
            testDeletePersistent(emp, "dynamic query object");
        }
    }

    /** Dynamic Employee class */
    public static class DynamicEmployee  extends DynamicObject  implements Employee {
        
        @Override
        public String table() {
            return "t_basic";
        }
        public int getId() {
            return (Integer)get(0);
        }
        public void setId(int id) {
            set(0, id);
        }

        public String getName() {
            return (String)get(1);
        }
        public void setName(String name) {
            set(1, name);
        }

        public int getMagic() {
            return (Integer)get(2);
        }
        public void setMagic(int magic) {
            set(2, magic);
        }

        public Integer getAge() {
            return (Integer)get(3);
        }
        public void setAge(Integer age) {
            set(3, age);
        }
    }

}
