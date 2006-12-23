/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/**
 * @mainpage DBA User Guide
 *
 * @section secIntro Introduction
 * DBA is an API to access the NDB Cluster.
 * 
 * DBA supports transactions using an asynchronous execution model.
 * Everything but transactions is synchronous.
 *
 * DBA uses the concept of bindings to simplify database access.
 * A <em>binding</em> is a relation between a database table and 
 * one or several C structs.
 * A binding is created initially and then used multiple time during
 * application execution.
 *
 * Each of the data accessing functions in DBA is implemented as a
 * transaction, i.e. the call will either fully complete or 
 * nothing happens (the transaction fails).
 *
 * DBA also supports "read as much as possible" with bulk read.
 * With bulk read the application can specify a set of primary keys and 
 * try to read all of the corresponding rows. The bulk read will not fail 
 * if a row does not exist but will instead inform the application using a 
 * RowFoundIndicator variable.
 *
 * A <em>request</em> is a transaction or a bulk read.
 *
 * @section secError Error Handling
 * When a synchronous method in DBA fails these methods are applicable:
 * -# DBA_GetLatestError()
 * -# DBA_GetLatestNdbError()
 * -# DBA_GetLatestErrorMsg()
 *
 * The DBA_GetLatestErrorMsg() will then return a description of 
 * what has failed.
 *
 * For asynchronous methods the application should:
 * -# check that the RequestId returned by function is not 
 *    @ref DBA_INVALID_REQID
 * -# check Status supplied in callback (see @ref DBA_AsyncCallbackFn_t)
 * 
 * If @ref DBA_INVALID_REQID is returned, 
 * the details of error can be found using
 * "latest"-functions.
 *
 * If error is indicated in callback (using Status), when the 
 * "latest"-functions are <b>NOT</b> applicable.
 *
 * @section secExamples Example Programs
 *
 * - @ref common.hpp
 * - @ref basic.cpp
 * - @ref br_test.cpp
 * - @ref ptr_binding_test.cpp
 *
 */

/**
 * @page basic.cpp basic.cpp
 * @include basic.cpp 
 */

/**
 * @page common.hpp common.hpp
 * @include common.hpp
 */

/**
 * @page br_test.cpp br_test.cpp
 * @include br_test.cpp 
 */

/**
 * @page ptr_binding_test.cpp ptr_binding_test.cpp 
 * @include ptr_binding_test.cpp 
 */

/** @addtogroup DBA
 *  @{
 */

/****** THIS LINE IS 80 CHARACTERS WIDE - DO *NOT* EXCEED 80 CHARACTERS! ****/

#ifndef DBA_H
#define DBA_H

/* --- Include files ---- */

#include <ndb_global.h>
#include <defs/pcn_types.h>

/* --- Types and definitions --- */

/**
 * Possible error status for DBA functions.
 */
typedef enum {
  DBA_NO_ERROR = 0,         /**< Success */
    
  DBA_NOT_IMPLEMENTED = -1, /**< Function not implemented */
  DBA_NDB_ERROR = -2,       /**< Uncategorised error from NDB */
  DBA_ERROR = -3,           /**< Uncategorised error from DBA implementation */
  
  DBA_APPLICATION_ERROR = 1,    /**< Function called with invalid argument(s)
				   or other application errors */
  DBA_NO_DATA = 2,              /**< No row with specified PK existed */
  DBA_CONSTRAINT_VIOLATION = 3, /**< There already exists a row with that PK*/ 
  
  DBA_SCHEMA_ERROR = 4,        /**< Table already exists */
  DBA_INSUFFICIENT_SPACE = 5,  /**< The DB is full */
  DBA_TEMPORARY_ERROR = 6,     /**< Some temporary problem occured */
  DBA_TIMEOUT = 7,             /**< The request timed out, probably due to 
				  dead-lock */
  DBA_OVERLOAD = 8,            /**< The DB is overloaded */
  DBA_UNKNOWN_RESULT = 9       /**< It is unknown wheater transaction was
				  commited or aborted */
} DBA_Error_t;

