/*
 *  Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License, version 2.0,
 *  as published by the Free Software Foundation.
 *
 *  This program is also distributed with certain software (including
 *  but not limited to OpenSSL) that is licensed under separate terms,
 *  as designated in a particular file or component or in included license
 *  documentation.  The authors of MySQL hereby grant you an additional
 *  permission to link the program and your derivative works with the
 *  separately licensed software that they have included with MySQL.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License, version 2.0, for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

package testsuite.clusterj;

import com.mysql.clusterj.ClusterJException;
import com.mysql.clusterj.ClusterJUserException;
import com.mysql.clusterj.DynamicObject;
import com.mysql.clusterj.Query;

import com.mysql.clusterj.annotation.Column;
import com.mysql.clusterj.annotation.PersistenceCapable;
import com.mysql.clusterj.annotation.Persistent;
import com.mysql.clusterj.annotation.Projection;

import com.mysql.clusterj.query.Predicate;
import com.mysql.clusterj.query.PredicateOperand;
import com.mysql.clusterj.query.QueryBuilder;
import com.mysql.clusterj.query.QueryDomainType;
import java.util.ArrayList;
import java.util.List;
import testsuite.clusterj.model.Employee;

/** Test Projections (domain classes that do not map all columns of a table)
 * interface ProjectHope maps partition_id and id only
 * 1. Persist
 * 2. Find
 * 3. Load
 * 4. Remove
 * 5. Update
 * 6. Save
 * 7. Release
 * 8. Scan on partition_id equal, range
 * 9. Scan on partition_id equal and id equal, range (pruned partition scan)
 * 10. Scan on id equal, range (table scan)
 * 11. Error: HopeLessNotAllPK not all PK columns mapped cannot do anything
 * 12. Error: HopeFulNotAllNonDefault not all non-default columns mapped cannot persist or save
 * 
 * drop table if exists `hope`;
 * create table `hope` (
 *   partition_id int NOT NULL, 
 *   id int NOT NULL, 
 *   int_col1 int NOT NULL, 
 *   int_col2 int NOT NULL, 
 *   str_col1 varchar(3000) default NULL,
 *   str_col2 varchar(3000) default NULL, 
 *   str_col3 varchar(3000) default NULL, 
 *   PRIMARY KEY (partition_id, id)
 *   ) ENGINE=ndbcluster partition by key (partition_id);
 */
public class ProjectionTest extends AbstractClusterJTest {

	protected Hope hope0;
	protected Hope hope1;
	protected Hope hope2;
	protected Hope hope3;
	
	@Override
    public void localSetUp() {
        createSessionFactory();
        session = sessionFactory.getSession();
        session.deletePersistentAll(Hope.class);
        hope0 =  setHopeFields(session.newInstance(Hope.class), 0, 0);
        hope1 =  setHopeFields(session.newInstance(Hope.class), 1, 1);
        hope2 =  setHopeFields(session.newInstance(Hope.class), 0, 2);
        hope3 =  setHopeFields(session.newInstance(Hope.class), 1, 3);
        // make some instances persistent
        session.persist(new Hope[] {hope0, hope1, hope2, hope3});
        addTearDownClasses(Hope.class);
    }

    protected Hope setHopeFields(Hope instance, int partition, int id) {
    	instance.setPartitionId(partition);
    	instance.setId(id);
    	instance.setInt_col1(id + 100);
    	instance.setInt_col2(id + 200);
    	instance.setStr_col1("Column1 " + (id + 100));
    	instance.setStr_col2("Column2 " + (id + 200));
    	instance.setStr_col3("Column3 " + (id + 300));
    	return instance;
    }

    protected ProjectHope setProjectHopeFields(ProjectHope instance, int partition, int id, int col1, int col2, String col3) {
    	instance.setPartitionId(partition);
    	errorIfNotEqual("setProjectHopeFields partition", partition, instance.getPartitionId());
    	instance.setId(id);
    	errorIfNotEqual("setProjectHopeFields id", id, instance.getId());
    	instance.setInt_col1(col1);
    	errorIfNotEqual("setProjectHopeFields int_col1", col1, instance.getInt_col1());
    	instance.setInt_col2(col2);
    	errorIfNotEqual("setProjectHopeFields int_col2", col2, instance.getInt_col2());
    	instance.setStr_col3(col3);
    	errorIfNotEqual("setProjectHopeFields str_col3", col3, instance.getStr_col3());
    	return instance;
    }

