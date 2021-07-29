// File: $Id$
// Author: John Wu <John.Wu at ACM.org>
// Copyright (c) 2000-2016 the Regents of the University of California
#ifndef IBIS_PART_H
#define IBIS_PART_H
///@file
/// Define the class ibis::part.  This class defines some rudimentary
/// functions for managing a vertically partitioned data partition and
/// answering simple queries.  It also provides limited number of functions
/// to modify the data partition.
///
#include "column.h"
#include "resource.h"
#include "utilidor.h"

#include <string>
#include <vector>

/// The class ibis::part represents a partition of a relational table.  The
/// current implementation is designed to work with vertically partitioned
/// data files.  This class contains common information and operations on a
/// data partition.  It must have a name.  Following SQL convention, the
/// name must start with a underscore or an ASCII alphabet and may be
/// followed by any number of ASCII alphanumeric characters or underscore.
///
/// @ingroup FastBitMain
class FASTBIT_CXX_DLLSPEC ibis::part {
public:
    enum TABLE_STATE {
	UNKNOWN_STATE=0, STABLE_STATE, RECEIVING_STATE,
	PRETRANSITION_STATE, TRANSITION_STATE, POSTTRANSITION_STATE
    };
    struct info; // To hold some basic information about a table.
    class readLock;

    /******************************************************************/
    // public functions
    /******************************************************************/

    /// Destuctor.
    virtual ~part();
    /// Initialize a data partition object.
    explicit part(const char* name=0, bool ro=false);
    /// Initialize a table from the named directories.
    part(const char* adir, const char* bdir, bool ro=false);
    /// Initialize a partition with given meta tags.
    part(const std::vector<const char*> &mtags, bool ro=false);
    /// Initialize a partition with given meta tags.
    part(const ibis::resource::vList &mtags, bool ro=false);

    inline info* getInfo() const;
    TABLE_STATE getState() const;
    /// Return the current state of data partition.
    TABLE_STATE getStateNoLocking() const {return state;}

    virtual int buildIndexes(const char* iopt, int nthr);
    virtual int buildIndexes(const ibis::table::stringArray&, int nthr=1);
    void buildSorted(const char* colname) const;
    void loadIndexes(const char* iopt=0, int ropt=0) const;
    void unloadIndexes() const;
    void purgeIndexFiles() const;

    /// Return the name of the partition.
    const char* name()		const {return (m_name?m_name:"?");}
    /// Return a text description of the partition.
    const char* description()	const {return m_desc.c_str();}
    /// Return the current index specification.
    const char* indexSpec()	const {return idxstr;}
    /// Replace existing index specification with a new one.
    void indexSpec(const char*);
    /// Return the time stamp on the partition.
    time_t timestamp()		const {return switchTime;}
    /// Return the list of meta tags as a single string.  The meta tags
    /// appears as 'name=value' pairs separated by comma (,).
    std::string metaTags()	const;
    ibis::table::stringArray columnNames() const;
    ibis::table::typeArray   columnTypes() const;
    column* getColumn(const char* name) const;
    column* getColumn(uint32_t ind) const;

    /// Return the name of the active data directory.
    const char* currentDataDir() const {return activeDir;}
    /// Return the number of attributes in the partition.
    uint32_t nColumns() const {return columns.size();}
    /// Return the number of rows.
    uint32_t nRows() const {return nEvents;}

    /// Output a description of every column in the data partition.
    void print(std::ostream &out) const;
    void logWarning(const char* event, const char* fmt, ...) const;
    void logMessage(const char* event, const char* fmt, ...) const;

    /// Match a name-value pair in the meta tags.
    bool matchNameValuePair(const char* name, const char* value) const;
    /// Match multiple name-value pairs against the internally stored meta
    /// tags.
    bool matchMetaTags(const std::vector<const char*> &mtags) const;
    /// Match multiple name-value pairs.
    bool matchMetaTags(const ibis::resource::vList &mtags) const;
    /// Return the value of the meta tag with the specified name.
    inline const char* getMetaTag(const char*) const;

    /// Perform predefined set of tests and return the number of failures.
    virtual long selfTest(int nth=1, const char* pref=0) const;
    void computeMinMax();

    /******************************************************************/
    // The following functions need to hold a read lock on the partition,
    // however in order for them to produce consistent results, they must
    // share the same read lock.  These functions do not contain the lock
    // themselves, but rely on the caller to maintain a consistent lock.
    /******************************************************************/
    /// Return the row number of the row with specified RID.
    uint32_t getRowNumber(const rid_t &rid) const;
    long evaluateRIDSet(const ibis::RIDSet&, ibis::bitvector&) const; 
    array_t<rid_t>* getRIDs() const {return rids;} // all RIDs
    array_t<rid_t>* getRIDs(const ibis::bitvector &mask) const;// some RIDs
    /// Does this partition have an explicit RID column?  Returns true for
    /// yes, false for no.
    bool explicitRIDs() const {
        return (rids!=0) ? (rids->size()==nEvents) : false;}

    virtual double estimateCost(const ibis::qContinuousRange &cmp) const;
    virtual double estimateCost(const ibis::qDiscreteRange &cmp) const;
    virtual double estimateCost(const ibis::qIntHod &cmp) const;
    virtual double estimateCost(const ibis::qUIntHod &cmp) const;
    virtual double estimateCost(const ibis::qString &cmp) const;
    virtual double estimateCost(const ibis::qAnyString &cmp) const;

    /// Return an upper bound on the number of hits.
    virtual long estimateRange(const ibis::qContinuousRange &cmp) const;

    /// Return an upper bound on the number of hits.
    virtual long estimateRange(const ibis::qDiscreteRange &cmp) const;
    /// Return an upper bound on the number of hits.
    virtual long estimateRange(const ibis::qIntHod &cmp) const;
    /// Return an upper bound on the number of hits.
    virtual long estimateRange(const ibis::qUIntHod &cmp) const;

    /// Evaluate a continue range expression accurately.
    virtual long evaluateRange(const ibis::qContinuousRange &cmp,
			       const ibis::bitvector &mask,
			       ibis::bitvector &res) const;
    /// Evaluate a discrete range expression accurately.
    virtual long evaluateRange(const ibis::qDiscreteRange &cmp,
			       const ibis::bitvector &mask,
			       ibis::bitvector &res) const;
    /// Evaluate a discrete range expression accurately.
    virtual long evaluateRange(const ibis::qIntHod &cmp,
			       const ibis::bitvector &mask,
			       ibis::bitvector &res) const;
    /// Evaluate a discrete range expression accurately.
    virtual long evaluateRange(const ibis::qUIntHod &cmp,
			       const ibis::bitvector &mask,
			       ibis::bitvector &res) const;

    /// Estimate a continuous range condition.
    virtual long estimateRange(const ibis::qContinuousRange &cmp,
			       ibis::bitvector &low,
			       ibis::bitvector &high) const;

    /// Discover the records that can not be decided using the index.
    virtual float getUndecidable(const ibis::qContinuousRange &cmp,
				 ibis::bitvector &iffy) const;

    /// Estimate the discrete range condition.
    virtual long estimateRange(const ibis::qDiscreteRange &cmp,
			       ibis::bitvector &low,
			       ibis::bitvector &high) const;
    /// Discover the records that can not be decided using the index.
    virtual float getUndecidable(const ibis::qDiscreteRange &cmp,
				 ibis::bitvector &iffy) const;

    /// Estimate the discrete range condition.
    virtual long estimateRange(const ibis::qIntHod &cmp,
			       ibis::bitvector &low,
			       ibis::bitvector &high) const;
    /// Discover the records that can not be decided using the index.
    virtual float getUndecidable(const ibis::qIntHod &cmp,
				 ibis::bitvector &iffy) const;

    /// Estimate the discrete range condition.
    virtual long estimateRange(const ibis::qUIntHod &cmp,
			       ibis::bitvector &low,
			       ibis::bitvector &high) const;
    /// Discover the records that can not be decided using the index.
    virtual float getUndecidable(const ibis::qUIntHod &cmp,
				 ibis::bitvector &iffy) const;

    virtual long doScan(const ibis::qRange &cmp,
			ibis::bitvector &hits) const;
    virtual long doScan(const ibis::qRange &cmp,
			const ibis::bitvector &mask,
			ibis::bitvector &hits) const;
    virtual long doScan(const ibis::qRange &cmp,
			const ibis::bitvector &mask,
			void *res) const;
    virtual long doScan(const ibis::qRange &cmp,
			const ibis::bitvector &mask,
			void *res, ibis::bitvector &hits) const;

    virtual long negativeScan(const ibis::qRange &cmp,
			      const ibis::bitvector &mask,
			      ibis::bitvector &hits) const;

    virtual long doScan(const ibis::math::term&, const ibis::bitvector&,
			ibis::bitvector&) const;
    virtual long doScan(const ibis::compRange &cmp,
			ibis::bitvector &hits) const;
    virtual long doScan(const ibis::compRange &cmp,
			const ibis::bitvector &mask,
			ibis::bitvector &hits) const;

    template <typename E> static long
	doScan(const array_t<E> &varr,
	       const ibis::qRange &cmp,
	       const ibis::bitvector &mask,
	       ibis::bitvector &hits);
    template <typename E> static long
	doScan(const array_t<E> &varr,
	       const ibis::qContinuousRange &cmp,
	       const ibis::bitvector &mask,
	       ibis::bitvector &hits);
    template <typename E> static long
	doScan(const array_t<E> &varr,
	       const ibis::qContinuousRange &cmp,
	       const ibis::bitvector &mask,
	       array_t<E> &res);
    template <typename E> static long
	doScan(const array_t<E> &varr,
	       const ibis::qContinuousRange &cmp,
	       const ibis::bitvector &mask,
	       array_t<E> &res, ibis::bitvector &hits);