/**
 * Error code. This is the error code that is returned by NDB.
 * Not to be confused by the status returned by the DBA implementation.
 */
typedef int DBA_ErrorCode_t;

/**
 * DBA column types
 */
typedef enum {
  DBA_CHAR,                      /**< String */
  DBA_INT                        /**< Integer */
} DBA_DataTypes_t;


/**
 * Column description.
 * Used for creating tables.
 */
typedef struct DBA_ColumnDesc {
  
    const char*      Name;       /**< Name of table column */
    DBA_DataTypes_t  DataType;   /**< Datatype of table column*/
    Size_t           Size;       /**< Column size in bytes */
    Boolean_t        IsKey;      /**< True if column is part of primary key */

} DBA_ColumnDesc_t;

/**
 * Used to simplify binding definitions. See @ref DBA_ColumnBinding
 * for example.
 * 
 * @param ColName Name of column in db table
 * @param Type Column/field type.
 * @param Struct Structure
 * @param Field Field in structure
 * @return Arg list for defining binding of type @ref DBA_Binding_t
 */
#define DBA_BINDING( ColName, Type, Struct, Field ) \
   { ColName, Type, PCN_SIZE_OF( Struct, Field ), \
     PCN_OFFSET_OF( Struct, Field ), 0, 0 }

/**
 * Used to simplify ptr binding definitions. See @ref DBA_ColumnBinding
 * for example.
 * 
 * @param Struct Structure
 * @param Field Field in structure
 * @return Arg list for defining binding of type @ref DBA_Binding_t
 */
#define DBA_BINDING_PTR(Struct, Field, ColBindings, NbCBindings) \
   { 0, DBA_CHAR, NbCBindings, PCN_OFFSET_OF( Struct, Field ), \
     1, ColBindings }

/**
 * The @ref DBA_ColumnBinding_t is used to describe a binding between one
 * column and one field of a C struct.
 *
 *<pre>
 * typedef struct Address {
 *   char StreetName[30];
 *   int  StreetNumber;
 * } Address_t;
 *
 * typdef struct Person {
 *   char        Name[30];
 *   Address_t * AddressPtr;
 * } Person_t; </pre>
 *
 *
 * For example, if the field Name of a Person_t data structure is
 * bound to the column "NAME", the corresponding binding would be
 * defined as:
 *
 *<pre>
 * DBA_ColumnBinding_t NameBinding =
 *   DBA_BINDING( "name", DBA_CHAR, Person_t, Name ); </pre>
 *
 *
 * There is also the @ref DBA_BINDING_PTR which is used when 
 * several linked structures should be put into one table.
 *
 * For example, if data in a Person_t data structure should be saved
 * in the same table as the Address_t data structure 
 * (as the address belongs to the person), the corresponding binding would be
 * defined as:
 *
 *<pre>
 * DBA_ColumnBinding_t AddrBinding[AddrLen]; This binding describes how the 
 *                                            fields in the Address_t 
 *                                            structure is linked to the 
 *                                            table PERSON_ADDRESS
 *
 * DBA_ColumnBinding_t AddressBinding = 
 *   DBA_BINDING_PTR(Person_t, AddressPtr, AddrBinding, AddrLen); </pre>
 *
 *
 */
struct DBA_ColumnBinding {
  const char*                Name;      /**< Name of table column */
  DBA_DataTypes_t            DataType;  /**< Type of member in structure */
  Size_t                     Size;      /**< Size in bytes of member
					   or no of @ref DBA_ColumnBinding's 
					   when doing ptr binding */
  Size_t                     Offset;    /**< Offset of the member */
  
  Boolean_t                  Ptr;       /**< True if binding is of ptr type */
  const struct DBA_ColumnBinding * SubBinding;  /**< Address of Binding Ptr 
						   valid if Ptr is true */
};

/**
 * Typedef: @ref DBA_ColumnBinding
 */
typedef struct DBA_ColumnBinding DBA_ColumnBinding_t;

