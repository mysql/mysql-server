// $Id$
// Author: John Wu <John.Wu at acm.org>
//      Lawrence Berkeley National Laboratory
// Copyright (c) 2009-2016 the Regents of the University of California
/** @file
    Declares ibis::fromClause class.
*/
#ifndef IBIS_FROMCLAUSE_H
#define IBIS_FROMCLAUSE_H
#include "qExpr.h"
#include "table.h"

namespace ibis {
    class fromLexer;
    class fromParser;
}

/// A class to represent the from clause.  It parses a string into a list
/// of names, a list of aliases and a join expression if present.
///
/// The alias may optionally be preceded by the keyword "AS".  For example,
/// "FROM table_a as a" and "FROM table_a a" mean the same thing.
///
/// The join expression is in the from clause can be of the form
/// @code
/// table_a JOIN table_b USING join_column
/// @endcode
/// or
/// @code
/// table_a JOIN table_b ON arithmetic_expression
/// @endcode
///
/// Note that the first form is equivalent to
/// @code
/// table_a JOIN table_b ON table_a.join_column = table_b.join_column
/// @endcode
///
/// @warning The current version of FastBit only supports join operating on
/// two data tables.
class FASTBIT_CXX_DLLSPEC ibis::fromClause {
public:
    /// Parse a new string as a from clause.
    explicit fromClause(const char *cl=0);
    /// Parse a list of strings.
    fromClause(const ibis::table::stringArray&);
    ~fromClause();
    fromClause(const fromClause&);

    int parse(const char *cl);

    /// Return a pointer to the string form of the from clause.
    const char* getString(void) const {return clause_.c_str();}
    /// Dereferences to the string form of the from clause.
    const char* operator*(void) const {return clause_.c_str();}

    /// Is it empty?  Returns true or false.
    bool empty() const {return names_.empty();}
    /// Returns the number of valid names.
    uint32_t size() const {return names_.size();}
    void getNames(ibis::table::stringArray&) const;

    /// Report the join condition.
    const ibis::compRange* getJoinCondition() const {return jcond_;}

    void print(std::ostream&) const;
    void clear();

    const char* realName(const char*) const;
    const char* alias(const char*) const;
    size_t position(const char*) const;
    void reorderNames(const char*, const char*);

    /// Assignment operator.
    fromClause& operator=(const fromClause& rhs) {
	fromClause tmp(rhs);
	swap(tmp);
	return *this;
    }
    /// Swap the content of two from clauses.
    void swap(fromClause& rhs) {
	names_.swap(rhs.names_);
	aliases_.swap(rhs.aliases_);
	clause_.swap(rhs.clause_);
	ordered_.swap(rhs.ordered_);
	ibis::compRange *tmp = jcond_;
	jcond_ = rhs.jcond_;
	rhs.jcond_ = tmp;
    }

protected:
    /// The names of data tables.
    std::vector<std::string> names_;
    /// The aliases.
    std::vector<std::string> aliases_;
    /// The ordered version of the names.
    std::map<const char*, size_t, ibis::lessi> ordered_;
    /// The join condition.  A join condition may be specified in the from
    /// clause with the key words ON and USING, where On is followed by an
    /// arithmetic expression and USING is followed by a column name.  The
    /// parser will translate the column name following USING into an
    /// eqaulity condition for generate a proper ibis::compRange object.
    /// If this variable is a nil pointer, no join condition has been
    /// specified in the from clause.
    ibis::compRange* jcond_;

    std::string clause_;	///!< String version of the from clause.
    ibis::fromLexer *lexer;	///!< A pointer for the parser.

    friend class ibis::fromParser;
}; // class ibis::fromClause

namespace std {
    inline ostream& operator<<(ostream& out, const ibis::fromClause& fc) {
	fc.print(out);
	return out;
    } // std::operator<<
}
#endif