    long countHits(const ibis::qRange &cmp) const;

    virtual long estimateMatchAny(const ibis::qAnyAny &cmp,
				  ibis::bitvector &low,
				  ibis::bitvector &high) const;
    virtual long matchAny(const ibis::qAnyAny &cmp,
			  ibis::bitvector &hits) const;
    virtual long matchAny(const ibis::qAnyAny &cmp,
			  const ibis::bitvector &mask,
			  ibis::bitvector &hits) const;

    long patternSearch(const ibis::qLike &cmp,
		       ibis::bitvector &low) const;
    long stringSearch(const ibis::qString &cmp,
		      ibis::bitvector &low) const;
    long stringSearch(const ibis::qAnyString &cmp,
		      ibis::bitvector &low) const;
    long keywordSearch(const ibis::qKeyword &cmp,
		       ibis::bitvector &low) const;
    long keywordSearch(const ibis::qAllWords &cmp,
		       ibis::bitvector &low) const;
    long patternSearch(const ibis::qLike &cmp) const;
    long stringSearch(const ibis::qString &cmp) const;
    long stringSearch(const ibis::qAnyString &cmp) const;
    long keywordSearch(const ibis::qKeyword &cmp) const;
    long keywordSearch(const ibis::qAllWords &cmp) const;

    /// Evaluate a self-join.  Return the number of pairs satisfying join
    /// condition.  Only records marked with mask=1 are considered.  The
    /// result pairs are stored in the bitvector @a pairs.  A pair <i, j>
    /// would be marked at position i*nRows() + j in @a pairs.
    int64_t evaluateJoin(const ibis::deprecatedJoin &cmp,
			 const ibis::bitvector &mask,
			 ibis::bitvector64 &pairs) const;
    /// Return the number of pairs satisfying the join condition.  In
    /// addition, write the pairs into the file named @c pairfile.
    int64_t evaluateJoin(const ibis::deprecatedJoin &cmp,
			 const ibis::bitvector &mask,
			 const char* pairfile) const;
    /// Return only the number of pairs satisfying the join condition.
    int64_t evaluateJoin(const ibis::deprecatedJoin &cmp,
			 const ibis::bitvector &mask) const;
    /// Evaluate a join defined with multiple (conjunctive) range join
    /// conditions.
    int64_t evaluateJoin(const std::vector<const ibis::deprecatedJoin*> &cmp,
			 const ibis::bitvector &mask,
			 ibis::bitvector64 &pairs) const;
    int64_t evaluateJoin(const std::vector<const ibis::deprecatedJoin*> &cmp,
			 const ibis::bitvector &mask) const;
    /// Evaluate all pairs in @c trial to determine whether they really
    /// satisfy the range join defined in @c cmp.  The result is stored in
    /// the argument @c result.  This function returns the number of hits
    /// found.
    int64_t evaluateJoin(const ibis::deprecatedJoin &cmp,
			 const ibis::bitvector64 &trial,
			 ibis::bitvector64 &result) const;
    int64_t evaluateJoin(const std::vector<const ibis::deprecatedJoin*> &cmp,
			 const ibis::bitvector64 &trial,
			 ibis::bitvector64 &result) const;
    /******************************************************************/

    /// Retrieve values of the named column as 8-bit integers.
    array_t<signed char>*
	selectBytes(const char* name, const ibis::bitvector &mask) const;
    /// Retrieve values of the named column as 8-bit unsigned integers.
    array_t<unsigned char>*
	selectUBytes(const char* name, const ibis::bitvector &mask) const;
    /// Retrieve values of the named column as 16-bit integers.
    array_t<int16_t>*
	selectShorts(const char* name, const ibis::bitvector &mask) const;
    /// Retrieve values of the named column as 16-bit unsigned integers.
    array_t<uint16_t>*
	selectUShorts(const char* name, const ibis::bitvector &mask) const;
    /// Retrieve values of the named column as 32-bit integers.
    array_t<int32_t>*
	selectInts(const char* name, const ibis::bitvector &mask) const;
    /// Retrieve values of the named column as 32-bit unsigned integers.
    array_t<uint32_t>*
	selectUInts(const char* name, const ibis::bitvector &mask) const;
    /// Retrieve values of the named column as 64-bit integers.
    array_t<int64_t>*
	selectLongs(const char* name, const ibis::bitvector &mask) const;
    /// Retrieve values of the named column as 64-bit unsigned integers.
    array_t<uint64_t>*
	selectULongs(const char* name, const ibis::bitvector &mask) const;
    /// Retrieve values of the named column as 32-bit floating-point values.
    array_t<float>*
	selectFloats(const char* name, const ibis::bitvector &mask) const;
    /// Retrieve values of the named column as 64-bit floating-point values.
    array_t<double>*
	selectDoubles(const char* name, const ibis::bitvector &mask) const;
    /// Retrieve values of the named column as strings.
    std::vector<std::string>*
	selectStrings(const char* name, const ibis::bitvector &mask) const;

    long selectValues(const char* cname, const ibis::bitvector &mask,
		      void* vals) const;
    long selectValues(const ibis::qContinuousRange& cond, void* vals) const;
    long calculate(const ibis::math::term&, const ibis::bitvector&,
		   array_t<double>&) const;
    long calculate(const ibis::math::stringFunction1&, const ibis::bitvector&,
		   std::vector<std::string>&) const;

    /******************************************************************/
    // A group of functions added August, 2005 to provide summary
    // information about the data.  Logically, they are an extension of
    // getInfo.

    /// The actual minimum value in the named column.
    double getActualMin(const char *name) const;
    /// The actual maximum value in the named column.
    double getActualMax(const char *name) const;
    /// Sum of all value in the named column.
    double getColumnSum(const char *name) const;

