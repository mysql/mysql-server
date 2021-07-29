// File: $Id$
// Author: John Wu <John.Wu at acm.org>
//      Lawrence Berkeley National Laboratory
// Copyright (c) 1998-2016 the Regents of the University of California
//
// Primary contact: John Wu <John.Wu at acm.org>
#ifndef IBIS_EXPR_H
#define IBIS_EXPR_H
///@file
/// Define the query expression.
///
#include "util.h"
#include "array_t.h"

namespace ibis { // additional names related to qExpr
    class qRange;	///!< A simple range defined on a single attribute.
    class qContinuousRange;///!< A range defined with one or two boundaries.
    class qDiscreteRange;	///!< A range defined with discrete values.
    class qString;	///!< An equality expression with a string literal.
    class qAnyString;	///!< A range condition involving multiple strings.
    class qKeyword;	///!< A keyword search
    class qAllWords;	///!< A range condition involving multiple strings.
    class compRange;	///!< A comparisons involving arithmetic expression.
    class deprecatedJoin;	///!< A deprecated range join operations.
    class qAnyAny;	///!< A special form of any-match-any query.
    class qLike;	///!< A representation of the operator LIKE.
    class qIntHod;	///!< A container of signed integers.
    class qUIntHod;	///!< A container of unsigned integers.
    class qExists;	///!< A test for existence of a name.
}

/// @ingroup FastBitIBIS
/// The top level query expression object.  It encodes the logical
/// operations between two child expressions, serving as the interior nodes
/// of an expression tree.  Leaf nodes are going to be derived later.
class FASTBIT_CXX_DLLSPEC ibis::qExpr {
public:
    /// Definition of node types.  Logical operators are listed in the
    /// front and leaf node types are listed at the end.
    enum TYPE {
	LOGICAL_UNDEFINED, LOGICAL_NOT, LOGICAL_AND, LOGICAL_OR, LOGICAL_XOR,
	LOGICAL_MINUS, RANGE, DRANGE, STRING, ANYSTRING, KEYWORD, ALLWORDS,
	COMPRANGE, MATHTERM, DEPRECATEDJOIN, TOPK, EXISTS, ANYANY, LIKE,
        INTHOD, UINTHOD
    };
    /// Comparison operator supported in RANGE.
    enum COMPARE {
	OP_UNDEFINED, OP_LT, OP_GT, OP_LE, OP_GE, OP_EQ
    };

    /// Default constructor.  It generates a node of undefined type.
    qExpr() : type(LOGICAL_UNDEFINED), left(0), right(0) {}

    /// Construct a node of specified type.  Not for implicit type conversion.
    explicit qExpr(TYPE op) : type(op), left(0), right(0) {}
    /// Construct a full specified node.  The ownership of qe1 and qe2 is
    /// transferred to the newly created object.  The destructor of this
    /// object will delete qe1 and qe2, the user shall not delete them
    /// directly or indirectly through the destruction of another object.
    /// This constructor is primarily used by the parsers that compose
    /// query expressions, where the final query expression tree is passed
    /// to the user.  This design allows the user to take control of the
    /// final query expression without directly managing any of the lower
    /// level nodes in the expression tree.
    qExpr(TYPE op, qExpr* qe1, qExpr* qe2) : type(op), left(qe1), right(qe2) {}
    /// Copy Constructor.  Deep copy.
    qExpr(const qExpr& qe) : type(qe.type),
			     left(qe.left ? qe.left->dup() : 0),
			     right(qe.right ? qe.right->dup() : 0) {}
    /// Destructor.  It recursively deletes the nodes of an expression
    /// tree.  In other word, it owns the descendents it points to.
    virtual ~qExpr() {delete right; delete left;}

    /// Change the left child.  This object takes the ownership of expr.
    /// The user can not delete expr either directly or indirectly through
    /// the destruction of another object (other than this one, of course).
    void setLeft(qExpr *expr) {delete left; left=expr;}
    /// Change the right child.  This object takes the ownership of expr.
    /// The user can not delete expr either directly or indirectly through
    /// the destruction of another object (other than this one, of course).
    void setRight(qExpr *expr) {delete right; right=expr;}
    /// Return a pointer to the left child.  The pointer can be modified.
    /// The object assigned to be the new left child is owned by this object.
    qExpr*& getLeft() {return left;}
    /// Return a pointer to the right child.  The pointer can be modified.
    /// The object assigned to be the new right child of this expression is
    /// owned by this object after the assignment.
    qExpr*& getRight() {return right;}

    /// Return the node type.
    TYPE getType() const {return type;}
    /// Return a const pointer to the left child.
    const qExpr* getLeft() const {return left;}
    /// Return a const pointer to the right child.
    const qExpr* getRight() const {return right;}

    /// Swap the content.  No exception expected.
    void swap(qExpr& rhs) {
	{TYPE t = type; type = rhs.type; rhs.type = t;}
	{qExpr* p = left; left = rhs.left; rhs.left = p;}
	{qExpr* q = right; right = rhs.right; rhs.right = q;}
    }

    /// Assignment operator.
    qExpr& operator=(const qExpr& rhs) {
	qExpr tmp(rhs); // copy
	swap(tmp);
	return *this;
    }

    /// Count the number of items in the query expression.
    virtual uint32_t nItems() const {
	return 1 + (left != 0 ? left->nItems() : 0) +
	    (right != 0 ? right->nItems() : 0);}

    /// Print out the node in the string form.
    virtual void print(std::ostream&) const;
    /// Print out the full expression.
    virtual void printFull(std::ostream& out) const;

    /// A functor to be used by the function reorder.
    struct weight {
	virtual double operator()(const qExpr* ex) const = 0;
	virtual ~weight() {};
    };
    double reorder(const weight&); ///!< Reorder the expressions tree.
    /// Duplicate this query expression.  Return the pointer to the new object.
    virtual qExpr* dup() const {
	qExpr* res = new qExpr(type);
	if (left)
	    res->left = left->dup();
	if (right)
	    res->right = right->dup();
	return res;
    }

    /// Is this expression a constant?  A constant remains the same not
    /// matter which row it is applied to.
    virtual bool isConstant() const {return false;}
    /// Is this expression a terminal node of an expression tree?
    bool isTerminal() const {return (left==0 && right==0);}
    /// Can the expression be directly evaluated?
    bool directEval() const
    {return (type==RANGE || type==STRING || type==COMPRANGE ||
	     type==DRANGE || type==ANYSTRING || type==ANYANY ||
	     type==INTHOD || type==UINTHOD || type==EXISTS ||
             type==KEYWORD || type==ALLWORDS || type==LIKE ||
             type==LOGICAL_UNDEFINED || type==TOPK || type==ANYANY ||
	     (type==LOGICAL_NOT && left && left->directEval()));}

    /// Is the expression simple? A simple expression contains only range
    /// conditions connected with logical operators.
    virtual bool isSimple() const {
	if (left) {
	    if (right) return left->isSimple() && right->isSimple();
	    else return left->isSimple();
	}
	else if (right) {
	    return right->isSimple();
	}
	else { // the derived classes essentially overrides this case.
	    return true;
	}
    }

    /// Separate an expression tree into two connected with an AND operator.
    int separateSimple(ibis::qExpr *&simple, ibis::qExpr *&tail) const;
    void extractDeprecatedJoins(std::vector<const deprecatedJoin*>&) const;
    /// Identify the data partitions involved in the query expression.
    virtual void getTableNames(std::set<std::string>& plist) const;

    /// Find the first range condition involving the named variable.
    qRange* findRange(const char* vname);

    /// Attempt to simplify the query expressions.
    static void simplify(ibis::qExpr*&);

    static std::string extractTableName(const char*);
    static void splitColumnName(const char*, std::string&, std::string&);

    /// A data structure including a query expression and the list of table
    /// names mentioned in the expression.
    struct TTN {
	const qExpr* term;
	std::set<std::string> tnames;
    }; // TTN
    typedef std::vector<TTN> termTableList;
    void getConjunctiveTerms(termTableList&) const;

protected:
    /// The type of node. It indicates the type of the operator or the leaf
    /// node.
    TYPE   type;
    /// The left child.
    qExpr* left;
    /// The right child.
    qExpr* right;

