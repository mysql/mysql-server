/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 *
 *	$Id: Dbt.java,v 11.6 2000/06/16 03:34:01 dda Exp $
 */

package com.sleepycat.db;

/**
 *
 * @author Donald D. Anderson
 */
public class Dbt
{
    // methods
    //

    protected native void finalize()
         throws Throwable;

    // get/set methods
    //

    // key/data

    public void set_data(byte[] data)
    {
        // internal_set_data is separated from set_data in case
        // we want to have set_data automatically set some other
        // fields (size, etc.) someday.
        //
        internal_set_data(data);
    }

    public native byte[] get_data();
    private native void internal_set_data(byte[] data);

    // These are not in the original DB interface,
    // but they can be used to get/set the offset
    // into the attached byte array.
    //
    public native void set_offset(int off);
    public native int get_offset();

    // key/data length
    public native /*u_int32_t*/ int get_size();
    public native void set_size(/*u_int32_t*/ int size);

    // RO: length of user buffer.
    public native /*u_int32_t*/ int get_ulen();
    public native void set_ulen(/*u_int32_t*/ int ulen);

    // RO: get/put record length.
    public native /*u_int32_t*/ int get_dlen();
    public native void set_dlen(/*u_int32_t*/ int dlen);

    // RO: get/put record offset.
    public native /*u_int32_t*/ int get_doff();
    public native void set_doff(/*u_int32_t*/ int doff);

    // flags
    public native /*u_int32_t*/ int get_flags();
    public native void set_flags(/*u_int32_t*/ int flags);

    // These are not in the original DB interface.
    // They can be used to set the recno key for a Dbt.
    // Note: you must set the data field to an array of
    // at least four bytes before calling either of these.
    //
    public native void set_recno_key_data(int recno);
    public native int get_recno_key_data();

    public Dbt(byte[] data)
    {
        init();
        internal_set_data(data);
        if (data != null)
            set_size(data.length);
    }

    public Dbt(byte[] data, int off, int len)
    {
        this(data);
        set_ulen(len);
        set_offset(off);
    }

    public Dbt()
    {
        init();
    }

    // private methods
    //
    private native void init();

    // private data
    //
    private long private_dbobj_ = 0;

    static {
        Db.load_db();
    }
}


// end of Dbt.java
