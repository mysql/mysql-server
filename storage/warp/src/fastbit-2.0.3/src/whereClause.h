// $Id$
// Author: John Wu <John.Wu at acm.org>
//      Lawrence Berkeley National Laboratory
// Copyright (c) 2007-2016 the Regents of the University of California
/** @file
    Declares ibis::whereClause class.
*/
#ifndef IBIS_WHERECLAUSE_H
#define IBIS_WHERECLAUSE_H
#include "qExpr.h"

namespace ibis {
    class selectClause;
    class whereClause;
    class whereLexer;
    class whereParser;
}

/// A representation of the where clause.  It parses a string into an
/// ibis::qExpr object.  One may access the functions defined for
/// ibis::qExpr through the operator->.
///
/// A where clause is a set of range conditions joined together with
/// logical operators.  The supported logical operators are
/// @code
/// NOT, AND, OR, XOR, &&, ||.
/// @endcode
///
/// The supported range conditions are equality conditions, discrete
/// ranges, one-sided range conditions and two-sided range conditions.
///
/// - An equality condition is defined by the equal operator and its two
///   operands can be arithematic expressions, column names, numbers or
///   string literals.  On string valued columns, FastBit currently only
///   supports equality comparisons.  In such a case, the comparison is of
///   the form "column_name = column_value".  Internally, when FastBit
///   detect that the type of the column named "column_name" is
///   ibis::CATEGORY or ibis::TEXT, it will interpret the other side as
///   literal string value to be compared.  Note that if the left operand
///   of the equality operator is not a known column name, the evaluation
///   function will examine the right operand to see if it is a column
///   name.  If the right operand is the name of string-valued column, the
///   left operand will be used as string literal.
///
/// - A discrete range is defined by the operator "IN", e.g.,
///   @code
///   column_name IN ( list_of_strings_or_numbers )
///   @endcode
///   Note unquoted string values must start with an alphabet or a
///   underscore.  Strings starting with anything else must be quoted.
///
/// - A one-side range condtion can be defined with any of the following
///   operators, <, <=, >, and >=.  The two operands of the operator can be
///   any arithmetic expressions, column names or numbers.
///
/// - A two-sided range condtion can be defined with two operators selected
///   from <, <=, >, and >=, if their directions much agree.  Alternatively,
///   they can be defined with operators "... between ... and ...", where
///   @code
///   A between B and C
///   @endcode
///   is equivalent to
///   @code
///   B <= A <= C
///   @endcode
///   In this example, A, B, and C can be arithematic expressions, column
///   names, and numbers.
///
/// An arithematic expression may contain operators +, -, *, /, %, ^, and
/// **, as well as common one-argument and two-argument functions defined
/// in the header file math.h.  Both operators ^ and ** denote the
/// exponential operation.
///
/// @note Operators & and | are reserved for bitwise logical operations
/// within an arithmetic expression, while && and || are for logical
/// operations between query conditions.
///
/// The following operations on text fields are also supported.  For the
/// purpose of query composition, they can be tought of as alternative form
/// of discrete ranges.
///
/// - Operator LIKE, e.g.,
///   @code
///   column_name LIKE regular_expression
///   @endcode
///
///   Note that the regular expression can only contain wild characters %
///   and _ per SQL standard.  Internally, this is referred to as pattern
///   matching and treats a string as a single atomic unit of data.  @sa
///   ibis::category
///
/// - Operator CONTAINS, e.g.,
///   @code
///   column_name CONTAINS literal_word
///   column_name CONTAINS ( list_of_literal_words )
///   @endcode
///
///   When multiple keywords are given, this operator is meant to look for
///   rows containing all of the given keywords.  Internally, this is
///   referred to as keyword matching and treats a string field as a list
///   of words.  Typically, the column is of type TEXT and a KEYWORD index
///   has been built on the column.  Note that the KEYWORD index could take
///   a user provided parser to extract the keywords.  @sa ibis::keywords
///
///   This operator can work with set-valued data, in which case, each row
///   of this column is a set but expressed as a string.  The user provides
///   a parser during the construction of the KEYWORD index to make sure
///   the string is parsd into the correct elements of the sets.  This
///   expression is used to identify sets with the speicified list of
///   elements.
///
/// - Operator NOT NULL
///   The only way to mention NULL values in a query expression is through
///   this operator.
///   @code
///   column_name NOT NULL
///   @endcode
/// 
///   Note that there is no support for NULL as an operator.  The way to
///   select only NULL values is through (NOT column_name NOT NULL).
/// 
/// - Time handling functions
///   An integer valued column could be used to store Unix time stamps
///   (i.e., seconds since beginning of 1970), in which case, it might be
///   useful to perform comparison on day of the week or hours of a day in
///   a where clause.  To support such operations, four functions are
///   provided.
/// 
///   -- from_unixtime_local(timestamp, "format"): extract a number from
///      the time stamp.  The timestamp should be the name of column to be
///      interpreted as Unix time stamps.  The format string follows the
///      convention of function @c strftime from @c libc.  Note that this
///      function actuall uses @c strftime to extract the information in
///      string form first and then interpret the leading portion of the
///      string as a floating-point number.  The tailing portion of the
///      string that could not be interpreted as part of a floating-point
///      number is ignored.  If this process results no number at all, for
///      example, @c strftime prints the first date and time with an
///      alphabet as the first character, then this function returns a NaN
///      (Not-a-number).
/// 
///      This function assumes the time stamps are in the local time zone.
/// 
///      Note that the format string must be quoted.
/// 
///   -- from_unixtime_gmt(timestamp, "format"): same functionality as
///      from_unixtime_local, but assumes the time stamps are in time zone
///      GMT/UTC.
/// 
///      Note that the format string must be quoted.
/// 
///   -- to_unixtime_local("date-time-string", "format"): This function
///      attempts to be the inverse of from_unixtime_local.  It is a
///      constant function at this time and only transform one specific
///      time value to unix time stamp.  Therefore this is only useful for
///      generating a constant for bounding a unix time stamp.
/// 
///   -- to_unixtime_gmt("date-time-string", "format"): the inverse of
///      from_unixtime_gmt, but only works with a simple time constant.
/// 
class ibis::whereClause {
public:
    /// Construct a where clause from a string.
    explicit whereClause(const char *cl=0);
    /// Construct a where clause from another where clause.
    whereClause(const whereClause&);
    /// Destructor.
    ~whereClause();