/**
 * A @ref DBA_Binding_t object is used to establish a binding between 
 * one or more columns of a table to the fields of C structs.
 *
 * It is used with insert, and update and read transactions to define
 * on which columns of the table the operations is performed, and to
 * which members of a C data structure they map.
 *
 * All key columns must be bound to a field of the struct.
 *
 * The function @ref DBA_CreateBinding is used to create this binding.
 */
typedef struct DBA_Binding DBA_Binding_t;

/* --- Exported functions --- */

/**
 * Set DBA configuration parameter
 *<pre>
 * Id Description                 Default Min  Max
 * == =========================== ======= ==== ====
 * 0  NBP Interval                   10    4   -
 * 1  Operations/Bulkread          1000    1   5000
 * 2  Start transaction timeout       0    0   -
 * 3  Force send algorithm            1    0   2
 *</pre>
 * @return Status
 */
DBA_Error_t DBA_SetParameter(int ParameterId, int Value);

/**
 * Set DBA configuration parameter.
 * See @ref DBA_SetParameter for description of parameters.
 *
 * @return Status
 */
DBA_Error_t DBA_GetParameter(int ParameterId, int * Value);

/**
 * Initialize DBA library and connect to NDB Cluster.
 *
 * @return Status
 */
DBA_Error_t DBA_Open( ); 

/**
 * Close connection to NDB cluster and free allocated memory.
 * 
 * @return Error status
 */
DBA_Error_t DBA_Close(void); 

/**
 * Get latest DBA error.
 *
 * @note Only applicable to synchronous methods
 */
DBA_Error_t DBA_GetLatestError();

/**
 * Get latest NDB error.
 *
 * @note Only applicable to synchronous methods
 */
DBA_ErrorCode_t DBA_GetLatestNdbError();

/**
 * Get latest error string associated with DBA_GetLatestError().
 *
 * @note String must not be free by caller of this method.
 * @note Only applicable to synchronous methods.
 */
const char * DBA_GetLatestErrorMsg();

/**
 * Get error msg associated with code
 *
 * @note String must not be free by caller of this method
 */
const char * DBA_GetErrorMsg(DBA_Error_t);

/**
 * Get error msg associated with code
 *
 * @note String must not be free by caller of this method
 */
const char * DBA_GetNdbErrorMsg(DBA_ErrorCode_t);

/**
 * Create a table.
 * 
 * @param TableName Name of table to create.
 * @param NbColumns numbers of columns.
 * @param Columns Column descriptions.
 * @return Status.
 */
DBA_Error_t 
DBA_CreateTable( const char* TableName, int NbColumns, 
		 const DBA_ColumnDesc_t Columns[] );

/**
 * Destroy a table.
 * 
 * @param TableName Table name.
 * @return Status.
 * @note Not implemented
 */
DBA_Error_t 
DBA_DropTable( const char* TableName );


/**
 * Test for existence of a table.
 * 
 * @param TableName Table name.
 * @return Boolean value indicating if table exists or not.
 */
Boolean_t 
DBA_TableExists( const char* TableName );

/**
 * Define a binding between the columns of a table and a C structure.
 * 
 * @param TableName table
 * @param NbCol number of columns bindings
 * @param ColBinding bindings
 * @param StructSz Sizeof structure.
 * @return Created binding, or NULL if binding could not be created.
 */
DBA_Binding_t*
DBA_CreateBinding( const char* TableName, 
		    int NbCol, const DBA_ColumnBinding_t ColsBinding[], 
		    Size_t StructSz );

/**
 * Destroys a @ref DBA_Binding_t allocated with @ref
 * DBA_CreateBinding.  
 *
 * @param pBinding Pointer to binding.
 * @return Status.
 */
DBA_Error_t 
DBA_DestroyBinding( DBA_Binding_t* Binding );

/**
 * Used to identify a pending db request
 */
typedef long DBA_ReqId_t;

/**
 * An asynchronous call returning this means that the function was called
 * with invalid arguments. The application should check error status
 * with DBA_GetLatestError() etc.
 */
#define DBA_INVALID_REQID 0

