/*
 *  Copyright (c) 2011, 2022, Oracle and/or its affiliates.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License, version 2.0,
 *  as published by the Free Software Foundation.
 *
 *  This program is also distributed with certain software (including
 *  but not limited to OpenSSL) that is licensed under separate terms,
 *  as designated in a particular file or component or in included license
 *  documentation.  The authors of MySQL hereby grant you an additional
 *  permission to link the program and your derivative works with the
 *  separately licensed software that they have included with MySQL.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License, version 2.0, for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

package com.mysql.clusterj;

/** Dbug allows clusterj applications to enable the DBUG functionality in cluster
 * ndbapi library.
 * The dbug state is a control string that consists of flags separated by colons. Flags are:
 * <ul><li>d set the debug flag
 * </li><li>a[,filename] append debug output to the file
 * </li><li>A[,filename] like a[,filename] but flush the output after each operation
 * </li><li>d[,keyword[,keyword...]] enable output from macros with specified keywords
 * </li><li>D[,tenths] delay for specified tenths of a second after each operation
 * </li><li>f[,function[,function...]] limit output to the specified list of functions
 * </li><li>F mark each output with the file name of the source file
 * </li><li>i mark each output with the process id of the current process
 * </li><li>g[,function[,function...]] profile specified list of functions
 * </li><li>L mark each output with the line number of the source file
 * </li><li>n mark each output with the current function nesting depth
 * </li><li>N mark each output with a sequential number
 * </li><li>o[,filename] overwrite debug output to the file
 * </li><li>O[,filename] like o[,filename] but flush the output after each operation
 * </li><li>p[,pid[,pid...]] limit output to specified list of process ids
 * </li><li>P mark each output with the process name
 * </li><li>r reset the indentation level to zero
 * </li><li>t[,depth] limit function nesting to the specified depth
 * </li><li>T mark each output with the current timestamp
 * </li></ul>
 * For example, the control string to trace calls and output debug information only for
 * "jointx" and overwrite the contents of file "/tmp/dbug/jointx", use "t:d,jointx:o,/tmp/dbug/jointx".
 * The above can be written as ClusterJHelper.newDbug().trace().debug("jointx").output("/tmp/dbug/jointx").set();
 */
public interface Dbug {
    /** Push the current state and set the parameter as the new state.
     * @param state the new state
     */
    void push(String state);

    /** Pop the current state. The new state will be the previously pushed state.
     */
    void pop();

    /** Set the current state from the parameter.
     * @param state the new state
     */
    void set(String state);

    /** Return the current state.
     * @return the current state
     */
    String get();

    /** Print debug message.
     * 
     */
    void print(String keyword, String message);

    /** Set the list of debug keywords.
     * @param strings the debug keywords
     * @return this
     */
    Dbug debug(String[] strings);

    /** Set the list of debug keywords.
     * @param string the comma separated debug keywords
     * @return this
     */
    Dbug debug(String string);

    /** Push the current state as defined by the methods.
     */
    void push();

    /** Set the current state as defined by the methods.
     */
    void set();

    /** Specify the file name for debug output (append).
     * @param fileName the name of the file
     * @return this
     */
    Dbug append(String fileName);

    /** Specify the file name for debug output (overwrite).
     * @param fileName the name of the file
     * @return this
     */
    Dbug output(String fileName);

    /** Force flush after each output operation.
     * @return this
     */
    Dbug flush();

    /** Set the trace flag.
     * @return this
     */
    Dbug trace();

}
