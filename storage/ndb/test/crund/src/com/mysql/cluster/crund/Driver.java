/*
  Copyright (c) 2010, 2013, Oracle and/or its affiliates. All rights reserved.

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

package com.mysql.cluster.crund;

import java.util.Properties;
import java.util.List;
import java.util.ArrayList;
import java.util.Date;
import java.text.SimpleDateFormat;
import java.text.DecimalFormat;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FileWriter;
import java.io.IOException;
import java.io.OutputStream;
import java.io.PrintWriter;
import java.io.InputStream;


/**
 * This class benchmarks transactions of standard database operations on
 * different datastore implementations.
 * <p>
 * The operations are variations of Create, Read, Update, Navigate, and
 * Delete (hence, the benchmark's name CRUND).  Subclasses implement these
 * operations for different datastore APIs.
 */
public abstract class Driver {

    // console
    static protected final PrintWriter out = new PrintWriter(System.out, true);
    static protected final PrintWriter err = new PrintWriter(System.err, true);

    // shortcuts
    static protected final String endl = System.getProperty("line.separator");
    static protected final Runtime rt = Runtime.getRuntime();

    // driver command-line arguments
    static private final List<String> propFileNames = new ArrayList<String>();
    static private String logFileName;

    // driver settings
    protected final Properties props = new Properties();
    protected boolean logRealTime;
    protected boolean logMemUsage;
    protected boolean logSumOfOps;
    protected boolean includeFullGC;
    protected int nRuns;

    // driver resources
    protected PrintWriter log;
    protected String descr = "";
    protected boolean logHeader;
    protected StringBuilder header;
    protected StringBuilder rtimes;
    protected StringBuilder musage;
    protected long t0 = 0, t1 = 0, ta = 0;
    protected long m0 = 0, m1 = 0, ma = 0;

    // ----------------------------------------------------------------------
    // driver usage
    // ----------------------------------------------------------------------

    /**
     * Prints a command-line usage message and exits.
     */
    static protected void exitUsage() {
        out.println("usage: [options]");
        out.println("    [-p <file name>]...    a properties file name");
        out.println("    [-l <file name>]       log file name for data output");
        out.println("    [-h|--help]            print usage message and exit");
        out.println();
        System.exit(1); // return an error code
    }

    /**
     * Parses the benchmark's command-line arguments.
     */
    static public void parseArguments(String[] args) {
        for (int i = 0; i < args.length; i++) {
            final String arg = args[i];
            if (arg.equals("-p")) {
                if (i >= args.length) {
                    exitUsage();
                }
                propFileNames.add(args[++i]);
            } else if (arg.equals("-l")) {
                if (i >= args.length) {
                    exitUsage();
                }
                logFileName = args[++i];
            } else if (arg.equals("-h") || arg.equals("--help")) {
                exitUsage();
            } else {
                out.println("unknown option: " + arg);
                exitUsage();
            }
        }

        if (propFileNames.size() == 0) {
            propFileNames.add("run.properties");
        }

        if (logFileName == null) {
            SimpleDateFormat sdf = new SimpleDateFormat("yyyyMMdd_HHMMss");
            logFileName = ("log_" + sdf.format(new Date()) + ".txt");
        }
    }

    /**
     * Creates an instance.
     */
    public Driver() {}

    /**
     * Runs the benchmark.
     */
    public void run() {
        try {
            init();
            runTests();
            close();
        } catch (Exception ex) {
            // end the program regardless of threads
            out.println("caught " + ex);
            ex.printStackTrace();
            System.exit(2); // return an error code
        }
    }

    // ----------------------------------------------------------------------
    // driver intializers/finalizers
    // ----------------------------------------------------------------------

    // loads a dynamically linked system library and reports any failures
    static protected void loadSystemLibrary(String name) {
        out.print("loading libary ...");
        out.flush();
        try {
            System.loadLibrary(name);
        } catch (UnsatisfiedLinkError e) {
            String path;
            try {
                path = System.getProperty("java.library.path");
            } catch (Exception ex) {
                path = "<exception caught: " + ex.getMessage() + ">";
            }
            err.println("NdbBase: failed loading library '"
                        + name + "'; java.library.path='" + path + "'");
            throw e;
        } catch (SecurityException e) {
            err.println("NdbBase: failed loading library '"
                        + name + "'; caught exception: " + e);
            throw e;
        }
        out.println("              [" + name + "]");
    }

