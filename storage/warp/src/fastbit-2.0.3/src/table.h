// File: $Id$
// Author: John Wu <John.Wu at ACM.org>
// Copyright (c) 2007-2016 the Regents of the University of California
#ifndef IBIS_TABLE_H
#define IBIS_TABLE_H
/**@file

   @brief FastBit Table Interface

   This is a facade to provide a high-level view of operations on
   relational tables.  Two main classes are defined here, @c table and @c
   tablex.  The class @c table is for read-only data and it provides mostly
   querying functions.  The class @c tablex is for users to add new records
   to a table and it does not support any querying operations.
 */
#include <iostream>	// std::ostream
#include <vector>	// std::vector
#include <map>		// std::map
#include <string>	// std::string
#include "const.h"	// intxx_t, uintxx_t, ... from stdint.h

namespace ibis {

    /// Supported data types.
    enum TYPE_T {
	/// Unknown type, a place holder.  Can not process data of this type!
	UNKNOWN_TYPE=0,
	/// A special eight-byte ID type for internal use.
	OID,
	BYTE,	///!< One-byte signed integers, internally char.
	UBYTE,	///!< One-byte unsigned integers, internally unsigned char.
	SHORT,	///!< Two-byte signed integers, internally int16_t.
	USHORT, ///!< Two-byte unsigned integers, internally uint16_t.
	INT,	///!< Four-byte signed integers, internally int32_t.
	UINT,	///!< Four-byte unsigned integers, internally uint32_t.
	LONG,	///!< Eight-byte signed integers, internally int64_t.
	ULONG,	///!< Eight-byte unsigned integers, internally uint64_t.
	FLOAT,	///!< Four-byte IEEE floating-point numbers, internally float.
	DOUBLE, ///!< Eight-byte IEEE floating-point numbers, internally double.
        BIT,    ///!< One bit per record, represented by a bit vector.
	/// Low cardinality null-terminated strings.  Strings are
	/// internally stored with the null terminators.  Each string value
	/// is intended to be treated as a single atomic item.
	CATEGORY,
	/// Arbitrary null-terminated strings.  Strings are internally
	/// stored with the null terminators.  Each string could be further
	/// broken into tokens for a full-text index known as keyword
	/// index.  Could search for presence of some keywords through
	/// expression "contains" such as "contains(textcolumn, 'Berkeley',
	/// 'California')".
	TEXT,
	/// Byte array.  Also known as Binary Large Objects (blob) or
	/// opaque objects.  A column of this type requires special
	/// handling for input and output.  It can not be used as a part of
	/// any searching criteria.
	BLOB,
        /// User-defined type.  FastBit does not know much about it.
        UDT
    };
    /// Human readable version of the enumeration types.
    FASTBIT_CXX_DLLSPEC extern const char** TYPESTRING;

    class table;
    class tablex;
    class tableList;
} // namespace ibis

/// @ingroup FastBitMain
/// The abstract table class.
/// This is an abstract base class that defines the common operations on a
/// data table.  Conceptually, data records in a table is organized into
/// rows and columns.  A query on a table produces a filtered version of
/// the table.  In many database systems this is known as a view of a
/// table.  All data tables and views are logically treated as
/// specialization of this class.  An example of using this class can be
/// found in <A HREF="http://su.pr/20kDXd">examples/thula.cpp</A>.
class FASTBIT_CXX_DLLSPEC ibis::table {
public:
    /// Create a simple of container of a partition.  The objective is to
    /// make the functions of this class available.  The caller retains the
    /// ownership of the data partition.
    static ibis::table* create(ibis::part&);
    /// Create a container of externally managed data partitions.  The
    /// objective is to make the functions of this class available.  The
    /// caller retains the ownership of the data partition.
    static ibis::table* create(const ibis::partList&);
    /// Create a table object from the specified data directory.  If the
    /// argument is a nil pointer, it will examine configuration parameters
    /// to find locations of data patitions.
    static ibis::table* create(const char* dir);
    /// Create a table object from a pair of data directories.  The
    /// intention of maintaining two sets of data files is to
    /// process queries using one set while accept new data records
    /// with the other.  However, such functionality is not currently
    /// implemented!
    static ibis::table* create(const char* dir1, const char* dir2);

    /// Destructor.
    virtual ~table() {};

    /// Name of the table.  A valid table shall not return a null pointer
    /// nor an empty string.
    virtual const char* name() const {return name_.c_str();}
    /// Free text description.  May return a null pointer.
    virtual const char* description() const {return desc_.c_str();}
    /// The number of rows in this table.
    virtual uint64_t nRows() const =0;
    /// The number of columns in this table.
    virtual uint32_t nColumns() const =0;

    /// A list of strings.
    /// @note The pointers are expected to point to names stored internally.
    /// The caller should not attempt to free these pointers.
    typedef ibis::array_t<const char*> stringArray;
    typedef std::vector<const char*> stringVector;
    /// A list of data types.
    typedef ibis::array_t<ibis::TYPE_T> typeArray;
    /// A list to hold the in-memory buffers.  The void* is either
    /// ibis::array_t* or std::vector<std::string> depending on the
    /// underlying data type.  Typically used together with typeArray.
    typedef ibis::array_t<void *> bufferArray;
    /// An associative array of names and types.
    typedef std::map<const char*, ibis::TYPE_T, ibis::lessi> namesTypes;

