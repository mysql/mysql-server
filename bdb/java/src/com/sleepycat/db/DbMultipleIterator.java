/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2002
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: DbMultipleIterator.java,v 1.5 2002/01/11 15:52:38 bostic Exp $
 */

package com.sleepycat.db;

/**
 *
 * @author David M. Krinsky
 */
// DbMultipleIterator is a shared package-private base class for the three
// types of bulk-return Iterator;  it should never be instantiated directly,
// but it handles the functionality shared by its subclasses.
class DbMultipleIterator
{
    // Package-private methods and members:  used by our subclasses.

    // Called implicitly by the subclass
    DbMultipleIterator(Dbt data)
    {
        buf = data.get_data();
        size = data.get_ulen();
        // The offset will always be zero from the front of the buffer
        // DB returns, and the buffer is opaque, so don't bother
        // handling an offset.

        // The initial position is pointing at the last u_int32_t
        // in the buffer.
        pos = size - int32sz;
    }

    // The C macros use sizeof(u_int32_t).  Fortunately, java ints
    // are always four bytes.  Make this a constant just for form's sake.
    static final int int32sz = 4;

    // Current position within the buffer;  equivalent to "pointer"
    // in the DB_MULTIPLE macros.
    int pos;

    // A reference to the encoded buffer returned from the original
    // Db/Dbc.get call on the data Dbt, and its size.
    byte[] buf;
    int size;
}

// end of DbMultipleIterator.java