    /// Adjust the tree to favor the sequential evaluation order.
    void adjust();
}; // ibis::qExpr

/// A class to represent simple range conditions.  This is an abstract base
/// class for qContinuousRange and qDiscreteRange.  The main virtual
/// functions, colName and inRange are used by procedures that evaluate the
/// conditions.
class FASTBIT_CXX_DLLSPEC ibis::qRange : public ibis::qExpr {
public:
    /// Returns the name of the attribute involved.
    virtual const char* colName() const = 0;
    /// Given a value, determine whether it is in the range defined.
    /// Return true if it is, return false, otherwise.
    virtual bool inRange(double val) const = 0;

    /// Reduce the range to be no more than [left, right].
    virtual void restrictRange(double left, double right) = 0;
    /// The lower bound of the range.
    virtual double leftBound() const = 0;
    /// The upper bound of the range.
    virtual double rightBound() const = 0;
    /// Is the current range empty?
    virtual bool empty() const = 0;
    virtual void getTableNames(std::set<std::string>& plist) const;

    virtual ~qRange() {}; // nothing to do

protected:
    // reduce the scope of the constructor
    qRange() : qExpr() {};
    qRange(TYPE t) : qExpr(t) {};

private:
#pragma GCC diagnostic ignored "-Wextra"
    qRange(const qRange&) {}; // no copy constructor allowed, must use dup
#pragma GCC diagnostic warning "-Wextra"
    qRange& operator=(const qRange&);
}; // ibis::qRange

/// Simple range condition.  It is implemented as a derived class of qExpr.
/// Possible range operator are defined in ibis::qExpr::COMPARE.  It is
/// designed to expression equality conditions, one-sided range conditions
/// and two-sided range conditions.
/// @verbatim
///
/// /* an equality expression */
/// ibis::qExpr *expr = new ibis::qContinuousRange("a", ibis::qExpr::OP_EQ, 5.0);
/// /* a one-sided range expression */
/// ibis::qExpr *expr = new ibis::qContinuousRange("a", ibis::qExpr::OP_GE, 1.3);
/// /* a two-sided range expression */
/// ibis::qExpr *expr = new ibis::qContinuousRange(3.6, ibis::qExpr::OP_LE,
/// "a", ibis::qExpr::OP_LT, 4.7);
///
/// @endverbatim
class FASTBIT_CXX_DLLSPEC ibis::qContinuousRange : public ibis::qRange {
public:
    /// Construct an empty range expression.
    qContinuousRange()
	: qRange(ibis::qExpr::RANGE), name(0), lower(0), upper(0),
	  left_op(OP_UNDEFINED), right_op(OP_UNDEFINED) {};
    /// Construct a range expression from strings.
    qContinuousRange(const char* lstr, COMPARE lop, const char* prop,
		     COMPARE rop, const char* rstr);
    /// Construct a range expression with an integer boundary.
    qContinuousRange(const char* col, COMPARE op, uint32_t val) :
	qRange(ibis::qExpr::RANGE), name(ibis::util::strnewdup(col)),
	lower(DBL_MAX), upper(val), left_op(OP_UNDEFINED), right_op(op) {};
    /// Copy constructor.
    qContinuousRange(const qContinuousRange& rhs) :
	qRange(ibis::qExpr::RANGE), name(ibis::util::strnewdup(rhs.name)),
	lower(rhs.lower), upper(rhs.upper), left_op(rhs.left_op),
	right_op(rhs.right_op) {};
    /// Construct a range expression from double-precision boundaries.
    qContinuousRange(double lv, COMPARE lop, const char* prop,
		     COMPARE rop, double rv)
	: qRange(ibis::qExpr::RANGE), name(ibis::util::strnewdup(prop)),
	  lower(lv), upper(rv), left_op(lop), right_op(rop) {};
    /// Construct a one-side range expression.
    qContinuousRange(const char* prop, COMPARE op, double val)
	: qRange(ibis::qExpr::RANGE), name(ibis::util::strnewdup(prop)),
	  lower(-DBL_MAX), upper(val), left_op(OP_UNDEFINED), right_op(op) {
	// prefer to use the operator < and <= rather than > and >=
	if (right_op == ibis::qExpr::OP_GT) {
	    right_op = ibis::qExpr::OP_UNDEFINED;
	    left_op = ibis::qExpr::OP_LT;
	    lower = upper;
	    upper = DBL_MAX;
	}
	else if (right_op == ibis::qExpr::OP_GE) {
	    right_op = ibis::qExpr::OP_UNDEFINED;
	    left_op = ibis::qExpr::OP_LE;
	    lower = upper;
	    upper = DBL_MAX;
	}
    };

    virtual ~qContinuousRange() {delete [] name;}

    // provide read access to all private variables
    virtual const char *colName() const {return name;}
    COMPARE leftOperator() const {return left_op;}
    COMPARE rightOperator() const {return right_op;}
    virtual double leftBound() const {return lower;}
    virtual double rightBound() const {return upper;}
    // allow one to possibly change the left and right bounds, the left and
    // right operator
    double& leftBound() {return lower;}
    double& rightBound() {return upper;}
    COMPARE& leftOperator() {return left_op;}
    COMPARE& rightOperator() {return right_op;}

    // Fold the boundaries to integers.
    void foldBoundaries();
    // Fold the boundaries to unsigned integers.
    void foldUnsignedBoundaries();

    /// Duplicate *this.
    virtual qContinuousRange* dup() const {return new qContinuousRange(*this);}
    virtual bool inRange(double val) const;
    virtual void restrictRange(double left, double right);
    virtual bool empty() const;

    virtual void print(std::ostream&) const;
    virtual void printFull(std::ostream& out) const;

    bool overlap(double, double) const;
    inline bool operator<(const qContinuousRange& y) const;

private:
    char* name;
    double lower, upper;
    COMPARE left_op, right_op;

    qContinuousRange& operator=(const qContinuousRange&);
    friend void ibis::qExpr::simplify(ibis::qExpr*&);
}; // ibis::qContinuousRange

/// A discrete range expression.  It is used to capture expression of the
/// form "A in (aaa, bbb, ccc, ...)."
class FASTBIT_CXX_DLLSPEC ibis::qDiscreteRange : public ibis::qRange {
public:
    /// Construct an empty dicrete range expression.
    qDiscreteRange() : qRange(DRANGE) {};
    qDiscreteRange(const char *col, const char *nums);
    qDiscreteRange(const char *col, const std::vector<uint32_t>& val);
    qDiscreteRange(const char *col, const std::vector<double>& val);
    qDiscreteRange(const char *col, ibis::array_t<uint32_t>& val);
    qDiscreteRange(const char *col, ibis::array_t<double>& val);

    /// Copy constructor.
    qDiscreteRange(const qDiscreteRange& dr)
	: qRange(DRANGE), name(dr.name), values(dr.values) {}
    virtual ~qDiscreteRange() {}; // private variables automatically destructs

    /// Name of the column.
    virtual const char* colName() const {return name.c_str();}
    /// Reference to the values.
    const ibis::array_t<double>& getValues() const {return values;}
    /// Reference to the values.
    ibis::array_t<double>& getValues() {return values;}

    /// Duplicate thy self.
    virtual qDiscreteRange* dup() const {return new qDiscreteRange(*this);}
    virtual bool inRange(double val) const;
    virtual void restrictRange(double left, double right);
    virtual bool empty() const {return values.empty();}
    virtual double leftBound() const {
	return (values.empty() ? DBL_MAX : values.front());}
    virtual double rightBound() const {
	return (values.empty() ? -DBL_MAX : values.back());}
    virtual uint32_t nItems() const {return values.size();}

    ibis::qExpr* convert() const;

    bool overlap(double, double) const;

    virtual void print(std::ostream&) const;
    virtual void printFull(std::ostream& out) const {print(out);}

private:
    std::string name; ///!< Column name.
    ibis::array_t<double> values; ///!< Values are sorted in ascending order.

    qDiscreteRange& operator=(const qDiscreteRange&);
}; // ibis::qDiscreteRange

