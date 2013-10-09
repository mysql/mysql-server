/*
   Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved.

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

package testsuite.clusterj.util;

import com.mysql.clusterj.core.util.I18NHelper;
import testsuite.clusterj.AbstractClusterJCoreTest;

public class I18NTest extends AbstractClusterJCoreTest {

    public void test() {
        I18NHelper helper = I18NHelper.getInstance(I18NTest.class);
        String msg0 = helper.message("MSG_0");
        errorIfNotEqual("Failure on message resolution", "Message", msg0);
        String msg1 = helper.message("MSG_1", 1);
        errorIfNotEqual("Failure on message resolution", "Message 1", msg1);
        String msg2 = helper.message("MSG_2", 1, "2");
        errorIfNotEqual("Failure on message resolution", "Message 1 2", msg2);
        String msg3 = helper.message("MSG_3", 1, 2.0, 3);
        errorIfNotEqual("Failure on message resolution", "Message 1 2 3", msg3);
        String msg4 = helper.message("MSG_4", 1, 2, 3L, 4);
        errorIfNotEqual("Failure on message resolution", "Message 1 2 3 4", msg4);
        String msg5 = helper.message("MSG_5", 1, 2, 3, 4F, 5);
        errorIfNotEqual("Failure on message resolution", "Message 1 2 3 4 5", msg5);
        try {
            String msg6 = helper.message("MSG_6", 1, 2, 3, 4, 5, 6);
            // bad; should not find MSG_6 in bundle
            error("Should not find MSG_6 in bundle; returned: " + msg6);
        } catch (Exception ex) {
            // good catch
        }
        failOnError();
    }

}