    /// Histogram functions.
    /// @{
    /// Compute conditional 1D histogram with regularly spaced bins.
    long get1DDistribution(const char *constraints, const char *cname,
			   double begin, double end, double stride,
			   std::vector<uint32_t> &counts) const;
    /// Compute conditional 2D histogram with regularly spaced bins.
    long get2DDistribution(const char *constraints, const char *cname1,
			   double begin1, double end1, double stride1,
			   const char *cname2,
			   double begin2, double end2, double stride2,
			   std::vector<uint32_t> &counts) const;
    /// Compute conditional 3D histogram with regularly spaced bins.
    long get3DDistribution(const char *constraints, const char *cname1,
			   double begin1, double end1, double stride1,
			   const char *cname2,
			   double begin2, double end2, double stride2,
			   const char *cname3,
			   double begin3, double end3, double stride3,
			   std::vector<uint32_t> &counts) const;
    /// Compute weighted conditional 1D histogram with regularly spaced bins.
    long get1DDistribution(const char *constraints, const char *cname,
			   double begin, double end, double stride,
			   const char *wtname,
			   std::vector<double> &weights) const;
    /// Compute weighted conditional 2D histogram with regularly spaced bins.
    long get2DDistribution(const char *constraints, const char *cname1,
			   double begin1, double end1, double stride1,
			   const char *cname2,
			   double begin2, double end2, double stride2,
			   const char *wtname,
			   std::vector<double> &weights) const;
    /// Compute weighted conditional 3D histogram with regularly spaced bins.
    long get3DDistribution(const char *constraints, const char *cname1,
			   double begin1, double end1, double stride1,
			   const char *cname2,
			   double begin2, double end2, double stride2,
			   const char *cname3,
			   double begin3, double end3, double stride3,
			   const char *wtname,
			   std::vector<double> &weights) const;
    /// Compute 1D histogram with adaptive bins.
    long get1DDistribution(const char *cname, uint32_t nbin,
			   std::vector<double> &bounds,
			   std::vector<uint32_t> &counts) const;
    /// Compute conditional 1D histogram with adaptive bins.
    long get1DDistribution(const char *constraints,
			   const char *cname, uint32_t nbin,
			   std::vector<double> &bounds,
			   std::vector<uint32_t> &counts) const;
    /// Compute 2D histogram with adaptive bins.
    long get2DDistribution(const char *cname1, const char *cname2,
			   uint32_t nb1, uint32_t nb2,
			   std::vector<double> &bounds1,
			   std::vector<double> &bounds2,
			   std::vector<uint32_t> &counts,
			   const char* const option=0) const;
    /// Compute conditional 2D histogram with adaptive bins.
    long get2DDistribution(const char *constraints,
			   const char *name1, const char *name2,
			   uint32_t nb1, uint32_t nb2,
			   std::vector<double> &bounds1,
			   std::vector<double> &bounds2,
			   std::vector<uint32_t> &counts) const;
    /// Compute 3D histogram with adaptive bins.
    long get3DDistribution(const char *cname1, const char *cname2,
			   const char *cname3,
			   uint32_t nb1, uint32_t nb2, uint32_t nb3,
			   std::vector<double> &bounds1,
			   std::vector<double> &bounds2,
			   std::vector<double> &bounds3,
			   std::vector<uint32_t> &counts,
			   const char* const option=0) const;
    /// Compute conditional 3D histogram with adaptive bins.
    long get3DDistribution(const char *constraints,
			   const char *cname1, const char *cname2,
			   const char *cname3,
			   uint32_t nb1, uint32_t nb2, uint32_t nb3,
			   std::vector<double> &bounds1,
			   std::vector<double> &bounds2,
			   std::vector<double> &bounds3,
			   std::vector<uint32_t> &counts) const;
    /// Partition values of the named variable into regularly spaced bins.
    long get1DBins(const char *constraints, const char *cname,
		   double begin, double end, double stride,
		   std::vector<ibis::bitvector> &bins) const;
    /// Partition values of the named variable into regularly spaced bins.
    long get1DBins(const char *constraints, const char *cname,
		   double begin, double end, double stride,
		   std::vector<ibis::bitvector*> &bins) const;
    /// Partition values of the named variable into regularly spaced bins.
    long get1DBins(const char *constraints, const char *cname,
		   double begin, double end, double stride,
		   const char *wtname,
		   std::vector<double> &weights,
		   std::vector<ibis::bitvector*> &bins) const;
    /// Partition values of named variables into regularly spaced 2D bins.
    long get2DBins(const char *constraints, const char *cname1,
		   double begin1, double end1, double stride1,
		   const char *cname2,
		   double begin2, double end2, double stride2,
		   std::vector<ibis::bitvector> &bins) const;
    /// Partition values of named variables into regularly spaced 2D bins.
    long get2DBins(const char *constraints, const char *cname1,
		   double begin1, double end1, double stride1,
		   const char *cname2,
		   double begin2, double end2, double stride2,
		   std::vector<ibis::bitvector*> &bins) const;
    /// Partition values of named variables into regularly spaced 2D bins.
    long get2DBins(const char *constraints, const char *cname1,
		   double begin1, double end1, double stride1,
		   const char *cname2,
		   double begin2, double end2, double stride2,
		   const char *wtname,
		   std::vector<double> &weights,
		   std::vector<ibis::bitvector*> &bins) const;
    /// Partition values of named variables into regularly spaced 3D bins.
    long get3DBins(const char *constraints, const char *cname1,
		   double begin1, double end1, double stride1,
		   const char *cname2,
		   double begin2, double end2, double stride2,
		   const char *cname3,
		   double begin3, double end3, double stride3,
		   std::vector<ibis::bitvector> &bins) const;
    /// Partition values of named variables into regularly spaced 3D bins.
    long get3DBins(const char *constraints, const char *cname1,
		   double begin1, double end1, double stride1,
		   const char *cname2,
		   double begin2, double end2, double stride2,
		   const char *cname3,
		   double begin3, double end3, double stride3,
		   std::vector<ibis::bitvector*> &bins) const;
    /// Partition values of named variables into regularly spaced 3D bins.
    long get3DBins(const char *constraints, const char *cname1,
		   double begin1, double end1, double stride1,
		   const char *cname2,
		   double begin2, double end2, double stride2,
		   const char *cname3,
		   double begin3, double end3, double stride3,
		   const char *wtname,
		   std::vector<double> &weights,
		   std::vector<ibis::bitvector*> &bins) const;
    /// Partition records satisfying specified conditions into bins with
    /// about the same number of records.
    long get1DBins(const char *constraints, const char *cname1, uint32_t nb1,
		   std::vector<double> &bounds1,
		   std::vector<ibis::bitvector> &bins) const;
    /// Partition records satisfying specified conditions into 2D bins.
    long get2DBins(const char *constraints,
		   const char *cname1, const char *cname2,
		   uint32_t nb1, uint32_t nb2,
		   std::vector<double> &bounds1,
		   std::vector<double> &bounds2,
		   std::vector<ibis::bitvector> &bins) const;
    /// Partition records satisfying specified conditions into 3D bins.
    long get3DBins(const char *constraints,
		   const char *cname1, const char *cname2, const char *cname3,
		   uint32_t nb1, uint32_t nb2, uint32_t nb3,
		   std::vector<double> &bounds1,
		   std::vector<double> &bounds2,
		   std::vector<double> &bounds3,
		   std::vector<ibis::bitvector> &bins) const;
    /// @}

    /// Obsolete histogram functions.  Avoid using these functions.  They
    /// might be removed without notice.
    /// @{
    /// Compute the binned distribution of the named variable.
    long getDistribution(const char *name,
			 std::vector<double> &bounds,
			 std::vector<uint32_t> &counts) const;
    /// Compute the conditional binned data distribution.
    long getDistribution(const char *constraints,
			 const char *name,
			 std::vector<double> &bounds,
			 std::vector<uint32_t> &counts) const;
    /// Compute the binned distribution with the specified maximum number
    /// of bins.
    long getDistribution(const char *name, uint32_t nbc,
			 double *bounds, uint32_t *counts) const;
    /// Compute the conditional binned data distribution with the specified
    /// maximum number of bins.
    long getDistribution(const char *name, const char *constraints,
			 uint32_t nbc, double *bounds,
			 uint32_t *counts) const;

    /// Compute the joint distribution of two variables.
    long getJointDistribution(const char *constraints,
			      const char *name1, const char *name2,
			      std::vector<double> &bounds1,
			      std::vector<double> &bounds2,
			      std::vector<uint32_t> &counts) const;

    /// Compute a cumulative distribution (a cumulative histogram).
    long getCumulativeDistribution(const char *name,
				   std::vector<double> &bounds,
				   std::vector<uint32_t> &counts) const;
    /// Compute the cumulative distribution of the variable named @c name
    /// under the specified constraints.
    long getCumulativeDistribution(const char *constraints,
				   const char *name,
				   std::vector<double> &bounds,
				   std::vector<uint32_t> &counts) const;
    /// This version of @c getCumulativeDistribution uses two user supplied
    /// arrays @c bounds and @c counts.
    long getCumulativeDistribution(const char *name, uint32_t nbc,
				   double *bounds, uint32_t *counts) const;
    /// Compute the conditional distribution and return the distribution in
    /// the arrays provided.
    long getCumulativeDistribution(const char *constraints, const char *name,
				   uint32_t nbc, double *bounds,
				   uint32_t *counts) const;
    /// @}

    /******************************************************************/
    /// In many scientific applications, data are defined on meshes.  The
    /// following functions assumes the meshes are regular.  Under this
    /// assumption, each column can be viewed as a multi-dimensional array,
    /// such as A[nz][ny][nx].  Following the convention in C/C++.  The
    /// dimensions of the array are ordered from left to the right, with
    /// the left most being the slowest varying dimension and the right
    /// most being the fast varying dimension.  This assumption about the
    /// dimensions is explicitly used in ibis::meshQuery functions
    /// toRanges, range2d, range3d and rangend.  The function getMeshShape
    /// returns the sizes of the dimensions in a std::vector.
    const std::vector<uint32_t>& getMeshShape() const {return shapeSize;}
    /// Return the name of the dimensions corresponding to the vector
    /// returned from getMeshShape.
    const std::vector<std::string>& getMeshDimensions() const {
	return shapeName;}
    void setMeshShape(const ibis::array_t<uint64_t>&);
    void setMeshShape(const char *shape);
    void updateMetaData() const;
    /// Update the list of columns with information in this data partition
    void combineNames(ibis::table::namesTypes &metalist) const;

    /// A group of functions to manipulate the data partition.
    ///@{
    int clear();
    int updateData();
    /// Append data from @c dir.
    long append(const char* dir);
    /// Commit the append operation involving data from @c dir.
    /// @note The content of @c dir must match the content of directory
    /// passed to @c append.  Clearly, the easiest way to achieve this is
    /// to use the same directory.
    long commit(const char* dir);
    /// Rollback the append operation.  This function can only be called
    /// before calling @c commit.
    long rollback();
    long addColumn(const char* aexpr, const char* cname,
		   ibis::TYPE_T ctype=ibis::DOUBLE);
    long addColumn(const ibis::math::term* xpr, ibis::bitvector& mask,
		   const char* cname, ibis::TYPE_T ctype=ibis::DOUBLE);

    /******************************************************************/
    virtual long reorder();
    virtual long reorder(const ibis::table::stringArray &names);
    virtual long reorder(const ibis::table::stringArray &names,
			 const std::vector<bool> &directions);

    long deactivate(const std::vector<uint32_t> &rows);
    long deactivate(const char* conds);
    long reactivate(const std::vector<uint32_t> &rows);
    long reactivate(const char* conds);
    long purgeInactive();
    void emptyCache() const;
    /// Copy the mask of active rows.
    void getNullMask(ibis::bitvector &m) const {m.copy(amask);}
    /// Return a reference to the mask of active rows.
    const ibis::bitvector& getMaskRef() const {return amask;}

    static const char* skipPrefix(const char*);
    /// A class function to read the meta tags in the tdc file.
    static char* readMetaTags(const char* const dir);
    /// Generate name for a partition based on the meta tags.
    static void  genName(const std::vector<const char*> &mtags,
			 std::string &name);
    /// Generate name for a partition based on the meta tags.
    static void  genName(const ibis::resource::vList &mtags,
			 std::string &name);
    void rename(const ibis::partAssoc& known);
    void rename(const char*);

    /// Estimate the number of pages to be accessed.
    static uint32_t countPages(const ibis::bitvector &mask,
			       unsigned elemsize=4);
    /// Evaluate the strategy for accessing a data file.
    ibis::fileManager::ACCESS_PREFERENCE
    accessHint(const ibis::bitvector &mask, unsigned elemsize=4) const;