    virtual stringArray columnNames() const =0; ///!< Return column names.
    virtual typeArray columnTypes() const =0; ///!< Return data types.

    /// Print a description of the table to the specified output stream.
    virtual void describe(std::ostream&) const =0;
    /// Print all column names on one line.
    virtual void dumpNames(std::ostream& out, const char* del=", ") const =0;
    /// Print the values in ASCII form to the specified output stream.  The
    /// default delimiter is coma (","), which produces
    /// Comma-Separated-Values (CSV).
    virtual int dump(std::ostream& out, const char* del=", ") const =0;
    /// Print the first nr rows.
    virtual int dump(std::ostream& out, uint64_t nr,
		     const char* del=", ") const =0;
    /// Print nr rows starting with row offset.  Note that the row number
    /// starts with 0, i.e., the first row is row 0.
    virtual int dump(std::ostream& out, uint64_t offset, uint64_t nr,
		     const char* del=", ") const =0;
    /// Write the current content to the specified output directory in
    /// the raw binary format.  May optionally overwrite the name and
    /// description of the table.
    virtual int backup(const char* dir, const char* tname=0,
		       const char* tdesc=0) const =0;

    /// Estimate the number of rows satisfying the selection conditions.
    /// The number of rows is between [@c nmin, @c nmax] (inclusive).
    virtual void estimate(const char* cond,
			  uint64_t& nmin, uint64_t& nmax) const =0;
    /// Estimate the number of rows satisfying the selection conditions.
    /// The number of rows is between [@c nmin, @c nmax] (inclusive).
    virtual void estimate(const ibis::qExpr* cond,
			  uint64_t& nmin, uint64_t& nmax) const =0;
    /// Given a set of column names and a set of selection conditions,
    /// compute another table that represents the selected values.
    virtual table* select(const char* sel, const char* cond) const =0;
    /// Process the selection conditions and generate another table to hold
    /// the answer.
    virtual table* select(const char* sel, const ibis::qExpr* cond) const;

    /// Perform the select operation on a list of data partitions.
    static table* select(const ibis::constPartList& parts,
			 const char* sel, const char* cond);
    /// Perform select operation using a user-supplied query expression.
    static table* select(const ibis::constPartList& parts,
			 const char* sel, const ibis::qExpr* cond);
    /// Compute the number of rows satisfying the specified conditions.
    static int64_t computeHits(const ibis::constPartList& parts,
			       const char* cond);
    /// Compute the number of rows satisfying the specified query expression.
    static int64_t computeHits(const ibis::constPartList& parts,
			       const ibis::qExpr* cond);

    /// Perform aggregate functions on the current table.  It produces a
    /// new table.  The list of strings passed to this function are
    /// interpreted as a set of names followed by a set of functions.
    /// Currently, only functions COUNT, AVG, MIN, MAX, SUM, VARPOP,
    /// VARSAMP, STDPOP, STDSAMP and DISTINCT are supported, and the
    /// functions can only accept a column name as arguments.
    virtual table* groupby(const stringArray&) const =0;
    /// Perform a group-by operation.  The column names and operations are
    /// separated by commas.
    virtual table* groupby(const char*) const;
    /// Reorder the rows.  Sort the rows in ascending order of the columns
    /// specified in the list of column names.  This function is not
    /// designated @c const even though it does not change the content
    /// in SQL logic, but it may change internal representations.
    /// @note If an empty list is passed to this function, it will reorder
    /// rows using all columns with the column having the smallest number
    /// of distinct values first.
    virtual void orderby(const stringArray&)=0;
    virtual void orderby(const stringArray&, const std::vector<bool>&)=0;
    /// Reorder the rows.  The column names are separated by commas.
    virtual void orderby(const char*);
    /// Reverse the order of the rows.
    virtual void reverseRows()=0;

    /// Add a data partition defined in the named directory.  Upon
    /// successful completion, it returns the number of data partitions
    /// found, otherwise it returns a negative number to indicate failure.
    ///
    /// If the name of the directory is a nil pointer, this function will
    /// examine the entries in the configuration parameters to identify
    /// locations of data partitions.  This matches the behavior of
    /// ibis::table::create.
    ///
    /// @note The intent is for this function to recursively examine its
    /// subdirecories when possible.  Therefore it may find an arbitrary
    /// number of data partitions.
    virtual int addPartition(const char*) {return -1;}
    /// Remove the named data partition from this data table.  The incoming
    /// argument is expected to the name of the data partition.
    ///
    /// @note If it is not a name of any data partition, we check if it is
    /// the name of the data directory.  In the process of matching
    /// directory names, we will match the leading port of the directory
    /// name only.  This allows the data partitions added through a single
    /// call of addPartition to be dropped with a single call to this
    /// function using the same arguement.
    virtual int dropPartition(const char*) {return -1;}
    /// Retrieve the list of partitions.
    virtual int getPartitions(ibis::constPartList&) const {
	return -1;}

