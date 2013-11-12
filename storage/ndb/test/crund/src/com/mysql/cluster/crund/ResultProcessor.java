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
import java.util.Map;
import java.util.TreeMap;
import java.text.ParseException;

import java.io.FileInputStream;
import java.io.FileReader;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.PrintWriter;
import java.io.InputStream;


/**
 * Processes the result files of the Crund benchmark.
 */
public class ResultProcessor {
    // console
    static protected final PrintWriter out = new PrintWriter(System.out, true);
    static protected final PrintWriter err = new PrintWriter(System.err, true);

    // shortcuts
    static protected final String endl = System.getProperty("line.separator");

    // command-line arguments
    static private final List<String> ilogFileNames = new ArrayList<String>();
    static private int nWarmupRuns;
    static private int largeRSDev;

    // resources
    protected ResultReporter reporter;
    protected String[] header;
    protected int nTxOps;
    protected int nRuns;
    protected double[] ravg;
    protected double[] rdev;

    // ----------------------------------------------------------------------
    // Reporting
    // ----------------------------------------------------------------------

    /**
     * Reports the results of the Crund benchmark.
     */
    public interface ResultReporter {

        /**
         * Reports a data series.
         *
         * @param       tag     a name for this data series
         * @param       op      the names of the operations
         * @param       avg     the average values
         * @param       sdev    the standard deviations
         * @param       rsdev   the relative standard deviations
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
     * Reports the results of the Crund benchmark.
     */
    static public class SimpleResultReporter implements ResultReporter {
        public void report(String tag,
                           int nRuns,
                           int nTxOps,
                           String[] op,
                           double[] avg,
                           double[] sdev,
                           double[] rsdev) {
            out.println();
            out.println("--------------------------------------------------------------------------------");
            out.println("ops: " + nTxOps + " " + tag
                        + " (skip " + largeRSDev + " of " + nRuns + " runs)");
            out.println("--------------------------------------------------------------------------------");

            // ops with large deviations
            final Map<Double,String> ldev = new TreeMap<Double,String>();

            out.println();
            out.println(String.format("%-24s", "op")
                        + String.format("%12s", "#op/metric")
                        + String.format("%12s", "avg")
                        + String.format("%12s", "sdev")
                        + String.format("%12s", "rsdev"));
            for (int i = 0; i < op.length; i++) {
                final double a = avg[i];
                final double opa = (a > 0 ? nTxOps / a : -1);
                out.println(String.format("%-24s", op[i])
                            + String.format("%12.2f", opa)
                            + String.format("%12.2f", avg[i])
                            + String.format("%12.2f", sdev[i])
                            + String.format("%11.2f%%", rsdev[i]));
                if (rsdev[i] > largeRSDev) {
                    ldev.put(-rsdev[i], op[i]);
                }
            }

            if (ldev.size() > 1) {
                out.println();
                out.println("large deviations:");
                for (Map.Entry<Double,String> e : ldev.entrySet()) {
                    out.println(String.format("%8.2f%%\t%s",
                                              -e.getKey(), e.getValue()));
                }
            }
        }
    }

    // ----------------------------------------------------------------------
    // Processing
    // ----------------------------------------------------------------------

    /**
     * Runs the result processor.
     */
    public void run() {
        try {
            init();

            for (String fn : ilogFileNames) {
                out.println();
                out.println("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
                out.println("reading log file: " + fn);
                out.println("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");

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
            long[] val = new long[values.length];
            for (int i = 1; i < values.length; i++) {
                long l;
                try {
                    val[i] = l = Long.valueOf(values[i]);
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
    // intializers/finalizers
    // ----------------------------------------------------------------------

    // initializes the result processor's resources.
    protected void init() throws Exception {
        reporter = new SimpleResultReporter();
        printProperties();
    }

    // releases the result processor's resources.
    protected void close() throws Exception {
        reporter = null;
    }

    // prints the result processor's properties
    protected void printProperties() {
        out.println("settings ...");
        out.println("nWarmupRuns:                    " + nWarmupRuns);
    }

    // ----------------------------------------------------------------------
    // usage
    // ----------------------------------------------------------------------

    // prints command-line usage and exits
    static protected void exitUsage() {
        out.println("usage: [options] <log file>...");
        out.println("    [-w <number>]          skip warmup runs (default 0)");
        out.println("    [-d <number>]          flag large rsdev (default 10)");
        out.println("    [-h|--help]            print usage message and exit");
        out.println();
        System.exit(1); // return an error code
    }

    // parses command-line arguments
    static public void parseArguments(String[] args) {
        String w = "0";
        String d = "10";
        for (int i = 0; i < args.length; i++) {
            final String arg = args[i];
            if (arg.equals("-w")) {
                if (i >= args.length)
                    exitUsage();
                w = args[++i];
            } else if (arg.equals("-d")) {
                if (i >= args.length)
                    exitUsage();
                d = args[++i];
            } else if (arg.equals("-h") || arg.equals("--help")) {
                exitUsage();
            } else if (arg.startsWith("-")) {
                out.println("unknown option: " + arg);
                exitUsage();
            } else {
                ilogFileNames.add(args[i]);
            }
        }

        try {
            nWarmupRuns = Integer.valueOf(w);
        } catch (NumberFormatException e) {
            out.println("not a number: -w '" + w + "'");
            exitUsage();
        }
        if (nWarmupRuns < 0) {
            out.println("illegal value: -w '" + w + "'");
            exitUsage();
        }
        try {
            largeRSDev = Integer.valueOf(d);
        } catch (NumberFormatException e) {
            out.println("not a number: -d '" + d + "'");
            exitUsage();
        }
        if (largeRSDev < 0) {
            out.println("illegal value: -d '" + d + "'");
            exitUsage();
        }
        if (ilogFileNames.size() == 0) {
            out.println("missing input files");
            exitUsage();
        }
    }

    static public void main(String[] args) {
        System.out.println("ResultProcessor.main()");
        parseArguments(args);
        new ResultProcessor().run();
        System.out.println();
        System.out.println("ResultProcessor.main(): done.");
    }
}
