/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: BtRecExample.java,v 11.6 2000/02/19 20:58:02 bostic Exp $
 */

package com.sleepycat.examples;

import com.sleepycat.db.*;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.InputStreamReader;
import java.io.IOException;
import java.io.PrintStream;

public class BtRecExample
{
    static final String progname =  "BtRecExample";	// Program name.
    static final String database =  "access.db";
    static final String wordlist =  "../test/wordlist";

    BtRecExample(BufferedReader reader)
        throws DbException, IOException, FileNotFoundException
    {
        int ret;

        // Remove the previous database.
        File f = new File(database);
        f.delete();

        dbp = new Db(null, 0);

        dbp.set_error_stream(System.err);
        dbp.set_errpfx(progname);
        dbp.set_pagesize(1024);			// 1K page sizes.

        dbp.set_flags(Db.DB_RECNUM);			// Record numbers.
        dbp.open(database, null, Db.DB_BTREE, Db.DB_CREATE, 0664);

        //
        // Insert records into the database, where the key is the word
        // preceded by its record number, and the data is the same, but
        // in reverse order.
        //

        for (int cnt = 1; cnt <= 1000; ++cnt) {
            String numstr = String.valueOf(cnt);
            while (numstr.length() < 4)
                numstr = "0" + numstr;
            String buf = numstr + '_' + reader.readLine();
            StringBuffer rbuf = new StringBuffer(buf).reverse();

            StringDbt key = new StringDbt(buf);
            StringDbt data = new StringDbt(rbuf.toString());

            if ((ret = dbp.put(null, key, data, Db.DB_NOOVERWRITE)) != 0) {
                if (ret != Db.DB_KEYEXIST)
                    throw new DbException("Db.put failed" + ret);
            }
        }
    }

    void run()
        throws DbException
    {
        int recno;
        int ret;

        // Acquire a cursor for the database.
        dbcp = dbp.cursor(null, 0);

        //
        // Prompt the user for a record number, then retrieve and display
        // that record.
        //
        InputStreamReader reader = new InputStreamReader(System.in);

        for (;;) {
            // Get a record number.
            String line = askForLine(reader, System.out, "recno #> ");
            if (line == null)
                break;

            try {
                recno = Integer.parseInt(line);
            }
            catch (NumberFormatException nfe) {
                System.err.println("Bad record number: " + nfe);
                continue;
            }

            //
            // Start with a fresh key each time, the dbp.get() routine returns
            // the key and data pair, not just the key!
            //
            RecnoStringDbt key = new RecnoStringDbt(recno, 100);
            RecnoStringDbt data = new RecnoStringDbt(100);

            if ((ret = dbcp.get(key, data, Db.DB_SET_RECNO)) != 0) {
                throw new DbException("Dbc.get failed", ret);
            }

            // Display the key and data.
            show("k/d\t", key, data);

            // Move the cursor a record forward.
            if ((ret = dbcp.get(key, data, Db.DB_NEXT)) != 0) {
                throw new DbException("Dbc.get failed", ret);
            }

            // Display the key and data.
            show("next\t", key, data);

            RecnoStringDbt datano = new RecnoStringDbt(100);

            //
            // Retrieve the record number for the following record into
            // local memory.
            //
            if ((ret = dbcp.get(key, datano, Db.DB_GET_RECNO)) != 0) {
                if (ret != Db.DB_NOTFOUND && ret != Db.DB_KEYEMPTY) {
                    throw new DbException("Dbc.get failed", ret);
                }
            }
            else {
                recno = datano.getRecno();
                System.out.println("retrieved recno: " + recno);
            }
        }

        dbcp.close();
        dbcp = null;
    }

    //
    // Print out the number of records in the database.
    //
    void stats()
        throws DbException
    {
        DbBtreeStat statp;

        statp = (DbBtreeStat)dbp.stat(0);
        System.out.println(progname + ": database contains " +
                          statp.bt_ndata + " records");
    }

    void show(String msg, RecnoStringDbt key, RecnoStringDbt data)
        throws DbException
    {
        System.out.println(msg + key.getString() + ": " + data.getString());
    }