    /// The following functions deal with auxillary data for accelerating
    /// query processing, primarily for building indexes.
    /// @{
    /// Create the index for the named column.  The existing index will be
    /// replaced.  If an indexing option is not specified, it will use the
    /// internally recorded option for the named column or the table
    /// containing the column.
    ///
    /// @note Unless there is a specific instruction to not index a column,
    /// the querying functions will automatically build indexes as
    /// necessary.  However, as building an index is relatively expensive,
    /// building an index on a column is on average about four or five
    /// times as expensive as reading the column from disk, this function
    /// is provided to build indexes beforehand.
    virtual int buildIndex(const char* colname, const char* option=0) =0;
    /// Create indexes for every column of the table.  Existing indexes
    /// will be replaced.  If an indexing option is not specified, the
    /// internally recorded options will be used.
    /// @sa buildIndex
    virtual int buildIndexes(const char* options) =0;
    virtual int buildIndexes(const stringArray&) =0;
    /// Retrieve the current indexing option.  If no column name is
    /// specified, it retrieve the indexing option for the table.
    virtual const char* indexSpec(const char* colname=0) const =0;
    /// Replace the current indexing option.  If no column name is
    /// specified, it resets the indexing option for the table.
    virtual void indexSpec(const char* opt, const char* colname=0) =0;
    /// Merge the dictionaries of categorical value from different data
    /// partitions.  The argument is a list of column names.  If the
    /// incoming list is empty, then dictionaries of categorical columns
    /// with the same names are combined.  If a list is provided by the
    /// caller, then all columns with the given names will be placed in a
    /// single dictionary.  Additionally, all indexes associated with the
    /// columns will be updated to make use of the new combined dictionary.
    ///
    /// A default implementation is provided.  This default implementation
    /// does nothing and returns 0.  This action is valid for a table with
    /// only a single partition and the incoming list is empty.
    virtual int mergeCategories(const stringArray&) {return 0;}
    /// @}

    /// Retrieve all values of the named column.  The member functions of
    /// this class only support access to one column at a time.  Use @c
    /// table::cursor class for row-wise accesses.
    ///
    /// The arguments begin and end are given in row numbers starting from
    /// 0.  If begin < end, then rows begin till end-1 are packed into the
    /// output array.  If 0 == end (i.e., leaving end as the default
    /// value), then the values from begin till end of the table is packed
    /// into the output array.  The default values where both begin and end
    /// are 0 define a range covering all rows of the table.
    ///
    /// These functions return the number of elements copied upon
    /// successful completion, otherwise they return a negative number to
    /// indicate failure.
    ///
    /// @note For fixed-width data types, the raw pointers are used to
    /// point to the values to be returned.  In these cases, the caller is
    /// responsible for allocating enough storage for the values to be
    /// returned.
    ///
    ///  @{
    virtual int64_t
	getColumnAsBytes(const char* cname, char* vals,
			 uint64_t begin=0, uint64_t end=0) const =0;
    virtual int64_t
	getColumnAsUBytes(const char* cname, unsigned char* vals,
			  uint64_t begin=0, uint64_t end=0) const =0;
    virtual int64_t
	getColumnAsShorts(const char* cname, int16_t* vals,
			  uint64_t begin=0, uint64_t end=0) const =0;
    virtual int64_t
	getColumnAsUShorts(const char* cname, uint16_t* vals,
			   uint64_t begin=0, uint64_t end=0) const =0;
    virtual int64_t
	getColumnAsInts(const char* cname, int32_t* vals,
			uint64_t begin=0, uint64_t end=0) const =0;
    virtual int64_t
	getColumnAsUInts(const char* cname, uint32_t* vals,
			 uint64_t begin=0, uint64_t end=0) const =0;
    virtual int64_t
	getColumnAsLongs(const char* cname, int64_t* vals,
			 uint64_t begin=0, uint64_t end=0) const =0;
    virtual int64_t
	getColumnAsULongs(const char* cname, uint64_t* vals,
			  uint64_t begin=0, uint64_t end=0) const =0;
    virtual int64_t
	getColumnAsFloats(const char* cname, float* vals,
			  uint64_t begin=0, uint64_t end=0) const =0;
    virtual int64_t
	getColumnAsDoubles(const char* cname, double* vals,
			   uint64_t begin=0, uint64_t end=0) const =0;
    virtual int64_t
	getColumnAsDoubles(const char* cname, std::vector<double>& vals,
			   uint64_t begin=0, uint64_t end=0) const =0;
    /// Retrieve the null-terminated strings as a vector of std::string
    /// objects.  Both ibis::CATEGORY and ibis::TEXT types can be retrieved
    /// using this function.
    virtual int64_t
	getColumnAsStrings(const char* cname, std::vector<std::string>& vals,
			   uint64_t begin=0, uint64_t end=0) const =0;
    /// Retrieve the blobs as ibis::opaque objects.  Only work on the
    /// column type BLOB.
    virtual int64_t
	getColumnAsOpaques(const char* cname, std::vector<ibis::opaque>& vals,
			   uint64_t begin=0, uint64_t end=0) const =0;

    /// Compute the minimum of all valid values in the name column.  In
    /// case of error, such as an invalid column name or an empty table,
    /// this function will return FASTBIT_DOUBLE_NULL or
    /// DBL_MAX to ensure that the following test fails
    /// getColumnMin <= getColumnMax.
    virtual double getColumnMin(const char* cname) const =0;
    /// Compute the maximum of all valid values in the name column.  In
    /// case of error, such as an invalid column name or an empty table,
    /// this function will return FASTBIT_DOUBLE_NULL or
    /// -DBL_MAX to ensure that the following test fails
    /// getColumnMin <= getColumnMax.
    virtual double getColumnMax(const char* cname) const =0;
    /// @}