    /// Parse a new string.
    int parse(const char *cl);
    /// Regenerate the string version of the query conditions.
    void resetString() {
	if (expr_ != 0) {
	    std::ostringstream oss;
	    oss << *expr_;
	    clause_ = oss.str();
	}
	else {
	    clause_.clear();
	}
    }
    /// Assign a new set of conditions directly.  The new set of conditions
    /// is copied here.
    void setExpr(const ibis::qExpr *ex) {
	clause_.clear();
	delete expr_;
	expr_ = ex->dup();
    }
    void addExpr(const ibis::qExpr *);
    void addConditions(const char *);

    /// Clear the existing content.
    void clear() throw ();
    /// The where clause is considered empty if the expr_ is a nil pointer.
    bool empty() const {return (expr_ == 0);}

    /// Return a pointer to the string form of the where clause.
    const char* getString(void) const {
	if (clause_.empty())
	    return "";
	else
	    return clause_.c_str();
    }
    /// Return a pointer to the root of the expression tree for the where
    /// clause.
    ///@note Functions that modify this object may invalidate the pointer
    /// returned by this function.
    const ibis::qExpr* getExpr(void) const {return expr_;}
    /// Return a pointer to the root of the expression tree for the where
    /// clause.
    ///@note Functions that modify this object may invalidate the pointer
    /// returned by this function.
    ibis::qExpr* getExpr(void) {return expr_;}
    /// Simplify the query expression.
    void simplify() {ibis::qExpr::simplify(expr_);}
    int verify(const ibis::part& p0, const ibis::selectClause *sel=0) const;
    void getNullMask(const ibis::part&, ibis::bitvector&) const;

    /// Member access operator redefined to point to ibis::qExpr.
    ibis::qExpr* operator->() {return expr_;}
    /// Member access operator redefined to point to const ibis::qExpr.
    const ibis::qExpr* operator->() const {return expr_;}

    /// Assignment operator.
    whereClause& operator=(const whereClause&);
    /// Swap the contents of two where clauses.
    void swap(whereClause& rhs) throw () {
	clause_.swap(rhs.clause_);
	ibis::qExpr* tmp = rhs.expr_;
	rhs.expr_ = expr_;
	expr_ = tmp;
    }

    static int verifyExpr(const ibis::qExpr*, const ibis::part&,
			  const ibis::selectClause *);
    static int verifyExpr(ibis::qExpr*&, const ibis::part&,
			  const ibis::selectClause *);
    static int removeAlias(ibis::qContinuousRange*&, const ibis::column*);

protected:
    std::string clause_;	///!< String version of the where clause.
    ibis::qExpr *expr_;		///!< The expression tree.

    void amplify(const ibis::part&);

private:
    ibis::whereLexer *lexer;	// hold a pointer for the parser

    friend class ibis::whereParser;
}; // class ibis::whereClause

namespace std {
    inline ostream& operator<<(ostream& out, const ibis::whereClause& wc) {
        if (!wc.empty())
            wc->print(out);
	return out;
    } // std::operator<<
}
#endif