    protected HopeFulNotAllNonDefault setHopeFulNotAllNonDefaultFields(
    		HopeFulNotAllNonDefault instance, int partition, int id) {
    	instance.setPartitionId(partition);
    	instance.setId(id);
    	return instance;
    }

    public void test() {
    	testPersistProjectHope();
    	testPersistHopeful();
    	testPersistHopeless();
        testFindProjectHope();
        testFindHopeful();
        testFindHopeless();
        testLoadProjectHope();
        testLoadHopeful();
        testUpdateProjectHope();
        testSaveProjectHope();
        testDeleteByClassAndKeyProjectHope();
        testDeleteByInstanceProjectHope();
        testDeleteByClassAndKeyHopeful();
        testDeleteByInstanceHopeful();
        testScanPartitionKeyProjectHope();
        testScanPartitionKeyHopeful();
        testTableScanIdProjectHope();
        testTableScanIdHopeful();
      failOnError();
  }

    protected void testPersistProjectHope() {
        session = sessionFactory.getSession();
        try {
        	ProjectHope instance = session.newInstance(ProjectHope.class);
        	session.persist(setProjectHopeFields(instance, 10, 100, 1100, 1200, "Column 3 100"));
        	session.persist(setProjectHopeFields(instance, 10, 101, 1101, 1201, "Column 3 101"));
        	session.persist(setProjectHopeFields(instance, 10, 102, 1102, 1202, "Column 3 102"));
        	session.persist(setProjectHopeFields(instance, 10, 103, 1103, 1203, "Column 3 103"));
        } finally {
        	session.close();
        }
    }

    /** Hopeful does not map non-default column so it cannot persist */
    protected void testPersistHopeful() {
        session = sessionFactory.getSession();
        try {
        	HopeFulNotAllNonDefault instance = session.newInstance(HopeFulNotAllNonDefault.class);
        	setHopeFulNotAllNonDefaultFields(instance, 0, 10000);
        	session.persist(instance);
        	error("testPersistHopeful did not throw any exception.");
        } catch (Throwable t) {
        	checkException("testPersistHopeful", "Illegal null attribute", t);
        } finally {
        	session.close();
        }
    }

    /** Hopeless does not map all pk column so it cannot persist */
    protected void testPersistHopeless() {
        session = sessionFactory.getSession();
        try {
        	session.newInstance(HopeLessNotAllPK.class);
        	error("testPersistHopeless did not throw any exception.");
        } catch (Throwable t) {
        	checkException("testPersistHopeless", "no field mapped to the primary key", t);
        } finally {
        	session.close();
        }
    }

    protected void testSaveProjectHope() {
    	testSave("testSaveProjectHope", new Integer[] {10, 102}, 600);
    }

    protected void testFindProjectHope() {
    	ProjectHope instance = testFind("testFindProjectHope", ProjectHope.class, new Integer[] {0, 0});
    	if (instance != null) {
    		errorIfNotEqual("testFindProjectHope getPartitionId", 0, instance.getPartitionId());
    		errorIfNotEqual("testFindProjectHope getId", 0, instance.getId());
    		errorIfNotEqual("testFindProjectHope getInt_col1", 100, instance.getInt_col1());
    	}
    }

    protected void testFindHopeful() {
        HopeFulNotAllNonDefault instance =
    			testFind("testFindHopeful", HopeFulNotAllNonDefault.class, new Integer[] {1, 3});
    	if (instance != null) {
    		errorIfNotEqual("testFindHopeful getPartitionId", 1, instance.getPartitionId());
    		errorIfNotEqual("testFindHopeful getId", 3, instance.getId());
    	}
    }

    protected void testFindHopeless() {
    	try {
    		testFind("testFindHopeless", HopeLessNotAllPK.class, new Integer[] {0, 0});
        	error("testFindHopeless did not throw any exception.");
    	} catch (Throwable t) {
        	checkException("testFindHopeless", "no field mapped to the primary key", t);    		
    	}
    }