    /// @{
    /// Compute the histogram of the named column.  This version uses the
    /// user specified bins:
    /// @code [begin, begin+stride) [begin+stride, begin+2*stride) ....@endcode
    /// A record is placed in bin
    /// @code (x - begin) / stride, @endcode where the first bin is bin 0.
    /// The total number of bins is
    /// @code 1 + floor((end - begin) / stride). @endcode
    /// @note Records (rows) outside of the range [begin, end] are not
    /// counted.
    /// @note Non-positive @c stride is considered as an error.
    /// @note If @c end is less than @c begin, an empty array @c counts is
    /// returned along with return value 0.
    virtual long getHistogram(const char* constraints,
			      const char* cname,
			      double begin, double end, double stride,
			      std::vector<uint32_t>& counts) const =0;
    /// Compute a two-dimension histogram on columns @c cname1 and @c
    /// cname2.  The bins along each dimension are defined the same way as
    /// in function @c getHistogram.  The array @c counts stores the
    /// two-dimensional bins with the first dimension as the slow varying
    /// dimension following C convention for ordering multi-dimensional
    /// arrays.
    virtual long getHistogram2D(const char* constraints,
				const char* cname1,
				double begin1, double end1, double stride1,
				const char* cname2,
				double begin2, double end2, double stride2,
				std::vector<uint32_t>& counts) const =0;
    /// Compute a three-dimensional histogram on the named columns.  The
    /// triplets <begin, end, stride> are used the same ways in @c
    /// getHistogram and @c getHistogram2D.  The three dimensional bins
    /// are linearized in @c counts with the first being the slowest
    /// varying dimension and the third being the fastest varying dimension
    /// following the C convention for ordering multi-dimensional arrays.
    virtual long getHistogram3D(const char* constraints,
				const char* cname1,
				double begin1, double end1, double stride1,
				const char* cname2,
				double begin2, double end2, double stride2,
				const char* cname3,
				double begin3, double end3, double stride3,
				std::vector<uint32_t>& counts) const =0;
    /// @}

    /// A simple struct for storing a row of a table.
    struct row {
	std::vector<std::string>   bytesnames; ///!< For ibis::BYTE.
	std::vector<signed char>   bytesvalues;
	std::vector<std::string>   ubytesnames; ///!< For ibis::UBYTE.
	std::vector<unsigned char> ubytesvalues;
	std::vector<std::string>   shortsnames; ///!< For ibis::SHORT.
	std::vector<int16_t>       shortsvalues;
	std::vector<std::string>   ushortsnames; ///!< For ibis::USHORT.
	std::vector<uint16_t>      ushortsvalues;
	std::vector<std::string>   intsnames; ///!< For ibis::INT.
	std::vector<int32_t>       intsvalues;
	std::vector<std::string>   uintsnames; ///!< For ibis::UINT.
	std::vector<uint32_t>      uintsvalues;
	std::vector<std::string>   longsnames; ///!< For ibis::LONG.
	std::vector<int64_t>       longsvalues;
	std::vector<std::string>   ulongsnames; ///!< For ibis::ULONG.
	std::vector<uint64_t>      ulongsvalues;
	std::vector<std::string>   floatsnames; ///!< For ibis::FLOAT.
	std::vector<float>         floatsvalues;
	std::vector<std::string>   doublesnames; ///!< For ibis::DOUBLE.
	std::vector<double>        doublesvalues;
	std::vector<std::string>   catsnames; ///!< For ibis::CATEGORY.
	std::vector<std::string>   catsvalues;
	std::vector<std::string>   textsnames; ///!< For ibis::TEXT.
	std::vector<std::string>   textsvalues;
	std::vector<std::string>   blobsnames; ///!< For ibis::BLOB
	std::vector<ibis::opaque>  blobsvalues;

	/// Clear all names and values.
	void clear();
	/// Clear the content of arrays of values.  Leave the names alone.
	void clearValues();
	/// The number of columns in the row.
	uint32_t nColumns() const {
	    return bytesvalues.size() + ubytesvalues.size() +
		shortsvalues.size() + ushortsvalues.size() +
		intsvalues.size() + uintsvalues.size() +
		longsvalues.size() + ulongsvalues.size() +
		floatsvalues.size() + doublesvalues.size() +
		catsvalues.size() + textsvalues.size() + blobsvalues.size();}
    }; // struct row

    // Cursor class for row-wise data accesses.
    class cursor;
    /// Create a @c cursor object to perform row-wise data access.
    virtual cursor* createCursor() const =0;

    static void* allocateBuffer(ibis::TYPE_T, size_t);
    static void freeBuffer(void* buffer, ibis::TYPE_T type);
    static void freeBuffers(bufferArray&, typeArray&);


    static void parseNames(char* in, stringVector& out);
    static void parseNames(char* in, stringArray& out);
    static void parseOrderby(char* in, stringArray& out,
			     std::vector<bool>& direc);
    static bool isValidName(const char*);
    static void consecrateName(char*);

protected:

