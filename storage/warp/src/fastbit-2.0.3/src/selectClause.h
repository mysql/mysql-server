// $Id$
// Author: John Wu <John.Wu at acm.org>
//      Lawrence Berkeley National Laboratory
// Copyright (c) 2007-2016 the Regents of the University of California
/** @file
    Declares ibis::selectClause class.
*/
#ifndef IBIS_SELECTCLAUSE_H
#define IBIS_SELECTCLAUSE_H
#include "qExpr.h"
#include "table.h"

namespace ibis {
    class selectLexer;
    class selectParser;
}

/// A class to represent the select clause.  It parses a string into a list
/// of arithmetic expressions and aggregation functions.
///
/// The terms in a select clause must be separated by comas ',' and each
/// term may be an arithmetic expression or an aggregation function over an
/// arithmetic expression, e.g., "age, avg(income)" and "temperature,
/// sqrt(vx*vx+vy*vy+vz*vz) as speed, max(duration * speed)".  An
/// arithmetic expression may contain any valid combination of numbers and
/// column names connected with operators +, -, *, /, %, ^, ** and standard
/// functions defined in math.h and having only one or two arguements.
/// A handful of functions requested by various uers have also been added
/// as collection of 1- and 2-argument functions.  These extra 1-argument
/// functions include: IS_ZERO, IS_NONZER, TRUNC (for truncating
/// floating-point values to whole numbers), and INT_FROM_DICT (meant for
/// reporting the integer values representing the categorical values, but
/// implemented as ROUND).  The extra 2-argument functions include: IS_EQL,
/// IS_GTE (>=), IS_LTE (<=).
/// 
/// The supported aggregation functions are:
///
/// - count(*): count the number of rows in each group.
/// - countdistinct(expression): count the number of distinct values
///   computed by the expression.  It is equivalent to SQL expression
///   'count(distinct expression)'.  It can be shorted to
///   distinct(expression).
/// - avg(expression): compute the average of the expression.  Note that
///   the computation is always performed in double-precision
///   floating-point values.  This cause 64-bit integer values to be
///   rounded to 48-bit precision and may produce inaccuracy in the result.
/// - sum(expression): compute the sum of the expression.  This computation
///   is performed as double-precision floating-point values.
/// - max(expression): compute the maximum value of the expression.
/// - min(expression): compute the minimum value of the expression.
/// - median(expression): compute the median of the expression.  Note that
///   if the arithmetic expression is a simple column name, the value
///   retuned by this function has the same type as the column.  In cases
///   that require the computation of the average of two values, the average is
///   computed using the arithmetic in the type of the column.  This means
///   the median of a set of integers is always an integer, which can be
///   slightly different from what one might expect.  Arithmetic
///   expressions are evaluated in double-precision arithmetic, their
///   median values will also be double-precision floating-point numbers.
/// - var(expression): compute the sample variance, i.e., the sum of
///   squared differences from the mean divided by the number of rows in
///   the group minus 1.  This function name may also appears as varsamp or
///   variance.  This computation is performed in double precision.
/// - varp(expression): compute the population variance, i.e., the sum of
///   squared differences from the mean divided by the number of rows in
///   the group.  This function name may also appears as varpop because the
///   origianal contributor of this function, Jan, used varpop.   This
///   computation is performed in double precision.
/// - stdev(expression): compute the sample standard deviation, i.e., the
///   square root of the sum of squared differences from the mean divided
///   by the number of rows minus 1.  Thisfunction name may also appears as
///   stdsamp or stddev.  This computation is performed in double precision.
/// - stdevp(expression): compute the population standard deviation, i.e.,
///   the square root of the sum of squared differences from the mean
///   divided by the number of rows.  This function name may also appears
///   as stdpop.  This computation is performed in double precision.
/// - group_concat(expression): concatenate all values of the given
///   expression in the string form.
///
/// Each term may optionally be followed by an alias for the term.  The
/// alias must be a valid SQL name.  The alias may optionally be preceded
/// by the keyword 'AS'.  The aliases can be used in the other part of the
/// query.
///
/// @note All select operations excludes null values!  In most SQL
/// implementations, the function 'count(*)' includes the null values.
/// However, in FastBit, null values are always excluded.  For example, the
/// return value for 'count(*)' in the following two select statements may
/// be different if there are any null values in column A,
/// @code
/// select count(*) from ...;
/// select avg(A), count(*) from ...;
/// @endcode
/// In the first case, the number reported is purely determined by the
/// where clause.  However, in the second case, because the select clause
/// also involves the column A, all of null values of A are excluded,
/// therefore 'count(*)' in the second example may be smaller than that
/// of the first example.
///
/// In cases where an integer-valued column is actually storing unix time
/// stamps, it might be useful to print out the integer values in the usual
/// date/time format.  In this case, the following pseudo-function notation
/// could be used.
/// @code
/// select FORMAT_UNIXTIME_LOCAL(colname, "formatstring") from ...;
/// select FORMAT_UNIXTIME_GMT(colname, "formatstring") from ...;
/// @endcode
/// Note that the format string is passed to @c strftime.  Please refer to the
/// documentation about @c strftime for details about the format.  Please
/// also note that the format string must be quoted, single quotes
/// and double quotes are accepted.
class FASTBIT_CXX_DLLSPEC ibis::selectClause {
public:
    /// Parse a new string as a select clause.
    explicit selectClause(const char *cl=0);
    /// Parse a list of strings.
    selectClause(const ibis::table::stringArray&);
    ~selectClause();

