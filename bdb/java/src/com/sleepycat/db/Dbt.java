/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2002
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: Dbt.java,v 11.15 2002/09/04 00:37:25 mjc Exp $
 */

package com.sleepycat.db;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;

/**
 *
 * @author Donald D. Anderson
 */
public class Dbt
{
    // methods
    //

    public Dbt(byte[] data)
    {
        init();
        this.data = data;
        if (data != null) {
            this.size = data.length;
        }
    }

    public Dbt(byte[] data, int off, int len)
    {
        init();
        this.data = data;
        this.offset = off;
        this.size = len;
    }

    public Dbt()
    {
        init();
    }

    public Dbt(Object serialobj) throws java.io.IOException
    {
        init();
        this.set_object(serialobj);
    }

    protected native void finalize()
         throws Throwable;

    // get/set methods
    //

    // key/data
    public byte[] get_data()
    {
        // In certain circumstances, like callbacks to
        // user code that have Dbt args, we do not create
        // data arrays until the user explicitly does a get_data.
        // This saves us from needlessly creating objects
        // (potentially large arrays) that may never be accessed.
        //
        if (must_create_data) {
            data = create_data();
            must_create_data = false;
        }
        return data;
    }

    public void set_data(byte[] data)
    {
        this.data = data;
        this.must_create_data = false;
    }


    // get_offset/set_offset is unique to the Java portion
    // of the DB APIs.  They can be used to get/set the offset
    // into the attached byte array.
    //
    public int get_offset()
    {
        return offset;
    }

    public void set_offset(int offset)
    {
        this.offset = offset;
    }

    // key/data length
    public /*u_int32_t*/ int get_size()
    {
        return size;
    }

    public void set_size(/*u_int32_t*/ int size)
    {
        this.size = size;
    }

    // RO: length of user buffer.
    public /*u_int32_t*/ int get_ulen()
    {
        return ulen;
    }

    public void set_ulen(/*u_int32_t*/ int ulen)
    {
        this.ulen = ulen;
    }


    // RO: get/put record length.
    public /*u_int32_t*/ int get_dlen()
    {
        return dlen;
    }

    public void set_dlen(/*u_int32_t*/ int dlen)
    {
        this.dlen = dlen;
    }

    // RO: get/put record offset.
    public /*u_int32_t*/ int get_doff()
    {
        return doff;
    }

    public void set_doff(/*u_int32_t*/ int doff)
    {
        this.doff = doff;
    }

    // flags
    public /*u_int32_t*/ int get_flags()
    {
        return flags;
    }

    public void set_flags(/*u_int32_t*/ int flags)
    {
        this.flags = flags;
    }

    // Helper methods to get/set a Dbt from a serializable object.
    public Object get_object() throws java.io.IOException,
      java.lang.ClassNotFoundException
    {
        ByteArrayInputStream bytestream = new ByteArrayInputStream(get_data());
        ObjectInputStream ois = new ObjectInputStream(bytestream);
        Object serialobj = ois.readObject();
        ois.close();
        bytestream.close();
        return (serialobj);
    }

    public void set_object(Object serialobj) throws java.io.IOException
    {
        ByteArrayOutputStream bytestream = new ByteArrayOutputStream();
        ObjectOutputStream oos = new ObjectOutputStream(bytestream);
        oos.writeObject(serialobj);
        oos.close();
        byte[] buf = bytestream.toByteArray();
        bytestream.close();
        set_data(buf);
        set_offset(0);
        set_size(buf.length);
    }

    // These are not in the original DB interface.
    // They can be used to set the recno key for a Dbt.
    // Note: if data is less than (offset + 4) bytes, these
    // methods may throw an ArrayIndexException.  get_recno_key_data()
    // will additionally throw a NullPointerException if data is null.
    public void set_recno_key_data(int recno)
    {
        if (data == null) {
            data = new byte[4];
            size = 4;
            offset = 0;
        }
        DbUtil.int2array(recno, data, offset);
    }

    public int get_recno_key_data()
    {
        return (DbUtil.array2int(data, offset));
    }

    // Used internally by DbMultipleRecnoIterator
    //
    /*package*/ void set_recno_key_from_buffer(byte[] data, int offset)
    {
        this.data = data;
        this.offset = offset;
        this.size = 4;
    }

    static {
        Db.load_db();
    }

    // private methods
    //
    private native void init();
    private native byte[] create_data();

    // private data
    //
    private long private_dbobj_ = 0;

    private byte[] data = null;
    private int offset = 0;
    private int size = 0;
    private int ulen = 0;
    private int dlen = 0;
    private int doff = 0;
    private int flags = 0;
    private boolean must_create_data = false;
}

// end of Dbt.java
