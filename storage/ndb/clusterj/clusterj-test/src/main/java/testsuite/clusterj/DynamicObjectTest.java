/*
   Copyright (c) 2010, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

package testsuite.clusterj;

import com.mysql.clusterj.ClusterJUserException;
import com.mysql.clusterj.ColumnType;
import com.mysql.clusterj.DynamicObject;
import com.mysql.clusterj.ColumnMetadata;
import com.mysql.clusterj.Query;
import com.mysql.clusterj.annotation.PersistenceCapable;
import com.mysql.clusterj.query.QueryBuilder;
import com.mysql.clusterj.query.QueryDomainType;

public class DynamicObjectTest extends AbstractClusterJModelTest {

    private static final String tablename = "t_basic";

    private static final int NUMBER_TO_INSERT = 5;

    private DynamicObject[] instances = new TBasic[NUMBER_TO_INSERT];

    private DynamicObject tbasic;

    private Object[] expectedTBasicNames = new Object[] {"id", "name", "age", "magic"};

    private Object[] expectedTBasicTypes = new Object[] {ColumnType.Int, ColumnType.Varchar, ColumnType.Int, ColumnType.Int};

    private Object[] expectedTBasicJavaTypes = new Object[] {int.class, String.class, Integer.class, int.class};

    private Object[] expectedTBasicMaximumLengths = new Object[] {1, 128, 1, 1};

    private Object[] expectedTBasicNumbers = new Object[] {0, 1, 2, 3};

    private Object[] expectedTBasicIsPrimaryKeys = new Object[] {true, false, false, false};

    private Object[] expectedTBasicIsPartitionKeys = new Object[] {true, false, false, false};

    private Object[] expectedTBasicPrecisions = new Object[] {0, 0, 0, 0};

    private Object[] expectedTBasicScales = new Object[] {0, 0, 0, 0};

    private Object[] expectedTBasicNullables = new Object[] {false, true, true, false};

    private Object[] expectedTBasicCharsetNames = new Object[] {null, "utf8mb4", null, null};
    
    @Override
    public void localSetUp() {
        createSessionFactory();
        session = sessionFactory.getSession();
        createDynamicInstances(TBasic.class, NUMBER_TO_INSERT);
        tbasic = instances[0];
        tx = session.currentTransaction();
        int count = 0;
        for (int i = 0; i < NUMBER_TO_INSERT; ++i) {
            try {
                session.deletePersistent(TBasic.class, i);
                ++count;
            } catch (Exception ex) {
                // ignore exceptions -- might not be any instances to delete
            }
        }
        addTearDownClasses(TBasic.class);
    }

    private <T extends DynamicObject> void createDynamicInstances(Class<T> dynamicClass, int numberToInsert) {
        for (int i = 0; i < numberToInsert; ++i) {
            DynamicObject instance = createInstance(dynamicClass, i);
            instance.set(1, String.valueOf(i)); // name
            instance.set(2, i); // age
            instance.set(3, i); // magic
            instances[i] = instance;
        }
    }

    private <T> T createInstance(Class<T> cls, int i) {
        T instance = session.newInstance(cls, i);
        return instance;
    }

    public static class TBasic extends DynamicObject {
        @Override
        public String table() {
            return tablename;
        }
    }

    @PersistenceCapable(table="t_basic")
    public static class AnnotatedTBasic extends DynamicObject {}

    public void test() {
        insert();
        find();
        findAnnotated();
        lookup();
        query();
        badClass(DynamicObjectPrivate.class);
        badClass(DynamicObjectProtectedConstructor.class);
        badClass(DynamicObjectNonStatic.class);
        badClass(DynamicObjectPrivateConstructor.class);
        badClass(DynamicObjectNullTableName.class);
        badClass(DynamicObjectTableDoesNotExist.class);
        checkMetadata();
        failOnError();
    }
    private void insert() {
        session.makePersistent(instances);
    }

    private void find() {
        TBasic instance = session.find(TBasic.class, 0);
        validateInstance(instance);
    }

    private void findAnnotated() {
        AnnotatedTBasic instance = session.find(AnnotatedTBasic.class, 0);
        validateInstance(instance);
    }

    private void lookup() {
        QueryBuilder builder = session.getQueryBuilder();
        QueryDomainType<TBasic> queryTBasic = builder.createQueryDefinition(TBasic.class);
        queryTBasic.where(queryTBasic.get("magic").equal(queryTBasic.param("magic")));
        Query<TBasic> query = session.createQuery(queryTBasic);
        query.setParameter("magic", 1);
        TBasic instance = query.getResultList().get(0);
        validateInstance(instance);
    }

    private void query() {
        QueryBuilder builder = session.getQueryBuilder();
        QueryDomainType<TBasic> queryTBasic = builder.createQueryDefinition(TBasic.class);
        queryTBasic.where(queryTBasic.get("name").equal(queryTBasic.param("name")));
        Query<TBasic> query = session.createQuery(queryTBasic);
        query.setParameter("name", "2");
        TBasic instance = query.getResultList().get(0);
        validateInstance(instance);
    }

    private void validateInstance(DynamicObject instance) {
        int id = (Integer)instance.get(0);
        errorIfNotEqual("validate name", String.valueOf(id), instance.get(1)); // name
        errorIfNotEqual("validate age", id, instance.get(2)); // age
        errorIfNotEqual("validate magic", id, instance.get(3)); // magic
    }
    
    private void badClass(Class<?> cls) {
        try {
            session.newInstance(cls);
        } catch (ClusterJUserException e) {
            // good catch
        } catch (Throwable t) {
            error(cls.getClass().getName() + " threw wrong exception: " + t.getMessage());
        }
    }

    public static class DynamicObjectProtectedConstructor extends DynamicObject {
        protected DynamicObjectProtectedConstructor() {}
        @Override
        public String table() {
            return "DynamicObjectProtectedConstructor";
        }        
    }

    private static class DynamicObjectPrivate extends DynamicObject {
    }

    public class DynamicObjectNonStatic extends DynamicObject {
        public DynamicObjectNonStatic() {}
        @Override
        public String table() {
            return "DynamicObjectProtectedConstructor";
        }        
    }

    public static class DynamicObjectPrivateConstructor extends DynamicObject {
        private DynamicObjectPrivateConstructor() {}
        @Override
        public String table() {
            return "DynamicObjectPrivateConstructor";
        }        
    }

    public static class DynamicObjectTableDoesNotExist extends DynamicObject {
        private DynamicObjectTableDoesNotExist() {}
        @Override
        public String table() {
            return "DynamicObjectTableDoesNotExist";
        }        
    }

    public static class DynamicObjectNullTableName extends DynamicObject {
        public DynamicObjectNullTableName() {}
    }

    protected void checkMetadata() {
        ColumnMetadata[] metadata = tbasic.columnMetadata();
        for (int i = 0; i < metadata.length; ++i) {
            errorIfNotEqual("t_basic column " + i + " name", expectedTBasicNames[i], metadata[i].name());
            errorIfNotEqual("t_basic column " + i + " type", expectedTBasicTypes[i], metadata[i].columnType());
            errorIfNotEqual("t_basic column " + i + " javaType", expectedTBasicJavaTypes[i], metadata[i].javaType());
            errorIfNotEqual("t_basic column " + i + " maximumLength", expectedTBasicMaximumLengths[i], metadata[i].maximumLength());
            errorIfNotEqual("t_basic column " + i + " charsetName", expectedTBasicCharsetNames [i], metadata[i].charsetName());
            errorIfNotEqual("t_basic column " + i + " number", expectedTBasicNumbers[i], metadata[i].number());
            errorIfNotEqual("t_basic column " + i + " isPrimaryKey", expectedTBasicIsPrimaryKeys[i], metadata[i].isPrimaryKey());
            errorIfNotEqual("t_basic column " + i + " isPartitionKey", expectedTBasicIsPartitionKeys[i], metadata[i].isPartitionKey());
            errorIfNotEqual("t_basic column " + i + " precision", expectedTBasicPrecisions[i], metadata[i].precision());
            errorIfNotEqual("t_basic column " + i + " scale", expectedTBasicScales[i], metadata[i].scale());
            errorIfNotEqual("t_basic column " + i + " nullable", expectedTBasicNullables[i], metadata[i].nullable());
        }
    }


}