/// This query expression has similar meaning as ibis::qDiscreteRange,
/// however, it stores the values as signed 64-bit integers.  This is
/// primarily useful for matching 64-bit integers.
///
/// @note About the name: hod is a portable trough for carrying mortar,
/// bricks, and so on.  We use it here as a short-hand for container.
/// Since this class is not meant for user by others, this is a suitable
/// obscure name for it.
class FASTBIT_CXX_DLLSPEC ibis::qIntHod : public ibis::qRange {
public:
    /// Default constructor.
    qIntHod() : qRange(INTHOD) {};
    qIntHod(const char* col, int64_t v1);
    qIntHod(const char* col, int64_t v1, int64_t v2);
    qIntHod(const char* col, const char* nums);
    qIntHod(const char* col, const std::vector<int64_t>& nums);
    qIntHod(const char* col, const ibis::array_t<int64_t>& nums);

    /// Copy constructor.
    qIntHod(const qIntHod& ih)
	: qRange(INTHOD), name(ih.name), values(ih.values) {};

    /// Destructor.
    virtual ~qIntHod() {};

    /// Name of the column involved.
    const char* colName() const {return name.c_str();}
    /// Reference to the values.
    const ibis::array_t<int64_t>& getValues() const {return values;}
    /// Reference to the values.
    ibis::array_t<int64_t>& getValues() {return values;}

    virtual bool inRange(double val) const;
    virtual bool inRange(int64_t val) const;
    virtual void restrictRange(double, double);
    virtual double leftBound() const {
	return (values.empty() ? DBL_MAX : values.front());}
    virtual double rightBound() const {
	return (values.empty() ? -DBL_MAX : values.back());}
    virtual bool empty() const {return values.empty();}
    /// Duplicate thy self.
    virtual qIntHod* dup() const {return new qIntHod(*this);}
    virtual uint32_t nItems() const {return values.size();}

    virtual void print(std::ostream&) const;
    virtual void printFull(std::ostream&) const;

private:
    /// Name of the column to be compared.
    std::string name;
    /// Values to be compared.  The constructor of this class shall sort
    /// the values in ascending order.
    ibis::array_t<int64_t> values;
}; // ibis::qIntHod

/// This query expression has similar meaning as ibis::qDiscreteRange,
/// however, it stores the values as unsigned 64-bit integers.  This is
/// primarily useful for matching 64-bit integers.
///
/// @note About the name: hod is a portable trough for carrying mortar,
/// bricks, and so on.  We use it here as a short-hand for container.
/// Since this class is not meant for user by others, this is a suitable
/// obscure name for it.
class FASTBIT_CXX_DLLSPEC ibis::qUIntHod : public ibis::qRange {
public:
    /// Default constructor.
    qUIntHod() : qRange(UINTHOD) {};
    qUIntHod(const char* col, uint64_t v1);
    qUIntHod(const char* col, uint64_t v1, uint64_t v2);
    qUIntHod(const char* col, const char* nums);
    qUIntHod(const char* col, const std::vector<uint64_t>& nums);
    qUIntHod(const char* col, const ibis::array_t<uint64_t>& nums);

    /// Copy constructor.
    qUIntHod(const qUIntHod& ih)
	: qRange(UINTHOD), name(ih.name), values(ih.values) {};

    /// Destructor.
    virtual ~qUIntHod() {};

    /// Name of the column involved.
    const char* colName() const {return name.c_str();}
    /// Reference to the values.
    const ibis::array_t<uint64_t>& getValues() const {return values;}
    /// Reference to the values.
    ibis::array_t<uint64_t>& getValues() {return values;}

    virtual bool inRange(double val) const;
    virtual bool inRange(uint64_t val) const;
    virtual void restrictRange(double, double);
    virtual double leftBound() const {
	return (values.empty() ? DBL_MAX : values.front());}
    virtual double rightBound() const {
	return (values.empty() ? -DBL_MAX : values.back());}
    virtual bool empty() const {return values.empty();}
    /// Duplicate thy self.
    virtual qUIntHod* dup() const {return new qUIntHod(*this);}
    virtual uint32_t nItems() const {return values.size();}

    virtual void print(std::ostream&) const;
    virtual void printFull(std::ostream&) const;

private:
    /// Name of the column to be compared.
    std::string name;
    /// Values to be compared.  The constructor of this class shall sort
    /// the values in ascending order.
    ibis::array_t<uint64_t> values;
}; // ibis::qUIntHod

/// The class qString encapsulates information for comparing string values.
/// Only equality comparison is supported at this point.  It does not
/// ensure the names are valid in any way.  When the check does happen,
/// the left side will be checked first.  If it matches the name of a
/// ibis::column, the right side will be assumed to be the value one is
/// trying to match.  If the left side does not match any know column name,
/// but the right side does, the right side will be assumed to name of
/// column to be searched and the left side will be the value to search
/// against.  If neither matches the name of any column, the expression
/// will evaluate to NULL (i.e., no hit).
class FASTBIT_CXX_DLLSPEC ibis::qString : public ibis::qExpr {
public:
    // construct the qString from two strings
    qString() : qExpr(STRING), lstr(0), rstr(0) {};
    qString(const char* ls, const char* rs);
    virtual ~qString() {delete [] rstr; delete [] lstr;}

    const char* leftString() const {return lstr;}
    const char* rightString() const {return rstr;}
    void swapLeftRight() {char* tmp = lstr; lstr = rstr; rstr = tmp;}

    virtual qString* dup() const {return new qString(*this);}
    virtual void print(std::ostream&) const;
    virtual void printFull(std::ostream& out) const {print(out);}
    virtual void getTableNames(std::set<std::string>& plist) const;

private:
    char* lstr;
    char* rstr;

    /// Copy Constructor.  Deep copy.
    qString(const qString& rhs) : qExpr(STRING),
	lstr(ibis::util::strnewdup(rhs.lstr)),
	rstr(ibis::util::strnewdup(rhs.rstr)) {}
    qString& operator=(const qString&);
}; // ibis::qString

/// This data structure holds a single name.  Note that the name in this
/// function is not checked against the list of the known variables.
/// Furthermore, when this expression appears as the left side of binary
/// operator, the names on the right-hand side of the expression is not
/// checked either.
class FASTBIT_CXX_DLLSPEC ibis::qExists : public ibis::qExpr {
public:
    qExists() : qExpr(EXISTS) {};
    qExists(const char *col) : qExpr(EXISTS), name(col) {};
    virtual ~qExists() {}; // name is automatically destroyed

    /// Duplicate the object.
    virtual qExists* dup() const {return new qExists(name.c_str());}
    virtual void print(std::ostream& out) const;
    virtual void printFull(std::ostream& out) const;
    virtual bool isSimple() const {
        return true;
    }

    /// Return the column name.
    const char* colName() const {return name.c_str();}

private:
    std::string name;
}; // ibis::qExists

/// The column contains one of the values in a list.  A data structure to
/// hold the string-valued version of the IN expression, name IN ('aaa',
/// 'bbb', ...).
class FASTBIT_CXX_DLLSPEC ibis::qAnyString : public ibis::qExpr {
public:
    qAnyString() : qExpr(ANYSTRING) {};
    qAnyString(const char *col, const char *sval);
    virtual ~qAnyString() {}; // name and values are automatically destroyed

    /// Duplicate the object.  Using the compiler generated copy constructor.
    virtual qAnyString* dup() const {return new qAnyString(*this);}
    virtual void print(std::ostream& out) const;
    virtual void printFull(std::ostream& out) const {print(out);}

    /// Return the column name, the left hand side of the IN operator.
    const char* colName() const {return name.c_str();}
    /// Return the string values in the parentheses as a vector.
    const std::vector<std::string>& valueList() const {return values;}
    /// Convert into a sequence of qString objects.
    ibis::qExpr* convert() const;
    virtual void getTableNames(std::set<std::string>& plist) const;

private:
    std::string name;
    std::vector<std::string> values;
}; // ibis::qAnyString

/// Representing the operator 'LIKE'.
class FASTBIT_CXX_DLLSPEC ibis::qLike : public ibis::qExpr {
public:
    /// Default constructor.
    qLike() : qExpr(LIKE), lstr(0), rpat(0) {};
    qLike(const char* ls, const char* rs);
    /// Destructor.
    virtual ~qLike() {delete [] rpat; delete [] lstr;}