    /// A struct to pack the arguments to function startTests.
    struct thrArg {
	const part* et;
	const char* pref;
	long* nerrors;	///!< Number of errors encountered.
	ibis::util::counter cnt;
	std::vector<std::string> conds; ///!< List of query conditions.
	std::vector<unsigned> super;///!< The condition encompassing this one.
	std::vector<unsigned> hits; ///!< The number of hits.
    };
    /// A struct to pack arguments to the function ibis_part_build_index.
    struct indexBuilderPool {
	ibis::util::counter cnt;
	ibis::table::stringArray opt;
	const part &tbl;
	indexBuilderPool(const part &t, const char* spec)
	    : cnt(), tbl(t) {opt.push_back(spec);}
	indexBuilderPool(const part&, const ibis::table::stringArray&);
    };

    /// Generate and run random queries for slefTest.
    void queryTest(const char* pref, long* nerrors) const;
    /// Generate and run random queries for slefTest.
    void quickTest(const char* pref, long* nerrors) const;
    /// Try a set of range conditions with different combinations of
    /// operators.
    void testRangeOperators(const ibis::column* col,
			    long* nerrors) const;

    void doBackup(); ///!< A function to start backing up the active dir.

    /// An associative array for columns of data.
    typedef std::map< const char*, column*, lessi > columnList;

    template <typename T> static int
	writeColumn(int fdes, ibis::bitvector::word_t nold,
		    ibis::bitvector::word_t nnew,
		    ibis::bitvector::word_t voffset,
		    const array_t<T>& vals, const T& fill,
		    ibis::bitvector& totmask,
		    const ibis::bitvector& newmask);
    static int writeStrings(const char *fnm, ibis::bitvector::word_t nold,
                            ibis::bitvector::word_t nnew,
                            ibis::bitvector::word_t voffset,
                            const std::vector<std::string>& vals,
                            ibis::bitvector& totmask,
                            const ibis::bitvector& newmask);
    static int writeRaw(int bdes, int sdes, ibis::bitvector::word_t nold,
			ibis::bitvector::word_t nnew,
                        ibis::bitvector::word_t voffset,
			const ibis::array_t<unsigned char>& bytes,
			const ibis::array_t<int64_t>& starts,
			ibis::bitvector& totmask,
			const ibis::bitvector& newmask);
    static int writeOpaques(int bdes, int sdes,
			    ibis::bitvector::word_t nold,
			    ibis::bitvector::word_t nnew,
                            ibis::bitvector::word_t voffset,
			    const std::vector<ibis::opaque>& opq,
			    ibis::bitvector& totmask,
			    const ibis::bitvector& newmask);
    int gainReadAccess() const;
    int releaseAccess() const;
    int gainWriteAccess() const;
    int tryWriteAccess() const;
    ///@}
    class barrel;
    class vault;

protected:
    class cleaner;
    class mutexLock;
    class writeLock;
    class softWriteLock;

    friend struct info;
    friend class cleaner;
    friend class readLock;
    friend class writeLock;
    friend class softWriteLock;
    friend class mutexLock;

    /******************************************************************/
    // protected member variables
    //
    char* m_name;		///!< Name of the data partition.
    std::string m_desc;		///!< Free form description of the partition.
    ibis::resource::vList metaList;	///!< Meta tags as name-value pairs.
    mutable array_t<rid_t>* rids;	///!< The object IDs (row id).
    columnList columns;		///!< List of the columns.
    uint32_t nEvents;		///!< Number of events (rows) in the partition.
    char* activeDir;		///!< The active data directory.
    char* backupDir;		///!< The backup data directory.
    time_t switchTime;		///!< Time of last switch operation.
    TABLE_STATE state;
    char* idxstr;		///!< Index specification.

    ibis::bitvector amask;	///!< Active rows are maked 1.
    std::vector<const column*> colorder;///!< An ordering of columns.
    std::vector<std::string> shapeName;	///!< Names of the dimensions.
    std::vector<uint32_t> shapeSize;	///!< Sizes of the dimensions.

    ibis::part::cleaner* myCleaner;	///!< The cleaner for the file manager.
    bool readonly;			///!< Don't change the data.


    /******************************************************************/
    // protected member functions
    //
    /// Read metadata file -part.txt.
    int  readMetaData(uint32_t &nrows, columnList &plist, const char* dir);
    /// Write metadata file -part.txt.
    void writeMetaData(const uint32_t nrows, const columnList &plist,
		       const char* dir) const;
    void readRIDs() const; ///!< Read RIDs from file 'rids'.
    void freeRIDs() const; ///!< Remove the rids list from memory.

    // functions to deal with meta tags -- those attributes that are
    // associated with a whole partition but are expected to be processed
    // like a categorical attribute in queries
    void extendMetaTags();
    void setMetaTags(const ibis::resource::vList &mts);
    void setMetaTags(const std::vector<const char*> &mts);

    /// Read shape of the mesh from the metadata file.
    void readMeshShape(const char* const dir);
    /// Convert the string describing the shape into internal storage format.
    void digestMeshShape(const char* shape);

    /// Write out a message with indication of severe error.
    void logError(const char* event, const char* fmt, ...) const;

    void makeBackupCopy(); // copy the content of activeDir to backupDir
    long verifyBackupDir(); // minimal consistency check
    void deriveBackupDirName();
    long appendToBackup(const char* dir); // append to the backup directory

    /// Write the named data file with values in the given order.
    template <typename T>
    long writeValues(const char *fname, const array_t<uint32_t> &ind);
    /// Write the named data file in a segmented sorted order.
    template <typename T>
    long reorderValues(const char *fname,
		       array_t<uint32_t> &starts,
		       array_t<uint32_t> &indout,
		       const array_t<uint32_t> &indin,
		       bool ascending);
    long append1(const char* dir);
    long append2(const char* dir);

    /// Rows marked 1 will become inactive.
    long deactivate(const ibis::bitvector &rows);
    /// Change all rows marked 1 to be active.
    long reactivate(const ibis::bitvector &rows);
    /// Turn a list of numbers into a bitvector.
    void numbersToBitvector(const std::vector<uint32_t>&,
			    ibis::bitvector&) const;
    void stringToBitvector(const char*, ibis::bitvector&) const;

    template <typename T> static long
	doCompare(const array_t<T> &array,
		  const ibis::qRange &cmp,
		  const ibis::bitvector &mask,
		  ibis::array_t<T> &res, ibis::bitvector &hits);
    template <typename T> static long
	doCompare(const char *file,
		  const ibis::qRange &cmp,
		  const ibis::bitvector &mask,
		  ibis::array_t<T> &res, ibis::bitvector &hits);

    template <typename T> static long
	doCompare(const array_t<T> &array,
		  const ibis::qRange &cmp,
		  const ibis::bitvector &mask,
		  ibis::array_t<T> &res);
    template <typename T> static long
	doCompare(const char *file,
		  const ibis::qRange &cmp,
		  const ibis::bitvector &mask,
		  ibis::array_t<T> &res);

    template <typename T> static long
	doCompare(const array_t<T> &array,
		  const ibis::qRange &cmp,
		  const ibis::bitvector &mask,
		  ibis::bitvector &hits);
    template <typename T> static long
	doCompare(const char *file,
		  const ibis::qRange &cmp,
		  const ibis::bitvector &mask,
		  ibis::bitvector &hits);

    template <typename T> static long
	negativeCompare(const array_t<T> &array,
			const ibis::qRange &cmp,
			const ibis::bitvector &mask,
			ibis::bitvector &hits);
    template <typename T> static long
	negativeCompare(const char *file,
			const ibis::qRange &cmp,
			const ibis::bitvector &mask,
			ibis::bitvector &hits);

    template <typename T> static long
	doCompare(const array_t<T> &array,
		  const ibis::qIntHod &cmp,
		  const ibis::bitvector &mask,
		  ibis::bitvector &hits);
    template <typename T> static long
	doCompare(const char *file,
		  const ibis::qIntHod &cmp,
		  const ibis::bitvector &mask,
		  ibis::bitvector &hits);
    template <typename T> static long
	negativeCompare(const array_t<T> &array,
			const ibis::qIntHod &cmp,
			const ibis::bitvector &mask,
			ibis::bitvector &hits);
    template <typename T> static long
	negativeCompare(const char *file,
			const ibis::qIntHod &cmp,
			const ibis::bitvector &mask,
			ibis::bitvector &hits);

    template <typename T> static long
	doCompare(const array_t<T> &array,
		  const ibis::qUIntHod &cmp,
		  const ibis::bitvector &mask,
		  ibis::bitvector &hits);
    template <typename T> static long
	doCompare(const char *file,
		  const ibis::qUIntHod &cmp,
		  const ibis::bitvector &mask,
		  ibis::bitvector &hits);
    template <typename T> static long
	negativeCompare(const array_t<T> &array,
			const ibis::qUIntHod &cmp,
			const ibis::bitvector &mask,
			ibis::bitvector &hits);
    template <typename T> static long
	negativeCompare(const char *file,
			const ibis::qUIntHod &cmp,
			const ibis::bitvector &mask,
			ibis::bitvector &hits);

    template <typename T, typename F> static long
	doComp(const array_t<T> &vals, F cmp,
	       const ibis::bitvector &mask,
	       array_t<T> &res, ibis::bitvector &hits);

    template <typename T, typename F> static long
	doComp(const array_t<T> &vals, F cmp,
	       const ibis::bitvector &mask,
	       array_t<T> &res);

