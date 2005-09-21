/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: TestAppendRecno.java,v 1.6 2004/01/28 03:36:34 bostic Exp $
 */

package com.sleepycat.test;

import com.sleepycat.db.*;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.InputStreamReader;
import java.io.IOException;
import java.io.PrintStream;

public class TestAppendRecno
    implements DbAppendRecno
{
    private static final String FileName = "access.db";
    int callback_count = 0;
    Db table = null;

    public TestAppendRecno()
    {
    }

    private static void usage()
    {
        System.err.println("usage: TestAppendRecno\n");
        System.exit(1);
    }

    public static void main(String argv[])
    {
        try
        {
            TestAppendRecno app = new TestAppendRecno();
            app.run();
        }
        catch (DbException dbe)
        {
            System.err.println("TestAppendRecno: " + dbe.toString());
            System.exit(1);
        }
        catch (FileNotFoundException fnfe)
        {
            System.err.println("TestAppendRecno: " + fnfe.toString());
            System.exit(1);
        }
        System.exit(0);
    }

    public void run()
         throws DbException, FileNotFoundException
    {
        // Remove the previous database.
        new File(FileName).delete();

        // Create the database object.
        // There is no environment for this simple example.
        table = new Db(null, 0);
        table.set_error_stream(System.err);
        table.set_errpfx("TestAppendRecno");
        table.set_append_recno(this);

        table.open(null, FileName, null, Db.DB_RECNO, Db.DB_CREATE, 0644);
        for (int i=0; i<10; i++) {
            System.out.println("\n*** Iteration " + i );
            try {
                RecnoDbt key = new RecnoDbt(77+i);
                StringDbt data = new StringDbt("data" + i + "_xyz");
                table.put(null, key, data, Db.DB_APPEND);
            }
            catch (DbException dbe) {
                System.out.println("dbe: " + dbe);
            }
        }

        // Acquire an iterator for the table.
        Dbc iterator;
        iterator = table.cursor(null, 0);

        // Walk through the table, printing the key/data pairs.
        // See class StringDbt defined below.
        //
        RecnoDbt key = new RecnoDbt();
        StringDbt data = new StringDbt();
        while (iterator.get(key, data, Db.DB_NEXT) == 0)
        {
            System.out.println(key.getRecno() + " : " + data.getString());
        }
        iterator.close();
        table.close(0);
        System.out.println("Test finished.");
    }

    public void db_append_recno(Db db, Dbt dbt, int recno)
        throws DbException
    {
        int count = callback_count++;

        System.out.println("====\ncallback #" + count);
        System.out.println("db is table: " + (db == table));
        System.out.println("recno = " + recno);

        // This gives variable output.
        //System.out.println("dbt = " + dbt);
        if (dbt instanceof RecnoDbt) {
            System.out.println("dbt = " +
                               ((RecnoDbt)dbt).getRecno());
        }
        else if (dbt instanceof StringDbt) {
            System.out.println("dbt = " +
                               ((StringDbt)dbt).getString());
        }
        else {
            // Note: the dbts are created out of whole
            // cloth by Berkeley DB, not us!
            System.out.println("internally created dbt: " +
                               new StringDbt(dbt) + ", size " +
                               dbt.get_size());
        }

        switch (count) {
            case 0:
                // nothing
                break;

            case 1:
                dbt.set_size(dbt.get_size() - 1);
                break;

            case 2:
                System.out.println("throwing...");
                throw new DbException("append_recno thrown");
                //not reached

            case 3:
                // Should result in an error (size unchanged).
                dbt.set_offset(1);
                break;

            case 4:
                dbt.set_offset(1);
                dbt.set_size(dbt.get_size() - 1);
                break;

            case 5:
                dbt.set_offset(1);
                dbt.set_size(dbt.get_size() - 2);
                break;

            case 6:
                dbt.set_data(new String("abc").getBytes());
                dbt.set_size(3);
                break;

            case 7:
                // Should result in an error.
                dbt.set_data(null);
                break;

            case 8:
                // Should result in an error.
                dbt.set_data(new String("abc").getBytes());
                dbt.set_size(4);
                break;

            default:
                break;
        }
    }


    // Here's an example of how you can extend a Dbt to store recno's.
    //
    static /*inner*/
    class RecnoDbt extends Dbt
    {
        RecnoDbt()
        {
            this(0);     // let other constructor do most of the work
        }

        RecnoDbt(int value)
        {
            set_flags(Db.DB_DBT_USERMEM); // do not allocate on retrieval
            arr = new byte[4];
            set_data(arr);                // use our local array for data
            set_ulen(4);                 // size of return storage
            setRecno(value);
        }

        public String toString() /*override*/
        {
            return String.valueOf(getRecno());
        }

        void setRecno(int value)
        {
            set_recno_key_data(value);
            set_size(arr.length);
        }

        int getRecno()
        {
            return get_recno_key_data();
        }

        byte arr[];
    }

    // Here's an example of how you can extend a Dbt in a straightforward
    // way to allow easy storage/retrieval of strings, or whatever
    // kind of data you wish.  We've declared it as a static inner
    // class, but it need not be.
    //
    static /*inner*/
    class StringDbt extends Dbt
    {
        StringDbt(Dbt dbt)
        {
            set_data(dbt.get_data());
            set_size(dbt.get_size());
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
        }

        String getString()
        {
            return new String(get_data(), 0, get_size());
        }

        public String toString() /*override*/
        {
            return getString();
        }
    }
}

