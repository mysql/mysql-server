package com.sleepycat.test;
import com.sleepycat.db.*;

public class CallbackTest
{
    public static void main(String args[])
    {
        try {
            Db db = new Db(null, 0);
            db.setBtreeCompare(new BtreeCompare());
            db.open(null, "test.db", "", Db.DB_BTREE, Db.DB_CREATE, 0666);
            StringDbt[] keys = new StringDbt[10];
            StringDbt[] datas = new StringDbt[10];
            for (int i = 0; i<10; i++) {
                int val = (i * 3) % 10;
                keys[i] = new StringDbt("key" + val);
                datas[i] = new StringDbt("data" + val);
                System.out.println("put " + val);
                db.put(null, keys[i], datas[i], 0);
            }
        }
        catch (DbException dbe) {
            System.err.println("FAIL: " + dbe);
        }
        catch (java.io.FileNotFoundException fnfe) {
            System.err.println("FAIL: " + fnfe);
        }

    }


}

class BtreeCompare
    implements DbBtreeCompare
{
    /* A weird comparator, for example.
     * In fact, it may not be legal, since it's not monotonically increasing.
     */
    public int compare(Db db, Dbt dbt1, Dbt dbt2)
    {
        System.out.println("compare function called");
        byte b1[] = dbt1.getData();
        byte b2[] = dbt2.getData();
        System.out.println("  " + (new String(b1)) + ", " + (new String(b2)));
        int len1 = b1.length;
        int len2 = b2.length;
        if (len1 != len2)
            return (len1 < len2) ? 1 : -1;
        int value = 1;
        for (int i=0; i<len1; i++) {
            if (b1[i] != b2[i])
                return (b1[i] < b2[i]) ? value : -value;
            value *= -1;
        }
        return 0;
    }
}

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