    // attempts to run the JVM's Garbage Collector
    static private void gc() {
        // empirically determined limit after which no further
        // reduction in memory usage has been observed
        //final int nFullGCs = 5;
        final int nFullGCs = 10;
        for (int i = 0; i < nFullGCs; i++) {
            //out.print("gc: ");
            long oldfree;
            long newfree = rt.freeMemory();
            do {
                oldfree = newfree;
                rt.runFinalization();
                rt.gc();
                newfree = rt.freeMemory();
                //out.print('.');
            } while (newfree > oldfree);
            //out.println();
        }
    }

    // initializes the driver's resources.
    protected void init() throws Exception {
        loadProperties();
        initProperties();
        printProperties();
        writeProperties("logging.properties");
        // setting this property allows implementations under test to use java.util.logging
        System.setProperty("java.util.logging.config.file", "logging.properties");
        openLogFile();
        clearLogBuffers();
    }

    // releases the driver's resources.
    protected void close() throws Exception {
        // release log buffers
        logHeader = false;
        header = null;
        rtimes = null;
        musage = null;

        closeLogFile();
        props.clear();
    }

    // loads the benchmark's properties from properties files
    private void loadProperties() throws IOException {
        out.println();
        for (String fn : propFileNames) {
            out.println("reading properties file:        " + fn);
            InputStream is = null;
            try {
                is = new FileInputStream(fn);
                props.load(is);
            } finally {
                if (is != null)
                    is.close();
            }
        }
    }

    // retrieves a property's value and parses it as a boolean
    protected boolean parseBoolean(String k, boolean vdefault) {
        final String v = props.getProperty(k);
        return (v == null ? vdefault : Boolean.parseBoolean(v));
    }

    // retrieves a property's value and parses it as a signed decimal integer
    protected int parseInt(String k, int vdefault) {
        final String v = props.getProperty(k);
        try {
            return (v == null ? vdefault : Integer.parseInt(v));
        } catch (NumberFormatException e) {
            final NumberFormatException nfe = new NumberFormatException(
                "invalid value of benchmark property ('" + k + "', '"
                + v + "').");
            nfe.initCause(e);
            throw nfe;
        }
    }

    // initializes the benchmark properties
    protected void initProperties() {
        //props.list(out);
        out.print("setting driver properties ...");
        out.flush();

        final StringBuilder msg = new StringBuilder();
        final String eol = System.getProperty("line.separator");

        logRealTime = parseBoolean("logRealTime", true);
        logMemUsage = parseBoolean("logMemUsage", false);
        logSumOfOps = parseBoolean("logSumOfOps", true);
        includeFullGC = parseBoolean("includeFullGC", false);

        nRuns = parseInt("nRuns", 1);
        if (nRuns < 1) {
            msg.append("[ignored] nRuns:                " + nRuns + eol);
            nRuns = 1;
        }

        if (msg.length() == 0) {
            out.println("   [ok]");
        } else {
            out.println();
            out.print(msg.toString());
        }
    }

    // prints the benchmark's properties
    protected void printProperties() {
        out.println();
        out.println("driver settings ...");
        out.println("logRealTime:                    " + logRealTime);
        out.println("logMemUsage:                    " + logMemUsage);
        out.println("logSumOfOps:                    " + logSumOfOps);
        out.println("includeFullGC:                  " + includeFullGC);
        out.println("nRuns:                          " + nRuns);
    }

    // writes the benchmark's properties
    protected void writeProperties(String fileName) {
        File logger = new File(fileName);
        OutputStream out;
        try {
            if (!logger.exists()) {
                logger.createNewFile();
            }
            out = new FileOutputStream(logger);
            props.store(out, "**** WARNING: DO NOT EDIT THIS FILE; IT IS GENERATED EACH RUN.");
        } catch (FileNotFoundException e) {
            throw new RuntimeException("Unexpected exception opening file logger.properties.", e);
        } catch (IOException e) {
            throw new RuntimeException("Unexpected exception writing file logger.properties.", e);
        }
    }

