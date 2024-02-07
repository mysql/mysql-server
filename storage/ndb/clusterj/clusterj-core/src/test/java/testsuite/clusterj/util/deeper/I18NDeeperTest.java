/*
   Copyright (c) 2010, 2024, Oracle and/or its affiliates.

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

package testsuite.clusterj.util.deeper;

import com.mysql.clusterj.core.util.I18NHelper;
import testsuite.clusterj.AbstractClusterJCoreTest;

public class I18NDeeperTest extends AbstractClusterJCoreTest {

    /** This test uses the Bundle.properties in the super-package.
     * 
     */
    public void test() {
        I18NHelper helper = I18NHelper.getInstance(I18NDeeperTest.class);
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
