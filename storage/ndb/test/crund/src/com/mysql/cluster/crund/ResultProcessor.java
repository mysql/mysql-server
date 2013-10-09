/* -*- mode: java; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=4:tabstop=4:smarttab:
 *
 * Copyright (c) 2011, 2012, Oracle and/or its affiliates. All rights reserved.
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

package com.mysql.cluster.crund;

import java.util.Properties;
import java.util.List;
import java.util.ArrayList;
import java.text.ParseException;
import java.text.DecimalFormat;

import java.io.FileInputStream;
import java.io.FileReader;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.PrintWriter;
import java.io.InputStream;


/**
 * This class parses and processes the result files of the Crund benchmark.
 */
public class ResultProcessor {

    /**
     * This class reports the results of the Crund benchmark.
     */
    public interface ResultReporter {

        /**
         * Reports a data series.
         *
         * @param	tag	a name for this data series
         * @param	op	the names of the operations
         * @param	avg	the average values
         * @param	avg	the standard deviations
         * @param	avg	the relative standard deviations
         */
        void report(String tag,
                    int nRuns,
                    int nTxOps,
                    String[] op,
                    double[] avg,
                    double[] sdev,
                    double[] rsdev);
    }

    /**
     * This class reports the results of the Crund benchmark.
     */
    static public class SimpleResultReporter implements ResultReporter {
        static protected final DecimalFormat df = new DecimalFormat("#.##");

        /**
         * 
         */
        public void report(String tag,
                           int nRuns,
                           int nTxOps,
                           String[] op,
                           double[] avg,
                           double[] sdev,
                           double[] rsdev) {
            out.println();
            out.println("tag    = " + tag);
            out.println("nRuns  = " + nRuns);
            out.println("nTxOps = " + nTxOps);
            out.println();

            // ops with large deviations
            final double thres = 10.0;
            final List<String> problematic = new ArrayList<String>();

            for (int i = 0; i < op.length; i++) {
                out.println("op     = " + op[i]);
                out.println("avg    = " + df.format(avg[i]));
                out.println("sdev   = " + df.format(sdev[i]));
                out.println("rsdev  = " + df.format(rsdev[i]) + "%");
                out.println();

                if (rsdev[i] > thres) {
                    problematic.add(df.format(rsdev[i]) + "%\t" + op[i]);
                }
            }
            
            if (problematic.size() > 1) {
                out.println("operations with large deviations:");
                for (String p : problematic) {
                    out.println("    " + p);
                }
            }
        }
    }

    // console
    static protected final PrintWriter out = new PrintWriter(System.out, true);
    static protected final PrintWriter err = new PrintWriter(System.err, true);

    // shortcuts
    static protected final String endl = System.getProperty("line.separator");

    // result processor command-line arguments
    static private final List<String> propFileNames = new ArrayList<String>();
    static private final List<String> ilogFileNames = new ArrayList<String>();

    // result processor settings
    protected final Properties props = new Properties();
    protected int nWarmupRuns;

    // result processor resources
    protected ResultReporter reporter;
    protected String[] header;
    protected int nTxOps;
    protected int nRuns;
    protected double[] ravg;
    protected double[] rdev;

    // ----------------------------------------------------------------------
    // result processor usage
    // ----------------------------------------------------------------------

    /**
     * Prints a command-line usage message and exits.
     */
    static protected void exitUsage() {
        out.println("usage: [options] <log file>...");
        out.println("    [-p <file name>]...    a properties file name");
        out.println("    [-h|--help]            print usage message and exit");
        out.println();
        System.exit(1); // return an error code
    }

    /**
     * Parses the result processor's command-line arguments.
     */
    static public void parseArguments(String[] args) {
        for (int i = 0; i < args.length; i++) {
            final String arg = args[i];
            if (arg.equals("-p")) {
                if (i >= args.length) {
                    exitUsage();
                }
                propFileNames.add(args[++i]);
            } else if (arg.equals("-h") || arg.equals("--help")) {
                exitUsage();
            } else if (arg.startsWith("-")) {
                out.println("unknown option: " + arg);
                exitUsage();
            } else {
                ilogFileNames.add(args[i]);
            }
        }

        if (propFileNames.size() == 0) {
            propFileNames.add("crundResult.properties");
        }

        if (ilogFileNames.size() == 0) {
            out.println("missing input files");
            exitUsage();
        }
    }

    // ----------------------------------------------------------------------

    /**
     * Creates an instance.
     */
    public ResultProcessor() {}