    std::string name_;	///!< Name of the table.
    std::string desc_;	///!< Description of the table.

    /// The default constructor.
    table() {};
    /// Constructor.  Use the user-supplied name and description.
    table(const char* na, const char* de)
	: name_(na?na:""), desc_(de?de:na?na:"") {};

private:
    // re-enforce the prohibitions on copying and assignment.
    table(const table&);
    table& operator=(const table&);
}; // class ibis::table

/// @ingroup FastBitMain
/// The class for expandable tables.
/// It is designed to temporarily store data in memory and then write the
/// records out through the function write.  After creating a object of
/// this type, the user must first add columns by calling addColumn.  New
/// data records may be added one column at a time or one row at a time.
/// An example of using this class is in <A
/// HREF="http://goo.gl/pJDFw">examples/ardea.cpp</A>.
///
/// @note Most functions that return an integer return 0 in case of
/// success, a negative value in case error and a positive number as
/// advisory information.
class FASTBIT_CXX_DLLSPEC ibis::tablex {
public:
    /// Create a minimalistic table exclusively for entering new records.
    static ibis::tablex* create();
//     /// Make the incoming table expandable.  Not yet implemented
//     static ibis::tablex* makeExtensible(ibis::table* t);

    virtual ~tablex() {}; // nothing to do.

    /// Add a column.
    virtual int addColumn(const char* cname, ibis::TYPE_T ctype,
			  const char* cdesc=0, const char* idx=0) =0;

    /// Add values to the named column.  The column name must be in the
    /// table already.  The first value is to be placed at row @c begin (the
    /// row numbers start with 0) and the last value before row @c end.
    /// The array @c values must contain (end - begin) values of the type
    /// specified through addColumn.
    ///
    /// The expected types of values are "const std::vector<std::string>*"
    /// for string valued columns, and "const T*" for a fix-sized column of
    /// type T.  For example, if the column type is float, the type of
    /// values is "const float*"; if the column type is category, the type
    /// of values is "const std::vector<std::string>*".
    ///
    /// @note Since each column may have different number of rows filled,
    /// the number of rows in the table is considered to be the maximum
    /// number of rows filled of all columns.
    ///
    /// @note This function can not be used to introduce new columns in a
    /// table.  A new column must be added with @c addColumn.
    ///
    /// @sa appendRow
    virtual int append(const char* cname, uint64_t begin, uint64_t end,
		       void* values) =0;

    /// Add one row.  If an array of names has the same number of elements
    /// as the array of values, the names are used as column names.  If the
    /// names are not specified explicitly, the values are assigned to the
    /// columns of the same data type in the order as they are specified
    /// through @c addColumn or if the same order as they are recreated
    /// from an existing dataset (which is typically alphabetical).
    ///
    /// Return the number of values added to the new row.
    ///
    /// @note The column names are not case-sensitive.
    ///
    /// @note Like @c append, this function can not be used to introduce
    /// new columns in a table.  A new column must be added with @c
    /// addColumn.
    ///
    /// @note Since the various columns may have different numbers of rows
    /// filled, the number of rows in the table is assumed to the largest
    /// number of rows filled so far.  The new row appended here increases
    /// the number of rows in the table by 1.  The unfilled rows are
    /// assumed to be null.
    ///
    /// @note A null value is internally denoted with a mask separated from
    /// the data values.  However, since the rows corresponding to the null
    /// values must be filled with some value in this implementation, the
    /// following is how their values are filled.  A null value of an
    /// integer column is filled as the maximum possible of the type of
    /// integer.  A null value of a floating-point valued column is filled
    /// as a quiet NaN (Not-a-Number).  A null value of a string-valued
    /// column is filled with an empty string.
    virtual int appendRow(const ibis::table::row&) =0;
    /// Append a row stored in ASCII form.  The ASCII form of the values
    /// are assumed to be separated by comma (,) or space, but additional
    /// delimiters may be added through the second argument.
    ///
    /// Return the number of values added to the new row.
    virtual int appendRow(const char* line, const char* delimiters=0) = 0;
    /// Add multiple rows.  Rows in the incoming vector are processed on
    /// after another.  The ordering of the values in earlier rows are
    /// automatically carried over to the later rows until another set of
    /// names is specified.
    ///
    /// Return the number of new rows added.
    /// @sa appendRow
    virtual int appendRows(const std::vector<ibis::table::row>&) =0;

