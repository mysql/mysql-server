// $Id$
// Author: John Wu <John.Wu at acm.org>
//      Lawrence Berkeley National Laboratory
// Copyright (c) 2007-2016 the Regents of the University of California
#include "part.h"       // ibis::part, used by verify and amplify
#include "whereLexer.h"
#include "whereClause.h"
#include "selectClause.h"
#include "dictionary.h" // ibis::dictionary

ibis::whereClause::~whereClause() {
    delete expr_;
} // destructor

ibis::whereClause::whereClause(const char* cl) : expr_(0) {
    (void) parse(cl);
} // constructor

ibis::whereClause::whereClause(const ibis::whereClause& rhs)
    : clause_(rhs.clause_), expr_(0), lexer(0) {
    if (rhs.expr_ != 0)
        expr_ = rhs.expr_->dup();
} // copy constructor

ibis::whereClause& ibis::whereClause::operator=(const ibis::whereClause& rhs) {
    ibis::whereClause tmp(rhs);
    swap(tmp);
    return *this;
} // assignment operator

int ibis::whereClause::parse(const char* cl) {
    int ierr = 0;
    if (cl != 0 && *cl != 0) {
        LOGGER(ibis::gVerbose > 5)
            << "whereClause::parse receives a new where clause \"" << cl
            << "\"";

        clause_ = cl;
        std::istringstream iss(clause_);
        ibis::util::logger lg;
        whereLexer lx(&iss, &(lg()));
        whereParser parser(*this);
        lexer = &lx;
#if DEBUG+0 > 2
        parser.set_debug_level(DEBUG-1);
#elif _DEBUG+0 > 2
        parser.set_debug_level(_DEBUG-1);
#endif
        parser.set_debug_stream(lg());

        delete expr_;
        expr_ = 0;

        ierr = parser.parse();
        lexer = 0;
        if (ierr == 0 && expr_ != 0) {
            ibis::qExpr::simplify(expr_);
        }
        else {
            delete expr_;
            expr_ = 0;
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- whereClause(" << cl
                << ") failed to parse the string into an expression tree";
#ifdef FASTBIT_HALT_ON_PARSER_ERROR
            throw "whereClause failed to parse query conditions" IBIS_FILE_LINE;
#endif
        }
    }
    return ierr;
} // ibis::whereClause::parse

/// Append a set of conditions to the existing where clause.  The new
/// conditions are joined together with the existing ones with the AND
/// operator.
void ibis::whereClause::addConditions(const char *cl) {
    if (cl == 0 || *cl == 0) return;

    if (expr_ != 0) {
        ibis::qExpr *old = expr_;
        expr_ = 0;
        int ierr = parse(cl);
        if (ierr == 0) {
            ibis::qExpr *tmp = expr_;
            expr_ = old;
            addExpr(tmp);
        }
        else {
            LOGGER(ibis::gVerbose > 1)
                << "whereClause::addConditions failed to parse " << cl
                << ", ierr = " << ierr;
            expr_ = old;
        }
    }
    else {
        (void) parse(cl);
    }
} // ibis::whereClause::addConditions

/// Append a set of conditions to the existing where clause.  The new
/// conditions are joined together with the existing ones with the AND
/// operator.
///
/// @note This object will have a copy of the incomping object.
void ibis::whereClause::addExpr(const ibis::qExpr *ex) {
    if (ex == 0) return;

    clause_.clear();
    if (expr_ == 0) {
        expr_ = ex->dup();
    }
    else {
        ibis::qExpr *root =
            new ibis::qExpr(ibis::qExpr::LOGICAL_AND, expr_, ex->dup());
        expr_ = root;
    }
} // ibis::whereClause::addExpr

void ibis::whereClause::clear() throw () {
    clause_.clear();
    delete expr_;
    expr_ = 0;
} // ibis::whereClause::clear

/// Add conditions implied by self-join conditions.
/// @note This name is intentionally vague to discourage its use.  It might
/// be completely removed in a later release.
void ibis::whereClause::amplify(const ibis::part& part0) {
    std::vector<const ibis::deprecatedJoin*> terms;
    expr_->extractDeprecatedJoins(terms);
    if (terms.empty()) // no join terms to use
        return;

    LOGGER(ibis::gVerbose > 6)
        << "whereClause::amplify -- current query expression\n" << *expr_;

    for (uint32_t i = 0; i < terms.size(); ++ i) {
        const ibis::deprecatedJoin* jn = terms[i];
        double delta = 0.0;
        if (jn->getRange()) {
            const ibis::math::term *tm = jn->getRange();
            if (tm != 0) {
                if (tm->termType() != ibis::math::NUMBER)
                    continue;
                else
                    delta = tm->eval();
            }
        }

        const char *nm1 = jn->getName1();
        const char *nm2 = jn->getName2();
        const ibis::column *col1 = part0.getColumn(nm1);
        const ibis::column *col2 = part0.getColumn(nm2);
        if (col1 == 0 || col2 == 0)
            continue;

        double cmin1 = col1->getActualMin();
        double cmax1 = col1->getActualMax();
        double cmin2 = col2->getActualMin();
        double cmax2 = col2->getActualMax();
        ibis::qRange* cur1 = expr_->findRange(nm1);
        ibis::qRange* cur2 = expr_->findRange(nm2);
        if (cur1) {
            double tmp = cur1->leftBound();
            if (tmp > cmin1)
                cmin1 = tmp;
            tmp = cur1->rightBound();
            if (tmp < cmax1)
                cmax1 = tmp;
        }
        if (cur2) {
            double tmp = cur2->leftBound();
            if (tmp > cmin2)
                cmin2 = tmp;
            tmp = cur2->rightBound();
            if (tmp < cmax2)
                cmax2 = tmp;
        }

        if (cmin1 < cmin2-delta || cmax1 > cmax2+delta) {
            double bd1 = (cmin1 >= cmin2-delta ? cmin1 : cmin2-delta);
            double bd2 = (cmax1 <= cmax2+delta ? cmax1 : cmax2+delta);
            if (cur1) { // reduce the range of an existing range condition
                cur1->restrictRange(bd1, bd2);
            }
            else { // add an addition term of nm1
                ibis::qContinuousRange *qcr =
                    new ibis::qContinuousRange(bd1, ibis::qExpr::OP_LE,
                                               nm1, ibis::qExpr::OP_LE, bd2);
                ibis::qExpr *qop = new ibis::qExpr(ibis::qExpr::LOGICAL_AND,
                                                   qcr, expr_->getRight());
                expr_->getRight() = qop;
            }
        }

        if (cmin2 < cmin1-delta || cmax2 > cmax1+delta) {
            double bd1 = (cmin2 >= cmin1-delta ? cmin2 : cmin1-delta);
            double bd2 = (cmax2 <= cmax1+delta ? cmax2 : cmax1+delta);
            if (cur2) {
                cur2->restrictRange(bd1, bd2);
            }
            else {
                ibis::qContinuousRange *qcr =
                    new ibis::qContinuousRange(bd1, ibis::qExpr::OP_LE,
                                               nm2, ibis::qExpr::OP_LE, bd2);
                ibis::qExpr *qop = new ibis::qExpr(ibis::qExpr::LOGICAL_AND,
                                                   qcr, expr_->getLeft());
                expr_->getLeft() = qop;
            }
        }
    }

    ibis::qExpr::simplify(expr_);
    if (expr_ != 0 && ibis::gVerbose > 6) {
        ibis::util::logger lg;
        lg() << "whereClause::amplify -- "
            "query expression with additional constraints\n";
        expr_->printFull(lg());
    }
} // ibis::whereClause::amplify

void ibis::whereClause::getNullMask(const ibis::part &part0,
                                    ibis::bitvector &mask) const {
    if (expr_ == 0) {
        part0.getNullMask(mask);
    }
    else {
        ibis::part::barrel bar(&part0);
        bar.recordVariable(expr_);
        bar.getNullMask(mask);
    }
} // ibis::whereClause::getNullMask

/// Verify that the names exist in the data partition.
/// This function also simplifies the arithmetic expression if
/// ibis::term::preserveInputExpression is not set and augment the
/// expressions with implied conditions.
///
/// @note The select clause is provided to make the aliases defined there
/// available to the where clause.
///
/// @note Simplifying the arithmetic expressions typically reduces the time
/// needed for evaluations, but may introduces a different set of round-off
/// erros in the evaluation process than the original expression.
int ibis::whereClause::verify(const ibis::part& part0,
                              const ibis::selectClause *sel) const {
    if (expr_ != 0) {
        //ibis::qExpr::simplify(const_cast<ibis::qExpr*&>(expr_));
        if (expr_ == 0) {
            return -1;
        }
        else {
            //amplify(part0);
            return verifyExpr(const_cast<ibis::qExpr*&>(expr_), part0, sel);
        }
    }
    else {
        return 0;
    }
} // ibis::whereClause::verify

/// A function to verify an single query expression.  This function checks
/// each variable name specified in the query expression to make sure they
/// all appear as column names of the given data partition.  It returns the
/// number of names NOT in the data partition.
///
/// It also removes the aliases and simplifies certain query expressions.
/// For example, it converts expressions of the form "string1 = string2" to
/// string lookups when one of the strings is a name of a string-valued
/// column.
int ibis::whereClause::verifyExpr(ibis::qExpr *&xp0, const ibis::part& part0,
                                  const ibis::selectClause *sel) {
    int ierr = 0;
    if (xp0 == 0) return ierr;

    switch (xp0->getType()) {
    case ibis::qExpr::RANGE: {
        ibis::qContinuousRange* range =
            static_cast<ibis::qContinuousRange*>(xp0);
        if (range->colName() != 0) { // allow name to be NULL
            const ibis::column* col = part0.getColumn(range->colName());
            if (col == 0 && sel != 0) {
                int isel = sel->find(range->colName());
                if (isel >= 0 && (unsigned)isel < sel->aggSize() &&
                    sel->getAggregator(isel) == ibis::selectClause::NIL_AGGR) {
                    const ibis::math::term *tm = sel->aggExpr(isel);
                    switch (tm->termType()) {
                    default: break; // can not do anything
                    case ibis::math::VARIABLE: {
                        const ibis::math::variable &var =
                            *static_cast<const ibis::math::variable*>(tm);
                        col = part0.getColumn(var.variableName());
                        if (col != 0) { // use the real name
                            removeAlias(range, col);
                        }
                        break;}
                    case ibis::math::NUMBER: {
                        col = part0.getColumn((uint32_t)0);
                        break;}
                    case ibis::math::STRING: {
                        const char *sval =
                            *static_cast<const ibis::math::literal*>(tm);
                        col = part0.getColumn(sval);
                        if (col != 0) { // use the real name
                            removeAlias(range, col);
                        }
                        break;}
                    case ibis::math::OPERATOR:
                    case ibis::math::STDFUNCTION1:
                    case ibis::math::STDFUNCTION2:
                    case ibis::math::CUSTOMFUNCTION1:
                    case ibis::math::CUSTOMFUNCTION2: {
                        ibis::math::number *num1;
                        ibis::math::number *num2;
                        if (range->leftOperator() !=
                            ibis::qExpr::OP_UNDEFINED) {
                            num1 =
                                new ibis::math::number(range->leftBound());
                        }
                        else {
                            num1 = 0;
                        }
                        if (range->rightOperator() !=
                            ibis::qExpr::OP_UNDEFINED) {
                            num2 =
                                new ibis::math::number(range->rightBound());
                        }
                        else {
                            num2 = 0;
                        }
                        if (num1 != 0 || num2 != 0) {
                            ibis::math::term *myterm = tm->dup();
                            ibis::compRange *tmp = new ibis::compRange
                                (num1, range->leftOperator(), myterm,
                                 range->rightOperator(), num2);
                            delete xp0;
                            xp0 = tmp;
                            ierr += verifyExpr(xp0, part0, sel);
                            col = part0.getColumn((uint32_t)0);
                        }
                        break;}
                    } // switch (tm->termType())
                }
            }
            if (col == 0) {
                ++ ierr;
                LOGGER(ibis::gVerbose > 2)
                    << "Warning -- whereClause::verifyExpr -- data partition "
                    << part0.name() << " does not contain a column named "
                    << range->colName();
            }
        }
        break;}
    case ibis::qExpr::STRING: {
        const ibis::qString* str =
            static_cast<const ibis::qString*>(xp0);
        const ibis::column* col = 0;
        if (str->leftString()) { // try the left side
            col = part0.getColumn(str->leftString());
        }
        if (col == 0 && str->rightString()) { // try the right side
            const ibis::column* col = part0.getColumn(str->rightString());
            if (col != 0) {
                const_cast<ibis::qString*>(str)->swapLeftRight();
            }
        }
        if (col != 0) {
            if (col->type() == ibis::UINT && col->getDictionary() != 0) {
                uint32_t ind = (*col->getDictionary())[str->rightString()];
                if (ind <= col->getDictionary()->size()) {
                    ibis::qContinuousRange *cr = new
                        ibis::qContinuousRange(col->name(),
                                               ibis::qExpr::OP_EQ, ind);
                    delete xp0;
                    xp0 = cr;
                }
            }
            else if (col->isFloat()) {
                // convert the string on the right hand side to a numeric
                // value
                double dval;
                const char *sval = str->rightString();
                if (0 == ibis::util::readDouble(dval, sval)) {
                    ibis::qContinuousRange *cr = new
                        ibis::qContinuousRange(col->name(),
                                               ibis::qExpr::OP_EQ, dval);
                    delete xp0;
                    xp0 = cr;
                }
                else {
                    ++ ierr;
                    LOGGER(ibis::gVerbose > 2)
                        << "Warning -- whereClause::verifyExpr -- column "
                        << col->name() << " can not be matched with string "
                        << str->rightString();
                }
            }
            else if ((col->type() == ibis::BYTE || col->type() == ibis::UBYTE)
                     && str->rightString()[1] == 0) {
                const unsigned int ival = *(str->rightString());
                ibis::qContinuousRange *cr = new
                    ibis::qContinuousRange(col->name(),
                                           ibis::qExpr::OP_EQ, ival);
                delete xp0;
                xp0 = cr;
            }
            else if (col->isInteger()) {
                int64_t ival = 0;
                const char *sval = str->rightString();
                if (sval[0] == '0' && (sval[1] == 'x' || sval[1] == 'X')) {
                    // hexadecimal
                    for (sval += 2; *sval != 0; ++ sval) {
                        ival <<= 4;
                        if (*sval >= '0' && *sval <= '9') {
                            ival += *sval - '0';
                        }
                        else if (*sval >= 'A' && *sval <= 'F') {
                            ival += 10 + (*sval - 'A');
                        }
                        else if (*sval >= 'a' && *sval <= 'f') {
                            ival += 10 + (*sval - 'a');
                        }
                        else {
                            ++ ierr;
                            LOGGER(ibis::gVerbose > 2)
                                << "Warning -- whereClause::verifyExpr failed "
                                " to convert string " << str->rightString()
                                << " to a hexadecimal integer";
                            return ierr;
                        }
                    }
                }
                else if (sval[0] == '0') {
                    // octal
                    for (++ sval; *sval != 0; ++ sval) {
                        ival <<= 3;
                        if (*sval >= '0' && *sval < '8') {
                            ival += *sval - '0';
                        }
                        else {
                            ++ ierr;
                            LOGGER(ibis::gVerbose > 2)
                                << "Warning -- whereClause::verifyExpr failed "
                                " to convert string " << str->rightString()
                                << " to an octal integer";
                            return ierr;
                        }
                    }
                }
                else if (sval[0] == '+' || sval[0] == '-' || isdigit(sval[0])) {
                    if (0 > ibis::util::readInt(ival, sval)) {
                        ++ ierr;
                        LOGGER(ibis::gVerbose > 2)
                            << "Warning -- whereClause::verifyExpr failed "
                            " to convert string " << str->rightString()
                            << " to an octal integer";
                        return ierr;
                    }
                }
                else {
                    ++ ierr;
                    LOGGER(ibis::gVerbose > 2)
                        << "Warning -- whereClause::verifyExpr failed to "
                        "convert string " << str->rightString()
                        << " to an integer";
                    return ierr;
                }

                ibis::qExpr *cr = 0;
                const double dval = static_cast<double>(ival);
                if (ival == static_cast<int64_t>(dval)) {
                    cr = new ibis::qContinuousRange(col->name(),
                                                    ibis::qExpr::OP_EQ, dval);
                }
                else {
                    cr = new ibis::qIntHod(col->name(), ival);
                }
                if (cr != 0) {
                    delete xp0;
                    xp0 = cr;
                }
            }
        }
        break;}
    case ibis::qExpr::LIKE: {
        const ibis::qLike* str =
            static_cast<const ibis::qLike*>(xp0);
        if (str->colName()) { // allow name to be NULL
            const ibis::column* col = part0.getColumn(str->colName());
            if (col == 0) {
                ++ ierr;
                LOGGER(ibis::gVerbose > 2)
                    << "Warning -- whereClause::verifyExpr -- data partition "
                    << part0.name() << " does not contain a column named "
                    << str->colName();
            }
        }
        break;}
    case ibis::qExpr::MATHTERM: {
        ibis::math::term* math =
            static_cast<ibis::math::term*>(xp0);
        if (math->termType() == ibis::math::VARIABLE) {
            const ibis::math::variable* var =
                static_cast<const ibis::math::variable*>(math);
            if (*(var->variableName()) == '*') break;

            const ibis::column* col =
                part0.getColumn(var->variableName());
            if (col == 0 && sel != 0) {
                int isel = sel->find(var->variableName());
                if (isel >= 0 && (unsigned)isel < sel->aggSize()) {
                    const ibis::math::term *tm = sel->aggExpr(isel);
                    switch (tm->termType()) {
                    default: break; // can not do anything
                    case ibis::math::VARIABLE: {
                        const ibis::math::variable &var2 =
                            *static_cast<const ibis::math::variable*>(tm);
                        col = part0.getColumn(var2.variableName());
                        if (col != 0) { // use the real name
                            delete xp0;
                            xp0 = var2.dup();
                        }
                        break;}
                    case ibis::math::NUMBER: {
                        delete xp0;
                        xp0 = tm->dup();
                        col = part0.getColumn((uint32_t)0);
                        break;}
                    case ibis::math::STRING: {
                        const char *sval =
                            *static_cast<const ibis::math::literal*>(tm);
                        col = part0.getColumn(sval);
                        if (col != 0) { // use the real name
                            ibis::math::variable *tmp =
                                new ibis::math::variable(sval);
                            delete xp0;
                            xp0 = tmp;
                        }
                        break;}
                    case ibis::math::OPERATOR:
                    case ibis::math::STDFUNCTION1:
                    case ibis::math::STDFUNCTION2:
                    case ibis::math::CUSTOMFUNCTION1:
                    case ibis::math::CUSTOMFUNCTION2: {
                        delete xp0;
                        xp0 = tm->dup();
                        col = part0.getColumn((uint32_t)0);
                        break;}
                    } // switch (tm->termType())
                }
            }
            if (col == 0) {
                ++ ierr;
                LOGGER(ibis::gVerbose > 2)
                    << "Warning -- whereClause::verifyExpr -- data partition "
                    << part0.name() << " does not contain a column named "
                    << var->variableName();
            }
        }
        ierr += verifyExpr(math->getLeft(), part0, sel);
        ierr += verifyExpr(math->getRight(), part0, sel);
        break;}
    case ibis::qExpr::COMPRANGE: {
        // a compRange has three terms instead of two
        if (reinterpret_cast<ibis::compRange*>(xp0)
            ->maybeStringCompare()) {
            const ibis::math::variable *v1 =
                reinterpret_cast<const ibis::math::variable*>
                (xp0->getLeft());
            const ibis::math::variable *v2 =
                reinterpret_cast<const ibis::math::variable*>
                (xp0->getRight());
            const ibis::column *c1 =
                part0.getColumn(v1->variableName());
            const ibis::column *c2 =
                part0.getColumn(v2->variableName());
            if (c1 != 0) {
                if (c2 == 0) {
                    if (c1->type() == ibis::TEXT ||
                        c1->type() == ibis::CATEGORY) {
                        LOGGER(ibis::gVerbose > 3)
                            << "whereClause::verifyExpr -- replacing ("
                            << v1->variableName() << " = "
                            << v2->variableName() << ") with ("
                            << v1->variableName() << " = \""
                            << v2->variableName() << "\")";
                        ibis::qString *tmp = new
                            ibis::qString(v1->variableName(),
                                          v2->variableName());
                        delete xp0;
                        xp0 = tmp;
                    }
                    else {
                        ++ ierr;
                        LOGGER(ibis::gVerbose > 2)
                            << "whereClause::verifyExpr -- expected column \""
                            << v1->variableName() << "\" to be of string type, "
                            << "but it is %s" << ibis::TYPESTRING[c1->type()];
                    }
                }
            }
            else if (c2 != 0) {
                if (c2->type() == ibis::TEXT ||
                    c2->type() == ibis::CATEGORY) {
                    LOGGER(ibis::gVerbose > 3)
                        << "whereClause::verifyExpr -- replacing ("
                        << v2->variableName() << " = " << v1->variableName()
                        << ") with (" << v2->variableName() << " = \""
                        << v1->variableName() << "\")";
                    ibis::qString *tmp = new
                        ibis::qString(v2->variableName(),
                                      v1->variableName());
                    delete xp0;
                    xp0 = tmp;
                }
                else {
                    ++ ierr;
                    LOGGER(ibis::gVerbose > 2)
                        << "whereClause::verifyExpr -- expected column \""
                        <<  v2->variableName() <<  "\" to be of string type, "
                        << "but it is " << ibis::TYPESTRING[c2->type()];
                }
            }
            else {
                ierr += 2;
                LOGGER(ibis::gVerbose > 0)
                    << "whereClause::verifyExpr -- neither "
                    << v1->variableName() << " or " << v2->variableName()
                    << " are columns names of table " << part0.name();
            }
        }
        else {
            if (xp0->getLeft() != 0)
                ierr += verifyExpr(xp0->getLeft(), part0, sel);
            if (xp0->getRight() != 0)
                ierr += verifyExpr(xp0->getRight(), part0, sel);
            if (reinterpret_cast<ibis::compRange*>(xp0)->getTerm3() != 0) {
                ibis::qExpr *cr = reinterpret_cast<ibis::compRange*>
                    (xp0)->getTerm3();
                ierr += verifyExpr(cr, part0, sel);
            }
        }
        break;}
    case ibis::qExpr::DRANGE : {
        ibis::qDiscreteRange *range =
            reinterpret_cast<ibis::qDiscreteRange*>(xp0);
        if (range->colName()) { // allow name to be NULL
            const ibis::column* col = part0.getColumn(range->colName());
            if (col == 0) {
                ++ ierr;
                LOGGER(ibis::gVerbose > 2)
                    << "Warning -- whereClause::verifyExpr -- data partition "
                    << part0.name() << " does not contain a column named "
                    << range->colName();
            }
            else if (col->type() == ibis::FLOAT) {
                // reduce the precision of the bounds
                ibis::array_t<double>& val = range->getValues();
                for (ibis::array_t<double>::iterator it = val.begin();
                     it != val.end(); ++ it)
                    *it = static_cast<float>(*it);
            }
        }
        break;}
    case ibis::qExpr::ANYSTRING : {
        ibis::qAnyString *range =
            reinterpret_cast<ibis::qAnyString*>(xp0);
        if (range->colName()) { // allow name to be NULL
            const ibis::column* col = part0.getColumn(range->colName());
            if (col == 0) {
                ++ ierr;
                LOGGER(ibis::gVerbose > 2)
                    << "Warning -- whereClause::verifyExpr -- data partition "
                    << part0.name() << " does not contain a column named "
                    << range->colName();
            }
        }
        break;}
    case ibis::qExpr::DEPRECATEDJOIN : {
        ibis::deprecatedJoin *rj = reinterpret_cast<ibis::deprecatedJoin*>(xp0);
        const ibis::column* c1 = part0.getColumn(rj->getName1());
        if (c1 == 0) {
            ++ ierr;
            LOGGER(ibis::gVerbose > 2)
                << "Warning -- whereClause::verifyExpr -- data partition "
                << part0.name() << " does not contain a column named "
                << rj->getName1();
        }
        const ibis::column* c2 = part0.getColumn(rj->getName2());
        if (c2 == 0) {
            ++ ierr;
            LOGGER(ibis::gVerbose > 2)
                << "Warning -- whereClause::verifyExpr -- data partition "
                << part0.name() << " does not contain a column named "
                << rj->getName2();
        }
        ibis::qExpr *t = rj->getRange();
        ierr += verifyExpr(t, part0, sel);
        break;}
    default: {
        if (xp0->getLeft() != 0) {
            if (xp0->getLeft()->getType() == ibis::qExpr::EXISTS ||
                (xp0->getLeft()->getType() == ibis::qExpr::LOGICAL_NOT &&
                 xp0->getLeft()->getLeft() != 0 &&
                 xp0->getLeft()->getLeft()->getType() == ibis::qExpr::EXISTS))
                break;
            ierr += verifyExpr(xp0->getLeft(), part0, sel);
        }
        if (xp0->getRight() != 0) {
            ierr += verifyExpr(xp0->getRight(), part0, sel);
        }
        break;}
    } // end switch

    return ierr;
} // ibis::whereClause::verifyExpr

int ibis::whereClause::verifyExpr(const ibis::qExpr *xp0,
                                  const ibis::part& part0,
                                  const ibis::selectClause *sel) {
    int ierr = 0;
    if (xp0 == 0) return ierr;

    switch (xp0->getType()) {
    case ibis::qExpr::RANGE: {
        const ibis::qContinuousRange* range =
            static_cast<const ibis::qContinuousRange*>(xp0);
        if (range->colName() != 0) { // allow name to be NULL
            const ibis::column* col = part0.getColumn(range->colName());
            if (col == 0 && sel != 0) {
                int isel = sel->find(range->colName());
                if (isel >= 0 && (unsigned)isel < sel->aggSize()) {
                    col = part0.getColumn(0U);
                }
            }
            if (col == 0) {
                ++ ierr;
                LOGGER(ibis::gVerbose > 2)
                    << "Warning -- whereClause::verifyExpr -- data partition "
                    << part0.name() << " does not contain a column named "
                    << range->colName();
            }
        }
        break;}
    case ibis::qExpr::STRING: {
        const ibis::qString* str =
            static_cast<const ibis::qString*>(xp0);
        if (str->leftString()) { // allow name to be NULL
            const ibis::column* col = part0.getColumn(str->leftString());
            if (col == 0) {
                ++ ierr;
                LOGGER(ibis::gVerbose > 2)
                    << "Warning -- whereClause::verifyExpr -- data partition "
                    << part0.name() << " does not contain a column named "
                    << str->leftString();
            }
        }
        break;}
    case ibis::qExpr::LIKE: {
        const ibis::qLike* str =
            static_cast<const ibis::qLike*>(xp0);
        if (str->colName()) { // allow name to be NULL
            const ibis::column* col = part0.getColumn(str->colName());
            if (col == 0) {
                ++ ierr;
                LOGGER(ibis::gVerbose > 2)
                    << "Warning -- whereClause::verifyExpr -- data partition "
                    << part0.name() << " does not contain a column named "
                    << str->colName();
            }
        }
        break;}
    case ibis::qExpr::MATHTERM: {
        ierr += ibis::selectClause::verifyTerm
            (*static_cast<const ibis::math::term*>(xp0), part0, sel);
        break;}
    case ibis::qExpr::COMPRANGE: {
        if (xp0->getLeft() != 0)
            ierr += verifyExpr(xp0->getLeft(), part0, sel);
        if (xp0->getRight() != 0)
            ierr += verifyExpr(xp0->getRight(), part0, sel);
        if (reinterpret_cast<const ibis::compRange*>(xp0)->getTerm3() != 0) {
            const ibis::qExpr *cr = reinterpret_cast<const ibis::compRange*>
                (xp0)->getTerm3();
            ierr += verifyExpr(cr, part0, sel);
        }
        break;}
    case ibis::qExpr::DRANGE : {
        const ibis::qDiscreteRange *range =
            reinterpret_cast<const ibis::qDiscreteRange*>(xp0);
        if (range->colName()) { // allow name to be NULL
            const ibis::column* col = part0.getColumn(range->colName());
            if (col == 0) {
                ++ ierr;
                LOGGER(ibis::gVerbose > 2)
                    << "Warning -- whereClause::verifyExpr -- data partition "
                    << part0.name() << " does not contain a column named "
                    << range->colName();
            }
        }
        break;}
    case ibis::qExpr::ANYSTRING : {
        const ibis::qAnyString *range =
            reinterpret_cast<const ibis::qAnyString*>(xp0);
        if (range->colName()) { // allow name to be NULL
            const ibis::column* col = part0.getColumn(range->colName());
            if (col == 0) {
                ++ ierr;
                LOGGER(ibis::gVerbose > 2)
                    << "Warning -- whereClause::verifyExpr -- data partition "
                    << part0.name() << " does not contain a column named "
                    << range->colName();
            }
        }
        break;}
    case ibis::qExpr::DEPRECATEDJOIN : {
        const ibis::deprecatedJoin *rj =
            reinterpret_cast<const ibis::deprecatedJoin*>(xp0);
        const ibis::column* c1 = part0.getColumn(rj->getName1());
        if (c1 == 0) {
            ++ ierr;
            LOGGER(ibis::gVerbose > 2)
                << "Warning -- whereClause::verifyExpr -- data partition "
                << part0.name() << " does not contain a column named "
                << rj->getName1();
        }
        const ibis::column* c2 = part0.getColumn(rj->getName2());
        if (c2 == 0) {
            ++ ierr;
            LOGGER(ibis::gVerbose > 2)
                << "Warning -- whereClause::verifyExpr -- data partition "
                << part0.name() << " does not contain a column named "
                << rj->getName2();
        }
        ierr += verifyExpr(rj->getRange(), part0, sel);
        break;}
    default: {
        if (xp0->getLeft() != 0) {
            if (xp0->getLeft()->getType() == ibis::qExpr::EXISTS ||
                (xp0->getLeft()->getType() == ibis::qExpr::LOGICAL_NOT &&
                 xp0->getLeft()->getLeft() != 0 &&
                 xp0->getLeft()->getLeft()->getType() == ibis::qExpr::EXISTS))
                break;
            ierr += verifyExpr(xp0->getLeft(), part0, sel);
        }
        if (xp0->getRight() != 0) {
            ierr += verifyExpr(xp0->getRight(), part0, sel);
        }
        break;}
    } // end switch

    return ierr;
} // ibis::whereClause::verifyExpr

/// Create a simple range expression as the replacement of the incoming
/// oldr.  It replaces the name of the column if the incoming expression
/// uses an alias.  It replaces the negative query boundaries with 0 for
/// unsigned integer columns.
int
ibis::whereClause::removeAlias(ibis::qContinuousRange*& oldr,
                               const ibis::column* col) {
    try {
        ibis::qExpr::COMPARE lop = oldr->leftOperator(),
            rop = oldr->rightOperator();
        double lbd = oldr->leftBound(),
            rbd = oldr->rightBound();
        if (col->isUnsignedInteger()) {
            if (oldr->leftBound() < 0.0) {
                switch (oldr->leftOperator()) {
                default:
                case ibis::qExpr::OP_UNDEFINED:
                    lop = ibis::qExpr::OP_UNDEFINED;
                    break;
                case ibis::qExpr::OP_LT:
                case ibis::qExpr::OP_LE:
                    lop = ibis::qExpr::OP_LE;
                    lbd = 0.0;
                    break;
                case ibis::qExpr::OP_GT:
                case ibis::qExpr::OP_GE:
                    lop = ibis::qExpr::OP_GT;
                    lbd = 0.0;
                    break;
                case ibis::qExpr::OP_EQ:
                    // a unsigned number can not equal to a negative number,
                    // nor can it equal to 0.5
                    lbd = 0.5;
                    break;
                }
            }
            if (oldr->rightBound() < 0.0) {
                switch (oldr->rightOperator()) {
                default:
                case ibis::qExpr::OP_UNDEFINED:
                    rop = ibis::qExpr::OP_UNDEFINED;
                    break;
                case ibis::qExpr::OP_LT:
                case ibis::qExpr::OP_LE:
                    rop = ibis::qExpr::OP_LT;
                    rbd = 0.0;
                    break;
                case ibis::qExpr::OP_GT:
                case ibis::qExpr::OP_GE:
                    rop = ibis::qExpr::OP_GE;
                    rbd = 0.0;
                    break;
                case ibis::qExpr::OP_EQ:
                    // a unsigned number can not equal to a negative number,
                    // nor can it equal to 0.5
                    rbd = 0.5;
                    break;
                }
            }
        }

        ibis::qContinuousRange* tmp
            = new ibis::qContinuousRange(lbd, lop, col->name(), rop, rbd);
        delete oldr;
        oldr = tmp;
        return 0;
    }
    catch (...) {
        return -1;
    }
} // ibis::whereClause::removeAlias

