/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: TestKeyRange.java,v 1.7 2004/01/28 03:36:34 bostic Exp $
 */

/*
 * NOTE: TestKeyRange is AccessExample changed to test Db.key_range.
 * See comments with ADDED for specific areas of change.
 */

package com.sleepycat.test;

import com.sleepycat.db.*;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.StringReader;
import java.io.Reader;
import java.io.IOException;
import java.io.PrintStream;

public class TestKeyRange
{
    private static final String FileName = "access.db";

    public TestKeyRange()
    {
    }

    private static void usage()
    {
        System.err.println("usage: TestKeyRange\n");
        System.exit(1);
    }

    public static void main(String argv[])
    {
        try
        {
            TestKeyRange app = new TestKeyRange();
            app.run();
        }
        catch (DbException dbe)
        {
            System.err.println("TestKeyRange: " + dbe.toString());
            System.exit(1);
        }
        catch (FileNotFoundException fnfe)
        {
            System.err.println("TestKeyRange: " + fnfe.toString());
            System.exit(1);
        }
        System.exit(0);
    }

    // Prompts for a line, and keeps prompting until a non blank
    // line is returned.  Returns null on error.
    //
    static public String askForLine(Reader reader,
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
    static public String getLine(Reader reader)
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

    public void run()
         throws DbException, FileNotFoundException
    {
        // Remove the previous database.
        new File(FileName).delete();

        // Create the database object.
        // There is no environment for this simple example.
        Db table = new Db(null, 0);
        table.setErrorStream(System.err);
        table.setErrorPrefix("TestKeyRange");
        table.open(null, FileName, null, Db.DB_BTREE, Db.DB_CREATE, 0644);

        //
        // Insert records into the database, where the key is the user
        // input and the data is the user input in reverse order.
        //
        Reader reader = new StringReader("abc\nmiddle\nzend\nmoremiddle\nZED\nMAMAMIA");

	int count= 0;/*ADDED*/
        for (;;) {
            String line = askForLine(reader, System.out, "input>");
            if (line == null)
                break;

            String reversed = (new StringBuffer(line)).reverse().toString();

            // See definition of StringDbt below
            //
            StringDbt key = new StringDbt(line);
            StringDbt data = new StringDbt(reversed);

            try
            {
                int err;
                if ((err = table.put(null, key, data, 0)) == Db.DB_KEYEXIST) {
                    System.out.println("Key " + line + " already exists.");
                }
            }
            catch (DbException dbe)
            {
                System.out.println(dbe.toString());
            }
            System.out.println("");

	    /*START ADDED*/
	    {
		    if (count++ > 0) {
			    DbKeyRange range = new DbKeyRange();
			    table.keyRange(null, key, range, 0);
			    System.out.println("less: " + range.less);
			    System.out.println("equal: " + range.equal);
			    System.out.println("greater: " + range.greater);
		    }
	    }
	    /*END ADDED*/

        }

        // Acquire an iterator for the table.
        Dbc iterator;
        iterator = table.cursor(null, 0);

        // Walk through the table, printing the key/data pairs.
        // See class StringDbt defined below.
        //
        StringDbt key = new StringDbt();
        StringDbt data = new StringDbt();
        while (iterator.get(key, data, Db.DB_NEXT) == 0)
        {
            System.out.println(key.getString() + " : " + data.getString());
        }
        iterator.close();
        table.close(0);
    }

    // Here's an example of how you can extend a Dbt in a straightforward
    // way to allow easy storage/retrieval of strings, or whatever
    // kind of data you wish.  We've declared it as a static inner
    // class, but it need not be.
    //
    static /*inner*/
    class StringDbt extends Dbt
    {
        StringDbt()
        {
            setFlags(Db.DB_DBT_MALLOC); // tell Db to allocate on retrieval
        }

        StringDbt(String value)
        {
            setString(value);
            setFlags(Db.DB_DBT_MALLOC); // tell Db to allocate on retrieval
        }

        void setString(String value)
        {
            setData(value.getBytes());
            setSize(value.length());
        }

        String getString()
        {
            return new String(getData(), 0, getSize());
        }
    }
}
