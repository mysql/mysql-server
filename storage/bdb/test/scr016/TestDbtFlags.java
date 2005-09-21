/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: TestDbtFlags.java,v 1.8 2004/01/28 03:36:34 bostic Exp $
 */

package com.sleepycat.test;

import com.sleepycat.db.*;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.InputStreamReader;
import java.io.IOException;
import java.io.PrintStream;

public class TestDbtFlags
{
    private static final String FileName = "access.db";
    private int flag_value;
    private int buf_size;
    private int cur_input_line = 0;

    /*zippy quotes for test input*/
    static final String[] input_lines = {
        "If we shadows have offended",
        "Think but this, and all is mended",
        "That you have but slumber'd here",
        "While these visions did appear",
        "And this weak and idle theme",
        "No more yielding but a dream",
        "Gentles, do not reprehend",
        "if you pardon, we will mend",
        "And, as I am an honest Puck, if we have unearned luck",
        "Now to 'scape the serpent's tongue, we will make amends ere long;",
        "Else the Puck a liar call; so, good night unto you all.",
        "Give me your hands, if we be friends, and Robin shall restore amends."
    };

    public TestDbtFlags(int flag_value, int buf_size)
    {
        this.flag_value = flag_value;
        this.buf_size = buf_size;
    }

    public static void runWithFlags(int flag_value, int size)
    {
        String msg = "=-=-=-= Test with DBT flags " + flag_value +
            " bufsize " + size;
        System.out.println(msg);
        System.err.println(msg);

        try
        {
            TestDbtFlags app = new TestDbtFlags(flag_value, size);
            app.run();
        }
        catch (DbException dbe)
        {
            System.err.println("TestDbtFlags: " + dbe.toString());
            System.exit(1);
        }
        catch (FileNotFoundException fnfe)
        {
            System.err.println("TestDbtFlags: " + fnfe.toString());
            System.exit(1);
        }
    }

    public static void main(String argv[])
    {
        runWithFlags(Db.DB_DBT_MALLOC, -1);
        runWithFlags(Db.DB_DBT_REALLOC, -1);
        runWithFlags(Db.DB_DBT_USERMEM, 20);
        runWithFlags(Db.DB_DBT_USERMEM, 50);
        runWithFlags(Db.DB_DBT_USERMEM, 200);
        runWithFlags(0, -1);

        System.exit(0);
    }

    String get_input_line()
    {
        if (cur_input_line >= input_lines.length)
            return null;
        return input_lines[cur_input_line++];
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
        table.setErrorPrefix("TestDbtFlags");
        table.open(null, FileName, null, Db.DB_BTREE, Db.DB_CREATE, 0644);

        //
        // Insert records into the database, where the key is the user
        // input and the data is the user input in reverse order.
        //
        for (;;) {
            //System.err.println("input line " + cur_input_line);
            String line = get_input_line();
            if (line == null)
                break;

            String reversed = (new StringBuffer(line)).reverse().toString();

            // See definition of StringDbt below
            //
            StringDbt key = new StringDbt(line, flag_value);
            StringDbt data = new StringDbt(reversed, flag_value);

            try
            {
                int err;
                if ((err = table.put(null,
		    key, data, Db.DB_NOOVERWRITE)) == Db.DB_KEYEXIST) {
                        System.out.println("Key " + line + " already exists.");
                }
                key.check_flags();
                data.check_flags();
            }
            catch (DbException dbe)
            {
                System.out.println(dbe.toString());
            }
        }

        // Acquire an iterator for the table.
        Dbc iterator;
        iterator = table.cursor(null, 0);

        // Walk through the table, printing the key/data pairs.
        // See class StringDbt defined below.
        //
        StringDbt key = new StringDbt(flag_value, buf_size);
        StringDbt data = new StringDbt(flag_value, buf_size);

        int iteration_count = 0;
        int dbreturn = 0;

        while (dbreturn == 0) {
            //System.err.println("iteration " + iteration_count);
            try {
                if ((dbreturn = iterator.get(key, data, Db.DB_NEXT)) == 0) {
                    System.out.println(key.get_string() + " : " + data.get_string());
                }
            }
            catch (DbMemoryException dme) {
                /* In a real application, we'd normally increase
                 * the size of the buffer.  Since we've created
                 * this error condition for testing, we'll just report it.
                 * We still need to skip over this record, and we don't
                 * want to mess with our original Dbt's, since we want
                 * to see more errors.  So create some temporary
                 * mallocing Dbts to get this record.
                 */
                System.err.println("exception, iteration " + iteration_count +
                                   ": " + dme);
                System.err.println("  key size: " + key.getSize() +
                                   " ulen: " + key.getUserBufferLength());
                System.err.println("  data size: " + key.getSize() +
                                   " ulen: " + key.getUserBufferLength());

                dme.getDbt().setSize(buf_size);
                StringDbt tempkey = new StringDbt(Db.DB_DBT_MALLOC, -1);
                StringDbt tempdata = new StringDbt(Db.DB_DBT_MALLOC, -1);
                if ((dbreturn = iterator.get(tempkey, tempdata, Db.DB_NEXT)) != 0) {
                    System.err.println("cannot get expected next record");
                    return;
                }
                System.out.println(tempkey.get_string() + " : " +
                                   tempdata.get_string());
            }
            iteration_count++;
        }
        key.check_flags();
        data.check_flags();

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
        int saved_flags;

        StringDbt(int flags, int buf_size)
        {
            this.saved_flags = flags;
            setFlags(saved_flags);
            if (buf_size != -1) {
                setData(new byte[buf_size]);
                setUserBufferLength(buf_size);
            }
        }

        StringDbt(String value, int flags)
        {
            this.saved_flags = flags;
            setFlags(saved_flags);
            set_string(value);
        }

        void set_string(String value)
        {
            setData(value.getBytes());
            setSize(value.length());
            check_flags();
        }

        String get_string()
        {
            check_flags();
            return new String(getData(), 0, getSize());
        }

        void check_flags()
        {
            int actual_flags = getFlags();
            if (actual_flags != saved_flags) {
                System.err.println("flags botch: expected " + saved_flags +
                                   ", got " + actual_flags);
            }
        }
    }
}
