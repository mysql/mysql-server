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

package testsuite.clusterj.util;

import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactory;
import com.mysql.clusterj.core.util.LoggerFactoryService;
import java.io.File;
import testsuite.clusterj.AbstractClusterJCoreTest;

public class LoggerTest extends AbstractClusterJCoreTest {

    public void test() {
        String loggingPropertiesName = System.getProperty("java.util.logging.config.file");
        if (loggingPropertiesName == null) {
            fail("Logger properties file name is null");
        }
        File loggingPropertyFile = new File(loggingPropertiesName);
        if (!loggingPropertyFile.exists()) {
            fail("File " + loggingPropertiesName + " does not exist");
        }
        LoggerFactory loggerFactory = LoggerFactoryService.getFactory();
        Logger logger = loggerFactory.getInstance("com.mysql.clusterj.core");
        if (logger == null) {
            fail("Logger com.mysql.clusterj.core not found.");
        }
        boolean debugEnabled = logger.isDebugEnabled();
        boolean traceEnabled = logger.isTraceEnabled();
        boolean infoEnabled = logger.isInfoEnabled();
        logger.debug("Debug here.");
        logger.trace("Trace here.");
        logger.info("Info here.");
        logger.warn("Warn here.");
        logger.error("Error here.");
        logger.fatal("Fatal here.");
    }
}
