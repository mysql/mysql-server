/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2002
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: DbMultipleKeyDataIterator.java,v 1.5 2002/01/11 15:52:39 bostic Exp $
 */

package com.sleepycat.db;

/**
 *
 * @author David M. Krinsky
 */
public class DbMultipleKeyDataIterator extends DbMultipleIterator
{
    // public methods
    public DbMultipleKeyDataIterator(Dbt data)
    {
        super(data);
    }

    public boolean next(Dbt key, Dbt data)
    {
        int keyoff = DbUtil.array2int(buf, pos);

        // crack out the key and data offsets and lengths.
        if (keyoff < 0) {
            return (false);
        }

        pos -= int32sz;
        int keysz = DbUtil.array2int(buf, pos);

        pos -= int32sz;
        int dataoff = DbUtil.array2int(buf, pos);

        pos -= int32sz;
        int datasz = DbUtil.array2int(buf, pos);

        pos -= int32sz;

        key.set_data(buf);
        key.set_size(keysz);
        key.set_offset(keyoff);

        data.set_data(buf);
        data.set_size(datasz);
        data.set_offset(dataoff);

        return (true);
    }
}

// end of DbMultipleKeyDataIterator.java