    template <typename T, typename F> static long
	doComp(const array_t<T> &vals, F cmp,
	       const ibis::bitvector &mask,
	       ibis::bitvector &hits);

    template <typename T, typename F1, typename F2> static long
	doComp(const array_t<T> &vals, F1 cmp1, F2 cmp2,
	       const ibis::bitvector &mask,
	       array_t<T> &res, ibis::bitvector &hits);

    template <typename T, typename F1, typename F2> static long
	doComp(const array_t<T> &vals, F1 cmp1, F2 cmp2,
	       const ibis::bitvector &mask,
	       array_t<T> &res);

    template <typename T, typename F1, typename F2> static long
	doComp(const array_t<T> &vals, F1 cmp1, F2 cmp2,
	       const ibis::bitvector &mask,
	       ibis::bitvector &hits);

    template <typename T, typename F> static long
	doComp0(const array_t<T> &vals, F cmp,
		const ibis::bitvector &mask,
		ibis::bitvector &hits);

    template <typename T, typename F1, typename F2> static long
	doComp0(const array_t<T> &vals, F1 cmp1, F2 cmp2,
		const ibis::bitvector &mask,
		ibis::bitvector &hits);

    template <typename T>
	long doCount(const ibis::qRange &cmp) const;

    /// Count the number rows satisfying the range expression.  T is an
    /// elementary type.
    template <typename T>
	long doCount(const array_t<T> &vals, const ibis::qIntHod &cmp,
		     const ibis::bitvector &mask) const;

    /// Count the number rows satisfying the range expression.  T is an
    /// elementary type.
    template <typename T>
	long doCount(const array_t<T> &vals, const ibis::qUIntHod &cmp,
		     const ibis::bitvector &mask) const;

    /// Count the number rows satisfying the range expression.  T is an
    /// elementary type.
    template <typename T>
	long doCount(const array_t<T> &vals, const ibis::qRange &cmp,
		     const ibis::bitvector &mask) const;

    /// Count the number rows satisfying the range expression.
    template <typename T, typename F>
	long doCount(const array_t<T> &vals,
		     const ibis::bitvector &mask, F cmp) const;

    /// Count the number rows satisfying the range expression.
    template <typename T, typename F1, typename F2>
	long doCount(const array_t<T> &vals,
		     const ibis::bitvector &mask, F1 cmp1, F2 cmp2) const;

    /// Pack a cumulative distribution stored in two std::vectors into two
    /// arrays provided by the caller.
    long packCumulativeDistribution(const std::vector<double> &bounds,
				    const std::vector<uint32_t> &counts,
				    uint32_t nbc,
				    double *bptr, uint32_t *cptr) const;
    /// Pack a binned distribution.
    long packDistribution(const std::vector<double> &bounds,
			  const std::vector<uint32_t> &counts,
			  uint32_t nbc, double *bptr, uint32_t *cptr) const;

    /// Count the number of values in 2D bins.
    template <typename T1, typename T2>
    long count2DBins(array_t<T1> &vals1,
		     const double &begin1, const double &end1,
		     const double &stride1,
		     array_t<T2> &vals2,
		     const double &begin2, const double &end2,
		     const double &stride2,
		     std::vector<uint32_t> &counts) const;
    /// Count the number of values in 3D bins.
    template <typename T1, typename T2, typename T3>
    long count3DBins(const array_t<T1> &vals1,
		     const double &begin1, const double &end1,
		     const double &stride1,
		     const array_t<T2> &vals2,
		     const double &begin2, const double &end2,
		     const double &stride2,
		     const array_t<T3> &vals3,
		     const double &begin3, const double &end3,
		     const double &stride3,
		     std::vector<uint32_t> &counts) const;
    /// Count the weights in 2D bins.
    template <typename T1, typename T2>
    long count2DWeights(array_t<T1> &vals1,
			const double &begin1, const double &end1,
			const double &stride1,
			array_t<T2> &vals2,
			const double &begin2, const double &end2,
			const double &stride2,
			array_t<double> &wts,
			std::vector<double> &weights) const;
    /// Count the weights in 3D bins.
    template <typename T1, typename T2, typename T3>
    long count3DWeights(const array_t<T1> &vals1,
			const double &begin1, const double &end1,
			const double &stride1,
			const array_t<T2> &vals2,
			const double &begin2, const double &end2,
			const double &stride2,
			const array_t<T3> &vals3,
			const double &begin3, const double &end3,
			const double &stride3,
			const array_t<double> &wts,
			std::vector<double> &weights) const;

    /// Fill the bitvectors representing the 1D bins.
    template <typename T1>
    long fill1DBins(const ibis::bitvector &mask, const array_t<T1> &vals1,
		    const double &begin1, const double &end1,
		    const double &stride1,
		    std::vector<ibis::bitvector> &bins) const;
    /// Fill the bitvectors representing the 1D bins.
    template <typename T1>
    long fill1DBins(const ibis::bitvector &mask, const array_t<T1> &vals1,
		    const double &begin1, const double &end1,
		    const double &stride1,
		    std::vector<ibis::bitvector*> &bins) const;
    /// Fill the bitvectors representing the 1D bins.
    template <typename T1>
    long fill1DBinsWeighted(const ibis::bitvector &mask,
			    const array_t<T1> &vals1,
			    const double &begin1, const double &end1,
			    const double &stride1,
			    const array_t<double> &wts,
			    std::vector<double> &weights,
			    std::vector<ibis::bitvector*> &bins) const;
    /// Fill the bitvectors representing the 2D bins.
    template <typename T1, typename T2>
    long fill2DBins(const ibis::bitvector &mask, const array_t<T1> &vals1,
		    const double &begin1, const double &end1,
		    const double &stride1,
		    const array_t<T2> &vals2,
		    const double &begin2, const double &end2,
		    const double &stride2,
		    std::vector<ibis::bitvector> &bins) const;
    template <typename T1>
    long fill2DBins2(const ibis::bitvector &mask, const array_t<T1> &vals1,
		     const double &begin1, const double &end1,
		     const double &stride1,
		     const ibis::column &col2,
		     const double &begin2, const double &end2,
		     const double &stride2,
		     std::vector<ibis::bitvector> &bins) const;
    /// Fill the bitvectors representing the 2D bins.
    template <typename T1, typename T2>
    long fill2DBins(const ibis::bitvector &mask, const array_t<T1> &vals1,
		    const double &begin1, const double &end1,
		    const double &stride1,
		    const array_t<T2> &vals2,
		    const double &begin2, const double &end2,
		    const double &stride2,
		    std::vector<ibis::bitvector*> &bins) const;
    template <typename T1>
    long fill2DBins2(const ibis::bitvector &mask, const array_t<T1> &vals1,
		     const double &begin1, const double &end1,
		     const double &stride1,
		     const ibis::column &col2,
		     const double &begin2, const double &end2,
		     const double &stride2,
		     std::vector<ibis::bitvector*> &bins) const;
    /// Fill the bitvectors representing the 2D bins.
    template <typename T1, typename T2>
    long fill2DBinsWeighted(const ibis::bitvector &mask,
			    const array_t<T1> &vals1,
			    const double &begin1, const double &end1,
			    const double &stride1,
			    const array_t<T2> &vals2,
			    const double &begin2, const double &end2,
			    const double &stride2,
			    const array_t<double> &wts,
			    std::vector<double> &weights,
			    std::vector<ibis::bitvector*> &bins) const;
    template <typename T1>
    long fill2DBinsWeighted2(const ibis::bitvector &mask,
			     const array_t<T1> &vals1,
			     const double &begin1, const double &end1,
			     const double &stride1,
			     const ibis::column &col2,
			     const double &begin2, const double &end2,
			     const double &stride2,
			     const array_t<double> &wts,
			     std::vector<double> &weights,
			     std::vector<ibis::bitvector*> &bins) const;
    /// Fill the bitvectors representing the 3D bins.
    template <typename T1, typename T2, typename T3>
    long fill3DBins(const ibis::bitvector &mask, const array_t<T1> &vals1,
		    const double &begin1, const double &end1,
		    const double &stride1,
		    const array_t<T2> &vals2,
		    const double &begin2, const double &end2,
		    const double &stride2,
		    const array_t<T3> &vals3,
		    const double &begin3, const double &end3,
		    const double &stride3,
		    std::vector<ibis::bitvector> &bins) const;
    template <typename T1>
    long fill3DBins2(const ibis::bitvector &mask, const array_t<T1> &vals1,
		     const double &begin1, const double &end1,
		     const double &stride1,
		     const ibis::column &col2,
		     const double &begin2, const double &end2,
		     const double &stride2,
		     const ibis::column &col3,
		     const double &begin3, const double &end3,
		     const double &stride3,
		     std::vector<bitvector> &bins) const;
    template <typename T1, typename T2>
    long fill3DBins3(const ibis::bitvector &mask, const array_t<T1> &vals1,
		     const double &begin1, const double &end1,
		     const double &stride1,
		     const array_t<T2> &vals2,
		     const double &begin2, const double &end2,
		     const double &stride2,
		     const ibis::column &col3,
		     const double &begin3, const double &end3,
		     const double &stride3,
		     std::vector<bitvector> &bins) const;
    template <typename T1, typename T2, typename T3>
    long fill3DBins(const ibis::bitvector &mask, const array_t<T1> &vals1,
		    const double &begin1, const double &end1,
		    const double &stride1,
		    const array_t<T2> &vals2,
		    const double &begin2, const double &end2,
		    const double &stride2,
		    const array_t<T3> &vals3,
		    const double &begin3, const double &end3,
		    const double &stride3,
		    std::vector<bitvector*> &bins) const;
    template <typename T1>
    long fill3DBins2(const ibis::bitvector &mask, const array_t<T1> &vals1,
		     const double &begin1, const double &end1,
		     const double &stride1,
		     const ibis::column &col2,
		     const double &begin2, const double &end2,
		     const double &stride2,
		     const ibis::column &col3,
		     const double &begin3, const double &end3,
		     const double &stride3,
		     std::vector<ibis::bitvector*> &bins) const;
    template <typename T1, typename T2>
    long fill3DBins3(const ibis::bitvector &mask, const array_t<T1> &vals1,
		     const double &begin1, const double &end1,
		     const double &stride1,
		     const array_t<T2> &vals2,
		     const double &begin2, const double &end2,
		     const double &stride2,
		     const ibis::column &col3,
		     const double &begin3, const double &end3,
		     const double &stride3,
		     std::vector<ibis::bitvector*> &bins) const;
    template <typename T1, typename T2, typename T3>
    long fill3DBinsWeighted(const ibis::bitvector &mask,
			    const array_t<T1> &vals1,
			    const double &begin1, const double &end1,
			    const double &stride1,
			    const array_t<T2> &vals2,
			    const double &begin2, const double &end2,
			    const double &stride2,
			    const array_t<T3> &vals3,
			    const double &begin3, const double &end3,
			    const double &stride3,
			    const array_t<double> &wts,
			    std::vector<double> &weights,
			    std::vector<bitvector*> &bins) const;
    template <typename T1>
    long fill3DBinsWeighted2(const ibis::bitvector &mask,
			     const array_t<T1> &vals1,
			     const double &begin1, const double &end1,
			     const double &stride1,
			     const ibis::column &col2,
			     const double &begin2, const double &end2,
			     const double &stride2,
			     const ibis::column &col3,
			     const double &begin3, const double &end3,
			     const double &stride3,
			     const array_t<double> &wts,
			     std::vector<double> &weights,
			     std::vector<ibis::bitvector*> &bins) const;
    template <typename T1, typename T2>
    long fill3DBinsWeighted3(const ibis::bitvector &mask,
			     const array_t<T1> &vals1,
			     const double &begin1, const double &end1,
			     const double &stride1,
			     const array_t<T2> &vals2,
			     const double &begin2, const double &end2,
			     const double &stride2,
			     const ibis::column &col3,
			     const double &begin3, const double &end3,
			     const double &stride3,
			     const array_t<double> &wts,
			     std::vector<double> &weights,
			     std::vector<ibis::bitvector*> &bins) const;