    /// Read the content of the named file as comma-separated values.
    /// Append the records to this table.  If the argument memrows is
    /// greater than 0, this function will reserve space to read this many
    /// records.  If the total number of records is more than memrows and
    /// the output directory name is specified, then the records will be
    /// written the outputdir and the memory is made available for later
    /// records.  If outputdir is not specified, this function attempts to
    /// expand the memory allocated, which may run out of memory.
    /// Furthermore, repeated allocations can be time-consuming.
    ///
    /// By default the records are delimited by comma (,) and blank space.
    /// One may specify alternative delimiters using the last argument.
    ///
    /// Upon successful completion of this funciton, the return value is
    /// the number of rows processed.  However, not all of them may remain
    /// in memory because ealier rows may have been written to disk.
    ///
    /// @note Information about column names and types must be provided
    /// before calling this function.
    ///
    /// @note The return value is intentionally left as 32-bit integer,
    /// which limits the maximum number of rows can be correctly handled.
    ///
    /// @note This function processes the input text file one line at a
    /// time by using the standard unix read function to perform the actual
    /// I/O operations.  Depending on the I/O libraries used, it may expect
    /// the end-of-line character to be unix-style.  If your text file is
    /// not terminated with the unix-style end-of-line character, then it
    /// is possible for this function to understand the lines incorrectly.
    /// If you see an entire line being read as one single field, then it
    /// is likely that you are have problem with the end-of-line character.
    /// Please try to convert the end-of-line character and give it another
    /// try.
    virtual int readCSV(const char* inputfile, int memrows=0,
			const char* outputdir=0, const char* delimiters=0) =0;
    /// Read a SQL dump from database systems such as MySQL.  The entire
    /// file will be read into memory in one shot unless both memrows and
    /// outputdir are specified.  In cases where both memrows and outputdir
    /// are specified, this function reads a maximum of memrows before
    /// write the data to outputdir under the name tname, which leaves no
    /// more than memrows number of rows in memory.  The value returned
    /// from this function is the number of rows processed including those
    /// written to disk.  Use function mRows to determine how many are
    /// still in memory.
    ///
    /// If the SQL dump file contains statement to create table, then the
    /// existing metadata is overwritten.  Otherwise, it reads insert
    /// statements and convert the ASCII data into binary format in memory.
    virtual int readSQLDump(const char* inputfile, std::string& tname,
			    int memrows=0, const char* outputdir=0) =0;

    /// Read a file containing the names and types of columns.
    virtual int readNamesAndTypes(const char* filename);
    /// Parse names and data types in string form.
    virtual int parseNamesAndTypes(const char* txt);

    /// Write the in-memory data records to the specified directory and
    /// update the metadata on disk.  If the table name (@c tname) is a
    /// null string or an empty string, the last component of the directory
    /// name is used.  If the description (@c tdesc) is a null string or an
    /// empty string, a time stamp will be printed in its place.  If the
    /// specified directory already contains data, the new records will be
    /// appended to the existing data.  In this case, the table name
    /// specified here will overwrite the existing name, but the existing
    /// name and description will be retained if the current arguments are
    /// null strings or empty strings.  The data type associated with this
    /// table will overwrite the existing data type information.  If the
    /// index specification is not null, the existing index specification
    /// will be overwritten.
    ///
    /// @arg @c dir The output directory name.  Must be a valid directory
    /// name.  The named directory will be created if it does not already
    /// exist.
    ///
    /// @arg @c tname Table name.  Should be a valid string, otherwise, a
    /// random name is generated as FastBit requires a name for each table.
    ///
    /// @arg @c tdesc Table description.  An optional description of the
    /// table.  It can be an arbitrary string.
    ///
    /// @arg @c idx Indexing option for all columns of the table without
    /// its own indexing option.  More information about <A
    /// href="http://goo.gl/rmvsr">indexing options</A> is available
    /// elsewhere.
    ///
    /// @arg @c nvpairs An arbitrary list of name-value pairs to be
    /// associated with the data table.  An arbitrary number of name-value
    /// pairs may be given here, however, FastBit may not be able to do
    /// much about them.  One useful of the form "columnShape=(nd1, ...,
    /// ndk)" can be used to tell FastBit that the table table is defined
    /// on a simple regular k-dimensional mesh of size nd1 x ... x ndk.
    /// Internally, these name-value pairs associated with a data table is
    /// known as meta tags or simply tags.
    ///
    /// Return the number of rows written to the specified directory on
    /// successful completion.
    virtual int write(const char* dir, const char* tname=0,
		      const char* tdesc=0, const char* idx=0,
		      const char* nvpairs=0) const =0;
    /// Write out the information about the columns.  It will write the
    /// metadata file containing the column information and index
    /// specifications if no metadata file already exists.  It returns the
    /// number of columns written to the metadata file upon successful
    /// completion, returns 0 if a metadata file already exists, and
    /// returns a negative number to indicate errors.  If there is no
    /// column in memory, nothing is written to the output directory.
    ///
    /// @note The formal arguments of this function are exactly same as
    /// those of ibis::tablex::write.
    ///
    /// @warning This function does not preserve the existing metadata!
    /// Use with care.
    virtual int writeMetaData(const char* dir, const char* tname=0,
			      const char* tdesc=0, const char* idx=0,
			      const char* nvpairs=0) const =0;

    /// Remove all data recorded.  Keeps the information about columns.  It
    /// is intended to prepare for new rows after invoking the function
    /// write.
    virtual void clearData() =0;
    /// Reserve enough buffer space for the specified number of rows.
    /// Return the number of rows that can be stored or a negative number
    /// to indicate error.  Since the return value is a 32-bit signed
    /// integer, it is not possible to represent number greater or equal to
    /// 2^31 (~2 billion), the caller shall not attempt to reserve space
    /// for 2^31 rows (or more).
    ///
    /// The intention is to mimize the number of dynamic memory allocations
    /// needed expand memory used to hold the data.  The implementation of
    /// this function is not required, and the user is not required to call
    /// this function.
    virtual int32_t reserveBuffer(uint32_t) {return 0;}
    /// Capacity of the memory buffer.  Report the maximum number of rows
    /// can be stored with this object before more memory will be
    /// allocated.  A return value of zero (0) may also indicate that it
    /// does not know about its capacity.
    ///
    /// @note For string valued columns, the resvation is not necessarily
    /// allocating space required for the actual string values.  Thus it is
    /// possible to run out of memory before the number of rows reported by
    /// mRows reaches the value returned by this function.
    virtual uint32_t bufferCapacity() const {return 0;}

