/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2002
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: DbMultipleDataIterator.java,v 1.5 2002/01/11 15:52:38 bostic Exp $
 */

package com.sleepycat.db;

/**
 *
 * @author David M. Krinsky
 */
public class DbMultipleDataIterator extends DbMultipleIterator
{
    // public methods
    public DbMultipleDataIterator(Dbt data)
    {
        super(data);
    }

    public boolean next(Dbt data)
    {
        int dataoff = DbUtil.array2int(buf, pos);

        // crack out the data offset and length.
        if (dataoff < 0) {
            return (false);
        }

        pos -= int32sz;
        int datasz = DbUtil.array2int(buf, pos);

        pos -= int32sz;

        data.set_data(buf);
        data.set_size(datasz);
        data.set_offset(dataoff);

        return (true);
    }
}

// end of DbMultipleDataIterator.java