    /// Compute 1D histogram from raw data.
    long get1DBins_(const ibis::bitvector &mask, const ibis::column &col,
		    uint32_t nbin, std::vector<double> &bounds,
		    std::vector<ibis::bitvector> &bins, const char *mesg) const;

    /// Compute 1D histogram from index.
    long get1DDistribution(const ibis::column &col, uint32_t nbin,
			   std::vector<double> &bounds,
			   std::vector<uint32_t> &counts) const;
    /// Compute 2D histogram with uniform bins from base data.
    long get2DDistributionU(const ibis::column &col1,
			    const ibis::column &col2,
			    uint32_t nb1, uint32_t nb2,
			    std::vector<double> &bounds1,
			    std::vector<double> &bounds2,
			    std::vector<uint32_t> &counts) const;
    /// Compute 2D histogram with adaptive bins from base data.
    long get2DDistributionA(const ibis::column &col1,
			    const ibis::column &col2,
			    uint32_t nb1, uint32_t nb2,
			    std::vector<double> &bounds1,
			    std::vector<double> &bounds2,
			    std::vector<uint32_t> &counts) const;
    /// Compute 2D histogram from indexes.
    long get2DDistributionI(const ibis::column &col1,
			    const ibis::column &col2,
			    uint32_t nb1, uint32_t nb2,
			    std::vector<double> &bounds1,
			    std::vector<double> &bounds2,
			    std::vector<uint32_t> &counts) const;
    long old2DDistribution(const char *constraints,
			   const char *name1, const char *name2,
			   uint32_t nb1, uint32_t nb2,
			   std::vector<double> &bounds1,
			   std::vector<double> &bounds2,
			   std::vector<uint32_t> &counts) const;
    /// Produce a set of bitmaps corresponding to a set of coarse bins.
    int coarsenBins(const ibis::column &col, uint32_t nbin,
		    std::vector<double> &bnds,
		    std::vector<ibis::bitvector*> &btmp) const;
    /// Compute 3D histogram with adaptive bins from base data.
    long get3DDistributionA(const ibis::bitvector &mask,
			    const ibis::column &col1,
			    const ibis::column &col2,
			    const ibis::column &col3,
			    uint32_t nb1, uint32_t nb2, uint32_t nb3,
			    std::vector<double> &bounds1,
			    std::vector<double> &bounds2,
			    std::vector<double> &bounds3,
			    std::vector<uint32_t> &counts) const;
    template <typename E1>
    long get3DDistributionA1(const ibis::bitvector &mask,
			     const array_t<E1> &vals1,
			     const ibis::column &col2,
			     const ibis::column &col3,
			     uint32_t nb1, uint32_t nb2, uint32_t nb3,
			     std::vector<double> &bounds1,
			     std::vector<double> &bounds2,
			     std::vector<double> &bounds3,
			     std::vector<uint32_t> &counts) const;
    template <typename E1, typename E2>
    long get3DDistributionA2(const ibis::bitvector &mask,
			     const array_t<E1> &vals1,
			     const array_t<E2> &vals2,
			     const ibis::column &col3,
			     uint32_t nb1, uint32_t nb2, uint32_t nb3,
			     std::vector<double> &bounds1,
			     std::vector<double> &bounds2,
			     std::vector<double> &bounds3,
			     std::vector<uint32_t> &counts) const;

    template <typename E1, typename E2>
	static void mapValues(array_t<E1> &val1, array_t<E2> &val2,
			      uint32_t nb1, uint32_t nb2,
			      array_t<E1> &bnd1, array_t<E2> &bnd2,
			      std::vector<uint32_t> &cnts);

    template <typename T>
	static void mapValues(const array_t<T> &vals,
			      std::map<T, uint32_t> &hist);

    template <typename T>
	static void equalWeightBins(const array_t<T> &vals,
				    uint32_t nbins, array_t<T> &bounds);

    template <typename T>
	static long adaptiveInts(const array_t<T> &vals, const T vmin,
				 const T vmax, uint32_t nbins,
				 std::vector<double> &bounds,
				 std::vector<uint32_t> &counts);

    template <typename T>
	static long adaptiveFloats(const array_t<T> &vals, const T vmin,
				   const T vmax, uint32_t nbins,
				   std::vector<double> &bounds,
				   std::vector<uint32_t> &counts);

    template <typename T1, typename T2>
	static long adaptive2DBins(const array_t<T1> &vals1,
				   const array_t<T2> &vals2,
				   uint32_t nb1, uint32_t nb2,
				   std::vector<double> &bounds1,
				   std::vector<double> &bounds2,
				   std::vector<uint32_t> &counts);

    template <typename T1, typename T2, typename T3>
	static long adaptive3DBins(const array_t<T1> &vals1,
				   const array_t<T2> &vals2,
				   const array_t<T3> &vals3,
				   uint32_t nb1, uint32_t nb2, uint32_t nb3,
				   std::vector<double> &bounds1,
				   std::vector<double> &bounds2,
				   std::vector<double> &bounds3,
				   std::vector<uint32_t> &counts);

    template <typename T> static long
	adaptiveIntsDetailed(const ibis::bitvector &mask,
			     const array_t<T> &vals,
			     const T vmin, const T vmax, uint32_t nbins,
			     std::vector<double> &bounds,
			     std::vector<ibis::bitvector> &detail);

    template <typename T> static long
	adaptiveFloatsDetailed(const ibis::bitvector &mask,
			       const array_t<T> &vals,
			       const T vmin, const T vmax, uint32_t nbins,
			       std::vector<double> &bounds,
			       std::vector<ibis::bitvector> &detail);

    static int reorderBitmap(ibis::bitvector &,
			     const ibis::bitvector &,
			     const ibis::array_t<uint32_t> &);

    void gatherSortKeys(ibis::table::stringArray& names);
    void composeQueryString(std::string &str,
			    const ibis::column* col1, const ibis::column* col2,
			    const double &lower1, const double &upper1,
			    const double &lower2, const double &upper2) const;
    void buildQueryList(ibis::part::thrArg &lst,
			unsigned nc, unsigned nq) const;
    void checkQueryList(const ibis::part::thrArg &lst) const;
    uint32_t recursiveQuery(const char* pref, const column* att,
			    double low, double high, long* nerr) const;

private:

    /******************************************************************/
    // private member variables
    mutable pthread_mutex_t mutex;	///!< Mutex for partition manipulation.
    mutable pthread_rwlock_t rwlock;	///!< Rwlock for access control.

    /******************************************************************/
    // private funcations

    void init(const char* prefix); ///!< Get directory names from gParameters.