    /// Name of the column to be searched.
    const char* colName() const {return lstr;}
    /// The string form of the pattern.
    const char* pattern() const {return rpat;}

    virtual qLike* dup() const {return new qLike(*this);}
    virtual void print(std::ostream&) const;
    virtual void printFull(std::ostream& out) const {print(out);}
    virtual void getTableNames(std::set<std::string>& plist) const;

private:
    /// Column name.
    char* lstr;
    /// Pattern
    char* rpat;

    /// Copy constructor.  Deep copy.
    qLike(const qLike& rhs)
	: qExpr(LIKE), lstr(ibis::util::strnewdup(rhs.lstr)),
	rpat(ibis::util::strnewdup(rhs.rpat)) {}
    qLike& operator=(const qLike&);
}; // ibis::qLike

/// The class qKeyword encapsulates a search for a single keyword in a text
/// field.  Only exact match is supported at this point.  This is similar
/// to the transct-SQL CONTAINS command, but does not accept nearly as many
/// options.  In addition, the keyword CONTAINS is used like a binary
/// operator as the keyword IN, e.g., <colunm-name> CONTAINS "keyword".
class FASTBIT_CXX_DLLSPEC ibis::qKeyword : public ibis::qExpr {
public:
    // construct the qKeyword from two strings
    qKeyword() : qExpr(KEYWORD), name(0), kword(0) {};
    qKeyword(const char* ls, const char* rs);
    virtual ~qKeyword() {delete [] kword; delete [] name;}

    /// Return the column name.  This is the first argument inside the
    /// CONTAINS operator.
    const char* colName() const {return name;}
    /// Return the keyword looking for.  This is the second argument inside
    /// the CONTAINS operator.
    const char* keyword() const {return kword;}

    virtual qKeyword* dup() const {return new qKeyword(*this);}
    virtual void print(std::ostream&) const;
    virtual void printFull(std::ostream& out) const {print(out);}
    virtual void getTableNames(std::set<std::string>& plist) const;

private:
    char* name;
    char* kword;

    /// Copy Constructor.  Deep copy.
    qKeyword(const qKeyword& rhs)
	: qExpr(KEYWORD), name(ibis::util::strnewdup(rhs.name)),
	kword(ibis::util::strnewdup(rhs.kword)) {}
    qKeyword& operator=(const qKeyword&);
}; // ibis::qKeyword

/// The class qAllWords encapsulates a search for many keywords.  This
/// class records the information expressed by the expression
/// @verbatim
/// <column-name> CONTAINS ("keyword1", ... "keywordn")
/// @endverbatim
///
/// The named column must be a text column.  The row satisfying this
/// condition must contain all the keywords listed.
class FASTBIT_CXX_DLLSPEC ibis::qAllWords : public ibis::qExpr {
public:
    qAllWords() : qExpr(ALLWORDS) {};
    qAllWords(const char *, const char *);
    qAllWords(const char *, const char *, const char *);
    virtual ~qAllWords() {}; // name and values automatically destroyed

    /// Duplicate the object.  Using the compiler generated copy constructor.
    virtual qAllWords* dup() const {return new qAllWords(*this);}
    virtual void print(std::ostream& out) const;
    virtual void printFull(std::ostream& out) const {print(out);}

    /// Return the column name, the left hand side of the IN operator.
    const char* colName() const {return name.c_str();}
    /// Return the string values in the parentheses as a vector.
    const std::vector<std::string>& valueList() const {return values;}
    virtual void getTableNames(std::set<std::string>& plist) const;
    ibis::qExpr* convert() const;

private:
    std::string name;
    std::vector<std::string> values;
}; // ibis::qAllWords

namespace ibis {
    /// A namespace for arithmetic expressions.
    namespace math {
	/// Types of terms allowed in the mathematical expressions.
	enum TERM_TYPE {UNDEF_TERM, VARIABLE, NUMBER, STRING, OPERATOR,
			STDFUNCTION1, STDFUNCTION2,
			CUSTOMFUNCTION1, CUSTOMFUNCTION2,
			STRINGFUNCTION1, STRINGFUNCTION2};
	/// All supported arithmetic operators.  The word operador is
	/// Spainish for operator.
	enum OPERADOR {UNKNOWN=0, BITOR, BITAND, PLUS, MINUS, MULTIPLY,
		       DIVIDE, REMAINDER, NEGATE, POWER};
	/// Standard 1-argument functions.
	enum STDFUN1 {ACOS=0, ASIN, ATAN, CEIL, COS, COSH, EXP, FABS, FLOOR,
		      FREXP, LOG10, LOG, MODF, ROUND, SIN, SINH, SQRT, TAN,
		      TANH, TRUNC, IS_ZERO, IS_NONZERO};
	/// Standard 2-argument functions.
	enum STDFUN2 {ATAN2=0, FMOD, LDEXP, ROUND2, POW, IS_EQL, IS_GTE,
		      IS_LTE};

	/// String form of the operators.
	extern const char* operator_name[];
	/// String form of the one-argument standard functions.
	extern const char* stdfun1_name[];
	/// String form of the two-argument standard functions.
	extern const char* stdfun2_name[];
	/// Whether to keep arithmetic expression as user inputed them.
	/// - If it is true, FastBit will not consolidate constant
	///   expressions nor perform other simple optimizations.
	/// - If it is false, the software will attempt to minimize the
	///   number of operations needed to apply them on data records.
	///
	/// @note Keep the arithmetic expressions unaltered will preserve
	/// its round-off properties and produce exactly the same numeric
	/// results as one might expect.  However, this is normally not the
	/// most important consideration as the differences are typically
	/// quite small.  Therefore, the default value of this variable is
	/// false.
	extern bool preserveInputExpressions;

	/// The abstract base class for arithmetic terms.  All allowed
	/// arithmetic expressions in a compRange or a select expression
	/// are derived from this class.  Constant expressions are also
	/// allowed to represent where clauses that are always true or
	/// always false.
	class term : public ibis::qExpr {
	public:
	    virtual ~term() {};

	    virtual TERM_TYPE termType() const = 0;

	    /// Evaluate the term.
	    virtual double eval() const = 0;
	    /// Should the value be treated as true?  This implementation
	    /// captures the normal case, where an arithmetic expression is
	    /// treated as 'true' if it is not zero.
	    virtual bool isTrue() const {return(eval() != 0);}
	    /// Make a duplicate copy of the term.
	    virtual term* dup() const = 0;
	    /// Print a human readable version of the expression.
	    virtual void print(std::ostream& out) const = 0;
	    /// Same as print.
	    virtual void printFull(std::ostream& out) const {print(out);}
	    /// Shorten the expression by evaluating the constants.  Return
	    /// a new pointer if the expression is changed, otherwise
	    /// Returnn the pointer this.
	    virtual term* reduce() {return this;};

	protected:
	    term() : qExpr(MATHTERM) {}; // used by concrete derived classes
            term(const term &rhs) : qExpr(rhs) {}
	}; // abstract term

	/// A barrel to hold a list of variables.  It defines an interface
	/// for evaluating arbitrary arithmetic expressions.  It is also a
	/// dummy implementation that assigns all variables to have value
	/// 0.
	class barrel {
	public:
	    /// Constructor.
	    barrel() {};
	    /// Constructor.
	    barrel(const term* const t) {recordVariable(t);}
	    /// Destructor.  Member variables clean themselves.
	    virtual ~barrel() {};

	    // access functions to the names and values
	    uint32_t size() const {return varmap.size();}
	    const char* name(uint32_t i) const {return namelist[i];}
	    const double& value(uint32_t i) const {return varvalues[i];}
	    double& value(uint32_t i) {return varvalues[i];}

	    void recordVariable(const qExpr* const t);
	    void recordVariable(const term* const t);
	    uint32_t recordVariable(const char* name);
	    /// Is the given @c barrel of variables equivalent to this one?
	    bool equivalent(const barrel& rhs) const;

	protected:
	    // the data structure to store the variable names in a mathematical
	    // expression
	    typedef std::map< const char*, uint32_t, ibis::lessi > termMap;

