/*
   Copyright (c) 2011, 2024, Oracle and/or its affiliates.

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

import com.mysql.clusterj.ClusterJDatastoreException;
import com.mysql.clusterj.ClusterJDatastoreException.Classification;
import com.mysql.clusterj.ClusterJUserException;
import com.mysql.clusterj.ColumnMetadata;
import com.mysql.clusterj.DynamicObject;
import com.mysql.clusterj.Query;

import com.mysql.clusterj.annotation.PersistenceCapable;
import com.mysql.clusterj.query.QueryDomainType;
import com.mysql.clusterj.query.PredicateOperand;

import java.util.List;
import java.util.Map;

public class SchemaChangeTest extends AbstractClusterJModelTest {

    private static final String alterTableDropBtreeIndexStatement =
            "alter table maldacena drop index idx_string_not_null_btree";

    private static final String alterTableAddBtreeIndexStatement =
            "alter table maldacena add index idx_string_not_null_btree (string_not_null_btree)";

    private static final String alterTableDropHashIndexStatement =
            "alter table maldacena drop index idx_string_not_null_hash";

    private static final String alterTableAddHashIndexStatement =
            "alter table maldacena add unique index idx_string_not_null_hash (string_not_null_hash) using hash";

    private static final String alterTableDropBtreeColumnStatement =
            "alter table maldacena drop column string_not_null_btree";

    private static final String alterTableAddBtreeColumnStatement =
            "alter table maldacena add string_not_null_btree varchar(20) not null default '0'";

    private static final String alterTableDropHashColumnStatement =
            "alter table maldacena drop column string_not_null_hash";

    private static final String alterTableAddHashColumnStatement =
            "alter table maldacena add string_not_null_hash varchar(300) not null default '0'";

    private static final String dropTableStatement =
            "drop table if exists maldacena";

    private static final String createTableStatement =
            "create table maldacena (" +
            "id int not null primary key," +

            "string_not_null_hash varchar(300) not null default '0'," +
            "string_not_null_btree varchar(20) not null default '0'," +
            "string_not_null_both varchar(300) not null default '0'," +
            "string_not_null_none varchar(20) not null default '0'," +

            "unique key idx_string_not_null_hash (string_not_null_hash) using hash," +
            "key idx_string_not_null_btree (string_not_null_btree)," +
            "unique key idx_string_not_null_both (string_not_null_both)" +

            ") ENGINE=ndbcluster DEFAULT CHARSET=latin1";

    private static final String truncateTableStatement =
            "truncate table maldacena";

    @Override
    public void localSetUp() {
        logger.info("PLEASE IGNORE THE FOLLOWING EXPECTED SEVERE ERRORS.");
        createSessionFactory();
        session = sessionFactory.getSession();
        executeSQL(dropTableStatement);
        executeSQL(createTableStatement);
        session.unloadSchema(Maldacena.class);
        session.makePersistent(session.newInstance(Maldacena.class, 0));
    }

    @Override
    public void localTearDown() {
        executeSQL(dropTableStatement);
    }

    public void test() {
        testTruncate();
        testDropTable();
        testDropBtreeIndex();
        testDropHashIndex();
        testDropBtreeColumn();
        testDropHashColumn();
        testClassification();
        failOnError();
    }

    protected void testClassification() {
        errorIfNotEqual("wrong classification", 0, Classification.lookup(0).value);
        errorIfNotEqual("wrong classification", 1, Classification.lookup(1).value);
        errorIfNotEqual("wrong classification", 2, Classification.lookup(2).value);
        errorIfNotEqual("wrong classification", 3, Classification.lookup(3).value);
        errorIfNotEqual("wrong classification", 4, Classification.lookup(4).value);
        errorIfNotEqual("wrong classification", 5, Classification.lookup(5).value);
        errorIfNotEqual("wrong classification", 6, Classification.lookup(6).value);
        errorIfNotEqual("wrong classification", 7, Classification.lookup(7).value);
        errorIfNotEqual("wrong classification", 8, Classification.lookup(8).value);
        errorIfNotEqual("wrong classification", 9, Classification.lookup(9).value);
        errorIfNotEqual("wrong classification", 10, Classification.lookup(10).value);
        errorIfNotEqual("wrong classification", 11, Classification.lookup(11).value);
        errorIfNotEqual("wrong classification", 12, Classification.lookup(12).value);
        errorIfNotEqual("wrong classification", 13, Classification.lookup(13).value);
        errorIfNotEqual("wrong classification", 14, Classification.lookup(14).value);
        errorIfNotEqual("wrong classification", 15, Classification.lookup(15).value);
        errorIfNotEqual("wrong classification", 17, Classification.lookup(17).value);
        errorIfNotEqual("wrong classification", 18, Classification.lookup(18).value);
        if (Classification.lookup(16) != null) error("wrong classification for Classification.lookup(16)");
        if (Classification.lookup(19) != null) error("wrong classification for Classification.lookup(19)");
        if (Classification.lookup(100) != null) error("wrong classification for Classification.lookup(100)");
    }

    protected void testTruncate() {
        tryFind("testTruncate before truncate find", Maldacena.class, 0, 1, "no error");
        tryQuery("testTruncate before truncate unique key", Maldacena.class, "string_not_null_hash", "0", 1,
                "UNIQUE_KEY", "idx_string_not_null_hash", "no error");
        tryQuery("testDropIndetestDropHashIndex before truncate unique key", Maldacena.class, "string_not_null_hash", "0", 1,
                "UNIQUE_KEY", "idx_string_not_null_hash", "no error");
        tryQuery("testDropHashIndex before truncate index scan", Maldacena.class, "string_not_null_btree", "0", 1,
                "INDEX_SCAN", "idx_string_not_null_btree", "no error");
        tryQuery("testDropHashIndex before truncate table scan", Maldacena.class, "string_not_null_none", "0", 1,
                "TABLE_SCAN", "no index", "no error");
        executeSQL(truncateTableStatement);
        tryFind("testTruncate after truncate find", Maldacena.class, 0, 1, "code 241");
        tryQuery("testTruncate after truncate unique key", Maldacena.class, "string_not_null_hash", "0", 0,
                "UNIQUE_KEY", "idx_string_not_null_hash", "code 241");
        tryQuery("testTruncate after truncate index scan", Maldacena.class, "string_not_null_btree", "0", 0,
                "INDEX_SCAN", "idx_string_not_null_btree", "code 241");
        tryQuery("testTruncate after truncate table scan", Maldacena.class, "string_not_null_none", "0", 0,
                "TABLE_SCAN", "no index", "code 241");
        session.unloadSchema(Maldacena.class);
        session.makePersistent(session.newInstance(Maldacena.class, 0));
        tryFind("testTruncate after unload schema find", Maldacena.class, 0, 1, "no error");
        tryQuery("testTruncate after unload schema unique key", Maldacena.class, "string_not_null_hash", "0", 1,
                "UNIQUE_KEY", "idx_string_not_null_hash", "no error");
        tryQuery("testTruncate after unload schema index scan", Maldacena.class, "string_not_null_btree", "0", 1,
                "INDEX_SCAN", "idx_string_not_null_btree", "no error");
        tryQuery("testTruncate after unload schema table scan", Maldacena.class, "string_not_null_none", "0", 1,
                "TABLE_SCAN", "no index", "no error");
    }

    protected void testDropTable() {
        tryFind("testDropTable before drop table find", Maldacena.class, 0, 1, "no error");
        tryQuery("testDropTable before drop table unique key", Maldacena.class, "string_not_null_hash", "0", 1,
                "UNIQUE_KEY", "idx_string_not_null_hash", "no error");
        tryQuery("testDropTable before drop table index scan", Maldacena.class, "string_not_null_btree", "0", 1,
                "INDEX_SCAN", "idx_string_not_null_btree", "no error");
        tryQuery("testDropTable before drop table table scan", Maldacena.class, "string_not_null_none", "0", 1,
                "TABLE_SCAN", "no index", "no error");
        executeSQL(dropTableStatement);
        tryFind("testDropTable after drop table find", Maldacena.class, 0, 1, "code 284");
        tryQuery("testDropTable after drop table unique key", Maldacena.class, "string_not_null_hash", "0", 0,
                "UNIQUE_KEY", "idx_string_not_null_hash", "code 284");
        tryQuery("testDropTable after drop table index scan", Maldacena.class, "string_not_null_btree", "0", 0,
                "INDEX_SCAN", "idx_string_not_null_btree", "code 284");
        tryQuery("testDropTable after drop table table scan", Maldacena.class, "string_not_null_none", "0", 0,
                "TABLE_SCAN", "no index", "code 284");
        executeSQL(createTableStatement);
        session.unloadSchema(Maldacena.class);
        session.makePersistent(session.newInstance(Maldacena.class, 0));
        tryFind("testDropTable after create table find", Maldacena.class, 0, 1, "no error");
        tryQuery("testDropTable after create table unique key", Maldacena.class, "string_not_null_hash", "0", 1,
                "UNIQUE_KEY", "idx_string_not_null_hash", "no error");
        tryQuery("testDropTable after create table index scan", Maldacena.class, "string_not_null_btree", "0", 1,
                "INDEX_SCAN", "idx_string_not_null_btree", "no error");
        tryQuery("testDropTable after create table table scan", Maldacena.class, "string_not_null_none", "0", 1,
                "TABLE_SCAN", "no index", "no error");
    }

    protected void testDropBtreeIndex() {
        tryFind("testDropBtreeIndex before drop btree index find", Maldacena.class, 0, 1, "no error");
        tryQuery("testDropBtreeIndex before drop btree index unique key", Maldacena.class, "string_not_null_hash", "0", 1,
                "UNIQUE_KEY", "idx_string_not_null_hash", "no error");
        tryQuery("testDropHashIndex before drop btree index index scan", Maldacena.class, "string_not_null_btree", "0", 1,
                "INDEX_SCAN", "idx_string_not_null_btree", "no error");
        tryQuery("testDropHashIndex before drop btree index table scan", Maldacena.class, "string_not_null_none", "0", 1,
                "TABLE_SCAN", "no index", "no error");
        executeSQL(alterTableDropBtreeIndexStatement);
        tryFind("testDropBtreeIndex after drop btree index find", Maldacena.class, 0, 1, "no error");
        tryQuery("testDropBtreeIndex after drop btree index unique key", Maldacena.class, "string_not_null_hash", "0", 1,
                "UNIQUE_KEY", "idx_string_not_null_hash", "no error");
        tryQuery("testDropBtreeIndex after drop btree index index scan", Maldacena.class, "string_not_null_btree", "0", 0,
                "INDEX_SCAN", "idx_string_not_null_btree", "code 284");
        tryQuery("testDropBtreeIndex after drop btree index table scan", Maldacena.class, "string_not_null_none", "0", 1,
                "TABLE_SCAN", "no index", "no error");
        executeSQL(alterTableAddBtreeIndexStatement);
        session.unloadSchema(Maldacena.class);
        tryFind("testDropBtreeIndex after add btree index find", Maldacena.class, 0, 1, "no error");
        tryQuery("testDropBtreeIndex after add btree index unique key", Maldacena.class, "string_not_null_hash", "0", 1,
                "UNIQUE_KEY", "idx_string_not_null_hash", "no error");
        tryQuery("testDropBtreeIndex after add btree index index scan", Maldacena.class, "string_not_null_btree", "0", 1,
                "INDEX_SCAN", "idx_string_not_null_btree", "no error");
        tryQuery("testDropBtreeIndex after add btree index table scan", Maldacena.class, "string_not_null_none", "0", 1,
                "TABLE_SCAN", "no index", "no error");
    }

    protected void testDropHashIndex() {
        tryFind("testDropHashIndex before drop hash index find", Maldacena.class, 0, 1, "no error");
        executeSQL(alterTableDropHashIndexStatement);
        tryFind("testDropHashIndex after drop hash index find", Maldacena.class, 0, 1, "no error");
        tryQuery("testDropHashIndex after drop hash index unique key", Maldacena.class, "string_not_null_hash", "0", 1,
                "UNIQUE_KEY", "idx_string_not_null_hash", "code 284");
        tryQuery("testDropHashIndex after drop hash index index scan", Maldacena.class, "string_not_null_btree", "0", 1,
                "INDEX_SCAN", "idx_string_not_null_btree", "no error");
        tryQuery("testDropHashIndex after drop hash index table scan", Maldacena.class, "string_not_null_none", "0", 1,
                "TABLE_SCAN", "no index", "no error");
        executeSQL(alterTableAddHashIndexStatement);
        session.unloadSchema(Maldacena.class);
        tryFind("testDropHashIndex after add hash index find", Maldacena.class, 0, 1, "no error");
        tryQuery("testDropHashIndex after add hash index unique key", Maldacena.class, "string_not_null_hash", "0", 1,
                "UNIQUE_KEY", "idx_string_not_null_hash", "no error");
        tryQuery("testDropHashIndex after add hash index index scan", Maldacena.class, "string_not_null_btree", "0", 1,
                "INDEX_SCAN", "idx_string_not_null_btree", "no error");
        tryQuery("testDropHashIndex after add hash index table scan", Maldacena.class, "string_not_null_none", "0", 1,
                "TABLE_SCAN", "no index", "no error");
    }

    protected void testDropBtreeColumn() {
        tryFind("testDropBtreeColumn before drop btree column find", Maldacena.class, 0, 1, "no error");
        executeSQL(alterTableDropBtreeColumnStatement);
        tryFind("testDropBtreeColumn after drop btree column find", Maldacena.class, 0, 0, "code 284");
        tryQuery("testDropBtreeColumn after drop btree column unique key", Maldacena.class, "string_not_null_hash", "0", 0,
                "UNIQUE_KEY", "idx_string_not_null_hash", "284");
        tryQuery("testDropBtreeColumn after drop btree column index scan", Maldacena.class, "string_not_null_btree", "0", 0,
                "INDEX_SCAN", "idx_string_not_null_btree", "284");
        tryQuery("testDropBtreeColumn after drop btree column table scan", Maldacena.class, "string_not_null_none", "0", 0,
                "TABLE_SCAN", "no index", "code 284");
        executeSQL(alterTableAddBtreeColumnStatement);
        executeSQL(alterTableAddBtreeIndexStatement);
        session.unloadSchema(Maldacena.class);
        tryFind("testDropBtreeColumn after add btree index find", Maldacena.class, 0, 1, "no error");
        tryQuery("testDropBtreeColumn after add btree index unique key", Maldacena.class, "string_not_null_hash", "0", 1,
                "UNIQUE_KEY", "idx_string_not_null_hash", "no error");
        tryQuery("testDropBtreeColumn after add btree index index scan", Maldacena.class, "string_not_null_btree", "0", 1,
                "INDEX_SCAN", "idx_string_not_null_btree", "no error");
        tryQuery("testDropBtreeColumn after add btree index table scan", Maldacena.class, "string_not_null_none", "0", 1,
                "TABLE_SCAN", "no index", "no error");
    }

    protected void testDropHashColumn() {
        tryFind("testDropHashColumn before drop hash column find", Maldacena.class, 0, 1, "no error");
        executeSQL(alterTableDropHashColumnStatement);
        tryFind("testDropHashColumn after drop hash column find", Maldacena.class, 0, 1, "code 284");
        tryQuery("testDropHashColumn after drop hash column unique key", Maldacena.class, "string_not_null_hash", "0", 1,
                "UNIQUE_KEY", "idx_string_not_null_hash", "code 284");
        tryQuery("testDropHashColumn after drop hash column index scan", Maldacena.class, "string_not_null_btree", "0", 0,
                "INDEX_SCAN", "idx_string_not_null_btree", "code 284");
        tryQuery("testDropHashColumn after drop hash column table scan", Maldacena.class, "string_not_null_none", "0", 0,
                "TABLE_SCAN", "no index", "code 284");
        executeSQL(alterTableAddHashColumnStatement);
        executeSQL(alterTableAddHashIndexStatement);
        session.unloadSchema(Maldacena.class);
        tryFind("testDropHashColumn after add hash column find", Maldacena.class, 0, 1, "no error");
        tryQuery("testDropHashColumn after add hash column unique key", Maldacena.class, "string_not_null_hash", "0", 1,
                "UNIQUE_KEY", "idx_string_not_null_hash", "no error");
        tryQuery("testDropHashColumn after add hash column index scan", Maldacena.class, "string_not_null_btree", "0", 1,
                "INDEX_SCAN", "idx_string_not_null_btree", "no error");
        tryQuery("testDropHashColumn after add hash column table scan", Maldacena.class, "string_not_null_none", "0", 1,
                "TABLE_SCAN", "no index", "no error");
    }

    protected boolean tryFind(String where, Class<?> domainClass, Object key, int expectedRows, String errorMessageFragment) {
        try {
            Object row = session.find(domainClass, key);
            if (!errorMessageFragment.equals("no error")) {
                error(where + " unexpected success for find class: " + domainClass.getName() + " key: " + key + " ");
            }
            if (expectedRows != 0 && row == null) {
                error(where + " row was not found for class: " + domainClass.getName());
            }
            return true;
        } catch (ClusterJDatastoreException dex) {
            //System.out.println("lookup classification: " + Classification.lookup((dex.getClassification())));
            //System.out.println("getCode(): " + dex.getCode());
            //System.out.println("getMysqlCode(): " + dex.getMysqlCode());
            //System.out.println("getClassification(): " + dex.getClassification());
            //System.out.println("getStatus(): " + dex.getStatus());
            String actualMessage = dex.getMessage();
            if (errorMessageFragment == null) {
                error(where + " unexpected failure for find class: " + domainClass.getName() + " key: " + key + " "
                        + " message: " + actualMessage);
            } else {
                errorIfNotEqual(where + " wrong error message, expected contains " + errorMessageFragment
                        + " actual: " + actualMessage, actualMessage.contains(errorMessageFragment), true);
              if (dex.getMysqlCode() == 159) {
                errorIfNotEqual(where + " wrong classification for mySqlCode 159", 4, dex.getClassification());
              }
            }
            return false;
        }
    }

    protected class QueryHandler<T> {
        QueryDomainType<T> qdt;
        PredicateOperand field;
        PredicateOperand param;
        Query<T> q;
    }

    protected <T> QueryHandler<T> getQueryHandler(Class<T> domainClass, String fieldName, Object key) {
        QueryHandler<T> handler = new QueryHandler<T>();
        handler.qdt = session.getQueryBuilder().createQueryDefinition(domainClass);
        handler.field = handler.qdt.get(fieldName);
        handler.param = handler.qdt.param("param");
        handler.qdt.where(handler.field.equal(handler.param));
        handler.q = session.createQuery(handler.qdt);
        handler.q.setParameter("param", key);
        return handler;
    }

    protected <T> boolean tryQuery(String where, Class<T> domainClass,
            String fieldName, Object key, int expectedRows,
            String expectedScanType, String expectedIndexUsed, String errorMessageFragment) {
        try {
            QueryHandler<T> handler = getQueryHandler(domainClass, fieldName, key);
            Map<String, Object> explain = handler.q.explain();
            errorIfNotEqual(where + " wrong scan type", expectedScanType, explain.get("ScanType"));
            if (!expectedIndexUsed.equals("no index")) {
                errorIfNotEqual(where + " wrong index used", expectedIndexUsed, explain.get("IndexUsed"));
            }
            List<T> resultList = handler.q.getResultList();
            errorIfNotEqual(where + " wrong number of result rows for query", expectedRows, resultList.size());
            if (!errorMessageFragment.equals("no error")) {
                error(where + " unexpected success for query by key class: " + domainClass.getName() + " key: " + key + " ");
            }
            return true;
        } catch (ClusterJDatastoreException dex) {
            String actualMessage = dex.getMessage();
            if (errorMessageFragment.equals("no error")) {
                error(where + " unexpected failure for query by key class: " + domainClass.getName() + " key: " + key + " "
                        + " message: " + actualMessage);
            } else {
                errorIfNotEqual(where + " wrong error message, expected contains " + errorMessageFragment
                        + " actual: " + actualMessage, actualMessage.contains(errorMessageFragment), true);
            }
            return false;
        }
    }

    /** Maldacena class to map table maldacena.
     */
    @PersistenceCapable(table="maldacena")
    public static interface Maldacena {

        int getId();
        void setId(int id);

        String getString_not_null_hash();
        void setString_not_null_hash(String value);

        String getString_not_null_btree();
        void setString_not_null_btree(String value);

        String getString_not_null_both();
        void setString_not_null_both(String value);

        String getString_not_null_none();
        void setString_not_null_none(String value);

    }
}