/**
 * Callback function for transactions. 
 * Will be called in NBP process (Newton Batch Process).
 *
 * @note The implementation of the callback function is not allowed to
 * make an asynchronous database call.
 *
 * @param ReqId Request identifier
 * @param Status Status of the request 
 * @param ErrorCode Error code given by NDB
 * @see DBA_Error_t
 */
typedef void (*DBA_AsyncCallbackFn_t)( DBA_ReqId_t ReqId,
				       DBA_Error_t Status,
				       DBA_ErrorCode_t ErrorCode );
/**
 * Insert row(s) in the table (one transaction)
 * 
 * @param pBinding Binding between table columns and struct fields.
 * @param pData Array of pointers to structures. 
 * @param NbRows No of rows to insert (i.e. length of pData array)
 * @return Request identifier
 *
 * @note All the table columns must be part of the binding.
 */
DBA_ReqId_t
DBA_InsertRows( const DBA_Binding_t* pBinding, const void * const pData[],
		int NbRows,
		DBA_AsyncCallbackFn_t CbFunc );

/**
 * Insert row(s) in the table (one transaction)
 * 
 * @param pBinding Binding between table columns and struct fields.
 * @param pData Array of structures. 
 * @param NbRows No of rows to insert (i.e. length of pData array)
 * @return Request identifier
 *
 * @note All the table columns must be part of the binding.
 */
DBA_ReqId_t
DBA_ArrayInsertRows( const DBA_Binding_t* pBinding, const void * pData,
		     int NbRows,
		     DBA_AsyncCallbackFn_t CbFunc );

/**
 * Update row(s) in the table (one transaction)
 * 
 * @param pBinding Binding between table columns and struct fields.
 * @param pData Array of pointers to structures. Fields that are part of the 
 *              key are used to generate the where clause, the
 *              other fields are used to update the row.
 * @param NbRows No of rows to update (i.e. length of pData array).
 * @return Request identifier
 */
DBA_ReqId_t
DBA_UpdateRows( const DBA_Binding_t* pBinding, const void * const pData[],
		int NbRows,
		DBA_AsyncCallbackFn_t CbFunc );

/**
 * Update row(s) in the table (one transaction)
 * 
 * @param pBinding Binding between table columns and struct fields.
 * @param pData Array of structures. Fields that are part of the 
 *              key are used to generate the where clause, the
 *              other fields are used to update the row.
 * @param NbRows No of rows to update (i.e. length of pData array).
 * @return Request identifier
 */
DBA_ReqId_t
DBA_ArrayUpdateRows( const DBA_Binding_t* pBinding, const void * pData,
		     int NbRows,
		     DBA_AsyncCallbackFn_t CbFunc );

/**
 * Delete row(s) from the table (one transaction)
 * 
 * @param pBinding Binding between table columns and struct fields.
 * @param pData Array of pointers to structures. 
 *              Only fields part of the primary key needs to be set.
 * @param NbRows No of rows to delete (i.e. length of pData array)
 * @return Request identifier
 */
DBA_ReqId_t
DBA_DeleteRows( const DBA_Binding_t* pBinding, const void * const pData[],
		int NbRows,
		DBA_AsyncCallbackFn_t CbFunc );


/**
 * Delete row(s) from the table (one transaction)
 * 
 * @param pBinding Binding between table columns and struct fields.
 * @param pData Array of structures. Only fields part of the primary
 *              key needs to be set.
 * @param NbRows No of rows to delete (i.e. length of pData array)
 * @return Request identifier
 */
DBA_ReqId_t
DBA_ArrayDeleteRows( const DBA_Binding_t* pBinding, const void * pData,
		     int NbRows,
		     DBA_AsyncCallbackFn_t CbFunc );

/**
 * Updates/Inserts row(s) in the table (one transaction)
 * 
 * @param pBinding Binding between table columns and struct fields.
 * @param pData Array of pointers to structures.
 * @param NbRows No of rows to update/insert (i.e. length of pData array)
 * @return Request identifier
 * @note All the table columns must be part of the binding.
 */
