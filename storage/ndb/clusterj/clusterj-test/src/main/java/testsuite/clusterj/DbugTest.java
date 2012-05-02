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

package testsuite.clusterj;

import java.io.File;

import com.mysql.clusterj.ClusterJHelper;
import com.mysql.clusterj.Dbug;

/**
 * Tests dbug methods.
 */
public class DbugTest extends AbstractClusterJTest{

    private static final String TMP_DIR_NAME = System.getProperty("java.io.tmpdir");
    private static final String FILE_SEPARATOR = File.separator;
    private static final String TMP_FILE_NAME = TMP_DIR_NAME + FILE_SEPARATOR + "clusterj-test-dbug";

    public boolean getDebug() {
        return false;
    }

    public void test() {
        Dbug dbug = ClusterJHelper.newDbug();
        if (dbug == null) {
            // nothing else can be tested
            fail("Failed to get new Dbug");
        }
        if (dbug.get() == null) {
            // ndbclient is compiled without DBUG; just make sure nothing blows up
            dbug.set("nothing");
            dbug.push("nada");
            dbug.pop();
            dbug.print("keyword", "message");
            return;
        }
        String originalState = "t";
        String newState = "d,jointx:o," + TMP_FILE_NAME;
        dbug.set(originalState);
        String actualState = dbug.get();
        errorIfNotEqual("Failed to set original state", originalState, actualState);
        dbug.push(newState);
        actualState = dbug.get();
        errorIfNotEqual("Failed to push new state", newState, actualState);
        dbug.pop();
        actualState = dbug.get();
        errorIfNotEqual("Failed to pop original state", originalState, actualState);

        dbug = ClusterJHelper.newDbug();
        dbug.output(TMP_FILE_NAME).flush().debug(new String[] {"a", "b", "c", "d", "e", "f"}).push();
        actualState = dbug.get();
        // keywords are stored LIFO
        errorIfNotEqual("Wrong state created", "d,f,e,d,c,b,a:O," + TMP_FILE_NAME, actualState);
        dbug.pop();

        dbug = ClusterJHelper.newDbug();
        dbug.append(TMP_FILE_NAME).trace().debug("a,b,c,d,e,f").set();
        actualState = dbug.get();
        // keywords are stored LIFO
        errorIfNotEqual("Wrong state created", "d,f,e,d,c,b,a:a," + TMP_FILE_NAME + ":t", actualState);
        dbug.pop();

        failOnError();
    }

}
