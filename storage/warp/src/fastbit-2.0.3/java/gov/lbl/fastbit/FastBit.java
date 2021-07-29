// $Id$
// Authors: John Wu and Aaron Hong, Lawrence Berkeley National Laboratory
// Copyright 2006-2015 the Regents of the University of California

/// @defgroup FastBitJava FastBit Java API.
/// @{
/**
   A package to contain functions related to FastBit.
 */
package gov.lbl.fastbit;

/**
   A facade for accessing some FastBit functions from java.

   This class exports some functions from @c ibis::part and @c
   ibis::query classes to java.  A very small example is available in
   java/milky.java.

   @note Only a limited number of functions are provided at this time.
   To inquire about possible extensions, please contact John Wu at
   <John.Wu at ACM.org>.

   @note An alternative Java Native Interface is available at
   https://bitbucket.org/olafW/fastbit4java/

   @author John Wu and Aaron Hong
*/
public class FastBit {

    /** Build indexes for all columns in the specified directory.
	@arg dir The name of the directory containing the data partition.
	@arg opt The indexing option.  A blank string can be used to
	invoke the existing indexing option specified in the existing
	metadata.  See http://sdm.lbl.gov/fastbit/doc/indexSpec.html for
	more description.

	@note The plaural form of the word index is usually written as
	indexes in the database community.  We follow this convention in
	naming FastBit functions.
    */
    public native int build_indexes(String dir, String opt);
    /** Remove all existing index files.
	@arg dir The name of the directory containing the data partition.
    */
    public native int purge_indexes(String dir);
    /** Build an index for the named column.
	@arg dir The name of the directory containing the data partition.
	@arg col The name of the column to be indexed.
	@arg opt The indexing option.  A blank string can be used to
	invoke the existing indexing option specified in the existing
	metadata.  See http://sdm.lbl.gov/fastbit/doc/indexSpec.html for
	more description.
    */
    public native int build_index(String dir, String col, String opt);
    /** Remove the index files associated with the specified column */
    public native int purge_index(String dir, String col);

    /** An auxiliary class to hold handles to query objects. */
    public class QueryHandle {}
    /** Build a new query object.  The three arguments mirros the three
	clauses of a simple SQL select statement.
	@arg select The select clause.  May contain a list of column
	names separated by comas.
	@arg datadir The name of data directory.  Must be the directory
	containing the data partition.  Can only be one directory name.
	@arg where The where clause.  Specify the conditions on the
	records/rows to be selected.  This is basically a set of range
	conditions joined together by logical operators, @sa
	ibis::query::setWhereClause.
    */
    public native QueryHandle build_query(String select, String datadir,
					  String where);
    /** Destroy a query object.  Reclaims all resources associated with
	the query object.
    */
    public native int destroy_query(QueryHandle handle);

    /** Retrieve the ids of rows satisfying the query conditions. */
    public native int[] get_result_row_ids(QueryHandle handle);
    /** Return the number of records/rows satisfying the query conditions. */
    public native int get_result_size(QueryHandle handle);
    /** Retrieve the values of the named column from the rows satisfying
	the query conditions.  If the column has appeared in the select
	clause in @c build_query, then the values are guarateed to not
	contain any null values.  If there is no null values to
	consider, then there is no need to specify anything in the
	select clause when building the query.

	@note The original data type of the column must be able to fit
	into bytes in order for this function to succeed, except
	get_qualified_longs may be invoked on a string-valued column to
	retrieve the starting positions of the strings satisfying the
	query conditions.
    */
    public native byte[] get_qualified_bytes(QueryHandle handle, String col);
    /** Retrieve values as shorts. @sa get_qualified_bytes. */
    public native short[] get_qualified_shorts(QueryHandle handle, String col);
    /** Retrieve values as ints. @sa get_qualified_bytes. */
    public native int[] get_qualified_ints(QueryHandle handle, String col);
    /** Retrieve values as longs.
	@sa get_qualified_bytes.
	@note The values of any integer-valued column may be retrieved
	with this function.  In addition, one may invoke this function
	with the name of a string-valued column to retrieve the starting
	positions (byte offsets) to read the strings from the data file
	containing the strings.  Note that the string values in the data
	files are null-terminated.
    */
    public native long[] get_qualified_longs(QueryHandle handle, String col);
    /** Retrieve values as floats. @sa get_qualified_bytes. */
    public native float[] get_qualified_floats(QueryHandle handle, String col);
    /** Retrieve values as doubles. @sa get_qualified_bytes. */
    public native double[] get_qualified_doubles(QueryHandle handle, String col);

    /** Write the buffer to the named directory.  It also flushes the
	current buffer.  All functions calls to various add functions
	are assumed to be adding to a single FastBit data partition.
	This set of functions are intended for adding rows to a data
	partition.  Considerable overhead is involved in creating the
	files when the function write_buffer is invoked, therefore it is
	not recommended for writing a single row.  Additionally, since
	all data values are stored in a memory buffer, one can not write
	a large number of records in one shot.
    */
    public native int write_buffer(String dir);
    /** Add the name column containing double values.  Note that if the
	target directory contains values already, the type specified
	here must match exactly.
    */
    public native int add_doubles(String colname, double[] arr);
    /** Add values to a values to a float-valued column. */
    public native int add_floats(String colname, float[] arr);
    /** Add values to a values to a long-valued column. */
    public native int add_longs(String colname, long[] arr);
    /** Add values to a values to a int-valued column. */
    public native int add_ints(String colname, int[] arr);
    /** Add values to a values to a short-valued column. */
    public native int add_shorts(String colname, short[] arr);
    /** Add values to a values to a byte-valued column. */
    public native int add_bytes(String colname, byte[] arr);
    /** Compute the number of rows in the specified directory. */
    public native int number_of_rows(String dir);
    /** Compute the number of columns in the specified directory. */
    public native int number_of_columns(String dir);

    /** Return the current message level used by FastBit.
     */
    public native int get_message_level();
    /** Change the message level to the specified value.  The default
	value is 0.  As the message level increases, more message will
	be printed.  Returns the current message level before the change
	is to take place.
    */
    public native int set_message_level(int level);
    /** Change the name of the log file.
     */
    public native int set_logfile(String filename);
    /** Retrieve the name of the log file.
     */
    public native String get_logfile();

    /** Function to initialize required data structure on C++ side.
	@note Must be called in order for initialize required data
	structures.
	@note May be called multiple times to incorporate information
	from multiple RC files.  It can also be called after calling
	function cleanup to start a clean instance of FastBit service.
    */
    protected native void init(String rcfile);
    /** Cleanup resources hold by FastBit services.
	@note Must be called to reclaim resources taken up on the C++
	side of FastBit functions.
	@note It is fine to call this function multiple times.
    */
    protected native void cleanup();

    /** Constructor.  The argument to the constructor can be an empty string.
	@sa ibis::init. @sa init.
    */
    public FastBit(String rcfile) {
	init(rcfile);
    }
    /** Perform the final clean up job.
     */
    protected void finalize() {cleanup();}

    static { // load the native FastBit library.	 
	System.loadLibrary("fastbitjni");	 
    }
} // class FastBit
/// @}
