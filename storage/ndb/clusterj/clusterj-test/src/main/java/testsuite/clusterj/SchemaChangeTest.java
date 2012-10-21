/*
   Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

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

import com.mysql.clusterj.ClusterJDatastoreException;
import com.mysql.clusterj.ClusterJUserException;
import com.mysql.clusterj.ColumnMetadata;
import com.mysql.clusterj.DynamicObject;
import com.mysql.clusterj.annotation.PersistenceCapable;

import testsuite.clusterj.model.StringTypes;

public class SchemaChangeTest extends AbstractClusterJModelTest {

    private static final String modifyTableStatement = 
        "alter table stringtypes drop column string_not_null_none";

    private static final String restoreTableStatement = 
        "alter table stringtypes add string_not_null_none varchar(20) DEFAULT NULL";

    @Override
    public void localSetUp() {
        createSessionFactory();
        session = sessionFactory.getSession();
        // create will cache the schema
        session.deletePersistentAll(StringTypes.class);
        session.makePersistent(session.newInstance(StringTypes.class, 0));
        addTearDownClasses(StringTypes.class);
    }

    public void testFind() {
        logger.info("PLEASE IGNORE THE FOLLOWING EXPECTED SEVERE ERROR.");
        // change the schema (drop a column)
        executeSQL(modifyTableStatement);
        try {
            // find the row (with a different schema) which will fail
            session.find(StringTypes.class, 0);
        } catch (ClusterJDatastoreException dex) {
            // make sure it's the right exception
            if (!dex.getMessage().contains("code 284")) {
                error("ClusterJDatastoreException must contain code 284 but contains only " + dex.getMessage());
            }
            // unload the schema for StringTypes which also clears the cached dictionary table
            String tableName = session.unloadSchema(StringTypes.class);
            // make sure we unloaded the right table
            errorIfNotEqual("Table name mismatch", "stringtypes", tableName);
            // it should work with a different schema that doesn't include the dropped column
            StringTypes2 zero = session.find(StringTypes2.class, 0);
            // verify that column string_not_null_none does not exist
            ColumnMetadata[] metadatas = zero.columnMetadata();
            for (ColumnMetadata metadata: metadatas) {
                if ("string_not_null_none".equals(metadata.name())) {
                    error("Column string_not_null_none should not exist after schema change.");
                }
            }
            try {
                // find the row (with a different schema) which will fail with a user exception
                session.find(StringTypes.class, 0);
                error("Unexpected success using StringTypes class without column string_not_null_none defined");
            } catch (ClusterJUserException uex) {
                // StringTypes can't be loaded because of the missing column, but
                // the cached dictionary table was removed when the domain type handler couldn't be created
                executeSQL(restoreTableStatement);
                // after restoreTableDefinition, string_not_null_none is defined again
                // find the row (with a different schema) which will now work
                session.find(StringTypes.class, 0);
            }
        }
        logger.info("PLEASE IGNORE THE PRECEDING EXPECTED SEVERE ERROR.\n");
        failOnError();
    }

    /** StringTypes dynamic class to map stringtypes after column string_not_null_none is removed.
     */
    @PersistenceCapable(table="stringtypes")
    public static class StringTypes2 extends DynamicObject {
        public StringTypes2() {}
    }
}