    void   fillRIDs(const char* fn) const; ///!< Generate new RIDs.
    void   sortRIDs() const; ///!< Sort current list of RIDs.
    uint32_t searchSortedRIDs(const ibis::rid_t &rid) const;
    uint32_t searchRIDs(const ibis::rid_t &rid) const;
    void   searchSortedRIDs(const ibis::RIDSet&, ibis::bitvector&) const;
    void   searchRIDs(const ibis::RIDSet&, ibis::bitvector&) const;

    // functions to perform join operations.
    int64_t equiJoin(const ibis::deprecatedJoin &cmp,
		     const ibis::bitvector64 &trial,
		     ibis::bitvector64 &result) const;
    int64_t deprecatedJoin(const ibis::deprecatedJoin &cmp,
		      const ibis::bitvector64 &trial,
		      ibis::bitvector64 &result) const;
    int64_t compJoin(const ibis::deprecatedJoin &cmp,
		     const ibis::bitvector64 &trial,
		     ibis::bitvector64 &result) const;

    int64_t loopJoin(const std::vector<const ibis::deprecatedJoin*> &cmp,
		     const ibis::bitvector &mask,
		     ibis::bitvector64 &pairs) const;
    int64_t loopJoin(const std::vector<const ibis::deprecatedJoin*> &cmp,
		     const ibis::bitvector &mask) const;
    int64_t loopJoin(const ibis::deprecatedJoin &cmp,
		     const ibis::bitvector &mask,
		     ibis::bitvector64 &pairs) const;
    int64_t loopJoin(const ibis::deprecatedJoin &cmp,
		     const ibis::bitvector &mask) const;
    int64_t equiJoinLoop1(const ibis::deprecatedJoin &cmp,
			  const ibis::bitvector &mask,
			  ibis::bitvector64 &pairs) const;
    int64_t equiJoinLoop1(const ibis::deprecatedJoin &cmp,
			  const ibis::bitvector &mask) const;
    int64_t equiJoinLoop2(const ibis::deprecatedJoin &cmp,
			  const ibis::bitvector &mask,
			  ibis::bitvector64 &pairs) const;
    int64_t equiJoinLoop2(const ibis::deprecatedJoin &cmp,
			  const ibis::bitvector &mask) const;
    template <class type1, class type2>
    void deprecatedJoinLoop(const array_t<type1> &arr1,
		       const ibis::bitvector &msk1,
		       const array_t<type2> &arr2,
		       const ibis::bitvector &msk2,
		       const double delta,
		       ibis::bitvector64 &pairs) const;
    template <class type1, class type2>
    int64_t deprecatedJoinLoop(const array_t<type1> &arr1,
			  const ibis::bitvector &msk1,
			  const array_t<type2> &arr2,
			  const ibis::bitvector &msk2,
			  const double delta) const;
    int64_t deprecatedJoinLoop(const ibis::deprecatedJoin &cmp,
			  const ibis::bitvector &mask,
			  ibis::bitvector64 &pairs) const;
    int64_t deprecatedJoinLoop(const ibis::deprecatedJoin &cmp,
			  const ibis::bitvector &mask) const;
    int64_t compJoinLoop(const ibis::deprecatedJoin &cmp,
			 const ibis::bitvector &mask,
			 ibis::bitvector64 &pairs) const;
    int64_t compJoinLoop(const ibis::deprecatedJoin &cmp,
			 const ibis::bitvector &mask) const;

    part(const part&);
    const part &operator=(const part&);
}; // class ibis::part

namespace ibis {
    // Extends the name space ibis::util to contain three more functions to
    // deal with the reconstruction of partitions.
    namespace util {
	/// Look for data partitions in the given pair of directories.
	unsigned int FASTBIT_CXX_DLLSPEC
	gatherParts(ibis::partList &parts,
		    const char *adir, const char *bdir, bool ro=false);
	/// Look for data partitions in the given directory.
	unsigned int FASTBIT_CXX_DLLSPEC
	gatherParts(ibis::partList &parts, const char *adir, bool ro=false);
	/// Reconstruct partitions using data directories specified in the
	/// resource list.
	unsigned int FASTBIT_CXX_DLLSPEC
	gatherParts(ibis::partList &parts, const ibis::resource &res,
		    bool ro=false);
    } // namespace util
} // namespace ibis

/// A simple class to describe an ibis::part object.  All members are
/// public and read-only.  An info object can not last longer than the
/// ibis::part object used to create it.
struct FASTBIT_CXX_DLLSPEC ibis::part::info {
    const char* name;		///!< Partition name.
    const char* description;	///!< A free-form description of the partition.
    const char* metaTags;	///!< A string of name-value pairs.
    const uint64_t nrows;	///!< The number of rows in the partition.
    /// The list of columns in the partition.
    std::vector<ibis::column::info*> cols;

    info(const char* na, const char* de, const uint64_t &nr,
	 const ibis::part::columnList &co);
    info(const ibis::part &tbl);
    ~info();

private:
    info();	// private default constructor, not implemented!
    info& operator=(const info&);
}; // ibis::part::info

/// A cleaner to be used by the function fileManager::unload.
class ibis::part::cleaner : public ibis::fileManager::cleaner {
public:
    virtual void operator()() const;
    cleaner(const part* tbl) : thePart(tbl) {}
    virtual ~cleaner() {}

private:
    const part* thePart;
}; // ibis::part::cleaner

/// Provide a read lock on an ibis::part.  Routines need read access to
/// ibis::part class should use this class instead directly calling
/// ibis::part::gainReadAccess so that in case of exceptions, the release
/// command would be always called.
class ibis::part::readLock {
public:
    readLock(const part* tbl, const char* m) : thePart(tbl), mesg(m) {
	int ierr = tbl->gainReadAccess();
        if (0 != ierr) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- part[" << thePart->name()
                << "]::gainReadAccess -- pthread_rwlock_rdlock for " << mesg
                << " returned " << ierr << " (" << strerror(ierr) << ')';
        }
        else {
            LOGGER(ibis::gVerbose > 9)
                << "part[" << thePart->name()
                << "]::gainReadAccess for " << mesg;
        }
    }
    ~readLock() {
        int ierr = thePart->releaseAccess();
        if (0 != ierr) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- part[" << thePart->name()
                << "]::releaseAccess -- pthread_rwlock_unlock for " << mesg
                << " returned " << ierr << " (" << strerror(ierr) << ')';
        }
        else {
            LOGGER(ibis::gVerbose > 9)
                << "part[" << thePart->name()
                << "]::releaseAccess for " << mesg;
        }
    }

private:
    const part* thePart;
    const char* mesg;

    readLock() {}; // no default constructor
    readLock(const readLock&) {}; // can not copy
    const readLock &operator=(const readLock&);
}; // ibis::part::readLock

/// Provide a write lock on an ibis::part.
class ibis::part::writeLock {
public:
    writeLock(const part* tbl, const char* m) : thePart(tbl), mesg(m) {
	int ierr = tbl->gainWriteAccess();
        if (0 != ierr) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- part[" << thePart->name()
                << "]::gainWriteAccess -- pthread_rwlock_wrlock for " << mesg
                << " returned " << ierr << " (" << strerror(ierr) << ')';
        }
        else {
            LOGGER(ibis::gVerbose > 9)
                << "part[" << thePart->name()
                << "]::gainWriteAccess for " << mesg;
        }
    }
    ~writeLock() {
        int ierr = thePart->releaseAccess();
        if (0 != ierr) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- part[" << thePart->name()
                << "]::releaseAccess -- pthread_rwlock_unlock for " << mesg
                << " returned " << ierr << " (" << strerror(ierr) << ')';
        }
        else {
            LOGGER(ibis::gVerbose > 9)
                << "part[" << thePart->name()
                << "]::releaseAccess for " << mesg;
        }
    }

private:
    const part* thePart;
    const char* mesg;

    writeLock() {}; // no default constructor
    writeLock(const writeLock&) {}; // can not copy
    const writeLock &operator=(const writeLock&);
}; // ibis::part::writeLock

/// An non-blocking version of writeLock.  The function @c acquired returns
/// true is the object has acquired a write lock successfully, otherwise the
/// function returns false.
class ibis::part::softWriteLock {
public:
    /// Constructor.
    softWriteLock(const part* tbl, const char* m);
    /// Destructor.
    ~softWriteLock();
    /// Have we acquired the desired lock?  Returns true if yes, otherwise
    /// false.
    bool isLocked() const {return (lckd==0);}

private:
    const part* thePart;
    const char* mesg;
    const int lckd;

    softWriteLock() : thePart(0), mesg(0), lckd(0) {};
    softWriteLock(const softWriteLock &rhs)
	: thePart(rhs.thePart), mesg(rhs.mesg), lckd(0) {};
    const softWriteLock &operator=(const softWriteLock&);
}; // ibis::part::softWriteLock

/// Provide a mutual exclusion lock on an ibis::part object.  Used
/// externally by derived class of ibis::part.
class ibis::part::mutexLock {
public:
    mutexLock(const part* tbl, const char* m) : thePart(tbl), mesg(m) {
	if (ibis::gVerbose > 9)
	    tbl->logMessage("gainExclusiveAccess",
			    "pthread_mutex_lock for %s", m);
	int ierr = pthread_mutex_lock(&(tbl->mutex));
	if (0 != ierr)
	    tbl->logWarning("gainExclusiveAccess", "pthread_mutex_lock for %s "
			    "returned %d (%s)", m, ierr, strerror(ierr));
    }
    ~mutexLock() {
	if (ibis::gVerbose > 9)
	    thePart->logMessage("releaseExclusiveAccess",
				"pthread_mutex_unlock for %s", mesg);
	int ierr = pthread_mutex_unlock(&(thePart->mutex));
	if (0 != ierr)
	    thePart->logWarning("releaseExclusiveAccess",
				"pthread_mutex_unlock for %s returned %d (%s)",
				mesg, ierr, strerror(ierr));
    }

private:
    const part* thePart;
    const char* mesg;