	    // functions used by the class variable for accessing values of the
	    // variables
	    friend class variable;
	    double getValue(uint32_t i) const {return varvalues[i];}
	    /// Return the value of the named variable.
	    double getValue(const char* nm) const {
		termMap::const_iterator it = varmap.find(nm);
		if (it != varmap.end()) {
		    uint32_t i = (*it).second;
		    return varvalues[i];
		}
		else {
		    return DBL_MAX;
		}
	    }

	    /// Associate a variable name with a position in @c varvalues and
	    /// @c namelist.
	    termMap varmap;
	    /// Cast values to double.
	    std::vector< double > varvalues;
	    /// List of variable names.
	    std::vector< const char* > namelist;
	}; // class barrel

	/// A variable.
	class variable : public term {
	public:
	    // The constructor inserts the variable name to a list in expr and
	    // record the position in private member variable (that is used
	    // later to retrieve value from expr class).
	    variable(const char* var)
		: name(ibis::util::strnewdup(var)), myBar(0), varind(0) {}
#pragma GCC diagnostic ignored "-Wextra"
	    variable(const variable& v)
		: name(ibis::util::strnewdup(v.name)), decor(v.decor),
                  myBar(v.myBar), varind(v.varind) {}
#pragma GCC diagnostic warning "-Wextra"
	    virtual ~variable() {delete [] name;}

	    virtual TERM_TYPE termType() const {return VARIABLE;}
	    virtual variable* dup() const {return new variable(*this);}
	    virtual double eval() const {return myBar->getValue(varind);}
	    virtual void getTableNames(std::set<std::string>& plist) const;

	    virtual uint32_t nItems() const {return 1U;}
	    virtual void print(std::ostream& out) const {out << name;}
	    virtual void printFull(std::ostream& out) const;
	    const char* variableName() const {return name;}

	    void recordVariable(barrel& bar) const {
		if (name != 0 && *name != 0 && *name != '*') {
		    varind = bar.recordVariable(name);
		    myBar = &bar;
		}
	    }

            void addDecoration(const char *, const char*);
            const char* getDecoration() const {return decor.c_str();}

	protected:
	    char* name;	// the variable name
            std::string decor; // name=value pairs
	    mutable barrel* myBar;// the barrel containing it
	    mutable uint32_t varind;// the token to retrieve value from myBar

	private:
	    variable& operator=(const variable&);
	}; // the variable term

	/// A number.
	class number : public term {
	public:
	    number(const char* num) : val(atof(num)) {};
	    number(double v) : val(v) {};
	    virtual ~number() {};

	    virtual TERM_TYPE termType() const {return NUMBER;}
	    virtual number* dup() const {return new number(val);}
	    virtual double eval() const {return val;}

	    virtual uint32_t nItems() const {return 1U;}
	    virtual void print(std::ostream& out) const {out << val;}
	    virtual void printFull(std::ostream& out) const {out << val;}
	    virtual bool isConstant() const {return true;}
	    virtual bool isTrue() const {return(val != 0);}

	    /// To negate the value
	    void negate() {val = -val;}
	    /// To invert the value
	    void invert() {val = 1.0/val;}

	private:
	    double val;
	    friend class bediener;
	    friend void ibis::qExpr::simplify(ibis::qExpr*&);
	}; // number

	/// A string literal.
	class literal : public term {
	public:
	    literal(const char* s) : str(ibis::util::strnewdup(s)) {};
	    virtual ~literal() {delete [] str;}

	    virtual TERM_TYPE termType() const {return ibis::math::STRING;}
	    virtual literal* dup() const {return new literal(str);}
	    virtual double eval() const {return 0.0;}
	    virtual bool isConstant() const {return true;}
	    /// Should the string literal be interpreted as true?  A string
	    /// literal is interpretted as true if it starts with letter
	    /// 't' or 'T', or it equals to "1".
	    virtual bool isTrue() const {
		return(str != 0 && (*str == 't' || *str == 'T' ||
				    (*str == '1' && *(str+1) == 0)));}

	    virtual uint32_t nItems() const {return 1U;}
	    virtual void print(std::ostream& out) const {out << str;}
	    virtual void printFull(std::ostream& out) const {out << str;}
	    operator const char* () const {return str;}

	private:
	    char* str;

	    literal(const literal&);
	    literal& operator=(const literal&);
	}; // literal

	/// An operator.  Bediener is German for operator.
	class bediener : public term {
	public:
	    bediener(ibis::math::OPERADOR op) : operador(op) {};
	    virtual ~bediener() {};

	    virtual TERM_TYPE termType() const {return OPERATOR;}
	    virtual bediener* dup() const {
		bediener *tmp = new bediener(operador);
		if (getRight() != 0)
		    tmp->setRight(getRight()->dup());
		if (getLeft() != 0)
		    tmp->setLeft(getLeft()->dup());
		return tmp;
	    }
	    virtual double eval() const;
	    virtual void print(std::ostream& out) const;
	    virtual void printFull(std::ostream& out) const {print(out);}
	    virtual term* reduce();
	    OPERADOR getOperator() const {return operador;}

	private:
	    ibis::math::OPERADOR operador; // Spanish for operator

	    void reorder(); // reorder the tree of operators
	    // place the operands into the list of terms if the operator
	    // matches the specified one.
	    void linearize(const ibis::math::OPERADOR op,
			   std::vector<ibis::math::term*>& terms);
	    // If the right operand is a constant, change operator from - to +
	    // or from / to *.
	    void convertConstants();
	    friend void ibis::qExpr::simplify(ibis::qExpr*&);
	}; // bediener

	/// One-argument standard functions.
	class stdFunction1 : public term {
	public:
	    stdFunction1(const char* name);
	    stdFunction1(const STDFUN1 ft) : ftype(ft) {}
	    virtual ~stdFunction1() {}

	    virtual stdFunction1* dup() const {
		stdFunction1 *tmp = new stdFunction1(ftype);
		tmp->setLeft(getLeft()->dup());
		return tmp;
	    }
	    virtual TERM_TYPE termType() const {return STDFUNCTION1;}
	    virtual double eval() const;
	    virtual void print(std::ostream& out) const;
	    virtual void printFull(std::ostream& out) const {print(out);}
	    virtual term* reduce();

	private:
	    STDFUN1 ftype;
	}; // stdFunction1

	/// Two-argument standard functions.
	class stdFunction2 : public term {
	public:
	    stdFunction2(const char* name);
	    stdFunction2(const STDFUN2 ft) : ftype(ft) {}
	    virtual ~stdFunction2() {}

	    virtual stdFunction2* dup() const {
		stdFunction2 *tmp = new stdFunction2(ftype);
		tmp->setRight(getRight()->dup());
		tmp->setLeft(getLeft()->dup());
		return tmp;
	    }
	    virtual TERM_TYPE termType() const {return STDFUNCTION2;}
	    virtual double eval() const;
	    virtual void print(std::ostream& out) const;
	    virtual void printFull(std::ostream& out) const {print(out);}
	    virtual term* reduce();

	private:
	    STDFUN2 ftype;
	}; // stdFunction2

        /// Pure virtual base function for 1-argument functions.  It takes
        /// an argument in double and return a double value.  Note that the
        /// derived classes much support copy construction through the
        /// function dup.
        class func1 {
        public:
            virtual ~func1() {};
            /// Duplicate thyself.  Should follow deep-copy semantics.
            virtual func1* dup() const =0;
            /// Evaluate the function on the given argument.
            virtual double eval(double) const =0;
            /// Print the name of this function.
            virtual void printName(std::ostream&) const =0;
            /// Print the decoration on this function.
            virtual void printDecoration(std::ostream&) const =0;
        }; // func1

        /// Pure virtual base function for 1-argument functions.  It takes
        /// an argument in double and return a std::string object.  Note
        /// that the derived classes much support copy construction through
        /// the function dup.
        class sfunc1 {
        public:
            virtual ~sfunc1() {};
            /// Duplicate thyself.  Should follow deep-copy semantics.
            virtual sfunc1* dup() const =0;
            /// Evaluate the function on the given argument.
            virtual std::string eval(double) const =0;
            /// Print the name of this function.
            virtual void printName(std::ostream&) const =0;
            /// Print the decoration on this function.
            virtual void printDecoration(std::ostream&) const =0;
        }; // sfunc1