DBA_ReqId_t
DBA_WriteRows( const DBA_Binding_t* pBinding, const void * const pData[],
	       int NbRows,
	       DBA_AsyncCallbackFn_t CbFunc );

/**
 * Update/Insert row(s) in the table (one transaction)
 * 
 * @param pBinding Binding between table columns and struct fields.
 * @param pData Array of structures. 
 * @param NbRows No of rows to update/insert (i.e. length of pData array)
 * @return Request identifier
 * @note All the table columns must be part of the binding.
 */
DBA_ReqId_t
DBA_ArrayWriteRows( const DBA_Binding_t* pBinding, const void * pData,
		    int NbRows,
		    DBA_AsyncCallbackFn_t CbFunc );

/**
 * Read row(s) from a table of the database (one transaction)
 * 
 * @param pBinding Binding between table columns and struct fields.
 * @param pData Array of pointers to structures. 
 *              Only fields part of the primary key needs to be set.
 *              The other fields in the binding will be populated.
 * @param NbRows No of rows to read (i.e. length of pData array)
 * @return Request identifier
 */
DBA_ReqId_t
DBA_ReadRows( const DBA_Binding_t* pBinding, void * const pData[],
	      int NbRows,
	      DBA_AsyncCallbackFn_t CbFunc );

/**
 * Read row(s) from a table of the database (one transaction)
 * 
 * @param pBinding Binding between table columns and struct fields.
 * @param pData Array of structures. 
 *              Only fields part of the primary key needs to be set.
 *              The other fields in the binding will be populated.
 * @param NbRows No of rows to read (i.e. length of pData array)
 * @return Request identifier
 */
DBA_ReqId_t
DBA_ArrayReadRows( const DBA_Binding_t* pBinding, void * pData,
		   int NbRows,
		   DBA_AsyncCallbackFn_t CbFunc );

/****** THIS LINE IS 80 CHARACTERS WIDE - DO *NOT* EXCEED 80 CHARACTERS! ****/

/**
 * Insert <b>one</b> row for each specified binding (as one transaction).
 * 
 * @param pBindings Array of pointers to bindings.
 * @param pData Array of pointers to structures. 
 * @param NbBindings No of bindings (tables) to insert into,
 *        i.e. length of arrays pBindings and pData
 * @return Request identifier
 * @note It is valid to specify the same binding twice 
 *       (with corresponding data pointer) if you want to insert two
 *       rows in one table 
 */
DBA_ReqId_t
DBA_MultiInsertRow(const DBA_Binding_t * const pBindings[],
		   const void * const pData[],
		   int NbBindings,
		   DBA_AsyncCallbackFn_t CbFunc );

/**
 * Update <b>one</b> row for each specified binding (as one transaction).
 * 
 * @param pBindings Array of pointers to bindings.
 * @param pData Array of pointers to structures. 
 * @param NbBindings No of bindings (tables) to insert into
 *        i.e. length of arrays pBindings and pData
 * @return Request identifier
 * @note It is valid to specify the same binding twice 
 *       (with corresponding data pointer) if you want to update two
 *       rows in one table 
 */
DBA_ReqId_t
DBA_MultiUpdateRow(const DBA_Binding_t * const pBindings[],
		   const void * const pData[],
		   int NbBindings,
		   DBA_AsyncCallbackFn_t CbFunc );

/**
 * Update/insert <b>one</b> row for each specified binding (as one transaction).
 * 
 * @param pBindings Array of pointers to bindings.
 * @param pData Array of pointers to structures. 
 * @param NbBindings No of bindings (tables) to insert into
 *        i.e. length of arrays pBindings and pData
 * @return Request identifier
 * @note It is valid to specify the same binding twice 
 *       (with corresponding data pointer) if you want to update/insert two
 *       rows in one table 
 */
DBA_ReqId_t
DBA_MultiWriteRow(const DBA_Binding_t * const pBindings[],
		  const void * const pData[],
		  int NbBindings,
		  DBA_AsyncCallbackFn_t CbFunc );

