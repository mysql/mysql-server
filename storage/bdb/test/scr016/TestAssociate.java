/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: TestAssociate.java,v 1.8 2004/01/28 03:36:34 bostic Exp $
 */

package com.sleepycat.test;

import com.sleepycat.db.*;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.Reader;
import java.io.StringReader;
import java.io.IOException;
import java.io.PrintStream;
import java.util.Hashtable;

public class TestAssociate
    implements DbDupCompare
{
    private static final String FileName = "access.db";
    public static Db saveddb1 = null;
    public static Db saveddb2 = null;

    public TestAssociate()
    {
    }

    private static void usage()
    {
        System.err.println("usage: TestAssociate\n");
        System.exit(1);
    }

    public static void main(String argv[])
    {
        try
        {
            TestAssociate app = new TestAssociate();
            app.run();
        }
        catch (DbException dbe)
        {
            System.err.println("TestAssociate: " + dbe.toString());
            System.exit(1);
        }
        catch (FileNotFoundException fnfe)
        {
            System.err.println("TestAssociate: " + fnfe.toString());
            System.exit(1);
        }
        System.exit(0);
    }

    public static int counter = 0;
    public static String results[] = { "abc", "def", "ghi", "JKL", "MNO", null };

    // Prompts for a line, and keeps prompting until a non blank
    // line is returned.  Returns null on error.
    //
    static public String askForLine(Reader reader,
                                    PrintStream out, String prompt)
    {
        /*
        String result = "";
        while (result != null && result.length() == 0) {
            out.print(prompt);
            out.flush();
            result = getLine(reader);
        }
        return result;
        */
        return results[counter++];
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

    static public String shownull(Object o)
    {
        if (o == null)
            return "null";
        else
            return "not null";
    }

    public void run()
         throws DbException, FileNotFoundException
    {
        // Remove the previous database.
        new File(FileName).delete();

        // Create the database object.
        // There is no environment for this simple example.
        DbEnv dbenv = new DbEnv(0);
        dbenv.open("./", Db.DB_CREATE|Db.DB_INIT_MPOOL, 0644);
        (new java.io.File(FileName)).delete();
        Db table = new Db(dbenv, 0);
        Db table2 = new Db(dbenv, 0);
        table2.set_dup_compare(this);
        table2.set_flags(Db.DB_DUPSORT);
        table.set_error_stream(System.err);
        table2.set_error_stream(System.err);
        table.set_errpfx("TestAssociate");
        table2.set_errpfx("TestAssociate(table2)");
        System.out.println("Primary database is " + shownull(table));
        System.out.println("Secondary database is " + shownull(table2));
        saveddb1 = table;
        saveddb2 = table2;
        table.open(null, FileName, null, Db.DB_BTREE, Db.DB_CREATE, 0644);
        table2.open(null, FileName + "2", null,
                    Db.DB_BTREE, Db.DB_CREATE, 0644);
        table.associate(null, table2, new Capitalize(), 0);

        //
        // Insert records into the database, where the key is the user
        // input and the data is the user input in reverse order.
        //
        Reader reader = new StringReader("abc\ndef\njhi");

        for (;;) {
            String line = askForLine(reader, System.out, "input> ");
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
                if ((err = table.put(null,
		    key, data, Db.DB_NOOVERWRITE)) == Db.DB_KEYEXIST) {
                        System.out.println("Key " + line + " already exists.");
                }
            }
            catch (DbException dbe)
            {
                System.out.println(dbe.toString());
            }
            System.out.println("");
        }

        // Acquire an iterator for the table.
        Dbc iterator;
        iterator = table2.cursor(null, 0);

        // Walk through the table, printing the key/data pairs.
        // See class StringDbt defined below.
        //
        StringDbt key = new StringDbt();
        StringDbt data = new StringDbt();
        StringDbt pkey = new StringDbt();

        while (iterator.get(key, data, Db.DB_NEXT) == 0)
        {
            System.out.println(key.getString() + " : " + data.getString());
        }

        key.setString("BC");
        System.out.println("get BC returns " + table2.get(null, key, data, 0));
        System.out.println("  values: " + key.getString() + " : " + data.getString());
        System.out.println("pget BC returns " + table2.pget(null, key, pkey, data, 0));
        System.out.println("  values: " + key.getString() + " : " + pkey.getString() + " : " + data.getString());
        key.setString("KL");
        System.out.println("get KL returns " + table2.get(null, key, data, 0));
        System.out.println("  values: " + key.getString() + " : " + data.getString());
        System.out.println("pget KL returns " + table2.pget(null, key, pkey, data, 0));
        System.out.println("  values: " + key.getString() + " : " + pkey.getString() + " : " + data.getString());

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

        public String toString()
        {
            return "StringDbt=" + getString();
        }
    }

    /* creates a stupid secondary index as follows:
     For an N letter key, we use N-1 letters starting at
     position 1.  If the new letters are already capitalized,
     we return the old array, but with offset set to 1.
     If the letters are not capitalized, we create a new,
     capitalized array.  This is pretty stupid for
     an application, but it tests all the paths in the runtime.
     */
    public static class Capitalize implements DbSecondaryKeyCreate
    {
	public int secondaryKeyCreate(Db secondary, Dbt key, Dbt value,
					Dbt result)
            throws DbException
        {
            String which = "unknown db";
            if (saveddb1.equals(secondary)) {
                which = "primary";
            }
            else if (saveddb2.equals(secondary)) {
                which = "secondary";
            }
            System.out.println("secondaryKeyCreate, Db: " + shownull(secondary) + "(" + which + "), key: " + show_dbt(key) + ", data: " + show_dbt(value));
            int len = key.get_size();
            byte[] arr = key.get_data();
            boolean capped = true;

            if (len < 1)
                throw new DbException("bad key");

            if (len < 2)
                return Db.DB_DONOTINDEX;

            result.set_size(len - 1);
            for (int i=1; capped && i<len; i++) {
                if (!Character.isUpperCase((char)arr[i]))
                    capped = false;
            }
            if (capped) {
                System.out.println("  creating key(1): " + new String(arr, 1, len-1));
                result.set_data(arr);
                result.set_offset(1);
            }
            else {
                System.out.println("  creating key(2): " + (new String(arr)).substring(1).
                                   toUpperCase());
                result.set_data((new String(arr)).substring(1).
                                toUpperCase().getBytes());
            }
            return 0;
        }
    }

    public int compareDuplicates(Db db, Dbt dbt1, Dbt dbt2)
    {
        System.out.println("compare");
        int sz1 = dbt1.get_size();
        int sz2 = dbt2.get_size();
        if (sz1 < sz2)
            return -1;
        if (sz1 > sz2)
            return 1;
        byte[] data1 = dbt1.get_data();
        byte[] data2 = dbt2.get_data();
        for (int i=0; i<sz1; i++)
            if (data1[i] != data2[i])
                return (data1[i] < data2[i] ? -1 : 1);
        return 0;
    }

    public static int nseen = 0;
    public static Hashtable ht = new Hashtable();

    public static String show_dbt(Dbt dbt)
    {
        String name;

        if (dbt == null)
            return "null dbt";

        name = (String)ht.get(dbt);
        if (name == null) {
            name = "Dbt" + (nseen++);
            ht.put(dbt, name);
        }

        byte[] value = dbt.get_data();
        if (value == null)
            return name + "(null)";
        else
            return name + "(\"" + new String(value) + "\")";
    }
}


