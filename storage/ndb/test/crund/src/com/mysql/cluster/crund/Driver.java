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

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.PrintWriter;
import java.io.StringWriter;


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
    static protected final String eol = System.getProperty("line.separator");
    static protected final Runtime rt = Runtime.getRuntime();

    // driver command-line arguments
    static private final List<String> propFileNames = new ArrayList<String>();
    static private String logFileName;

    // driver settings
    protected final Properties props = new Properties();
    protected int nRuns;
    protected boolean logRealTime;
    protected boolean logMemUsage;
    protected boolean logSumOfOps;
    protected boolean includeFullGC;
    protected boolean failOnError;
    protected final List<String> loadClassNames = new ArrayList<String>();

    // driver resources
    protected boolean hasIgnoredSettings;
    protected PrintWriter log;
    protected boolean logHeader = true;
    protected StringBuilder header = new StringBuilder();
    protected StringBuilder rtimes = new StringBuilder();
    protected StringBuilder musage = new StringBuilder();
    protected StringBuilder errors = new StringBuilder();
    protected long t0 = 0, t1 = 0, ta = 0;
    protected long m0 = 0, m1 = 0, ma = 0;
    protected final List<Load> loads = new ArrayList<Load>();

    // ----------------------------------------------------------------------
    // driver usage
    // ----------------------------------------------------------------------

    /**
     * Prints a command-line usage message and exits.
     */
    static protected void exitUsage() {
        out.println("usage: [options]");
        out.println("    [-p <file name>]...    properties file name");
        out.println("    [-l <file name>]       log file name for results");
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
            runLoads();
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

    // loads a dynamically linked system library
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
        out.println("              [ok: " + name + "]");
    }

    // initializes the driver's resources
    protected void init() throws Exception {
        loadProperties();
        initProperties();
        printProperties();
        writeProperties();
        openLogFile();
        clearLogBuffers();
        initLoads();
    }

    // releases the driver's resources
    protected void close() throws Exception {
        closeLoads();
        clearLogBuffers();
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

    protected boolean parseBoolean(String k, boolean vdefault) {
        final String v = props.getProperty(k);
        return (v == null ? vdefault : Boolean.parseBoolean(v));
    }

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

    protected void initProperties() {
        out.println();
        out.print("reading driver properties ...");
        out.flush();
        final StringBuilder msg = new StringBuilder();

        // allow implementations under test to use java.util.logging
        System.setProperty("java.util.logging.config.file",
                           "logging.properties");

        nRuns = parseInt("nRuns", 1);
        if (nRuns < 1) {
            msg.append("[IGNORED] nRuns:                " + nRuns + eol);
            nRuns = 1;
        }

        logRealTime = parseBoolean("logRealTime", true);
        logMemUsage = parseBoolean("logMemUsage", false);
        logSumOfOps = parseBoolean("logSumOfOps", true);
        includeFullGC = parseBoolean("includeFullGC", false);
        failOnError = parseBoolean("failOnError", true);

        // initialize load classes set
        final String[] loadsProp = props.getProperty("loads", "").split(",");
        for (String s : loadsProp) {
            if (s.isEmpty()) {
                // skip
            } else if (s.contains(".")) {
                loadClassNames.add(s); // use qualified class name
            } else {
                loadClassNames.add("com.mysql.cluster.crund." + s);
            }
        }

        if (msg.length() == 0) {
            out.println("   [ok: nRuns=" + nRuns + "]");
        } else {
            hasIgnoredSettings = true;
            out.println();
            out.print(msg.toString());
        }
    }

    protected void printProperties() {
        out.println();
        out.println("driver settings ...");
        out.println("nRuns:                          " + nRuns);
        out.println("logRealTime:                    " + logRealTime);
        out.println("logMemUsage:                    " + logMemUsage);
        out.println("logSumOfOps:                    " + logSumOfOps);
        out.println("includeFullGC:                  " + includeFullGC);
        out.println("failOnError:                    " + failOnError);
        final List<String> lcn = new ArrayList<String>(loadClassNames);
        out.println("loads:                          "
                    + (lcn.isEmpty() ? "[]" : lcn.remove(0)));
        for (String s : lcn)
            out.println("                                " + s);
    }

    protected void writeProperties() {
        final String fileName = "logging.properties";
        final File logger = new File(fileName);
        final OutputStream out;
        try {
            if (!logger.exists())
                logger.createNewFile();
            out = new FileOutputStream(logger);
            props.store(out, "**** WARNING: DO NOT EDIT THIS FILE; IT IS GENERATED EACH RUN.");
            final Properties sprops = System.getProperties();
            sprops.store(out, "**** SYSTEM PROPERTIES:");
        } catch (FileNotFoundException e) {
            throw new RuntimeException("Unexpected exception opening file logger.properties.", e);
        } catch (IOException e) {
            throw new RuntimeException("Unexpected exception writing file logger.properties.", e);
        }
    }

    private void openLogFile() throws IOException {
        out.println();
        out.println("writing results to file:        " + logFileName);
        log = new PrintWriter(new FileWriter(logFileName, false));
    }

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

    protected void addLoad(Load load) {
        loads.add(load);
    }

    protected Load createLoad(String className) throws Exception {
        Load load;
        final Class<?> a = Class.forName(className);
        final Class<? extends Load> c = a.asSubclass(Load.class);
        load = c.getConstructor(CrundDriver.class).newInstance(this);
        return load;
    }

    protected void addLoads() throws Exception {
        for (String s : loadClassNames) {
            final StringBuilder msg = new StringBuilder();
            out.print("instantiating load ...");
            try {
                createLoad(s);
            } catch (Exception e) {
                msg.append("caught " + e + eol);
                msg.append("[SKIPPING] load class:          " + s + eol);
            }
            if (msg.length() == 0) {
                final String c = s.replaceAll(".*\\.", "");
                out.println("          [ok: " + c + "]");
            } else {
                hasIgnoredSettings = true;
                out.println();
                out.print(msg.toString());
            }
        }
    }

    protected void initLoads() throws Exception {
        out.println();

        if (loads.isEmpty())
            addLoads();

        if (loads.isEmpty())
            out.println("++++++++++  NOTHING TO TO, NO LOAD CLASSES GIVEN  ++++++++++");

        for (Load l : loads)
            l.init();
    }

    protected void closeLoads() throws Exception {        
        for (Load l : loads)
            l.close();
        loads.clear();
    }

    // ----------------------------------------------------------------------
    // benchmark operations
    // ----------------------------------------------------------------------

    abstract protected void runLoad(Load load) throws Exception;

    protected void runLoads() throws Exception {
        if (hasIgnoredSettings) {
            out.println();
            out.println("++++++++++++  SOME SETTINGS IGNORED, SEE ABOVE  ++++++++++++");
        }

        for (Load l : loads)
            runLoad(l);
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

    protected void logError(String load, String op, Exception e) {
        out.println("!!! ERRORS OCCURRED, SEE LOG FILE: " + logFileName);
        errors.append(eol + "****************************************" + eol);
        errors.append("Error in load: " + load + eol);
        errors.append("operation: " + op + eol);
        errors.append("exception: " + e + eol + eol);
        final StringWriter s = new StringWriter();
        e.printStackTrace(new PrintWriter(s));
        errors.append(s);

        if (failOnError)
            abortIfErrors();
    }

    protected void abortIfErrors() {
        if (errors.length() != 0) {
            log.println("!!! ERRORS OCCURRED:");
            log.println(errors.toString() + eol);
            log.close();
            String msg = "Errors occurred, see log file " + logFileName;
            throw new RuntimeException(msg);
        }
    }

    protected void clearLogBuffers() {
        logHeader = true;
        header = new StringBuilder();
        rtimes = new StringBuilder();
        musage = new StringBuilder();
        errors = new StringBuilder();
    }

    protected void writeLogBuffers(String prefix) {
        if (logRealTime) {
            log.println("rtime[ms]," + prefix
                        + header.toString() + eol
                        + rtimes.toString() + eol);
        }
        if (logMemUsage) {
            log.println("net_mem_usage[KiB]," + prefix
                        + header.toString() + eol
                        + musage.toString() + eol);
        }
        abortIfErrors();
        clearLogBuffers();
    }

    protected void beginOps(int nOps) {
        if (logRealTime) {
            rtimes.append(nOps);
            ta = 0;
        }
        if (logMemUsage) {
            musage.append(nOps);
            ma = 0;
        }
    }

    protected void finishOps(int nOps) {
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
            rtimes.append(eol);
        }
        if (logMemUsage) {
            if (logSumOfOps) {
                musage.append("\t" + ma);
            }
            musage.append(eol);
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
        if (includeFullGC)
            rt.gc();

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