    // opens the benchmark's data log file
    private void openLogFile() throws IOException {
        out.println();
        out.println("writing results to file:        " + logFileName);
        log = new PrintWriter(new FileWriter(logFileName, false));
    }

    // closes the benchmark's data log file
    private void closeLogFile() throws IOException {
        out.println();
        out.print("closing files ...");
        out.flush();
        if (log != null) {
            log.close();
            log = null;
        }
        out.println("               [ok]");
    }

    // ----------------------------------------------------------------------
    // benchmark operations
    // ----------------------------------------------------------------------

    abstract protected void runTests() throws Exception;

    protected void clearLogBuffers() {
        logHeader = true;
        header = new StringBuilder();
        if (logRealTime) {
            rtimes = new StringBuilder();
        }
        if (logMemUsage) {
            musage = new StringBuilder();
        }
    }
    
    protected void writeLogBuffers(String descr) {
        if (logRealTime) {
            log.println(descr + ", rtime[ms]"
                        + header.toString() + endl
                        + rtimes.toString() + endl);
        }
        if (logMemUsage) {
            log.println(descr + ", net musage[KiB]"
                        + header.toString() + endl
                        + musage.toString() + endl);
        }
    }

    protected void beginOpSeq(int nOps) {
        if (logRealTime) {
            rtimes.append(nOps);
            ta = 0;
        }
        if (logMemUsage) {
            musage.append(nOps);
            ma = 0;
        }
    }

    protected void finishOpSeq(int nOps) {
        if (logSumOfOps) {
            out.println();
            out.println("total");
            if (logRealTime) {
                out.println("tx real time                    "
                            + String.format("%,9d", ta) + " ms ");
            }
            if (logMemUsage) {
                out.println("net mem usage                   "
                            + String.format("%,9d", ma) + " KiB");
            }
        }

        if (logHeader) {
            if (logSumOfOps) {
                header.append("\ttotal");
            }
            logHeader = false;
        }
        if (logRealTime) {
            if (logSumOfOps) {
                rtimes.append("\t" + ta);
            }
            rtimes.append(endl);
        }
        if (logMemUsage) {
            if (logSumOfOps) {
                musage.append("\t" + ma);
            }
            musage.append(endl);
        }
    }

    protected void beginOp(String name) {
        out.println();
        out.println(name);

        // attempt max GC, before tx
        gc();

        if (logMemUsage) {
            m0 = rt.totalMemory() - rt.freeMemory();
        }

        if (logRealTime) {
            //t0 = System.currentTimeMillis();
            t0 = System.nanoTime() / 1000000;
        }
    }

    protected void finishOp(String name, int nOps) {
        // attempt one full GC, before timing tx end
        if (includeFullGC) {
            rt.gc();
        }

        if (logRealTime) {
            //t1 = System.currentTimeMillis();
            t1 = System.nanoTime() / 1000000;
            final long t = t1 - t0;
            final long ops = (t > 0 ? (nOps * 1000) / t : 0);
            final String df = "%,9d";
            out.println("tx real time                    "
                        + String.format(df, t) + " ms "
                        + String.format(df, ops) + " ops/s");
            //rtimes.append("\t" + (Math.round(t / 100.0) / 10.0));
            rtimes.append("\t" + t);
            ta += t;
        }

        if (logMemUsage) {
            // attempt max GC, after tx
            gc();
            m1 = rt.totalMemory() - rt.freeMemory();
            final long m0K = (m0 / 1024);
            final long m1K = (m1 / 1024);
            final long mK = m1K - m0K;
            out.println("net mem usage                   "
                        + String.format("%,9d", mK) + " KiB "
                        + String.format("%14s", "["+ m0K + "->" + m1K + "]"));
            musage.append("\t" + mK);
            ma += mK;
        }

        if (logHeader)
            header.append("\t" + name);
    }
}