    public void shutdown()
        throws DbException
    {
        if (dbcp != null) {
            dbcp.close();
            dbcp = null;
        }
        if (dbp != null) {
            dbp.close(0);
            dbp = null;
        }
    }

    public static void main(String argv[])
    {

        try {
            // Open the word database.
            FileReader freader = new FileReader(wordlist);

	    BtRecExample app = new BtRecExample(new BufferedReader(freader));

	    // Close the word database.
            freader.close();
            freader = null;

            app.stats();
            app.run();
        }
        catch (FileNotFoundException fnfe) {
            System.err.println(progname + ": unexpected open error " + fnfe);
            System.exit (1);
        }
        catch (IOException ioe) {
            System.err.println(progname + ": open " + wordlist + ": " + ioe);
            System.exit (1);
        }
	catch (DbException dbe) {
	    System.err.println("Exception: " + dbe);
	    System.exit(dbe.get_errno());
	}

	System.exit(0);
    }

    void
    usage()
    {
	System.err.println("usage: " + progname);
	System.exit(1);
    }

    // Prompts for a line, and keeps prompting until a non blank
    // line is returned.  Returns null on error.
    //
    static public String askForLine(InputStreamReader reader,
                                    PrintStream out, String prompt)
    {
        String result = "";
        while (result != null && result.length() == 0) {
            out.print(prompt);
            out.flush();
            result = getLine(reader);
        }
        return result;
    }

    // Not terribly efficient, but does the job.
    // Works for reading a line from stdin or a file.
    // Returns null on EOF.  If EOF appears in the middle
    // of a line, returns that line, then null on next call.
    //
    static public String getLine(InputStreamReader reader)
    {
        StringBuffer b = new StringBuffer();
        int c;
        try {
            while ((c = reader.read()) != -1 && c != '\n') {
                if (c != '\r')
                    b.append((char)c);
            }
        }
        catch (IOException ioe) {
            c = -1;
        }

        if (c == -1 && b.length() == 0)
            return null;
        else
            return b.toString();
    }

    private Dbc dbcp;
    private Db dbp;

    // Here's an example of how you can extend a Dbt in a straightforward
    // way to allow easy storage/retrieval of strings.
    // We've declared it as a static inner class, but it need not be.
    //
    static /*inner*/
    class StringDbt extends Dbt
    {
        StringDbt(byte[] arr)
        {
            set_flags(Db.DB_DBT_USERMEM);
            set_data(arr);
            set_size(arr.length);
        }

        StringDbt()
        {
            set_flags(Db.DB_DBT_MALLOC); // tell Db to allocate on retrieval
        }

        StringDbt(String value)
        {
            setString(value);
            set_flags(Db.DB_DBT_MALLOC); // tell Db to allocate on retrieval
        }

        void setString(String value)
        {
            set_data(value.getBytes());
            set_size(value.length());
            // must set ulen because sometimes a string is returned
            set_ulen(value.length());
        }

        String getString()
        {
            return new String(get_data(), 0, get_size());
        }
    }

    // Here's an example of how you can extend a Dbt to store
    // (potentially) both recno's and strings in the same
    // structure.
    //
    static /*inner*/
    class RecnoStringDbt extends Dbt
    {
        RecnoStringDbt(int maxsize)
        {
            this(0, maxsize);     // let other constructor do most of the work
        }

        RecnoStringDbt(int value, int maxsize)
        {
            set_flags(Db.DB_DBT_USERMEM); // do not allocate on retrieval
            arr = new byte[maxsize];
            set_data(arr);                // use our local array for data
            set_ulen(maxsize);           // size of return storage
            setRecno(value);
        }

        RecnoStringDbt(String value, int maxsize)
        {
            set_flags(Db.DB_DBT_USERMEM); // do not allocate on retrieval
            arr = new byte[maxsize];
            set_data(arr);                // use our local array for data
            set_ulen(maxsize);           // size of return storage
            setString(value);
        }

        void setRecno(int value)
        {
            set_recno_key_data(value);
            set_size(arr.length);
        }

        void setString(String value)
        {
            set_data(value.getBytes());
            set_size(value.length());
        }

        int getRecno()
        {
            return get_recno_key_data();
        }

        String getString()
        {
            return new String(get_data(), 0, get_size());
        }

        byte arr[];
    }
}
