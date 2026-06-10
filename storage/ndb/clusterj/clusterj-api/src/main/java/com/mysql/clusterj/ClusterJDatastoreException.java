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

package com.mysql.clusterj;

/**
 * ClusterJUserException represents a database error. The underlying cause
 * of the exception is contained in the "cause".
 */
public class ClusterJDatastoreException extends ClusterJException {

    private static final long serialVersionUID = 2208896230646592560L;

    protected static final int HA_ERR_NO_SUCH_TABLE = 155;

    protected static final int HA_ERR_TABLE_DEF_CHANGED = 159;

    private static final int ndberror_st_temporary = 1;

    private static final int ndberror_st_permanent = 2;

    protected int code = 0;

    protected int mysqlCode = 0;

    protected int status = 0;

    protected int classification = 0;

    private Object syncObject = null;

    /** Get the code
     @since 7.3.15, 7.4.13, 7.5.4
    */
    public int getCode() {
        return code;
    }

    /** Get the mysql code
     @since 7.3.15, 7.4.13, 7.5.4
     */
    public int getMysqlCode() {
        return mysqlCode;
    }

    /** Get the status
     */
    public int getStatus() {
        return status;
    }

    /** Get the classification
     */
    public int getClassification() {
        return classification;
    }

    /** canRetry()
     * @return true if the error is an Ndb temporary error,
     * and the operation can be retried.
     * @since 9.4.0
     */
    public boolean canRetry() {
        return (status == ndberror_st_temporary);
    }

    /** tableNotFound()
     *  @return true if the error is a "Table Not Found" condition
     *  @since 9.4.0
     */
    public boolean tableNotFound() {
        return (mysqlCode == HA_ERR_NO_SUCH_TABLE);
    }

    /** isStaleMetadata() returns true if the exception was caused by stale
     *  metadata encountered while attempting to perform a data operation.
     *  On true, the user should call session.unloadSchema() to refresh the
     *  metadata, then retry.
     */
    public boolean isStaleMetadata() {
        // Check whether schema change handling has already started
        if (isSchemaChangePending()) return false;

        // Table definition has changed - codes 241, 284, and 20021
        if (mysqlCode == HA_ERR_TABLE_DEF_CHANGED) return true;

        // Table is being dropped
        if (code == 283 || code == 1226) return true;

        // Index is being dropped
        if (code == 910) return true;

        // Schema object is busy with another schema transaction
        if (code == 785) return true;

        // NdbRecord obtained from cache is out of date
        if (code == 4292) return true;

        return false;
    }

    /* isSchemaChangePending() returns true when some other thread has
     * already called session.unloadSchema() to initiate schema change handling,
     * but the handling had not yet completed at the time the exception was
     * thrown. If true, the user can call awaitSchemaChange() to pause until
     * handling completes.
     */
    public boolean isSchemaChangePending() {
        return (syncObject != null);
    }

    /* Wait for schema change handling to complete.
     * The thread handling the schema change is holding the intrinsic lock
     * on syncObject, and will release the lock after handling is complete.
     */
    public void awaitSchemaChange() {
        synchronized(syncObject) { }
    }


    /* Constructors*/
    public ClusterJDatastoreException(String message) {
        super(message);
    }

    public ClusterJDatastoreException(String message, Throwable t) {
        super(message, t);
    }

    public ClusterJDatastoreException(Throwable t) {
        super(t);
    }

    public ClusterJDatastoreException(String msg, int code, int mysqlCode,
            int status, int classification) {
        super(msg);
        this.code = code;
        this.mysqlCode = mysqlCode;
        this.status = status;
        this.classification = classification;
    }

    public static ClusterJDatastoreException forSchemaChange(Object obj) {
        ClusterJDatastoreException ex =
            new ClusterJDatastoreException("Schema change in progress");
        ex.syncObject = obj;
        return ex;
    }

    /* ClusterJDatastoreException produced internally when Cluster/J catches stale
       metadata before it is passed into the NDBAPI. Code will usually be negative.
    */
    public static ClusterJDatastoreException forSchemaChange(String msg, int code, Throwable cause) {
        var ex = (cause == null) ? new ClusterJDatastoreException(msg)
                                 : new ClusterJDatastoreException(msg, cause);
        ex.code = code;
        ex.mysqlCode = HA_ERR_TABLE_DEF_CHANGED;
        ex.status = ndberror_st_permanent;
        ex.classification = 5;
        return ex;
    }

    public ClusterJDatastoreException setRetriable() {
        status = ndberror_st_temporary;
        return this;
    }

    /** Helper class for getClassification().
     * import com.mysql.clusterj.ClusterJDatastoreException.Classification;
     * Classification c = Classification.lookup(datastoreException.getClassification());
     * System.out.println("exceptionClassification " + c + " with value " + c.value);
     * @since 7.3.15, 7.4.13, 7.5.4
     */
    public enum Classification {
        NoError                ( 0 ) /*_ndberror_cl_none_*/,
        ApplicationError       ( 1 ) /*_ndberror_cl_application_*/,
        NoDataFound            ( 2 ) /*_ndberror_cl_no_data_found_*/,
        ConstraintViolation    ( 3 ) /*_ndberror_cl_constraint_violation_*/,
        SchemaError            ( 4 ) /*_ndberror_cl_schema_error_*/,
        UserDefinedError       ( 5 ) /*_ndberror_cl_user_defined_*/,
        InsufficientSpace      ( 6 ) /*_ndberror_cl_insufficient_space_*/,
        TemporaryResourceError ( 7 ) /*_ndberror_cl_temporary_resource_*/,
        NodeRecoveryError      ( 8 ) /*_ndberror_cl_node_recovery_*/,
        OverloadError          ( 9 ) /*_ndberror_cl_overload_*/,
        TimeoutExpired         ( 10 ) /*_ndberror_cl_timeout_expired_*/,
        UnknownResultError     ( 11 ) /*_ndberror_cl_unknown_result_*/,
        InternalError          ( 12 ) /*_ndberror_cl_internal_error_*/,
        FunctionNotImplemented ( 13 ) /*_ndberror_cl_function_not_implemented_*/,
        UnknownErrorCode       ( 14 ) /*_ndberror_cl_unknown_error_code_*/,
        NodeShutdown           ( 15 ) /*_ndberror_cl_node_shutdown_*/,
        SchemaObjectExists     ( 17 ) /*_ndberror_cl_schema_object_already_exists_*/,
        InternalTemporary      ( 18 ) /*_ndberror_cl_internal_temporary_*/;

        Classification(int value) {
            this.value = value;
        }

        public final int value;

        private static Classification[] entries = new Classification[] {
            NoError, ApplicationError, NoDataFound, ConstraintViolation, SchemaError,
            UserDefinedError,InsufficientSpace, TemporaryResourceError, NodeRecoveryError,
            OverloadError, TimeoutExpired, UnknownResultError, InternalError,
            FunctionNotImplemented, UnknownErrorCode, NodeShutdown, null,
            SchemaObjectExists, InternalTemporary};

        /** Get the Classification enum for a value returned by
         * ClusterJDatastoreException.getClassification().
         * @param value the classification returned by getClassification()
         * @return the Classification for the error
         */
        public static Classification lookup(int value) {
            return (value >= 0) && (value < entries.length) ? entries[value] : null;
        }
    }
}