    /// The number of rows in memory.  It is the maximum number of rows in
    /// any column.
    virtual uint32_t mRows() const =0;
    /// The number of columns in this table.
    virtual uint32_t mColumns() const =0;
    /// Print a description of the table to the specified output stream.
    virtual void describe(std::ostream&) const =0;

    /// Stop expanding the current set of data records.  Convert a tablex
    /// object into a table object, so that they can participate in
    /// queries.  The data records held by the tablex object is transfered
    /// to the table object, however, the metadata remains with this
    /// object.
    virtual table* toTable(const char* nm=0, const char* de=0) =0;

    /// Set the recommended number of rows in a data partition.
    virtual void setPartitionMax(uint32_t m) {maxpart=m;}
    /// Get the recommended number of rows in a data partition.
    virtual uint32_t getPartitionMax() const {return maxpart;}
    /// Set the name of the ASCII dictionary file for a column of
    /// categorical values.
    virtual void setASCIIDictionary(const char*, const char*) =0;
    /// Retrieve the name of the ASCII dictionary file associated with a
    /// column of categorical values.
    virtual const char* getASCIIDictionary(const char*) const =0;

protected:
    /// Protected default constructor.  Derived classes need a default
    /// constructor.
    tablex() : maxpart(0), ipart(0) {};

    /// Recommended size of data partitions to be created.
    uint32_t maxpart;
    /// Current partition number being used for writing.
    mutable uint32_t ipart;

private:
    tablex(const tablex&); // no copying
    tablex& operator=(const tablex&); // no assignment
}; // class ibis::tablex

/// A list of tables.  It supports simple lookup through operator[] and
/// manages the table objects passed to it.  Most functions are simply
/// wrappers on std::map.
class FASTBIT_CXX_DLLSPEC ibis::tableList {
public:
    typedef std::map< const char*, ibis::table*, ibis::lessi > tableSet;
    typedef tableSet::const_iterator iterator;

    /// Is the list empty?  Returns true if the list is empty, otherwise
    /// returns false.
    bool empty() const {return tables.empty();}
    /// Return the number of tables in the list.
    uint32_t size() const {return tables.size();}
    /// Return the iterator to the first table.
    iterator begin() const {return tables.begin();}
    /// Return the iterator to the end of the list.  Following STL
    /// convention, the @c end is always one past the last element in the
    /// list.
    iterator end() const {return tables.end();}

    /// Find the named table.  Returns null pointer if no table with the
    /// given name is found.
    const ibis::table* operator[](const char* tname) const {
	tableSet::const_iterator it = tables.find(tname);
	if (it != tables.end())
	    return (*it).second;
	else
	    return 0;
    }

    /// Add a new table object to the list.  Transfers the control of the
    /// object to the tableList.  If the name of the table already exists,
    /// the existing table will be passed back out, otherwise, the argument
    /// @c tb is set to null.  In either case, the caller can call delete
    /// on the variable and should do so to avoid memory leak.
    void add(ibis::table*& tb) {
	tableSet::iterator it = tables.find(tb->name());
	if (it == tables.end()) {
	    tables[tb->name()] = tb;
	    tb=0;
	}
	else {
	    ibis::table* tmp = (*it).second;
	    tables[tb->name()] = tb;
	    tb = tmp;
	}
    }

    /// Remove the named data table from the list.  The destructor of this
    /// function automatically clean up all table objects, there is no need
    /// to explicit remove them.
    void remove(const char* tname) {
	tableSet::iterator it = tables.find(tname);
	if (it != tables.end()) {
	    ibis::table* tmp = (*it).second;
	    tables.erase(it);
	    delete tmp;
	}
    }

    /// Default constructor.
    tableList() {};

    /// Destructor.  Delete all table objects.
    ~tableList() {
	while (! tables.empty()) {
	    tableSet::iterator it = tables.begin();
	    ibis::table* tmp = (*it).second;
	    tables.erase(it);
	    delete tmp;
	}
    }

private:
    /// Actual storage of the sets of tables.
    tableSet tables;

    // Can not copy or assign.
    tableList(const tableList&);
    tableList& operator=(const tableList&);
}; // ibis::tableList