    protected void testLoadProjectHope() {
    	ProjectHope instance = testLoad("testLoadProjectHope", ProjectHope.class, new Integer[] {1, 3});
    	errorIfNotEqual("testLoadProjectHope getPartitionId", 1, instance.getPartitionId());
    	errorIfNotEqual("testLoadProjectHope getId", 3, instance.getId());
    	errorIfNotEqual("testLoadProjectHope getInt_col1", 103, instance.getInt_col1());
    	errorIfNotEqual("testLoadProjectHope getStr_col3", "Column3 303", instance.getStr_col3());
    }

    protected void testLoadHopeful() {
    	HopeFulNotAllNonDefault instance = testLoad("testLoadHopeful", HopeFulNotAllNonDefault.class, new Integer[] {1, 3});
    	errorIfNotEqual("testLoadProjectHope getPartitionId", 1, instance.getPartitionId());
    	errorIfNotEqual("testLoadProjectHope getId", 3, instance.getId());
    }

    protected void testUpdateProjectHope() {
    	testUpdate("testUpdateProjectHope", ProjectHope.class, new Integer[] {10, 102}, 700);
    }

    protected void testUpdateHopeful() {
    	testUpdate("testUpdateHopeful", HopeFulNotAllNonDefault.class, new Integer[] {10, 103}, 800);
    }

    protected void testDeleteByClassAndKeyProjectHope() {
    	testDeleteByClassAndKey("testDeleteByClassAndKeyProjectHope",
    			ProjectHope.class, new Integer[] {10, 100});
    }

    protected void testDeleteByInstanceProjectHope() {
    	testDeleteByInstance("testLoadProjectHope",
    			ProjectHope.class, new Integer[] {10, 101});
    }

    protected void testDeleteByClassAndKeyHopeful() {
    	testDeleteByClassAndKey("testDeleteByClassAndKeyHopeful",
    			HopeFulNotAllNonDefault.class, new Integer[] {10, 102});
    }

    protected void testDeleteByInstanceHopeful() {
    	testDeleteByInstance("testDeleteByInstanceHopeful",
    			HopeFulNotAllNonDefault.class, new Integer[] {10, 103});
    }

    protected void testScanPartitionKeyProjectHope() {
    	List<ProjectHope> result = testScanPartitionKey(ProjectHope.class, 0);
    	errorIfNotEqual("testScanPartitionKeyProjectHope", 2, result.size());
    }

    protected void testScanPartitionKeyHopeful() {
    	List<HopeFulNotAllNonDefault> result = testScanPartitionKey(HopeFulNotAllNonDefault.class, 1);
    	errorIfNotEqual("testScanPartitionKeyHopeful", 2, result.size());
    }

    protected void testTableScanIdProjectHope() {
    	testTableScanId(ProjectHope.class, 100);
    }

    protected void testTableScanIdHopeful() {
    	testTableScanId(HopeFulNotAllNonDefault.class, 101);
    }

    protected <T> T testFind(String where, Class<T> cls, Object key) {
    	try {
    		session = sessionFactory.getSession();
        	T instance = session.find(cls, key);
        	if (instance == null) {
        		error(where + " testFind: instance was null for " + cls.getName());
        	}
        	return instance;
    	} finally {
    		session.close();
    	}
    }

    protected <T> T testLoad(String where, Class<T> cls, Object key) {
    	try {
    		session = sessionFactory.getSession();
        	T instance = session.newInstance(cls, key);
        	session.currentTransaction().begin();
        	session.load(instance);
        	session.flush();
        	errorIfNotEqual(where + "testLoad", true, session.found(instance));
        	return instance;
    	} finally {
        	session.currentTransaction().commit();
    		session.close();
    	}
    }

    protected <T extends HopeFulNotAllNonDefault> HopeFulNotAllNonDefault testUpdate(
    		String where, Class<T> cls, Object key, int int_col1) {
    	try {
    		session = sessionFactory.getSession();
    		HopeFulNotAllNonDefault instance = session.find(cls, key);
        	instance.setInt_col1(int_col1);
        	session.updatePersistent(instance);
        	HopeFulNotAllNonDefault result = session.find(cls, key);
        	if (result == null) {
        		error(where + " testUpdate find returned null");
        		return null;
        	}
        	errorIfNotEqual(where + "testUpdate", int_col1, instance.getInt_col1());
        	return instance;
    	} finally {
    		session.close();
    	}
    }

