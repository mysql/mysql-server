/*
   Copyright 2010 Sun Microsystems, Inc.
   All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

package com.mysql.clusterj.core.util;

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

    public void detail(String message) {
        Throwable t = new Throwable();
        StackTraceElement[] stack = t.getStackTrace();
        StackTraceElement element = stack[1];
        String className = element.getClassName();
        String methodName = element.getMethodName();
        delegate.logp(Level.FINEST, className, methodName, message);
    }

    public void debug(String message) {
        Throwable t = new Throwable();
        StackTraceElement[] stack = t.getStackTrace();
        StackTraceElement element = stack[1];
        String className = element.getClassName();
        String methodName = element.getMethodName();
        delegate.logp(Level.FINER, className, methodName, message);
    }

    public void trace(String message) {
        Throwable t = new Throwable();
        StackTraceElement[] stack = t.getStackTrace();
        StackTraceElement element = stack[1];
        String className = element.getClassName();
        String methodName = element.getMethodName();
        delegate.logp(Level.FINE, className, methodName, message);
    }

    public void info(String message) {
        Throwable t = new Throwable();
        StackTraceElement[] stack = t.getStackTrace();
        StackTraceElement element = stack[1];
        String className = element.getClassName();
        String methodName = element.getMethodName();
        delegate.logp(Level.INFO, className, methodName, message);
    }

    public void warn(String message) {
        Throwable t = new Throwable();
        StackTraceElement[] stack = t.getStackTrace();
        StackTraceElement element = stack[1];
        String className = element.getClassName();
        String methodName = element.getMethodName();
        delegate.logp(Level.WARNING, className, methodName, message);
    }

    public void error(String message) {
        Throwable t = new Throwable();
        StackTraceElement[] stack = t.getStackTrace();
        StackTraceElement element = stack[1];
        String className = element.getClassName();
        String methodName = element.getMethodName();
        delegate.logp(Level.SEVERE, className, methodName, message);
    }

    public void fatal(String message) {
        Throwable t = new Throwable();
        StackTraceElement[] stack = t.getStackTrace();
        StackTraceElement element = stack[1];
        String className = element.getClassName();
        String methodName = element.getMethodName();
        delegate.logp(Level.SEVERE, className, methodName, message);
    }

}