    /**
     * Runs the result processor.
     */
    public void run() {
        try {
            init();

            for (String fn : ilogFileNames) {
                out.println();
                out.println("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
                out.println("reading log file:        " + fn);
                out.println("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");

                BufferedReader ilog = null;
                try {
                    ilog = new BufferedReader(new FileReader(fn));
                    process(ilog);
                } finally {
                    if (ilog != null)
                        ilog.close();
                }
            }
            
            close();
        } catch (Exception ex) {
            // end the program regardless of threads
            out.println("caught " + ex);
            ex.printStackTrace();
            System.exit(2); // return an error code
        }
    }

    // processes the log file
    protected void process(BufferedReader ilog)
        throws IOException, ParseException {

        int lineNo = 1;
        String line;
        int nIgnored = nWarmupRuns;
        while (true) {
            line = ilog.readLine();
            if (line == null) {
                return; // eof
            }
            lineNo++;
            //out.println(line);

            // skip comments
            if (line.startsWith("#")) {
                continue;
            }

            // parse empty line
            if (line.equals("")) {
                if (header != null) {
                    report();
                }

                header = null;
                ravg = null;
                rdev = null;
                continue;
            }

            // parse header line
            if (header == null) {
                header = line.split("\\t");
                assert (header.length > 0);

                nRuns = 0;
                nTxOps = 0;
                ravg = new double[header.length];
                rdev = new double[header.length];
                continue;
            }

            // parse value line
            final String[] values = line.split("\\t");
            if (values.length != header.length) {
                String msg = ("line # " + lineNo
                              + ": wrong number of tokens; "
                              + "expected: " + header.length
                              + ", found: " + values.length);
                throw new ParseException(msg, 0);
            }
            nRuns++;

            // parse nTxOps
            int n;
            try {
                n = Integer.valueOf(values[0]);
            } catch (NumberFormatException e) {
                String msg = ("line # " + lineNo
                              + ": " + e);
                throw new ParseException(msg, 0);
            }
            if (nRuns == 1) {
                nTxOps = n;
            } else if (nTxOps != n) {
                String msg = ("line # " + lineNo
                              + ": unexpected nTxOps; "
                              + "expected: " + nTxOps
                              + ", found: " + n);
                throw new ParseException(msg, 0);
            }

            // skip warmup runs
            if (nRuns <= nIgnored) {
                nRuns--;
                nIgnored--;
                continue;
            }

            // parse values
            for (int i = 1; i < values.length; i++) {
                long l;
                try {
                    l = Long.valueOf(values[i]);
                } catch (NumberFormatException e) {
                    String msg = ("line # " + lineNo
                                  + ": " + e);
                    throw new ParseException(msg, i);
                }

                // compute running averages and squared deviations
                final double v = l;
                final double oavg = ravg[i];
                final double navg = oavg + (v - oavg) / nRuns;
                final double odev = rdev[i];
                final double ndev = odev + (v - oavg) * (v - navg);
                ravg[i] = navg;
                rdev[i] = ndev;
            }
        }
    }

    protected void report() {
        // result data
        final int nops = header.length - 1;
        final String[] op = new String[nops];
        final double[] avg = new double[nops];
        final double[] sdev = new double[nops];
        final double[] rsdev = new double[nops];

        // compute results
        for (int i = 1; i <= nops; i++) {
            op[i-1] = header[i];
            avg[i-1] = ravg[i];
            sdev[i-1] = Math.sqrt(rdev[i] / nRuns);
            rsdev[i-1] = (sdev[i-1] * 100.0) / avg[i-1];
        }
        final String tag = header[0];

        reporter.report(tag, nRuns, nTxOps, op, avg, sdev, rsdev);
    }
    
    // ----------------------------------------------------------------------
    // result processor intializers/finalizers
    // ----------------------------------------------------------------------

    // initializes the result processor's resources.
    protected void init() throws Exception {
        loadProperties();
        initProperties();
        printProperties();
        reporter = new SimpleResultReporter();
    }

    // releases the result processor's resources.
    protected void close() throws Exception {
        reporter = null;
        props.clear();
    }

    // loads the result processor's properties from properties files
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
                "invalid value of property ('" + k + "', '"
                + v + "').");
            nfe.initCause(e);
            throw nfe;
        }
    }

    // initializes the result processor properties
    protected void initProperties() {
        //props.list(out);
        out.print("setting properties ...");
        out.flush();

        final StringBuilder msg = new StringBuilder();
        final String eol = System.getProperty("line.separator");

        nWarmupRuns = parseInt("nWarmupRuns", 0);
        if (nWarmupRuns < 0) {
            msg.append("[ignored] nWarmupRuns:          " + nWarmupRuns + eol);
            nWarmupRuns = 0;
        }

        if (msg.length() == 0) {
            out.println("          [ok]");
        } else {
            out.println();
            out.print(msg.toString());
        }
    }

    // prints the result processor's properties
    protected void printProperties() {
        out.println();
        out.println("result processor settings ...");
        out.println("nWarmupRuns:                    " + nWarmupRuns);
    }

    // ----------------------------------------------------------------------

    static public void main(String[] args) {
        System.out.println("ResultProcessor.main()");
        parseArguments(args);
        new ResultProcessor().run();
        System.out.println();
        System.out.println("ResultProcessor.main(): done.");
    }
}