    protected ProjectHope testSave(String where, Object key, int int_col1) {
    	try {
    		session = sessionFactory.getSession();
    		ProjectHope instance = session.newInstance(ProjectHope.class, key);
        	instance.setInt_col1(int_col1);
        	session.savePersistent(instance);
        	ProjectHope result = session.find(ProjectHope.class, key);
        	if (result == null) {
        		error(where + " testSave find returned null");
        		return null;
        	}
        	errorIfNotEqual(where + "testSave", true, session.found(instance));
        	errorIfNotEqual(where + "testSave", int_col1, instance.getInt_col1());
        	return instance;
    	} finally {
    		session.close();
    	}
    }

    protected <T> void testDeleteByClassAndKey(String where, Class<T> cls, Object key) {
    	try {
    		session = sessionFactory.getSession();
        	session.deletePersistent(cls, key);
    	} finally {
    		session.close();
    	}
    }

    protected <T> void testDeleteByInstance(String where, Class<T> cls, Object key) {
    	try {
    		session = sessionFactory.getSession();
        	T instance = session.newInstance(cls, key);
        	session.deletePersistent(instance);
    	} finally {
    		session.close();
    	}
    }

    protected <T> List<T> testScanPartitionKey(Class<T> cls, int partitionId) {
    	try {
    		session = sessionFactory.getSession();
            QueryBuilder qb = session.getQueryBuilder();
            QueryDomainType<T> dobj = qb.createQueryDefinition(cls);
            Predicate pred = dobj.get("partitionId").equal(dobj.param("partitionIDParam"));
            dobj.where(pred);
            Query<T> query = session.createQuery(dobj);
            query.setParameter("partitionIDParam", partitionId);
            List<T> result = query.getResultList();
            return result;
    	} finally {
    		session.close();
    	}
    }

    protected <T> List<T> testTableScanId(Class<T> cls, int id) {
    	try {
    		session = sessionFactory.getSession();
            QueryBuilder qb = session.getQueryBuilder();
            QueryDomainType<T> dobj = qb.createQueryDefinition(cls);
            Predicate pred = dobj.get("id").equal(dobj.param("idParam"));
            dobj.where(pred);
            Query<T> query = session.createQuery(dobj);
            query.setParameter("idParam", id);
            List<T> result = query.getResultList();
            return result;
    	} finally {
    		session.close();
    	}
    }

    protected void checkException(String where, String message, Throwable t) {
    	if (!(t instanceof ClusterJException)) {
        	error(where + " threw the wrong exception: " + t.getClass().getName() +
        			" " + t.getMessage());
    	} else {
    		if (!t.getMessage().contains(message)) {
    			error(where + " ClusterJException did not contain <" + message +
    	    		"> in message: " + t.getMessage());
    		}
    	}
    }

	@Projection
    @PersistenceCapable (table="hope")
	public interface HopeLessNotAllPK {
    	@Persistent (column="partition_id")
		int getPartitionId();
		void setPartitionId(int value);
	}
	@Projection
    @PersistenceCapable (table="hope")
	public interface HopeFulNotAllNonDefault {
    	@Persistent (column="partition_id")
		int getPartitionId();
		void setPartitionId(int value);
		int getId();
		void setId(int value);
		int getInt_col1();
		void setInt_col1(int value);
	}
	@Projection
    @PersistenceCapable (table="hope")
	public interface ProjectHope extends HopeFulNotAllNonDefault {
    	@Persistent (column="partition_id")
		int getPartitionId();
		void setPartitionId(int value);
		int getId();
		void setId(int value);
		int getInt_col1();
		void setInt_col1(int value);
		int getInt_col2();
		void setInt_col2(int value);
		String getStr_col3();
		void setStr_col3(String value);
	}
    @PersistenceCapable (table="hope")
    public interface Hope {
    	@Persistent (column="partition_id")
		int getPartitionId();
		void setPartitionId(int value);
		int getId();
		void setId(int value);
		int getInt_col1();
		void setInt_col1(int value);
		int getInt_col2();
		void setInt_col2(int value);
		String getStr_col1();
		void setStr_col1(String value);
		String getStr_col2();
		void setStr_col2(String value);
		String getStr_col3();
		void setStr_col3(String value);
	}
	
}
