/*
 *  Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.
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

import com.mysql.clusterj.ClusterJHelper;
import com.mysql.clusterj.Dbug;

/**
 * Tests dbug methods.
 */
public class DbugTest extends AbstractClusterJTest{

    public boolean getDebug() {
        return false;
    }

    public void test() {
        String originalState = "t";
        String newState = "d,jointx:o,/tmp/jointx";
        Dbug dbug = ClusterJHelper.newDbug();
        errorIfEqual("Failed to get new Dbug", null, dbug);
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
        dbug.output("/fully/qualified").flush().debug(new String[] {"how", "now"}).push();
        actualState = dbug.get();
        errorIfNotEqual("Wrong state created", "d,how,now:O,/fully/qualified", actualState);
        dbug.pop();

        dbug = ClusterJHelper.newDbug();
        dbug.append("partly/qualified").trace().debug(new String[] {"brown", "cow"}).set();
        actualState = dbug.get();
        errorIfNotEqual("Wrong state created", "t:d,brown,cow:a,partly/qualified", actualState);
        dbug.pop();

        failOnError();
    }

}