    /// Copy constructor.  Deep copy.
    selectClause(const selectClause&);

    /// Parse a new string.
    int parse(const char *cl);

    /// Return a pointer to the string form of the select clause.
    const char* getString(void) const {return clause_.c_str();}
    /// Dereferences to the string form of the select clause.
    const char* operator*(void) const {return clause_.c_str();}

    /// Returns true if this select clause is empty.
    bool empty() const {return atms_.empty();}

    void printDetails(std::ostream&) const;
    void print(std::ostream&) const;
    int find(const char*) const;

    /// Functions related to extenally visible portion of the select
    /// clause.
    ///@{
    /// A vector of arithematic expressions.
    typedef std::vector<ibis::math::term*> mathTerms;
    /// Retrieve all top-level arithmetic expressions.
    const mathTerms& getTerms() const {return xtms_;}
    /// Fetch the ith term visible to the outside.  No array bound checking.
    const ibis::math::term* termExpr(unsigned i) const {return xtms_[i];}

    /// Number of terms visible to the outside.
    uint32_t numTerms() const {return xtms_.size();}
    /// Name given to the top-level function.  This is the external name
    /// assigned to termExpr(i) (which is also getTerms()[i]).  To produce
    /// a string version of the term use termDescription.
    const char* termName(unsigned i) const {return xnames_[i].c_str();}
    std::string termDescription(unsigned i) const;
    typedef std::map<const char*, const char*, ibis::lessi> nameMap;
    int getAliases(nameMap&) const;
    uint32_t numGroupbyKeys() const;
    int getGroupbyKeys(std::vector<std::string>& keys) const;
    ///@}

    /// Functions related to internal aggregation operations.
    ///@{
    /// Aggregation functions.  @note "Agregado" is Spanish for aggregate.
    enum AGREGADO {NIL_AGGR, AVG, CNT, MAX, MIN, SUM, DISTINCT,
		   VARPOP, VARSAMP, STDPOP, STDSAMP, MEDIAN, CONCAT};
    /// The number of arithmetic expressions inside the select clause.
    uint32_t aggSize() const {return atms_.size();}
    /// Return the aggregation function used for the ith term.
    AGREGADO getAggregator(uint32_t i) const {return aggr_[i];}