	/// One-argument custom functions.
	class customFunction1 : public term {
	public:
	    virtual ~customFunction1() {delete fun_;}
	    customFunction1(const func1 &ft)
                : fun_(ft.dup()) {}
	    #pragma GCC diagnostic ignored "-Wextra"
	    customFunction1(const customFunction1 &rhs)
                : fun_(rhs.fun_->dup()) {
                if (rhs.getLeft() != 0) {
                    setLeft(rhs.getLeft()->dup());
                }
                else if (rhs.getRight() != 0) {
                    setLeft(rhs.getRight()->dup());
                }
            }
	    #pragma GCC diagnostic warning "-Wextra"

	    virtual customFunction1* dup() const {
		return new customFunction1(*this);
	    }
	    virtual TERM_TYPE termType() const {return CUSTOMFUNCTION1;}
	    virtual double eval() const;
	    virtual void print(std::ostream& out) const;
	    virtual void printFull(std::ostream& out) const {print(out);}
	    virtual term* reduce() {return this;}

	private:
	    func1 *fun_;
	}; // customFunction1

        /// Functor for converting a unix time stamp into date-time format
        /// throught @c strftime.  Note that both incoming argument and
        /// output are treated as double precision floating-point values.
        ///
        /// @warning The conversioin only makes use of the leading porting
        /// of the string printed by @c strftime.  If the format string
        /// passed to @c strftime produces a string starting with letters
        /// (not numbers) then the resulting number will be NaN
        /// (non-a-number).
        class fromUnixTime : public func1 {
        public:
            virtual ~fromUnixTime() {}
            fromUnixTime(const char *f, const char *z=0)
                : fmt_(f), tzname_(z!=0?z:"") {}
            fromUnixTime(const fromUnixTime& rhs)
                : fmt_(rhs.fmt_), tzname_(rhs.tzname_) {}

            virtual fromUnixTime* dup() const {
                return new fromUnixTime(*this);
            }

            virtual double eval(double) const;
            virtual void printName(std::ostream&) const;
            virtual void printDecoration(std::ostream&) const;

        private:
            std::string fmt_;
            std::string tzname_;
        }; // fromUnixTime

        /// Functor to convert ISO 8601 style date time value to a unix
        /// time stamp.  The incoming value is expected to be in the format
        /// of YYYYMMDDhhmmss.  If the fractinal part of the incoming
        /// argument is not zero, this fraction is transfered to the return
        /// values as the fraction as well.  However, since @c strftime
        /// does not print the fraction of a second, therefore, the user
        /// will have deal with the fractions of a second themselves.
        class toUnixTime : public func1 {
        public:
            virtual ~toUnixTime() {}
            toUnixTime(const char *z=0) : tzname_(z!=0?z:"") {}
            toUnixTime(const toUnixTime& rhs) : tzname_(rhs.tzname_) {}

            virtual toUnixTime* dup() const {
                return new toUnixTime(*this);
            }

            virtual double eval(double) const;
            virtual void printName(std::ostream&) const;
            virtual void printDecoration(std::ostream&) const;

        private:
            std::string tzname_;
        }; // toUnixTime

	/// One-argument string functions.
	class stringFunction1 : public term {
	public:
	    virtual ~stringFunction1() {delete fun_;}
	    stringFunction1(const sfunc1 &ft)
                : fun_(ft.dup()) {}
	    #pragma GCC diagnostic ignored "-Wextra"
	    stringFunction1(const stringFunction1 &rhs)
                : fun_(rhs.fun_->dup()) {
                if (rhs.getLeft() != 0) {
                    setLeft(rhs.getLeft()->dup());
                }
                else if (rhs.getRight() != 0) {
                    setLeft(rhs.getRight()->dup());
                }
            }
	    #pragma GCC diagnostic warning "-Wextra"

	    virtual stringFunction1* dup() const {
		return new stringFunction1(*this);
	    }
	    virtual TERM_TYPE termType() const {return STRINGFUNCTION1;}
	    virtual double eval() const {return FASTBIT_DOUBLE_NULL;}
	    virtual std::string sval() const;
	    virtual void print(std::ostream& out) const;
	    virtual void printFull(std::ostream& out) const {print(out);}
	    virtual term* reduce() {return this;}

	private:
	    sfunc1 *fun_;
	}; // stringFunction1

        /// Format unix time stamps as strings through the function @c
        /// strftime and then output the leading portion as a
        /// floating-point number.
        class formatUnixTime : public sfunc1 {
        public:
            virtual ~formatUnixTime() {}
            formatUnixTime(const char *f, const char *z=0)
                : fmt_(f), tzname_(z!=0?z:"") {}
            formatUnixTime(const formatUnixTime& rhs)
                : fmt_(rhs.fmt_), tzname_(rhs.tzname_) {}

            virtual formatUnixTime* dup() const {
                return new formatUnixTime(*this);
            }

            virtual std::string eval(double) const;
            virtual void printName(std::ostream&) const;
            virtual void printDecoration(std::ostream&) const;

        private:
            std::string fmt_;
            std::string tzname_;
        }; // formatUnixTime
    } // namespace ibis::math
} // namespace ibis

/// The class compRange stores computed ranges.  It is for those
/// comparisons involving nontrivial arithmetic expression.
class FASTBIT_CXX_DLLSPEC ibis::compRange : public ibis::qExpr {
public:

    /// Default constructor.
    compRange() : qExpr(ibis::qExpr::COMPRANGE), expr3(0),
		  op12(ibis::qExpr::OP_UNDEFINED),
		  op23(ibis::qExpr::OP_UNDEFINED) {;}
    /// Constructor with two arithmetic expressions.  Takes the ownership
    /// of me1 and me2.
    compRange(ibis::math::term* me1, COMPARE lop,
	      ibis::math::term* me2)
	: qExpr(ibis::qExpr::COMPRANGE, me1, me2), expr3(0),
	  op12(lop), op23(ibis::qExpr::OP_UNDEFINED) {;}
    /// Constructor with three arithmetic expressions.  Takes the ownership
    /// of me1, me2, and me3.
    compRange(ibis::math::term* me1, ibis::qExpr::COMPARE lop,
	      ibis::math::term* me2, ibis::qExpr::COMPARE rop,
	      ibis::math::term* me3)
	: qExpr(ibis::qExpr::COMPRANGE, me1, me2), expr3(me3),
	  op12(lop), op23(rop) {;}
    /// Copy constructor.  Deep copy -- actually copy the math expressions.
    compRange(const compRange& rhs) :
	ibis::qExpr(rhs), expr3(rhs.expr3 ? rhs.expr3->dup() : 0),
	op12(rhs.op12), op23(rhs.op23) {};
    /// Destructor.
    virtual ~compRange() {delete expr3;}

    // provide read access to the operators
    ibis::qExpr::COMPARE leftOperator() const {return op12;}
    ibis::qExpr::COMPARE rightOperator() const {return op23;}
    ibis::math::term* getTerm3() {return expr3;}
    const ibis::math::term* getTerm3() const {return expr3;}
    void setTerm3(ibis::math::term* t) {delete expr3; expr3 = t;}

    /// Duplicate this object.  Return a pointer to the new copy.
    virtual qExpr* dup() const {return new compRange(*this);}
    /// Evaluate the logical expression.
    inline bool inRange() const;

    virtual uint32_t nItems() const {
	return ibis::qExpr::nItems() +
	    (expr3 != 0 ? expr3->nItems() : 0);}
    /// Print the query expression.
    virtual void print(std::ostream&) const;
    virtual void printFull(std::ostream& out) const {print(out);}
    virtual void getTableNames(std::set<std::string>& plist) const;

    virtual bool isConstant() const {
	return ((getLeft() != 0 ? getLeft()->isConstant() : true) &&
		(getRight() != 0 ? getRight()->isConstant() : true) &&
		(expr3 != 0 ? expr3->isConstant() : true));}
    virtual bool isSimple() const {return isSimpleRange();}
    /// Is this a simple range expression that can be stored as ibis::qRange?
    inline bool isSimpleRange() const;

