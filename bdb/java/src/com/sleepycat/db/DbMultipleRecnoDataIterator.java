/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2002
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: DbMultipleRecnoDataIterator.java,v 1.5 2002/01/11 15:52:39 bostic Exp $
 */

package com.sleepycat.db;

/**
 *
 * @author David M. Krinsky
 */
public class DbMultipleRecnoDataIterator extends DbMultipleIterator
{
    // public methods
    public DbMultipleRecnoDataIterator(Dbt data)
    {
        super(data);
    }

    public boolean next(Dbt key, Dbt data)
    {
        int keyoff = DbUtil.array2int(buf, pos);

        // crack out the key offset and the data offset and length.
        if (keyoff < 0) {
            return (false);
        }

        pos -= int32sz;
        int dataoff = DbUtil.array2int(buf, pos);

        pos -= int32sz;
        int datasz = DbUtil.array2int(buf, pos);

        pos -= int32sz;

        key.set_recno_key_from_buffer(buf, keyoff);

        data.set_data(buf);
        data.set_size(datasz);
        data.set_offset(dataoff);

        return (true);
    }
}

// end of DbMultipleRecnoDataIterator.java