    /// Fetch the ith term inside the select clause.
    ///
    /// @warning No array bound checking!
    const ibis::math::term* aggExpr(unsigned i) const {return atms_[i];}
    /// Name inside the aggregation function.  To be used together with
    /// aggSize() and aggExpr().
    ///
    /// @warning No array bound checking!
    const char* aggName(unsigned i) const {return names_[i].c_str();}
    /// Produce a string description for the ith aggregation expression.
    ///
    /// @warning No array bound checking!
    std::string aggDescription(unsigned i) const {
	return aggDescription(aggr_[i], atms_[i]);}
    std::string aggDescription(AGREGADO, const ibis::math::term*) const;

    bool needsEval(const ibis::part&) const;
    bool isSeparable() const;
    const char* isUnivariate() const;

    typedef std::map<std::string, unsigned> StringToInt;
    const StringToInt& getOrdered() const {return ordered_;}
    ///@}

    int verify(const ibis::part&) const;
    int verifySome(const std::vector<uint32_t>&, const ibis::part&) const;
    static int verifyTerm(const ibis::math::term&, const ibis::part&,
			  const ibis::selectClause* =0);

    void getNullMask(const ibis::part&, ibis::bitvector&) const;
    void clear();

    /// Assignment operator.
    selectClause& operator=(const selectClause& rhs) {
	selectClause tmp(rhs);
	swap(tmp);
	return *this;
    }
    /// Swap the content of two select clauses.
    void swap(selectClause& rhs) {
	atms_.swap(rhs.atms_);
	aggr_.swap(rhs.aggr_);
	names_.swap(rhs.names_);
	ordered_.swap(rhs.ordered_);
	xtms_.swap(rhs.xtms_);
	xalias_.swap(rhs.xalias_);
	xnames_.swap(rhs.xnames_);
	clause_.swap(rhs.clause_);
    }

    // forward declaration
    class variable;
    friend class variable;

protected:
    /// Arithmetic expressions used by aggregators.
    mathTerms atms_;
    /// Aggregators.
    std::vector<AGREGADO> aggr_;
    /// Names of the variables inside the aggregation functions.
    std::vector<std::string> names_;
    /// A ordered version of names_.
    StringToInt ordered_;
    /// Top-level terms.  Externally visible arithmetic expressions.
    mathTerms xtms_;
    /// Aliases.
    StringToInt xalias_;
    /// Names of the top-level terms.
    std::vector<std::string> xnames_;

    std::string clause_;	///!< String version of the select clause.

    ibis::selectLexer *lexer;	///!< A pointer for the parser.

    friend class ibis::selectParser;

    void fillNames();
    ibis::math::variable*
	addAgregado(ibis::selectClause::AGREGADO, ibis::math::term*);
    uint64_t decodeAName(const char*) const;
    void addTerm(ibis::math::term*, const std::string*);
    ibis::math::term* addRecursive(ibis::math::term*&);
    bool hasAggregation(const ibis::math::term *tm) const;

    typedef std::map<const char*, ibis::selectClause::variable*,
		     ibis::lessi> varMap;
    void gatherVariables(varMap &vmap, ibis::math::term* t) const;
}; // class ibis::selectClause

/// A specialization of ibis::math::variable.  It represents a name that
/// refers to an aggregation function inside a select clause.
class ibis::selectClause::variable : public ibis::math::variable {
public:
    variable(const char* s, const ibis::selectClause *c)
	: ibis::math::variable(s), sc_(c) {};
    variable(const variable &v) : ibis::math::variable(v), sc_(v.sc_) {};
    virtual variable* dup() const {return new variable(*this);}
    virtual void print(std::ostream&) const;
    void updateReference(const ibis::selectClause *c) {sc_=c;}

private:
    const ibis::selectClause *sc_;

    variable();
}; // class ibis::selectClause::variable

namespace std {
    inline ostream& operator<<(ostream& out, const ibis::selectClause& sel) {
	sel.print(out);
	return out;
    } // std::operator<<
}
#endif
