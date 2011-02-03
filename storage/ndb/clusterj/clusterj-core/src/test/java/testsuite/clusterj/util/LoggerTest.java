/*
   Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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
