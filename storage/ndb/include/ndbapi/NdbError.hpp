/*
   Copyright (C) 2003-2006 MySQL AB, 2009 Sun Microsystems, Inc.
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

#ifndef NDB_ERROR_HPP
#define NDB_ERROR_HPP

#include "ndberror.h"

/**
 * @struct NdbError
 * @brief Contains error information
 *
 * A NdbError consists of five parts:
 * -# Error status         : Application impact
 * -# Error classification : Logical error group
 * -# Error code           : Internal error code
 * -# Error message        : Context independent description of error 
 * -# Error details        : Context dependent information 
 *                           (not always available)
 *
 * <em>Error status</em> is usually used for programming against errors.
 * If more detailed error control is needed, it is possible to 
 * use the <em>error classification</em>.
 *
 * It is not recommended to write application programs dependent on
 * specific <em>error codes</em>.
 *
 * The <em>error messages</em> and <em>error details</em> may
 * change without notice.
 * 
 * For example of use, see @ref ndbapi_retries.cpp.
 */
struct NdbError {
  /**
   * Status categorizes error codes into status values reflecting
   * what the application should do when encountering errors
   */
  enum Status {
    /**
     * The error code indicate success<br>
     * (Includes classification: NdbError::NoError)
     */
    Success = ndberror_st_success,

    /**
     * The error code indicates a temporary error.
     * The application should typically retry.<br>
     * (Includes classifications: NdbError::InsufficientSpace, 
     *  NdbError::TemporaryResourceError, NdbError::NodeRecoveryError,
     *  NdbError::OverloadError, NdbError::NodeShutdown 
     *  and NdbError::TimeoutExpired.)
     */
    TemporaryError = ndberror_st_temporary,
    
    /**
     * The error code indicates a permanent error.<br>
     * (Includes classificatons: NdbError::PermanentError, 
     *  NdbError::ApplicationError, NdbError::NoDataFound,
     *  NdbError::ConstraintViolation, NdbError::SchemaError,
     *  NdbError::UserDefinedError, NdbError::InternalError, and, 
     *  NdbError::FunctionNotImplemented.)
     */
    PermanentError = ndberror_st_permanent,
  
    /**
     * The result/status is unknown.<br>
     * (Includes classifications: NdbError::UnknownResultError, and
     *  NdbError::UnknownErrorCode.)
     */
    UnknownResult = ndberror_st_unknown
  };
  
  /**
   * Type of error
   */
  enum Classification {
    /**
     * Success.  No error occurred.
     */
    NoError = ndberror_cl_none,

    /**
     * Error in application program.
     */
    ApplicationError = ndberror_cl_application,

    /**
     * Read operation failed due to missing record.
     */
    NoDataFound = ndberror_cl_no_data_found,

    /**
     * E.g. inserting a tuple with a primary key already existing 
     * in the table.
     */
    ConstraintViolation = ndberror_cl_constraint_violation,

    /**
     * Error in creating table or usage of table.
     */
    SchemaError = ndberror_cl_schema_error,

    /**
     * Error occurred in interpreted program.
     */
    UserDefinedError = ndberror_cl_user_defined,
    
    /**
     * E.g. insufficient memory for data or indexes.
     */
    InsufficientSpace = ndberror_cl_insufficient_space,

    /**
     * E.g. too many active transactions.
     */
    TemporaryResourceError = ndberror_cl_temporary_resource,

    /**
     * Temporary failures which are probably inflicted by a node
     * recovery in progress.  Examples: information sent between
     * application and NDB lost, distribution change.
     */
    NodeRecoveryError = ndberror_cl_node_recovery,

    /**
     * E.g. out of log file space.
     */
    OverloadError = ndberror_cl_overload,

    /**
     * Timeouts, often inflicted by deadlocks in NDB.
     */
    TimeoutExpired = ndberror_cl_timeout_expired,
    
    /**
     * Is is unknown whether the transaction was committed or not.
     */
    UnknownResultError = ndberror_cl_unknown_result,
    
    /**
     * A serious error in NDB has occurred.
     */
    InternalError = ndberror_cl_internal_error,

    /**
     * A function used is not yet implemented.
     */
    FunctionNotImplemented = ndberror_cl_function_not_implemented,

    /**
     * Error handler could not determine correct error code.
     */
    UnknownErrorCode = ndberror_cl_unknown_error_code,

    /**
     * Node shutdown
     */
    NodeShutdown = ndberror_cl_node_shutdown,

    /**
     * Schema object already exists
     */
    SchemaObjectExists = ndberror_cl_schema_object_already_exists,

    /**
     * Request sent to non master
     */
    InternalTemporary = ndberror_cl_internal_temporary
  };
  
  /**
   * Error status.  
   */
  Status status;

  /**
   * Error type
   */
  Classification classification;
  
  /**
   * Error code
   */
  int code;

  /**
   * Mysql error code
   */
  int mysql_code;

  /**
   * Error message
   */
  const char * message;

#ifndef DOXYGEN_SHOULD_SKIP_DEPRECATED
  /**
   * The detailed description.  This is extra information regarding the 
   * error which is not included in the error message.
   *
   * @note Is NULL when no details specified
   */
  char * details;
#endif

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  NdbError(){
    status = UnknownResult;
    classification = NoError;
    code = 0;
    mysql_code = 0;
    message = 0;
    details = 0;
  }
  NdbError(const ndberror_struct & ndberror){
    status = (NdbError::Status) ndberror.status;
    classification = (NdbError::Classification) ndberror.classification;
    code = ndberror.code;
    mysql_code = ndberror.mysql_code;
    message = ndberror.message;
    details = ndberror.details;
  }
  operator ndberror_struct() const {
    ndberror_struct ndberror;
    ndberror.status = (ndberror_status_enum) status;
    ndberror.classification = (ndberror_classification_enum) classification;
    ndberror.code = code;
    ndberror.mysql_code = mysql_code;
    ndberror.message = message;
    ndberror.details = details;
    return ndberror;
  }
#endif
};

class NdbOut& operator <<(class NdbOut&, const NdbError &);
class NdbOut& operator <<(class NdbOut&, const NdbError::Status&);
class NdbOut& operator <<(class NdbOut&, const NdbError::Classification&);
#endif