/// Cursor class for row-wise data accesses.
/// @note Note that this cursor is associated with a table object and can
/// only iterate over all rows of a table.  To iterate an arbitrary
/// selection of rows, use the select function to create a new table and then
/// iterate over the new table.
class FASTBIT_CXX_DLLSPEC ibis::table::cursor {
public:
    virtual ~cursor() {};
    virtual uint64_t nRows() const =0;
    virtual uint32_t nColumns() const =0;
    virtual ibis::table::typeArray columnTypes() const =0;
    virtual ibis::table::stringArray columnNames() const =0;
    /// Make the next row of the data set available for retrieval.  Returns
    /// 0 if successful, returns a negative number to indicate error.
    virtual int fetch() =0;
    /// Make the specified row in the data set available for retrieval.
    /// Returns 0 if the specified row is found, returns a negative number
    /// to indicate error, such as @c rownum out of range (-1).
    virtual int fetch(uint64_t rownum) =0;
    /// Return the current row number.  Rows in a data set are numbered [0
    /// - @c nRows()-1].  If the cursor is not ready, such as before the
    /// first call to @c fetch or function @c fetch returned an error, this
    /// function return the same value as function @c nRows.
    virtual uint64_t getCurrentRowNumber() const =0;

    /// Fetch the content of the next row and make the next row as the
    /// current row as well.
    virtual int fetch(ibis::table::row&) =0;
    /// Fetch the content of the specified row and make that row the
    /// current row as well.
    virtual int fetch(uint64_t rownum, ibis::table::row&) =0;

    /// Print out the values of the current row.
    virtual int dump(std::ostream& out, const char* del=", ") const =0;

    ///@{
    /// Retrieve the value of the named column.
    ///
    /// These functions return the number of elements copied upon
    /// successful completion, otherwise they return a negative number to
    /// indicate failure.
    ///
    /// @note Note the cost of name lookup is likely to dominate the total
    /// cost of such a function.
    virtual int getColumnAsByte(const char* cname, char&) const =0;
    virtual int getColumnAsUByte(const char* cname, unsigned char&) const =0;
    virtual int getColumnAsShort(const char* cname, int16_t&) const =0;
    virtual int getColumnAsUShort(const char* cname, uint16_t&) const =0;
    virtual int getColumnAsInt(const char* cname, int32_t&) const =0;
    virtual int getColumnAsUInt(const char* cname, uint32_t&) const =0;
    virtual int getColumnAsLong(const char* cname, int64_t&) const =0;
    virtual int getColumnAsULong(const char* cname, uint64_t&) const =0;
    virtual int getColumnAsFloat(const char* cname, float&) const =0;
    virtual int getColumnAsDouble(const char* cname, double&) const =0;
    virtual int getColumnAsString(const char* cname, std::string&) const =0;
    virtual int getColumnAsOpaque(const char* cname, ibis::opaque&) const =0;
    ///@}

    ///@{
    /// This version of getColumnAsTTT directly use the column number, i.e.,
    /// the position of a column in the list returned by function @c
    /// columnNames or @c columnTypes.  This version of the data access
    /// function may be able to avoid the name lookup and reduce the
    /// execution time.
    ///
    /// These functions return the number of elements copied upon
    /// successful completion, otherwise they return a negative number to
    /// indicate failure.
    ///
    virtual int getColumnAsByte(uint32_t cnum, char& val) const =0;
    virtual int getColumnAsUByte(uint32_t cnum, unsigned char& val) const =0;
    virtual int getColumnAsShort(uint32_t cnum, int16_t& val) const =0;
    virtual int getColumnAsUShort(uint32_t cnum, uint16_t& val) const =0;
    virtual int getColumnAsInt(uint32_t cnum, int32_t& val) const =0;
    virtual int getColumnAsUInt(uint32_t cnum, uint32_t& val) const =0;
    virtual int getColumnAsLong(uint32_t cnum, int64_t& val) const =0;
    virtual int getColumnAsULong(uint32_t cnum, uint64_t& val) const =0;
    virtual int getColumnAsFloat(uint32_t cnum, float& val) const =0;
    virtual int getColumnAsDouble(uint32_t cnum, double& val) const =0;
    virtual int getColumnAsString(uint32_t cnum, std::string& val) const =0;
    virtual int getColumnAsOpaque(uint32_t cnum, ibis::opaque& val) const =0;
    ///@}

protected:
    cursor() {};
    cursor(const cursor&); // not implemented
    cursor& operator=(const cursor&) ; // not implemented
}; // ibis::table::cursor

inline void ibis::table::row::clear() {
    bytesnames.clear();
    bytesvalues.clear();
    ubytesnames.clear();
    ubytesvalues.clear();
    shortsnames.clear();
    shortsvalues.clear();
    ushortsnames.clear();
    ushortsvalues.clear();
    intsnames.clear();
    intsvalues.clear();
    uintsnames.clear();
    uintsvalues.clear();
    longsnames.clear();
    longsvalues.clear();
    ulongsnames.clear();
    ulongsvalues.clear();
    floatsnames.clear();
    floatsvalues.clear();
    doublesnames.clear();
    doublesvalues.clear();
    catsnames.clear();
    catsvalues.clear();
    textsnames.clear();
    textsvalues.clear();
    blobsnames.clear();
    blobsvalues.clear();
} // ibis::table::row::clear

inline void ibis::table::row::clearValues() {
    bytesvalues.clear();
    ubytesvalues.clear();
    shortsvalues.clear();
    ushortsvalues.clear();
    intsvalues.clear();
    uintsvalues.clear();
    longsvalues.clear();
    ulongsvalues.clear();
    floatsvalues.clear();
    doublesvalues.clear();
    catsvalues.clear();
    textsvalues.clear();
    blobsvalues.clear();
} // ibis::table::row::clearValues
#endif // IBIS_TABLE_H
