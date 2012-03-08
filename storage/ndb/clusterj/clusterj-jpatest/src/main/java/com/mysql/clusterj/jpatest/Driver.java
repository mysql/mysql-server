/*
   Copyright 2010 Sun Microsystems, Inc.
   All rights reserved. Use is subject to license terms.

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

package com.mysql.clusterj.jpatest;

import java.util.Properties;
import java.util.List;
import java.util.Set;
import java.util.HashSet;
import java.util.ArrayList;
import java.util.Date;
import java.text.SimpleDateFormat;

import java.io.FileInputStream;
import java.io.FileWriter;
import java.io.IOException;
import java.io.PrintWriter;
import java.io.InputStream;


/**
 * This class is part of the CRUND benchmark that measures standard database operations 
 * over a series of transactions on an increasing data set.
 * <p>
 * The abstract database operations are variations of: Create,
 * Read, Update, Navigate, and Delete -- hence, the benchmark's name: CRUND.
 * <p>
 * The actual operations are defined by subclasses to allow measuring the
 * operation performance across different datastore implementations.
 *
 * @see <a href="http://www.urbandictionary.com/define.php?term=crund">Urban Dictionary: crund</a>
 * <ol>
 * <li> used to debase people who torture others with their illogical
 * attempts to make people laugh;
 * <li> reference to cracking obsolete jokes;
 * <li> a dance form;
 * <li> to hit hard or smash.
 * </ol>
 */
abstract public class Driver {

    /**
     *  The stream to write messages to.
     */
    static protected final PrintWriter out = new PrintWriter(System.out, true);

    /**
     *  The stream to write error messages to.
     */
    static protected final PrintWriter err = new PrintWriter(System.err, true);

    /**
     *  Shortcut to the end-of-line character sequence.
     */
    static protected final String endl = System.getProperty("line.separator");

    /**
     *  Shortcut to the Runtime.
     */
    static private final Runtime rt = Runtime.getRuntime();

    // command-line arguments
    static private final List<String> propFileNames = new ArrayList<String>();
    static private String logFileName
        = ("log_"
           + new SimpleDateFormat("yyyyMMdd_HHMMss").format(new Date())
           + ".txt");

    // the data output writer
    private PrintWriter log;

    // benchmark settings
    protected final Properties props = new Properties();
    protected String descr = "";
    protected boolean logRealTime = false;
    protected boolean logMemUsage = false;
    protected boolean includeFullGC = false;
    protected boolean logSumOfOps = false;
    protected boolean renewOperations = false;
    protected boolean renewConnection = false;
    protected boolean allowExtendedPC = false;
    protected int aStart = (1 << 8), aEnd = (1 << 12), aIncr = (1 << 2);
    protected int bStart = (1 << 8), bEnd = (1 << 12), bIncr = (1 << 2);
    protected int maxStringLength = 100;
    protected int warmupRuns = 0;
    protected int hotRuns = 0;
    protected final Set<String> exclude = new HashSet<String>();

    // ----------------------------------------------------------------------

    /**
     * A database operation to be benchmarked.
     */
    protected abstract class Op {
        final protected String name;

        public Op(String name) {
            this.name = name;
        }

        public String getName() {
            return name;
        }

        public abstract void run(int countA, int countB) throws Exception;
    };

    /**
     * The list of database operations to be benchmarked.
     * While the list instance is final, its content is managed by methods
     * initOperations() and closeOperations() as defined by subclasses.
     */
    protected final List<Op> ops = new ArrayList<Op>();

    // buffers collecting the header and data lines written to log
    boolean logHeader;
    private StringBuilder header;
    private StringBuilder rtimes;
    private StringBuilder musage;

    // benchmark data fields
    private long t0 = 0, t1 = 0, ta = 0;
    private long m0 = 0, m1 = 0, ma = 0;

    // benchmark methods to be defined by subclasses
    abstract protected void initConnection() throws Exception;
    abstract protected void closeConnection() throws Exception;
    abstract protected void initOperations() throws Exception;
    abstract protected void closeOperations() throws Exception;
    abstract protected void clearPersistenceContext() throws Exception;
    abstract protected void clearData() throws Exception;
    abstract protected void beginTransaction() throws Exception;
    abstract protected void commitTransaction() throws Exception;
    abstract protected void rollbackTransaction() throws Exception;

    /**
     * Reports an error if a condition is not met.
     *
     * An invariant method to ensure the consistent application
     * of verifying read results.
     */
    static protected final void verify(boolean cond) {
        //assert (cond);
        if (!cond)
            throw new RuntimeException("wrong data; verification failed");
    }