/**
 * Delete <b>one</b> row for each specified binding (as one transaction).
 * 
 * @param pBindings Array of pointers to bindings.
 * @param pData Array of pointers to structures. 
 * @param NbBindings No of bindings (tables) to insert into
 *        i.e. length of arrays pBindings and pData
 * @return Request identifier
 * @note It is valid to specify the same binding twice 
 *       (with corresponding data pointer) if you want to delete two
 *       rows in one table 
 */
DBA_ReqId_t
DBA_MultiDeleteRow(const DBA_Binding_t * const pBindings[],
		   const void * const pData[],
		   int NbBindings,
		   DBA_AsyncCallbackFn_t CbFunc );

/**
 * Read <b>one</b> row for each specified binding (as one transaction).
 * 
 * @param pBindings Array of pointers to bindings.
 * @param pData Array of pointers to structures. 
 * @param NbBindings No of bindings (tables) to insert into
 *        i.e. length of arrays pBindings and pData
 * @return Request identifier
 * @note It is valid to specify the same binding twice 
 *       (with corresponding data pointer) if you want to read two
 *       rows in one table 
 */
DBA_ReqId_t
DBA_MultiReadRow(const DBA_Binding_t * const pBindings[],
		 void * const pData[],
		 int NbBindings,
		 DBA_AsyncCallbackFn_t CbFunc );

/****** THIS LINE IS 80 CHARACTERS WIDE - DO *NOT* EXCEED 80 CHARACTERS! ****/

/**
 * A structure used for bulk reads.
 * The structure contains a pointer to the data and an indicator.
 * After the bulk read has completed, the indicator is set to 1 if the row
 * was found and to 0 if the row was not found.
 *
 */
typedef struct DBA_BulkReadResultSet {
  void * DataPtr;               /**< Pointer to data. Only fields part of 
				   primary key members needs
				   to be set before bulk read. */
  Boolean_t RowFoundIndicator;  /**< This indicator has a valid value
				   only after bulk read has completed.
				   If the value is 1 then the row was found */
} DBA_BulkReadResultSet_t;

/**
 * Read rows from a table of the database (potentially multiple transactions)
 * The users should for each NbRows specify the fields part of the primary key
 *
 * @param pBinding Binding between table columns and struct fields.
 * @param pData Array of DBA_BulkReadResultSet_t, with DataPtr pointing to 
 *              structure. Only the fields which are part of the 
 *              primary key need be set.
 *              The RowFoundIndicator will be set when the request returns.
 * @param NbRows No of rows to read (i.e. length of pData array)
 * @return Request identifier
 *
 */
DBA_ReqId_t
DBA_BulkReadRows(const DBA_Binding_t * pBinding,
		 DBA_BulkReadResultSet_t pData[],
		 int NbRows,
		 DBA_AsyncCallbackFn_t CbFunc );

/**
 * Read rows from several tables of the database in potentially multiple 
 * transactions.
 *
 *<pre>
 * The pData array <b>must</b> be organized as follows:
 *   NbRows with DataPtr pointing to structure of type pBindings[0]
 *   NbRows with DataPtr pointing to structure of type pBindings[1]
 *   ... </pre>
 * Meaning that the pData array must be (NbBindings * NbRows) in length.
 *
 * The user should for each (NbRows * NbBindings) specify the primary key 
 * fields.
 *
 * @param pBindings Array of pointers to bindings 
 * @param pData Array of DBA_BulkReadResultSet_t. 
 *              With DataPtr pointing to structure. Only the fields which 
 *              are part of the key need be set.
 *              The RowFoundIndicator will be set when the operations returns.
 * @param NbBindings No of bindings (i.e. length of pBindings array)
 * @param NbRows No of rows per binding to read
 * @return Request identifier
 */
DBA_ReqId_t
DBA_BulkMultiReadRows(const DBA_Binding_t * const pBindings[],
		      DBA_BulkReadResultSet_t pData[],
		      int NbBindings,
		      int NbRows,
		      DBA_AsyncCallbackFn_t CbFunc );

/** @} */

#endif
