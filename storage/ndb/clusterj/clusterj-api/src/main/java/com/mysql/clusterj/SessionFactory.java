/*
 *  Copyright (c) 2010, 2024, Oracle and/or its affiliates.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License, version 2.0,
 *  as published by the Free Software Foundation.
 *
 *  This program is designed to work with certain software (including
 *  but not limited to OpenSSL) that is licensed under separate terms,
 *  as designated in a particular file or component or in included license
 *  documentation.  The authors of MySQL hereby grant you an additional
 *  permission to link the program and your derivative works with the
 *  separately licensed software that they have either included with
 *  the program or referenced in the documentation.
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

import java.util.List;
import java.util.Map;

/** SessionFactory represents a cluster.
 *
 */
public interface SessionFactory {

    /** Create a Session to use with the cluster, using all the
     * properties of the SessionFactory.
     * @return the session
     */
    Session getSession();

    /** Create a session to use with the cluster, overriding some properties.
     * Properties PROPERTY_CLUSTER_CONNECTSTRING, PROPERTY_CLUSTER_DATABASE,
     * and PROPERTY_CLUSTER_MAX_TRANSACTIONS may not be overridden.
     * @param properties overriding some properties for this session
     * @return the session
     */
    Session getSession(Map properties);

    /** Get a list containing the number of open sessions for each connection
     * in the connection pool.
     * @since 7.3.14, 7.4.12, 7.5.2
     */
    public List<Integer> getConnectionPoolSessionCounts();

    /** Close this session factory. Release all resources. Set the current state to Closed.
     * When closed, calls to getSession will throw ClusterJUserException.
     */
    void close();

    /** Disconnect and reconnect this session factory using the specified timeout value
     * and change the saved timeout value. This is a heavyweight method and should be used rarely.
     * It is intended for cases where the process in which clusterj is running has lost connectivity
     * to the cluster and is not able to function normally. Reconnection is done in several phases.
     * First, the session factory is set to state Reconnecting and a reconnect thread is started to
     * manage the reconnection procedure. In the Reconnecting state, the getSession methods throw
     * ClusterJUserException and the connection pool is quiesced until all sessions have closed.
     * If sessions fail to close normally after timeout seconds, the sessions are forced to close.
     * Next, all connections in the connection pool are closed, which frees their connection slots
     * in the cluster. Finally, the connection pool is recreated using the original connection pool
     * properties and the state is set to Open.
     * The reconnection procedure is asynchronous. To observe the progress of the procedure, use the
     * methods currentState and getConnectionPoolSessionCounts.
     * If the timeout value is non-zero, automatic reconnection will be done by the clusterj
     * implementation upon recognizing that a network failure has occurred.
     * If the timeout value is 0, automatic reconnection is disabled.
     * If the current state of this session factory is Reconnecting, this method silently does nothing.
     * @param timeout the timeout value in seconds; 0 to disable automatic reconnection
     * @since 7.5.7
     */
    void reconnect(int timeout);

    /** Reconnect this session factory using the most recent timeout value specified. The timeout may
     * have been specified in the original session factory properties or may have been changed
     * by an application call to reconnect(int timeout).
     * @see reconnect(int timeout)
     * @since 7.5.7
     */
    void reconnect();

    /** Get the current state of this session factory.
     * @since 7.5.7
     * @see State
     */
    SessionFactory.State currentState();

    /** State of this session factory
     * @since 7.5.7
     */
    public enum State {
        Open                ( 2 ),
        Reconnecting        ( 1 ),
        Closed              ( 0 );
        State(int value) {
            this.value = value;
        }
        public final int value;
    }

    /** Bind receive threads to cpuids for all connections in the connection
     * pool. Specify -1 to unset receive thread cpu binding for a connection.
     * The cpuid must be between 0 and the number of cpus in the machine.
     * @throws ClusterJUserException if the cpuid is illegal or if the
     *   number of elements in cpuids is not equal to the number of connections
     *   in the connection pool.
     * @throws ClusterJFatalInternalException if the binding fails due to some
     *   internal reason.
     * @since 7.5.7
     */
    public void setRecvThreadCPUids(short[] cpuids);

    /** Get receive thread bindings to cpus for all connections in the
     * connection pool. If a receive thread is not bound to a cpu, the
     * corresponding value will be -1.
     * @since 7.5.7
     */
    public short[] getRecvThreadCPUids();

    /** Set the receive thread activation threshold for all connections in the
     * connection pool. 16 or higher means that receive threads are never used
     * as receivers. 0 means that the receive thread is always active, and that
     * retains poll rights for its own exclusive use, effectively blocking all
     * user threads from becoming receivers. In such cases care should be taken
     * to ensure that the receive thread does not compete with the user thread
     * for CPU resources; it is preferable for it to be locked to a CPU for its
     * own exclusive use. The default is 8.
     * @throws ClusterJUserException if the value is negative
     * @throws ClusterJFatalInternalException if the method fails due to some
     *    internal reason.
     * @since 7.5.7
     */
    public void setRecvThreadActivationThreshold(int threshold);

    /** Get the receive thread activation threshold for all connections in the
     * connection pool. 16 or higher means that receive threads are never used
     * as receivers. 0 means that the receive thread is always active, and that
     * retains poll rights for its own exclusive use, effectively blocking all
     * user threads from becoming receivers. In such cases care should be taken
     * to ensure that the receive thread does not compete with the user thread
     * for CPU resources; it is preferable for it to be locked to a CPU for its
     * own exclusive use. The default is 8.
     * @since 7.5.7
     */
    public int getRecvThreadActivationThreshold();

}