    /**
     * Loads a dynamically linked system library and reports any failures.
     */
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
        out.println("          [" + name + "]");
    }

    // ----------------------------------------------------------------------

    /**
     * Runs the entire benchmark.
     */
    public void run() {
        try {
            init();

            // warmup runs
            for (int i = 0; i < warmupRuns; i++)
                runTests();

            // truncate log file, reset log buffers
            out.println();
            out.println("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
            out.println("start logging results ...");
            out.println("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
            out.println();
            header = new StringBuilder();
            rtimes = new StringBuilder();
            musage = new StringBuilder();
            closeLogFile();
            openLogFile();

            // hot runs
            for (int i = 0; i < hotRuns; i++)
                runTests();

            // write log buffers
            if (logRealTime) {
                log.println(descr + ", rtime[ms]"
                            + header.toString() + endl
                            + rtimes.toString() + endl + endl + endl);
            }
            if (logMemUsage) {
                log.println(descr + ", net musage[KiB]"
                            + header.toString() + endl
                            + musage.toString() + endl + endl + endl);
            }

            close();
        } catch (Exception ex) {
            // end the program regardless of threads
            out.println("caught " + ex);
            ex.printStackTrace();
            System.exit(2); // return an error code
        }
    }

    /**
     * Initializes the benchmark's resources.
     */
    protected void init() throws Exception {
        loadProperties();
        initProperties();
        printProperties();
        openLogFile();

        // init log buffers
        logHeader = true;
        header = new StringBuilder();
        rtimes = new StringBuilder();
        musage = new StringBuilder();
    }

    /**
     * Releases the benchmark's resources.
     */
    protected void close() throws Exception {
        // close log buffers
        header = null;
        rtimes = null;
        musage = null;

        closeLogFile();
    }

    /**
     * Loads the benchmark's properties from properties files.
     * Keys might appear multiple times in the same or different files.
     * If there are duplicate keys, the last key definition overrides any
     * previous value for the same key in the same or different file.
     */
    private void loadProperties() throws IOException {
        if (propFileNames.size() == 0) {
            propFileNames.add("crund.properties");
        }
        
        out.println();
        for (String fn : propFileNames) {
            out.println("reading properties file:    " + fn);
            InputStream is = null;
            try {
                is = new FileInputStream(fn);
                props.load(is);
            } catch (Exception e) {
                out.println("error reading file.");
            } finally {
                if (is != null)
                    is.close();
            }
        }
    }

    /**
     * Retrieves a property's value and parses it as a boolean.
     */
    protected boolean parseBoolean(String k) {
        return Boolean.parseBoolean(props.getProperty(k));
    }
    
    /**
     * Retrieves a property's value and parses it as a signed decimal integer.
     * @throws NumberFormatException with a descriptive error message
     */
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
    
    /**
     * Initializes the benchmark properties.
     */
    protected void initProperties() {
        // initialize boolean/numeric properties
        logRealTime = parseBoolean("logRealTime");
        logMemUsage = parseBoolean("logMemUsage");
        includeFullGC = parseBoolean("includeFullGC");
        logSumOfOps = parseBoolean("logSumOfOps");
        renewOperations = parseBoolean("renewOperations");
        renewConnection = parseBoolean("renewConnection");
        allowExtendedPC = parseBoolean("allowExtendedPC");
        aStart = parseInt("aStart", 1 << 8);
        aEnd = parseInt("aEnd", 1 << 12);
        aIncr = parseInt("aIncr", 1 << 2);
        bStart = parseInt("bStart", 1 << 8);
        bEnd = parseInt("bEnd", 1 << 12);
        bIncr = parseInt("bIncr", 1 << 2);
        maxStringLength = parseInt("maxStringLength", 100);
        warmupRuns = parseInt("warmupRuns", 0);
        hotRuns = parseInt("hotRuns", 1);

        // initialize exclude set
        final String[] e = props.getProperty("exclude", "").split(",");
        for (int i = 0; i < e.length; i++) {
            exclude.add(e[i]);
        }
    }

    /**
     * Prints the benchmark's properties.
     */
    protected void printProperties() {
        //props.list(out);
        out.println();
        out.println("main settings:");
        out.println("logRealTime:                " + logRealTime);
        out.println("logMemUsage:                " + logMemUsage);
        out.println("includeFullGC:              " + includeFullGC);
        out.println("logSumOfOps:                " + logSumOfOps);
        out.println("renewOperations:            " + renewOperations);
        out.println("renewConnection:            " + renewConnection);
        out.println("allowExtendedPC:            " + allowExtendedPC);
        out.println("aStart:                     " + aStart);
        out.println("aEnd:                       " + aEnd);
        out.println("aIncr:                      " + aIncr);
        out.println("bStart:                     " + bStart);
        out.println("bEnd:                       " + bEnd);
        out.println("bIncr:                      " + bIncr);
        out.println("maxStringLength:            " + maxStringLength);
        out.println("warmupRuns:                 " + warmupRuns);
        out.println("hotRuns:                    " + hotRuns);
        out.println("exclude:                    " + exclude);
    }

    /**
     * Opens the benchmark's data log file.
     */
    private void openLogFile() throws IOException {
        out.println();
        out.println("writing results to file:    " + logFileName);
        log = new PrintWriter(new FileWriter(logFileName, false));
    }

    /**
     * Closes the benchmark's data log file.
     */
    private void closeLogFile() throws IOException {
        out.print("closing files ...    ");
        out.flush();
        if (log != null) {
            log.close();
            log = null;
        }
        out.println("       [ok]");
    }

    // ----------------------------------------------------------------------

    /**
     * Runs a series of benchmark operations on scaled-up data.
     */
    protected void runTests() throws Exception {
        initConnection();
        initOperations();

        for (int i = aStart; i <= aEnd; i *= aIncr) {
            //for (int j = bBeg; j <= bEnd; j *= bIncr)
            for (int j = (i > bStart ? i : bStart); j <= bEnd; j *= bIncr) {
                try {
                    runOperations(i, j);
                } catch (Exception ex) {
                    // already in rollback for database/orm exceptions
                    //rollbackTransaction();
                    throw ex;
                }
            }
        }

        out.println();
        out.println("------------------------------------------------------------");
        out.println();

        clearData();
        closeOperations();
        closeConnection();
    }

    /**
     * Runs a series of benchmark operations.
     */
    protected void runOperations(int countA, int countB) throws Exception {
        out.println();
        out.println("------------------------------------------------------------");
        out.println("countA = " + countA + ", countB = " + countB);
        out.println();

        // log buffers
        if (logRealTime) {
            rtimes.append("A=" + countA + ", B=" + countB);
            ta = 0;
        }
        if (logMemUsage) {
            musage.append("A=" + countA + ", B=" + countB);
            ma = 0;
        }
        
        // pre-run cleanup
        if (renewConnection) {
            closeOperations();
            closeConnection();
            initConnection();
            initOperations();
        } else if (renewOperations) {
            closeOperations();
            initOperations();
        }
        clearData();

        // run operations
        for (Op op : ops) {
            // pre-tx cleanup
            if (!allowExtendedPC) {
                // effectively prevent caching beyond Tx scope by clearing
                // any data/result caches before the next transaction
                clearPersistenceContext();
            }
            runOp(op, countA, countB);
        }
        if (logHeader) {
            if (logSumOfOps)
                header.append("\ttotal");
        }

        // log buffers
        logHeader = false;
        if (logRealTime) {
            if (logSumOfOps) {
                rtimes.append("\t" + ta);
                out.println();
                out.println("total");
                out.println("tx real time      = " + ta + "\tms [begin..commit]");
            }
            rtimes.append(endl);
        }
        if (logMemUsage) {
            if (logSumOfOps) {
                musage.append("\t" + ma);
                out.println();
                out.println("total");
                out.println("net mem usage     = " + (ma >= 0 ? "+" : "") + ma
                            + "\tKiB");
            }
            musage.append(endl);
        }               
    }

    /**
     * Runs a benchmark operation.
     */
    protected void runOp(Op op, int countA, int countB) throws Exception {
        final String name = op.getName();
        if (!exclude.contains(name)) {
            begin(name);
            op.run(countA, countB);
            commit(name);
        }
    }

    /**
     * Begins a benchmarked transaction.
     */
    protected void begin(String name) throws Exception {
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

        beginTransaction();
    }

    /**
     * Closes a benchmarked transaction.
     */
    protected void commit(String name) throws Exception {
        commitTransaction();

        // attempt one full GC, before timing tx end
        if (includeFullGC) {
            rt.gc();
        }

        if (logRealTime) {
            //t1 = System.currentTimeMillis();
            t1 = System.nanoTime() / 1000000;
            final long t = t1 - t0;
            out.println("tx real time      = " + t + "\tms [begin..commit]");
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
            out.println("net mem usage     = " + (mK >= 0 ? "+" : "") + mK
                        + "\tKiB [" + m0K + "K->" + m1K + "K]");
/*
            out.println("allocated memory  = "
                        + m1 + "\tK after commit");
            out.println("total memory      = "
                        + (rt.totalMemory() / 1024) + "\tK after commit");
            out.println("max memory        = "
                        + (rt.maxMemory() / 1024) + "\tK after commit");
*/
            musage.append("\t" + mK);
            ma += mK;
        }

        if (logHeader)
            header.append("\t" + name);
    }

    /**
     * Attempts to run the JVM's Garbage Collector.
     */
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

    // ----------------------------------------------------------------------

    /**
     * Prints a command-line usage message and exits.
     */
    static private void exitUsage() {
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
    }

    /** Clears the propFileNames from a previous run
     *
     */
    public static void clearPropFileNames() {
        propFileNames.clear();
    }

}
