/*
   Copyright (c) 2010, 2026, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

package com.mysql.clusterj.core.util;

import java.util.function.Supplier;
import java.util.logging.Level;

public class JDK14LoggerImpl implements Logger {

    /** The logger delegate */
    protected java.util.logging.Logger delegate;

    JDK14LoggerImpl(java.util.logging.Logger delegate) {
        this.delegate = delegate;
    }

    public boolean isDetailEnabled() {
        return delegate.isLoggable(Level.FINEST);
    }

    public boolean isDebugEnabled() {
        return delegate.isLoggable(Level.FINER);
    }

    public boolean isTraceEnabled() {
        return delegate.isLoggable(Level.FINE);
    }

    public boolean isInfoEnabled() {
        return delegate.isLoggable(Level.INFO);
    }

    private void write(Level level, int stackDepth, String message) {
        Throwable t = new Throwable();
        StackTraceElement[] stack = t.getStackTrace();
        StackTraceElement element = stack[stackDepth];
        String className = element.getClassName();
        String methodName = element.getMethodName();
        delegate.logp(level, className, methodName, message);
    }

    private void write(Level level, int stackDepth, Supplier<String> generator) {
        Throwable t = new Throwable();
        StackTraceElement[] stack = t.getStackTrace();
        StackTraceElement element = stack[stackDepth];
        String className = element.getClassName();
        String methodName = element.getMethodName();
        delegate.logp(level, className, methodName, generator);
    }

    public void detail(String message) {
        assert isDetailEnabled();
        this.write(Level.FINEST, 2, message);
    }

    public void detail(Supplier<String> generator) {
        if(! isDetailEnabled()) return;
        this.write(Level.FINEST, 2, generator);
    }

    public void debug(String message) {
        assert isDebugEnabled();
        this.write(Level.FINER, 2, message);
    }

    public void debug(Supplier<String> generator) {
        if(! isDebugEnabled()) return;
        this.write(Level.FINER, 2, generator);
    }

    public void trace(String message) {
        assert isTraceEnabled();
        this.write(Level.FINE, 2, message);
    }

    public void trace(Supplier<String> generator) {
        if(! isTraceEnabled()) return;
        this.write(Level.FINE, 2, generator);
    }

    public void info(String message) {
        assert isInfoEnabled();
        this.write(Level.INFO, 2, message);
    }

    public void info(Supplier<String> generator) {
        if(! isInfoEnabled()) return;
        this.write(Level.INFO, 2, generator);
    }

    public void warn(String message) {
        this.write(Level.WARNING, 2, message);
    }

    public void error(String message) {
        this.write(Level.SEVERE, 2, message);
    }

    public void fatal(String message) {
        this.write(Level.SEVERE, 2, message);
    }

}