    mutexLock() {}; // no default constructor 	 
    mutexLock(const mutexLock&) {}; // can not copy 	 
    const mutexLock &operator=(const mutexLock&); 	 
}; // ibis::part::mutexLock

/// To read a list of variables at the same time.
/// This implementation opens each data file and read the values from the
/// files one at a time.
class ibis::part::barrel : public ibis::math::barrel {
public:
    barrel(const ibis::part *t=0) : _tbl(t), position(0) {};
    virtual ~barrel() {close();} ///!< Destructor closes the open files.

    virtual long open(const ibis::part *t=0); ///!< Open all data files.
    virtual long close(); ///!< Close all open files.
    virtual long read(); ///!< Read one value for each variable.
    /// Move the file pointers to the posth record.  Return 0 for success
    /// and other integer for error.
    virtual long seek(uint32_t pos);
    uint32_t tell() const {return position;}

    void getNullMask(ibis::bitvector &mask) const;
    const ibis::column* getColumn(uint32_t i) const {return cols[i];}

protected:
    const ibis::part *_tbl;
    uint32_t position;
    std::vector<const ibis::column*> cols;
    std::vector<ibis::fileManager::storage*> stores;
    std::vector<int> fdes;
}; // ibis::part::barrel

/// To read variables in certain order.
/// A version of barrel that keys on an index array (i.e., a roster).
class ibis::part::vault : public ibis::part::barrel {
public:
    vault(const ibis::roster &r);
    virtual ~vault() {close();}

    virtual long open(const ibis::part *t=0); ///!< Open all data files.
    virtual long read(); ///!< Read the values at the current position.
    virtual long seek(uint32_t pos); ///!< Move the logical position.
    /// Move to the first position that value(var) >= val.
    long seek(double val);
    /// Tell the physical record number.  User may called _roster[tell()]
    /// to avoid the overhead of calling this function.
    uint32_t tellReal() const;

private:
    const ibis::roster &_roster;

    template <class T>
    uint32_t seekValue(int fd, const T &val) const;
    template <class T>
    uint32_t seekValue(const array_t<T>&arr, const T &val) const;

    vault();
    vault& operator=(const vault&);
}; // ibis::part::vault
/*
barrel

Vault \Vault\ (v[add]lt; see Note, below), n. [OE. voute, OF. voute, volte,
F. vo[^u]te, LL. volta, for voluta, volutio, fr. L. volvere, volutum, to
roll, to turn about. See Voluble, and cf. Vault a leap, Volt a turn,
Volute.] 1. (Arch.) An arched structure of masonry, forming a ceiling or
canopy.

The long-drawn aisle and fretted vault. --Gray.

2. An arched apartment; especially, a subterranean room, use for storing
   articles, for a prison, for interment, or the like; a cell; a
   cellar. ``Charnel vaults.'' --Milton.

The silent vaults of death. --Sandys.

To banish rats that haunt our vault. --Swift.

3. The canopy of heaven; the sky.

That heaven's vault should crack. --Shak.

4. [F. volte, It. volta, originally, a turn, and the same word as volta an
   arch. See the Etymology above.] A leap or bound. Specifically: (a)
   (Man.) The bound or leap of a horse; a curvet. (b) A leap by aid of the
   hands, or of a pole, springboard, or the like.

Note: The l in this word was formerly often suppressed in pronunciation.

Barrel, Cradle, Cylindrical, or Wagon, vault (Arch.), a kind of vault
having two parallel abutments, and the same section or profile at all
points. It may be rampant, as over a staircase (see Rampant vault, under
Rampant), or curved in plan, as around the apse of a church.

Coved vault. (Arch.) See under 1st Cove, v. t.

Groined vault (Arch.), a vault having groins, that is, one in which
different cylindrical surfaces intersect one another, as distinguished from
a barrel, or wagon, vault.

Rampant vault. (Arch.) See under Rampant.

Ribbed vault (Arch.), a vault differing from others in having solid ribs
which bear the weight of the vaulted surface. True Gothic vaults are of
this character.

Vault light, a partly glazed plate inserted in a pavement or ceiling to
admit light to a vault below.

Source: Webster's Revised Unabridged Dictionary, (c) 1996, 1998 MICRA, Inc.
 */

namespace ibis {
    // Explicit template specialization for member function
    // ibis::part::equalWeightBins
    template <> void
    part::equalWeightBins(const array_t<float> &vals,
			  uint32_t nbins, array_t<float> &bounds);
    template <> void
    part::equalWeightBins(const array_t<double> &vals,
			  uint32_t nbins, array_t<double> &bounds);

    template <> long
    part::doScan(const array_t<float> &,
		 const ibis::qContinuousRange &,
		 const ibis::bitvector &, ibis::bitvector &);
    template <> long
    part::doScan(const array_t<double> &,
		 const ibis::qContinuousRange &,
		 const ibis::bitvector &, ibis::bitvector &);

    template <> long
    part::doScan(const array_t<float> &,
		 const ibis::qContinuousRange &,
		 const ibis::bitvector &, ibis::array_t<float> &);
    template <> long
    part::doScan(const array_t<double> &,
		 const ibis::qContinuousRange &,
		 const ibis::bitvector &, ibis::array_t<double> &);

    template <> long
    part::doScan(const array_t<float> &,
		 const ibis::qContinuousRange &,
		 const ibis::bitvector &, ibis::array_t<float> &,
		 ibis::bitvector &);
    template <> long
    part::doScan(const array_t<double> &,
		 const ibis::qContinuousRange &,
		 const ibis::bitvector &, ibis::array_t<double> &,
		 ibis::bitvector &);

    template <> long
    part::doCount<float>(const ibis::qRange&) const;
    template <> long
    part::doCount<double>(const ibis::qRange&) const;
}

/// Return an ibis::part::info object that describes the current partition.
inline ibis::part::info* ibis::part::getInfo() const {
    return new info(*this);
}

/// Returns the pointer to the ith column.  If an external order has been
/// established, that order is used, otherwise, the alphabetical order is
/// used.  Index starts with 0.  An out of range index will result a nil
/// pointer.
inline ibis::column* ibis::part::getColumn(uint32_t ind) const {
    if (ind < columns.size()) {
	if (colorder.size() == columns.size()) {
	    return const_cast<ibis::column*>(colorder[ind]);
	}
	else {
	    ibis::part::columnList::const_iterator it = columns.begin();
	    for (; ind > 0; -- ind) ++ it;
	    return (*it).second;
	}
    }
    else {
	return 0;
    }
} // ibis::part::getColumn

inline const char* ibis::part::getMetaTag(const char* name) const {
    ibis::resource::vList::const_iterator it = metaList.find(name);
    if (it != metaList.end())
	return (*it).second;
    else
	return static_cast<const char*>(0);
} // ibis::part::getMetaTag

inline int64_t ibis::part::evaluateJoin(const ibis::deprecatedJoin &cmp,
					const ibis::bitvector &mask,
					ibis::bitvector64 &pairs) const {
    return loopJoin(cmp, mask, pairs);
} // ibis::part::evaluateJoin

inline int64_t ibis::part::evaluateJoin(const ibis::deprecatedJoin &,
					const ibis::bitvector &,
					const char *) const {
    logWarning("evaluate", "not implemented yet");
    return -1;
} // ibis::part::evaluateJoin

inline int64_t ibis::part::evaluateJoin(const ibis::deprecatedJoin &cmp,
					const ibis::bitvector &mask) const {
    return loopJoin(cmp, mask);
} // ibis::part::evaluateJoin

inline int64_t ibis::part::evaluateJoin
(const std::vector<const ibis::deprecatedJoin*> &cmp,
 const ibis::bitvector &mask, ibis::bitvector64 &pairs) const {
    return loopJoin(cmp, mask, pairs);
} // ibis::part::evaluateJoin

/// Attempt to release a read or write access to this part object.  A
/// simple wrap over pthread_rwlock_unlock.  Returns the return value of
/// pthread_rwlock_unlock.
inline int ibis::part::releaseAccess() const {
    return pthread_rwlock_unlock(&rwlock);
} // ibis::part::releaseAccess

/// Attempt to gain a read access to this part object.  A simple wrap over
/// pthread_rwlock_rdlock.  Returns the return value of
/// pthread_rwlock_rdlock.
inline int ibis::part::gainReadAccess() const {
    return pthread_rwlock_rdlock(&rwlock);
} // ibis::part::gainReadAccess

/// Attempt to gain a write access to this part object.  A simple wrap over
/// pthread_rwlock_wrlock.  Returns the return value of
/// pthread_rwlock_wrlock.
inline int ibis::part::gainWriteAccess() const {
    return pthread_rwlock_wrlock(&rwlock);
} // ibis::part::gainWriteAccess

/// A soft attempt to gain a write access to this part object.  A simple
/// wrap over pthread_rwlock_trywrlock.  Returns the return value of
/// pthread_rwlock_trywrlock.
inline int ibis::part::tryWriteAccess() const {
    return pthread_rwlock_trywrlock(&rwlock);
} // ibis::part::tryWriteAccess
#endif // IBIS_PART_H