    /// Is the expression possibly a simple string comparison?
    bool maybeStringCompare() const;

    // convert a simple expression to qContinuousRange
    ibis::qContinuousRange* simpleRange() const;
    static compRange* makeConstantFalse();
    static compRange* makeConstantTrue();

private:
    ibis::math::term *expr3;	// the right most expression
    ibis::qExpr::COMPARE op12;	// between qExpr::left and qExpr::right
    ibis::qExpr::COMPARE op23;	// between qExpr::right and expr3
}; // ibis::compRange

/// A join is defined by two names and a numerical expression.  If the
/// numerical expression is not specified, it is a standard equal-join,
/// 'name1 = name2'.  If the numerical expression is specified, it is a
/// range-join, 'name1 between name2 - expr and name2 + expr'.
class ibis::deprecatedJoin : public ibis::qExpr {
public:
    deprecatedJoin(const char* n1, const char *n2)
	: ibis::qExpr(ibis::qExpr::DEPRECATEDJOIN), name1(n1), name2(n2),
	  expr(0) {};
    deprecatedJoin(const char* n1, const char *n2, ibis::math::term *x) : 
	ibis::qExpr(ibis::qExpr::DEPRECATEDJOIN), name1(n1), name2(n2),
	expr(x) {};
    virtual ~deprecatedJoin() {delete expr;};

    virtual void print(std::ostream& out) const;
    virtual void printFull(std::ostream& out) const {print(out);}
    virtual deprecatedJoin* dup() const
    {return new deprecatedJoin(name1.c_str(), name2.c_str(), expr->dup());};

    const char* getName1() const {return name1.c_str();}
    const char* getName2() const {return name2.c_str();}
    ibis::math::term* getRange() {return expr;}
    const ibis::math::term* getRange() const {return expr;}
    void setRange(ibis::math::term *t) {delete expr; expr = t;}

    virtual uint32_t nItems() const {
	return ibis::qExpr::nItems() +
	    (expr != 0 ? expr->nItems() : 0);}

private:
    std::string name1;
    std::string name2;
    ibis::math::term *expr;

    deprecatedJoin(const deprecatedJoin&);
    deprecatedJoin& operator=(const deprecatedJoin&);
}; // class ibis::deprecatedJoin

/// A user specifies this type of query expression with the following
/// syntax,
/// @arg any(prefix) = value
/// @arg any(prefix) in (list of values)
/// If any column with the given prefix contains the specified values,
/// the row is considered as a hit.  This is intended to be used in the
/// cases where the @c prefix is actually the name of a set-valued
/// attribute, such as triggerID in STAR datasets.  In this case, the
/// set-valued attribute is translated into a number of columns with the
/// same prefix.  A common query is "does the set contain a particular
/// value?" or "does the set contain a particular set of values?"
class ibis::qAnyAny : public ibis::qExpr {
public:
    qAnyAny() : qExpr(ANYANY) {};
    qAnyAny(const char *pre, const double dbl);
    qAnyAny(const char *pre, const char *val);
    ~qAnyAny() {}; // all data members can delete themselves.

    const char* getPrefix() const {return prefix.c_str();}
    const ibis::array_t<double>& getValues() const {return values;}

    /// Duplicate this.  Use the compiler generated copy constructor to
    /// perform duplication.
    virtual qExpr* dup() const {return new qAnyAny(*this);}

    virtual void print(std::ostream& out) const;
    virtual void printFull(std::ostream& out) const {print(out);}
    virtual void getTableNames(std::set<std::string>& plist) const;

private:
    std::string prefix; ///!< The prefix of the column names to search.
    ibis::array_t<double> values; ///!< The list of values to match.
}; // class ibis::qAnyAny

inline void ibis::qContinuousRange::foldBoundaries() {
    switch (left_op) {
    case ibis::qExpr::OP_LT:
	lower = floor(lower);
	break;
    case ibis::qExpr::OP_LE:
	lower = ceil(lower);
	break;
    case ibis::qExpr::OP_GT:
	lower = ceil(lower);
	break;
    case ibis::qExpr::OP_GE:
	lower = floor(lower);
	break;
    case ibis::qExpr::OP_EQ:
	if (lower != floor(lower))
	    left_op = ibis::qExpr::OP_UNDEFINED;
	break;
    default:
	break;
    }
    switch (right_op) {
    case ibis::qExpr::OP_LT:
	upper = ceil(upper);
	break;
    case ibis::qExpr::OP_LE:
	upper = floor(upper);
	break;
    case ibis::qExpr::OP_GT:
	upper = floor(upper);
	break;
    case ibis::qExpr::OP_GE:
	upper = ceil(upper);
	break;
    case ibis::qExpr::OP_EQ:
	if (upper != floor(upper))
	    right_op = ibis::qExpr::OP_UNDEFINED;
	break;
    default:
	break;
    }
} //ibis::qContinuousRange::foldBoundaries

inline void ibis::qContinuousRange::foldUnsignedBoundaries() {
    switch (left_op) {
    case ibis::qExpr::OP_LT:
	if (lower >= 0.0) {
	    lower = floor(lower);
	}
	else {
	    left_op = ibis::qExpr::OP_LE;
	    lower = 0.0;
	}
	break;
    case ibis::qExpr::OP_LE:
	if (lower >= 0.0)
	    lower = ceil(lower);
	else
	    lower = 0.0;
	break;
    case ibis::qExpr::OP_GT:
	lower = ceil(lower);
	break;
    case ibis::qExpr::OP_GE:
	lower = floor(lower);
	break;
    case ibis::qExpr::OP_EQ:
	if (lower != floor(lower) || lower < 0.0)
	    left_op = ibis::qExpr::OP_UNDEFINED;
	break;
    default:
	break;
    }
    switch (right_op) {
    case ibis::qExpr::OP_LT:
	upper = ceil(upper);
	break;
    case ibis::qExpr::OP_LE:
	upper = floor(upper);
	break;
    case ibis::qExpr::OP_GT:
	if (upper > 0.0) {
	    upper = floor(upper);
	}
	else {
	    right_op = ibis::qExpr::OP_GE;
	    upper = 0.0;
	}
	break;
    case ibis::qExpr::OP_GE:
	if (upper >= 0.0)
	    upper = ceil(upper);
	else
	    upper = 0.0;
	break;
    case ibis::qExpr::OP_EQ:
	if (upper != floor(upper) || upper < 0.0)
	    right_op = ibis::qExpr::OP_UNDEFINED;
	break;
    default:
	break;
    }
} //ibis::qContinuousRange::foldUnsignedBoundaries

/// An operator for comparing two query expressions.
/// The comparison is based on the name first, then the left bound and then
/// the right bound.
inline bool 
ibis::qContinuousRange::operator<(const ibis::qContinuousRange& y) const {
    int cmp = strcmp(colName(), y.colName());
    if (cmp < 0)
	return true;
    else if (cmp > 0)
	return false;
    else if (left_op < y.left_op)
	return true;
    else if (left_op > y.left_op)
	return false;
    else if (right_op < y.right_op)
	return true;
    else if (right_op > y.right_op)
	return false;
    else if (lower < y.lower)
	return true;
    else if (lower > y.lower)
	return false;
    else if (upper < y.upper)
	return true;
    else
	return false;
} // ibis::qContinuousRange::operator<

inline bool ibis::compRange::isSimpleRange() const {
    bool res = false;
    if (expr3 == 0 && getLeft() != 0)
	res = ((static_cast<const ibis::math::term*>(getLeft())->
		termType()==ibis::math::VARIABLE &&
		static_cast<const ibis::math::term*>(getRight())->
		termType()==ibis::math::NUMBER) ||
	       (static_cast<const ibis::math::term*>(getLeft())->
		termType()==ibis::math::NUMBER &&
		static_cast<const ibis::math::term*>(getRight())->
		termType()==ibis::math::VARIABLE));
    else if (expr3 != 0 && expr3->termType()==ibis::math::NUMBER)
	res = (getLeft() == 0 &&
	       static_cast<const ibis::math::term*>(getRight())->termType()
	       == ibis::math::VARIABLE) || 
	    (static_cast<const ibis::math::term*>(getLeft())->
	     termType()==ibis::math::NUMBER &&
	     static_cast<const ibis::math::term*>(getRight())->
	     termType()==ibis::math::VARIABLE);
    return res;
} // ibis::compRange::isSimpleRange

