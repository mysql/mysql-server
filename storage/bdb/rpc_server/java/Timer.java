/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: Timer.java,v 1.4 2004/04/06 20:43:42 mjc Exp $
 */

package com.sleepycat.db.rpcserver;

/**
 * Class to keep track of access times. This is slightly devious by having
 * both the access_time and a reference to another Timer that can be
 * used to group/share access times. This is done to keep the Java code
 * close to the canonical C implementation of the RPC server.
 */
public class Timer {
    Timer timer = this;
    long last_access;
}