inline bool ibis::compRange::maybeStringCompare() const {
    return (expr3 == 0 && op12==OP_EQ && getLeft() != 0 && getRight() != 0 &&
	    (static_cast<const ibis::math::term*>(getLeft())->termType()
	     ==ibis::math::VARIABLE ||
	     static_cast<const ibis::math::term*>(getLeft())->termType()
	     ==ibis::math::STRING) &&
	    (static_cast<const ibis::math::term*>(getRight())->termType()
	     ==ibis::math::VARIABLE ||
	     static_cast<const ibis::math::term*>(getRight())->termType()
	     ==ibis::math::STRING));
} // ibis::compRange::maybeStringCompare

/// Does the input value match any of the values on record ?
/// It returns false in case of error.
inline bool ibis::compRange::inRange() const {
    if (getRight() == 0) return false;

    const double tm2 =
	static_cast<const ibis::math::term*>(getRight())->eval();
    if (op12 == OP_UNDEFINED && op23 == OP_UNDEFINED)
	return (tm2 != 0.0);

    bool res = true;
    if (getLeft() != 0 && op12 != OP_UNDEFINED) {
	const double tm1 =
	    static_cast<const ibis::math::term*>(getLeft())->eval();
	switch (op12) {
	case OP_LT: res = (tm1 < tm2);  break;
	case OP_LE: res = (tm1 <= tm2); break;
	case OP_GT: res = (tm1 > tm2);  break;
	case OP_GE: res = (tm1 >= tm2); break;
	case OP_EQ: res = (tm1 == tm2); break;
	default:    break;
	}
    }
    if (expr3 != 0 && op23 != OP_UNDEFINED && res == true) {
	const double tm3 = expr3->eval();
	switch (op23) {
	case OP_LT: res = (tm2 < tm3);  break;
	case OP_LE: res = (tm2 <= tm3); break;
	case OP_GT: res = (tm2 > tm3);  break;
	case OP_GE: res = (tm2 >= tm3); break;
	case OP_EQ: res = (tm2 == tm3); break;
	default:    break;
	}
    }
    return res;
} // ibis::compRange::inRange

/// Is the argument @a val one of the values stored ?  Return true or
/// false.
/// It uses a binary search if there are more than 32 elements and uses
/// linear search otherwise.
inline bool ibis::qDiscreteRange::inRange(double val) const {
    if (values.empty()) return false;
    if (val < values[0] || val > values.back()) return false;

    uint32_t i = 0, j = values.size();
    if (j < 32) { // sequential search
	// -- because the heavy branch prediction cost, linear search is
	// more efficient for fairly large range.
	for (i = 0; i < j; ++ i)
	    if (values[i] == val) return true;
	return false;
    }
    else { // binary search
	uint32_t m = (i + j) / 2;
	while (i < m) {
	    if (values[m] == val) return true;
	    if (values[m] < val)
		i = m;
	    else
		j = m;
	    m = (i + j) / 2;
	}
	return (values[m] == val);
    }
} // ibis::qDiscreteRange::inRange

/// Is the argument @a val one of the values stored ?  Return true or
/// false.
/// It uses a binary search if there are more than 32 elements and uses
/// linear search otherwise.
inline bool ibis::qIntHod::inRange(double val) const {
    if (values.empty()) return false;
    if (val < values[0] || val > values.back()) return false;

    uint32_t i = 0, j = values.size();
    if (j < 32) { // sequential search
	// -- because the heavy branch prediction cost, linear search is
	// more efficient for fairly large range.
	for (i = 0; i < j; ++ i)
	    if (values[i] == val) return true;
	return false;
    }
    else { // binary search
	uint32_t m = (i + j) / 2;
	while (i < m) {
	    if (values[m] == val) return true;
	    if (values[m] < val)
		i = m;
	    else
		j = m;
	    m = (i + j) / 2;
	}
	return (values[m] == val);
    }
} // ibis::qIntHod::inRange

/// Is the argument @a val one of the values stored ?  Return true or
/// false.
/// It uses a binary search if there are more than 32 elements and uses
/// linear search otherwise.
inline bool ibis::qUIntHod::inRange(double val) const {
    if (values.empty()) return false;
    if (val < values[0] || val > values.back()) return false;

    uint32_t i = 0, j = values.size();
    if (j < 32) { // sequential search
	// -- because the heavy branch prediction cost, linear search is
	// more efficient for fairly large range.
	for (i = 0; i < j; ++ i)
	    if (values[i] == val) return true;
	return false;
    }
    else { // binary search
	uint32_t m = (i + j) / 2;
	while (i < m) {
	    if (values[m] == val) return true;
	    if (values[m] < val)
		i = m;
	    else
		j = m;
	    m = (i + j) / 2;
	}
	return (values[m] == val);
    }
} // ibis::qUIntHod::inRange

/// Is the argument @a val one of the values stored ?  Return true or
/// false.
/// It uses a binary search if there are more than 32 elements and uses
/// linear search otherwise.
inline bool ibis::qIntHod::inRange(int64_t val) const {
    if (values.empty()) return false;
    if (val < values[0] || val > values.back()) return false;

    uint32_t i = 0, j = values.size();
    if (j < 32) { // sequential search
	// -- because the heavy branch prediction cost, linear search is
	// more efficient for fairly large range.
	for (i = 0; i < j; ++ i)
	    if (values[i] == val) return true;
	return false;
    }
    else { // binary search
	uint32_t m = (i + j) / 2;
	while (i < m) {
	    if (values[m] == val) return true;
	    if (values[m] < val)
		i = m;
	    else
		j = m;
	    m = (i + j) / 2;
	}
	return (values[m] == val);
    }
} // ibis::qIntHod::inRange

/// Is the argument @a val one of the values stored ?  Return true or
/// false.
/// It uses a binary search if there are more than 32 elements and uses
/// linear search otherwise.
inline bool ibis::qUIntHod::inRange(uint64_t val) const {
    if (values.empty()) return false;
    if (val < values[0] || val > values.back()) return false;

    uint32_t i = 0, j = values.size();
    if (j < 32) { // sequential search
	// -- because the heavy branch prediction cost, linear search is
	// more efficient for fairly large range.
	for (i = 0; i < j; ++ i)
	    if (values[i] == val) return true;
	return false;
    }
    else { // binary search
	uint32_t m = (i + j) / 2;
	while (i < m) {
	    if (values[m] == val) return true;
	    if (values[m] < val)
		i = m;
	    else
		j = m;
	    m = (i + j) / 2;
	}
	return (values[m] == val);
    }
} // ibis::qUIntHod::inRange

namespace std {
    inline ostream& operator<<(ostream&, const ibis::qExpr&);
    inline ostream& operator<<(ostream&, const ibis::qExpr::COMPARE&);
}

/// Wrap the function print as operator<<.  Print the full query expression
/// when the global verbose level is higher than 5, otherwise print a
/// shorter version of the query expression.
inline std::ostream& std::operator<<(std::ostream& out, const ibis::qExpr& pn) {
    if (ibis::gVerbose > 5)
	pn.printFull(out);
    else
	pn.print(out);
    return out;
} // std::operator<<

/// Print a comparison operator.
inline std::ostream& std::operator<<(std::ostream& out,
				     const ibis::qExpr::COMPARE& op) {
    switch (op) {
    default:
    case ibis::qExpr::OP_UNDEFINED:
	out << "??"; break;
    case ibis::qExpr::OP_LT:
	out << "<"; break;
    case ibis::qExpr::OP_LE:
	out << "<="; break;
    case ibis::qExpr::OP_GT:
	out << ">"; break;
    case ibis::qExpr::OP_GE:
	out << ">="; break;
    case ibis::qExpr::OP_EQ:
	out << "=="; break;
    }
    return out;
} // std::operator<<
#endif // IBIS_EXPR_H
