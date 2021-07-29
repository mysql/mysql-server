// $Id$
// Author: John Wu <John.Wu at acm.org>
//      Lawrence Berkeley National Laboratory
// Copyright (c) 1998-2016 the Regents of the University of California
//
// implement the functions defined in qExpr.h
//
#include "util.h"
#include "part.h"
#include "qExpr.h"

#ifdef sun
#include <ieeefp.h>     // finite
#endif
#include <stdlib.h>
#include <limits.h>

#include <set>          // std::set
#include <iterator>     // std::ostream_iterator
#include <algorithm>    // std::copy, std::sort
#include <iomanip>      // std::setprecision

// the names of the operators used in ibis::compRange
const char* ibis::math::operator_name[] =
    {"?", "|", "&", "+", "-", "*", "/", "%", "-", "**"};

const char* ibis::math::stdfun1_name[] =
    {"acos", "asin", "atan", "ceil", "cos", "cosh", "exp", "fabs", "floor",
     "frexp", "log10", "log", "modf", "round", "sin", "sinh", "sqrt", "tan",
     "tanh", "is_zero", "is_nonzero"};

const char* ibis::math::stdfun2_name[] = 
    {"atan2", "fmod", "ldexp", "round", "pow",
     "is_eql", "is_gte", "is_lte"};
bool ibis::math::preserveInputExpressions = false;

/// Operations performed include converting compRanges into qRanges,
/// qDiscreteRange into qContinuousRange, perform constant evaluations,
/// combining pairs of inverse functions.  This is necessary because the
/// parser always generates compRange instead of qRange.  The goal of
/// simplifying arithmetic expressions is to reduce the number of accesses
/// to the variable values (potentially reducing the number of disk
/// accesses).
///
/// @note Be aware that rearranging the arithmetic expressions may affect
/// the round-off perperties of these expressions, and therefore affect
/// their computed results.  Even though the typical differences might be
/// small (after ten significant digits), however, the differences could
/// accumulated and became noticeable.  To turn off this optimization, set
/// ibis::math::preserveInputExpressions to true.
void ibis::qExpr::simplify(ibis::qExpr*& expr) {
    if (expr == 0) return;
    LOGGER(ibis::gVerbose > 5)
        << "qExpr::simplify --  input expression " << *expr;

    switch (expr->getType()) {
    default:
        break;
    case ibis::qExpr::LOGICAL_NOT:
        simplify(expr->left);
        break;
    case ibis::qExpr::LOGICAL_AND: {
        simplify(expr->left);
        simplify(expr->right);
        bool emptyleft = (expr->left == 0 ||
                          ((expr->left->getType() == RANGE ||
                            expr->left->getType() == DRANGE) &&
                           reinterpret_cast<qRange*>(expr->left)->empty()));
        bool emptyright = (expr->right == 0 ||
                           ((expr->right->getType() == RANGE ||
                             expr->right->getType() == DRANGE) &&
                            reinterpret_cast<qRange*>(expr->right)->empty()));
        if (emptyleft || emptyright) {
            delete expr;
            expr = ibis::compRange::makeConstantFalse();
        }
        else if (expr->left != 0 && expr->left->isConstant() &&
                 expr->left->getType() == ibis::qExpr::COMPRANGE) {
            if (static_cast<ibis::compRange*>(expr->left)->inRange()) {
                // copy right
                ibis::qExpr *tmp = expr->right;
                expr->right = 0;
                delete expr;
                expr = tmp;
            }
            else { // copy left
                ibis::qExpr *tmp = expr->left;
                expr->left = 0;
                delete expr;
                expr = tmp;
            }
        }
        else if (expr->right != 0 && expr->right->isConstant() &&
                 expr->right->getType() == ibis::qExpr::COMPRANGE) {
            if (static_cast<ibis::compRange*>(expr->right)->inRange()) {
                // copy left
                ibis::qExpr *tmp = expr->left;
                expr->left = 0;
                delete expr;
                expr = tmp;
            }
            else { // copy right
                ibis::qExpr *tmp = expr->right;
                expr->right = 0;
                delete expr;
                expr = tmp;
            }
        }
        else if (expr->left != 0 && expr->right != 0 &&
                 expr->left->type == ibis::qExpr::RANGE &&
                 expr->right->type == ibis::qExpr::RANGE &&
                 stricmp(static_cast<ibis::qRange*>(expr->left)->colName(),
                         static_cast<ibis::qRange*>(expr->right)->colName())
                 == 0) {
            // two range conditions on the same variable
            ibis::qContinuousRange* tm1 =
                static_cast<ibis::qContinuousRange*>(expr->left);
            ibis::qContinuousRange* tm2 =
                static_cast<ibis::qContinuousRange*>(expr->right);
            if ((tm1->left_op == ibis::qExpr::OP_LE ||
                 tm1->left_op == ibis::qExpr::OP_LT) &&
                (tm2->left_op == ibis::qExpr::OP_LE ||
                 tm2->left_op == ibis::qExpr::OP_LT) &&
                (tm1->right_op == ibis::qExpr::OP_LE ||
                 tm1->right_op == ibis::qExpr::OP_LT) &&
                (tm2->right_op == ibis::qExpr::OP_LE ||
                 tm2->right_op == ibis::qExpr::OP_LT)) { // two two-sided ranges
                if (tm1->lower < tm2->lower) {
                    tm1->left_op = tm2->left_op;
                    tm1->lower = tm2->lower;
                }
                else if (tm1->lower == tm2->lower &&
                         tm1->left_op == ibis::qExpr::OP_LE &&
                         tm2->left_op == ibis::qExpr::OP_LT) {
                    tm1->left_op = ibis::qExpr::OP_LT;
                }
                if (tm1->upper > tm2->upper) {
                    tm1->right_op = tm2->right_op;
                    tm1->upper = tm2->upper;
                }
                else if (tm1->upper == tm2->upper &&
                         tm1->right_op == ibis::qExpr::OP_LE &&
                         tm2->right_op == ibis::qExpr::OP_LT) {
                    tm1->right_op = ibis::qExpr::OP_LT;
                }
                expr->left = 0;
                delete expr;
                expr = tm1;
            }
            else if ((tm1->left_op == ibis::qExpr::OP_LE ||
                      tm1->left_op == ibis::qExpr::OP_LT) &&
                     (tm2->left_op == ibis::qExpr::OP_LE ||
                      tm2->left_op == ibis::qExpr::OP_LT) &&
                     (tm1->right_op == ibis::qExpr::OP_LE ||
                      tm1->right_op == ibis::qExpr::OP_LT) &&
                     (tm2->right_op == ibis::qExpr::OP_UNDEFINED)) {
                // tm1 two-sided range, tm2 one-sided
                if (tm1->lower < tm2->lower) {
                    tm1->left_op = tm2->left_op;
                    tm1->lower = tm2->lower;
                }
                else if (tm1->lower == tm2->lower &&
                         tm1->left_op == ibis::qExpr::OP_LE &&
                         tm2->left_op == ibis::qExpr::OP_LT) {
                    tm1->left_op = ibis::qExpr::OP_LT;
                }
                expr->left = 0;
                delete expr;
                expr = tm1;
            }
            else if ((tm1->left_op == ibis::qExpr::OP_LE ||
                      tm1->left_op == ibis::qExpr::OP_LT) &&
                     (tm2->left_op == ibis::qExpr::OP_LE ||
                      tm2->left_op == ibis::qExpr::OP_LT) &&
                     (tm2->right_op == ibis::qExpr::OP_LE ||
                      tm2->right_op == ibis::qExpr::OP_LT) &&
                     (tm1->right_op == ibis::qExpr::OP_UNDEFINED)) {
                // tm1 one-sided range, tm2 two-sided
                if (tm2->lower < tm1->lower) {
                    tm2->left_op = tm1->left_op;
                    tm2->lower = tm1->lower;
                }
                else if (tm1->lower == tm2->lower &&
                         tm2->left_op == ibis::qExpr::OP_LE &&
                         tm1->left_op == ibis::qExpr::OP_LT) {
                    tm2->left_op = ibis::qExpr::OP_LT;
                }
                expr->right = 0;
                delete expr;
                expr = tm2;
            }
            else if ((tm1->left_op == ibis::qExpr::OP_LE ||
                      tm1->left_op == ibis::qExpr::OP_LT) &&
                     (tm2->right_op == ibis::qExpr::OP_LE ||
                      tm2->right_op == ibis::qExpr::OP_LT) &&
                     (tm1->right_op == ibis::qExpr::OP_LE ||
                      tm1->right_op == ibis::qExpr::OP_LT) &&
                     (tm2->left_op == ibis::qExpr::OP_UNDEFINED)) {
                // tm1 two-sided range, tm2 one-sided
                if (tm1->upper > tm2->upper) {
                    tm1->right_op = tm2->right_op;
                    tm1->upper = tm2->upper;
                }
                else if (tm1->upper == tm2->upper &&
                         tm1->right_op == ibis::qExpr::OP_LE &&
                         tm2->right_op == ibis::qExpr::OP_LT) {
                    tm1->right_op = ibis::qExpr::OP_LT;
                }
                expr->left = 0;
                delete expr;
                expr = tm1;
            }
            else if ((tm1->right_op == ibis::qExpr::OP_LE ||
                      tm1->right_op == ibis::qExpr::OP_LT) &&
                     (tm2->left_op == ibis::qExpr::OP_LE ||
                      tm2->left_op == ibis::qExpr::OP_LT) &&
                     (tm2->right_op == ibis::qExpr::OP_LE ||
                      tm2->right_op == ibis::qExpr::OP_LT) &&
                     (tm1->left_op == ibis::qExpr::OP_UNDEFINED)) {
                // tm1 one-sided range, tm2 two-sided
                if (tm2->upper > tm1->upper) {
                    tm2->right_op = tm1->right_op;
                    tm2->upper = tm1->upper;
                }
                else if (tm1->upper == tm2->upper &&
                         tm2->right_op == ibis::qExpr::OP_LE &&
                         tm1->right_op == ibis::qExpr::OP_LT) {
                    tm2->right_op = ibis::qExpr::OP_LT;
                }
                expr->right = 0;
                delete expr;
                expr = tm2;
            }
            else if ((tm1->left_op == ibis::qExpr::OP_LE ||
                      tm1->left_op == ibis::qExpr::OP_LT) &&
                     (tm2->left_op == ibis::qExpr::OP_LE ||
                      tm2->left_op == ibis::qExpr::OP_LT) &&
                     (tm1->right_op == ibis::qExpr::OP_UNDEFINED) &&
                     (tm2->right_op == ibis::qExpr::OP_UNDEFINED)) {
                // both one-sided
                if (tm1->lower < tm2->lower) {
                    tm1->left_op = tm2->left_op;
                    tm1->lower = tm2->lower;
                }
                else if (tm1->lower == tm2->lower &&
                         tm1->left_op == ibis::qExpr::OP_LE &&
                         tm2->left_op == ibis::qExpr::OP_LT) {
                    tm1->left_op = ibis::qExpr::OP_LT;
                }
                expr->left = 0;
                delete expr;
                expr = tm1;
            }
            else if ((tm1->right_op == ibis::qExpr::OP_LE ||
                      tm1->right_op == ibis::qExpr::OP_LT) &&
                     (tm2->right_op == ibis::qExpr::OP_LE ||
                      tm2->right_op == ibis::qExpr::OP_LT) &&
                     (tm2->left_op == ibis::qExpr::OP_UNDEFINED) &&
                     (tm1->left_op == ibis::qExpr::OP_UNDEFINED)) {
                // both one-sided
                if (tm2->upper > tm1->upper) {
                    tm2->right_op = tm1->right_op;
                    tm2->upper = tm1->upper;
                }
                else if (tm2->upper == tm1->upper &&
                         tm1->right_op == ibis::qExpr::OP_LT &&
                         tm2->right_op == ibis::qExpr::OP_LE) {
                    tm2->right_op = tm1->right_op;
                }
                expr->right = 0;
                delete expr;
                expr = tm2;
            }
            else if ((tm1->left_op == ibis::qExpr::OP_LE ||
                      tm1->left_op == ibis::qExpr::OP_LT) &&
                     (tm2->right_op == ibis::qExpr::OP_LE ||
                      tm2->right_op == ibis::qExpr::OP_LT) &&
                     (tm1->right_op == ibis::qExpr::OP_UNDEFINED) &&
                     (tm2->left_op == ibis::qExpr::OP_UNDEFINED)) {
                // both one-sided
                tm1->right_op = tm2->right_op;
                tm1->upper = tm2->upper;
                expr->left = 0;
                delete expr;
                expr = tm1;
            }
            else if ((tm1->right_op == ibis::qExpr::OP_LE ||
                      tm1->right_op == ibis::qExpr::OP_LT) &&
                     (tm2->left_op == ibis::qExpr::OP_LE ||
                      tm2->left_op == ibis::qExpr::OP_LT) &&
                     (tm1->left_op == ibis::qExpr::OP_UNDEFINED) &&
                     (tm2->right_op == ibis::qExpr::OP_UNDEFINED)) {
                // both one-sided
                tm1->left_op = tm2->left_op;
                tm1->lower = tm2->lower;
                expr->left = 0;
                delete expr;
                expr = tm1;
            }
            else if ((tm1->left_op == ibis::qExpr::OP_LE ||
                      tm1->left_op == ibis::qExpr::OP_LT) &&
                     (tm1->right_op == ibis::qExpr::OP_LE ||
                      tm1->right_op == ibis::qExpr::OP_LT)) {
                if (tm2->left_op == ibis::qExpr::OP_EQ) {
                    if (tm1->inRange(tm2->lower)) {
                        expr->right = 0;
                        delete expr;
                        expr = tm2;
                    }
                    else {
                        delete expr;
                        expr = ibis::compRange::makeConstantFalse();
                    }
                }
                else if (tm2->right_op == ibis::qExpr::OP_EQ) {
                    if (tm1->inRange(tm2->upper)) {
                        expr->right = 0;
                        delete expr;
                        expr = tm2;
                    }
                    else {
                        delete expr;
                        expr = ibis::compRange::makeConstantFalse();
                    }
                }
            }
            else if ((tm2->left_op == ibis::qExpr::OP_LE ||
                      tm2->left_op == ibis::qExpr::OP_LT) &&
                     (tm2->right_op == ibis::qExpr::OP_LE ||
                      tm2->right_op == ibis::qExpr::OP_LT)) {
                if (tm1->left_op == ibis::qExpr::OP_EQ) {
                    if (tm2->inRange(tm1->lower)) {
                        expr->left = 0;
                        delete expr;
                        expr = tm1;
                    }
                    else {
                        delete expr;
                        expr = ibis::compRange::makeConstantFalse();
                    }
                }
                else if (tm1->right_op == ibis::qExpr::OP_EQ) {
                    if (tm2->inRange(tm1->upper)) {
                        expr->left = 0;
                        delete expr;
                        expr = tm1;
                    }
                    else {
                        delete expr;
                        expr = ibis::compRange::makeConstantFalse();
                    }
                }
            }
            else if ((tm1->left_op == ibis::qExpr::OP_LE ||
                      tm1->left_op == ibis::qExpr::OP_LT) &&
                     (tm1->right_op == ibis::qExpr::OP_UNDEFINED)) {
                if (tm2->left_op == ibis::qExpr::OP_EQ) {
                    if (tm1->inRange(tm2->lower)) {
                        expr->right = 0;
                        delete expr;
                        expr = tm2;
                    }
                    else {
                        delete expr;
                        expr = ibis::compRange::makeConstantFalse();
                    }
                }
                else if (tm2->right_op == ibis::qExpr::OP_EQ) {
                    if (tm1->inRange(tm2->upper)) {
                        expr->right = 0;
                        delete expr;
                        expr = tm2;
                    }
                    else {
                        delete expr;
                        expr = ibis::compRange::makeConstantFalse();
                    }
                }
            }
            else if ((tm2->left_op == ibis::qExpr::OP_UNDEFINED) &&
                     (tm2->right_op == ibis::qExpr::OP_LE ||
                      tm2->right_op == ibis::qExpr::OP_LT)) {
                if (tm1->left_op == ibis::qExpr::OP_EQ) {
                    if (tm2->inRange(tm1->lower)) {
                        expr->left = 0;
                        delete expr;
                        expr = tm1;
                    }
                    else {
                        delete expr;
                        expr = ibis::compRange::makeConstantFalse();
                    }
                }
                else if (tm1->right_op == ibis::qExpr::OP_EQ) {
                    if (tm2->inRange(tm1->upper)) {
                        expr->left = 0;
                        delete expr;
                        expr = tm1;
                    }
                    else {
                        delete expr;
                        expr = ibis::compRange::makeConstantFalse();
                    }
                }
            }
        }
        break;}
    case ibis::qExpr::LOGICAL_OR: {
        simplify(expr->left);
        simplify(expr->right);
        bool emptyleft = (expr->left == 0 ||
                          ((expr->left->getType() == RANGE ||
                            expr->left->getType() == DRANGE) &&
                           reinterpret_cast<qRange*>(expr->left)->empty()));
        bool emptyright = (expr->right == 0 ||
                           ((expr->right->getType() == RANGE ||
                             expr->right->getType() == DRANGE) &&
                            reinterpret_cast<qRange*>(expr->right)->empty()));
        if (emptyleft) {
            if (emptyright) { // false
                delete expr;
                expr = ibis::compRange::makeConstantFalse();
            }
            else { // keep right
                ibis::qExpr* tmp = expr->right;
                expr->right = 0;
                delete expr;
                expr = tmp;
            }
        }
        else if (emptyright) { // keep left
            ibis::qExpr *tmp = expr->left;
            expr->left = 0;
            delete expr;
            expr = tmp;
        }
        else if (expr->left != 0 && expr->left->isConstant() &&
                 expr->left->type == ibis::qExpr::COMPRANGE) {
            if (static_cast<ibis::compRange*>(expr->left)->inRange()) {
                // copy left
                ibis::qExpr *tmp = expr->left;
                expr->left = 0;
                delete expr;
                expr = tmp;
            }
            else { // copy right
                ibis::qExpr *tmp = expr->right;
                expr->right = 0;
                delete expr;
                expr = tmp;
            }
        }
        else if (expr->right != 0 && expr->right->isConstant() &&
                 expr->right->getType() == ibis::qExpr::COMPRANGE) {
            if (static_cast<ibis::compRange*>(expr->right)->inRange()) {
                // copy right
                ibis::qExpr *tmp = expr->right;
                expr->right = 0;
                delete expr;
                expr = tmp;
            }
            else { // copy left
                ibis::qExpr *tmp = expr->left;
                expr->left = 0;
                delete expr;
                expr = tmp;
            }
        }
        break;}
    case ibis::qExpr::LOGICAL_XOR: {
        simplify(expr->left);
        simplify(expr->right);
        bool emptyleft = (expr->left == 0 ||
                          ((expr->left->getType() == RANGE ||
                            expr->left->getType() == DRANGE) &&
                           reinterpret_cast<qRange*>(expr->left)->empty()));
        bool emptyright = (expr->right == 0 ||
                           ((expr->right->getType() == RANGE ||
                             expr->right->getType() == DRANGE) &&
                            reinterpret_cast<qRange*>(expr->right)->empty()));
        if (emptyleft) {
            if (emptyright) { // false
                delete expr;
                expr = ibis::compRange::makeConstantFalse();
            }
            else { // keep right
                ibis::qExpr* tmp = expr->right;
                expr->right = 0;
                delete expr;
                expr = tmp;
            }
        }
        else if (emptyright) { // keep left
            ibis::qExpr *tmp = expr->left;
            expr->left = 0;
            delete expr;
            expr = tmp;
        }
        else if (expr->left != 0 && expr->left->isConstant() &&
                 expr->left->getType() == ibis::qExpr::COMPRANGE) {
            if (static_cast<ibis::compRange*>(expr->left)->inRange()) {
                // do nothing
            }
            else { // copy right
                ibis::qExpr *tmp = expr->right;
                expr->right = 0;
                delete expr;
                expr = tmp;
            }
        }
        else if (expr->right != 0 && expr->right->isConstant() &&
                 expr->right->getType() == ibis::qExpr::COMPRANGE) {
            if (static_cast<ibis::compRange*>(expr->right)->inRange()) {
                // do nothing
            }
            else { // copy left
                ibis::qExpr *tmp = expr->left;
                expr->left = 0;
                delete expr;
                expr = tmp;
            }
        }
        break;}
    case ibis::qExpr::LOGICAL_MINUS: {
        simplify(expr->left);
        simplify(expr->right);
        bool emptyleft = (expr->left == 0 ||
                          ((expr->left->getType() == RANGE ||
                            expr->left->getType() == DRANGE) &&
                           reinterpret_cast<qRange*>(expr->left)->empty()));
        bool emptyright = (expr->right == 0 ||
                           ((expr->right->getType() == RANGE ||
                             expr->right->getType() == DRANGE) &&
                            reinterpret_cast<qRange*>(expr->right)->empty()));
        if (emptyleft || emptyright) {
            // keep left: if left is empty, the overall result is empty;
            // if the right is empty, the overall result is the left
            ibis::qExpr* tmp = expr->left;
            expr->left = 0;
            delete expr;
            expr = tmp;
        }
        else if (expr->left != 0 && expr->left->isConstant() &&
                 expr->left->type == ibis::qExpr::COMPRANGE) {
            if (static_cast<ibis::compRange*>(expr->left)->inRange()) {
                // leave it alone
            }
            else { // copy left, whole expression is false
                ibis::qExpr *tmp = expr->left;
                expr->left = 0;
                delete expr;
                expr = tmp;
            }
        }
        else if (expr->right != 0 && expr->right->isConstant() &&
                 expr->right->getType() == ibis::qExpr::COMPRANGE) {
            if (static_cast<ibis::compRange*>(expr->right)->inRange()) {
                // whole expression is false
                delete expr;
                expr = ibis::compRange::makeConstantFalse();
            }
            else { // copy left
                ibis::qExpr *tmp = expr->left;
                expr->left = 0;
                delete expr;
                expr = tmp;
            }
        }
        break;}
    case ibis::qExpr::COMPRANGE: {
        ibis::compRange* cr = reinterpret_cast<ibis::compRange*>(expr);
        ibis::math::term *t1, *t2;
        t1 = reinterpret_cast<ibis::math::term*>(cr->getLeft());
        if (t1 != 0 && ibis::math::preserveInputExpressions == false) {
            t2 = t1->reduce();
            if (t2 != t1)
                cr->setLeft(t2);
        }

        t1 = reinterpret_cast<ibis::math::term*>(cr->getRight());
        if (t1 != 0 && ibis::math::preserveInputExpressions == false) {
            t2 = t1->reduce();
            if (t2 != t1)
                cr->setRight(t2);
        }

        t1 = reinterpret_cast<ibis::math::term*>(cr->getTerm3());
        if (t1 != 0 && ibis::math::preserveInputExpressions == false) {
            t2 = t1->reduce();
            if (t2 != t1)
                cr->setTerm3(t2);
        }

        if (cr->getLeft() != 0 && cr->getRight() != 0 && cr->getTerm3() != 0) {
            ibis::math::term* tm1 =
                reinterpret_cast<ibis::math::term*>(cr->getLeft());
            ibis::math::term* tm2 =
                reinterpret_cast<ibis::math::term*>(cr->getRight());
            ibis::math::term* tm3 =
                reinterpret_cast<ibis::math::term*>(cr->getTerm3());
            if (tm1->termType() == ibis::math::NUMBER &&
                tm3->termType() == ibis::math::NUMBER &&
                tm2->termType() == ibis::math::OPERATOR) {
                if (reinterpret_cast<ibis::math::term*>
                    (tm2->getLeft())->termType() == ibis::math::NUMBER &&
                    reinterpret_cast<ibis::math::term*>
                    (tm2->getRight())->termType() ==
                    ibis::math::VARIABLE) {
                    const ibis::math::bediener& op2 =
                        *static_cast<ibis::math::bediener*>(tm2);
                    double cnst = static_cast<ibis::math::number*>
                        (tm2->getLeft())->eval();
                    switch (op2.operador) {
                    default: break; // do nothing
                    case ibis::math::PLUS: {
                        ibis::qContinuousRange *tmp = new
                            ibis::qContinuousRange
                            (tm1->eval()-cnst, cr->leftOperator(),
                             static_cast<const ibis::math::variable*>
                             (op2.getRight())->variableName(),
                             cr->rightOperator(), tm2->eval()-cnst);
                        delete expr;
                        expr = tmp;
                        cr = 0;
                        break;}
                    case ibis::math::MINUS: {
                        ibis::qContinuousRange *tmp = new
                            ibis::qContinuousRange
                            (tm1->eval()+cnst, cr->leftOperator(),
                             static_cast<const ibis::math::variable*>
                             (op2.getRight())->variableName(),
                             cr->rightOperator(), tm2->eval()+cnst);
                        delete expr;
                        expr = tmp;
                        cr = 0;
                        break;}
                    case ibis::math::MULTIPLY: {
                        if (cnst > 0.0) {
                            ibis::qContinuousRange *tmp = new
                                ibis::qContinuousRange
                                (tm1->eval()/cnst, cr->leftOperator(),
                                 static_cast<const ibis::math::variable*>
                                 (op2.getRight())->variableName(),
                                 cr->rightOperator(), tm2->eval()/cnst);
                            delete expr;
                            expr = tmp;
                            cr = 0;
                        }
                        break;}
                    }
                }
            }
        } // three terms
        else if (cr->getLeft() != 0 && cr->getRight() != 0) { // two terms
            ibis::math::term* tm1 =
                reinterpret_cast<ibis::math::term*>(cr->getLeft());
            ibis::math::term* tm2 =
                reinterpret_cast<ibis::math::term*>(cr->getRight());
            if (tm1->termType() == ibis::math::NUMBER &&
                tm2->termType() == ibis::math::OPERATOR) {
                ibis::math::number* nm1 =
                    static_cast<ibis::math::number*>(tm1);
                ibis::math::bediener* op2 =
                    static_cast<ibis::math::bediener*>(tm2);
                ibis::math::term* tm21 =
                    reinterpret_cast<ibis::math::term*>(tm2->getLeft());
                ibis::math::term* tm22 =
                    reinterpret_cast<ibis::math::term*>(tm2->getRight());
                if (tm21->termType() == ibis::math::NUMBER) {
                    switch (op2->operador) {
                    default: break;
                    case ibis::math::PLUS: {
                        nm1->val -= tm21->eval();
                        cr->getRight() = tm22;
                        op2->getRight() = 0;
                        delete op2;
                        cr = 0;
                        ibis::qExpr::simplify(expr);
                        break;}
                    case ibis::math::MINUS: {
                        cr->getLeft() = tm22;
                        nm1->val = tm21->eval() - nm1->val;
                        cr->getRight() = nm1;
                        op2->getRight() = 0;
                        delete op2;
                        cr = 0;
                        ibis::qExpr::simplify(expr);
                        break;}
                    case ibis::math::MULTIPLY: {
                        const double cnst = tm21->eval();
                        if (cnst > 0.0) {
                            nm1->val /= cnst;
                            cr->getRight() = tm22;
                            op2->getRight() = 0;
                            delete op2;
                            cr = 0;
                            ibis::qExpr::simplify(expr);
                        }
                        else {
                            nm1->val /= tm21->eval();
                            op2->getRight() = 0;
                            delete op2;
                            cr->getRight() = nm1;
                            cr->getLeft() = tm22;
                            cr = 0;
                            ibis::qExpr::simplify(expr);
                        }
                        break;}
                    case ibis::math::DIVIDE: {
                        nm1->val = tm21->eval() / nm1->val;
                        cr->getLeft() = tm22;
                        cr->getRight() = nm1;
                        op2->getRight() = 0;
                        delete op2;
                        cr = 0;
                        ibis::qExpr::simplify(expr);
                        break;}
                    }
                }
                else if (tm22->termType() == ibis::math::NUMBER) {
                    switch (op2->operador) {
                    default: break;
                    case ibis::math::PLUS: {
                        nm1->val -= tm21->eval();
                        cr->getRight() = tm22;
                        op2->getLeft() = 0;
                        delete op2;
                        cr = 0;
                        ibis::qExpr::simplify(expr);
                        break;}
                    case ibis::math::MINUS: {
                        nm1->val += tm22->eval();
                        cr->getRight() = tm21;
                        op2->getLeft() = 0;
                        delete op2;
                        cr = 0;
                        ibis::qExpr::simplify(expr);
                        break;}
                    case ibis::math::MULTIPLY: {
                        const double cnst = tm22->eval();
                        if (cnst > 0.0) {
                            cr->getRight() = tm21;
                            nm1->val /= cnst;
                            op2->getLeft() = 0;
                            delete op2;
                            cr = 0;
                            ibis::qExpr::simplify(expr);
                        }
                        else {
                            nm1->val /= tm22->eval();
                            op2->getLeft() = 0;
                            delete op2;
                            cr->getRight() = nm1;
                            cr->getLeft() = tm21;
                            cr = 0;
                            ibis::qExpr::simplify(expr);
                        }
                        break;}
                    case ibis::math::DIVIDE: {
                        nm1->val *= tm22->eval();
                        cr->getRight() = tm21;
                        op2->getLeft() = 0;
                        delete op2;
                        cr = 0;
                        ibis::qExpr::simplify(expr);
                        break;}
                    }
                }
            }
            else if (tm1->termType() == ibis::math::OPERATOR &&
                     tm2->termType() == ibis::math::NUMBER) {
                ibis::math::bediener* op1 =
                    static_cast<ibis::math::bediener*>(tm1);
                ibis::math::number* nm2 =
                    static_cast<ibis::math::number*>(tm2);
                ibis::math::term* tm11 =
                    reinterpret_cast<ibis::math::term*>(tm1->getLeft());
                ibis::math::term* tm12 =
                    reinterpret_cast<ibis::math::term*>(tm1->getRight());
                if (tm11->termType() == ibis::math::NUMBER) {
                    switch (op1->operador) {
                    default: break;
                    case ibis::math::PLUS: {
                        nm2->val -= tm11->eval();
                        cr->getLeft() = tm12;
                        op1->getRight() = 0;
                        delete op1;
                        cr = 0;
                        ibis::qExpr::simplify(expr);
                        break;}
                    case ibis::math::MINUS: {
                        cr->getRight() = tm12;
                        nm2->val = tm11->eval() - nm2->val;
                        cr->getLeft() = nm2;
                        op1->getRight() = 0;
                        cr = 0;
                        delete op1;
                        ibis::qExpr::simplify(expr);
                        break;}
                    case ibis::math::MULTIPLY: {
                        const double cnst = tm11->eval();
                        if (cnst > 0.0) {
                            cr->getLeft() = tm12;
                            nm2->val /= cnst;
                            op1->getRight() = 0;
                            delete op1;
                            cr = 0;
                            ibis::qExpr::simplify(expr);
                        }
                        else {
                            nm2->val /= tm11->eval();
                            op1->getRight() = 0;
                            delete op1;
                            cr->getLeft() = nm2;
                            cr->getRight() = tm12;
                            cr = 0;
                            ibis::qExpr::simplify(expr);
                        }
                        break;}
                    case ibis::math::DIVIDE: {
                        if (nm2->val > 0.0) {
                            nm2->val = tm11->eval() / nm2->val;
                            cr->getLeft() = nm2;
                            cr->getRight() = tm12;
                            op1->getRight() = 0;
                            delete op1;
                            cr = 0;
                            ibis::qExpr::simplify(expr);
                        }
                        break;}
                    }
                }
            }
        } // two terms

        if (cr != 0 && cr != expr) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
            LOGGER(ibis::gVerbose >= 0)
                << "replace a compRange with a qRange " << *expr;
#endif
            expr = cr->simpleRange();
            delete cr;
        }
        else if (expr->getType() == ibis::qExpr::COMPRANGE &&
                 static_cast<ibis::compRange*>(expr)->isSimpleRange()) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
            LOGGER(ibis::gVerbose >= 0)
                << "replace a compRange with a qRange " << *expr;
#endif
            cr = static_cast<ibis::compRange*>(expr);
            expr = cr->simpleRange();
            delete cr;
        }
        break;}
    case ibis::qExpr::RANGE: { // a continuous range
        // ibis::qContinuousRange *cr =
        //     reinterpret_cast<ibis::qContinuousRange*>(expr);
        // if (cr->empty()) {
        //     expr = new ibis::math::number((double)0.0);
        //     delete cr;
        // }
        break;}
    case ibis::qExpr::DRANGE: { // break a DRANGE into multiple RANGE
        ibis::qDiscreteRange *dr =
            reinterpret_cast<ibis::qDiscreteRange*>(expr);
        if (dr->nItems() < 3) {
            ibis::qExpr *tmp = dr->convert();
            delete expr;
            expr = tmp;
        }
        break;}
    case ibis::qExpr::ANYSTRING: { // break a ANYSTRING into multiple STRING
        ibis::qAnyString *astr = reinterpret_cast<ibis::qAnyString*>(expr);
        if (astr->valueList().size() < 3) {
            ibis::qExpr *tmp = astr->convert();
            delete expr;
            expr = tmp;
        }
        break;}
    case ibis::qExpr::DEPRECATEDJOIN: {
        ibis::math::term *range =
            reinterpret_cast<ibis::deprecatedJoin*>(expr)->getRange();
        if (range != 0 && ibis::math::preserveInputExpressions == false) {
            ibis::math::term *tmp = range->reduce();
            if (tmp != range)
                reinterpret_cast<ibis::deprecatedJoin*>(expr)->setRange(tmp);
        }
        break;}
    } // switch(...

    if (ibis::gVerbose > 5 || (ibis::gVerbose >= 0 && expr == 0)) {
        ibis::util::logger lg;
        if (expr != 0) {
            lg() << "qExpr::simplify -- output expression "
                 << "(@" << static_cast<const void*>(expr) << ") ";
            if (ibis::gVerbose > 7)
                expr->printFull(lg());
            else
                expr->print(lg());
        }
        else {
            lg() << "Warning -- qExpr::simplify has turned a non-nil "
                "expression into nil";
        }
    }
} // ibis::qExpr::simplify

/// The short-form of the print function.  It only prints information about
/// the current node of the query expression tree.
void ibis::qExpr::print(std::ostream& out) const {
    out << '(';
    switch (type) {
    case LOGICAL_AND: {
        out << static_cast<const void*>(left) << " AND "
            << static_cast<const void*>(right);
        break;
    }
    case LOGICAL_OR: {
        out << static_cast<const void*>(left) << " OR "
            << static_cast<const void*>(right);
        break;
    }
    case LOGICAL_XOR: {
        out << static_cast<const void*>(left) << " XOR "
            << static_cast<const void*>(right);
        break;
    }
    case LOGICAL_MINUS: {
        out << static_cast<const void*>(left) << " AND NOT "
            << static_cast<const void*>(right);
        break;
    }
    case LOGICAL_NOT: {
        out << " ! " << static_cast<const void*>(left);
        break;
    }
    default:
        out << "UNKNOWN LOGICAL OPERATOR";
    }
    out << ')';
} // ibis::qExpr::print

/// The long form of the print function.  It recursively prints out the
/// whole query expression tree, which can be quite long.
void ibis::qExpr::printFull(std::ostream& out) const {
    switch (type) {
    case LOGICAL_AND: {
        out << '(';
        left->printFull(out);
        out << " AND ";
        right->printFull(out);
        out << ')';
        break;
    }
    case LOGICAL_OR: {
        out << '(';
        left->printFull(out);
        out << " OR ";
        right->printFull(out);
        out << ')';
        break;
    }
    case LOGICAL_XOR: {
        out << '(';
        left->printFull(out);
        out << " XOR ";
        right->printFull(out);
        out << ')';
        break;
    }
    case LOGICAL_MINUS: {
        out << '(';
        left->printFull(out);
        out << " AND NOT ";
        right->printFull(out);
        out << ')';
        break;
    }
    case LOGICAL_NOT: {
        out << "( ! ";
        left->printFull(out);
        out << ')';
        break;
    }
    default:
        print(out);
        break;
    }
} // ibis::qExpr::printFull

/// Make the expression tree lean left.
void ibis::qExpr::adjust() {
    ibis::qExpr* lptr = left;
    ibis::qExpr* rptr = right;
    if (left && right) {
        if (type == LOGICAL_AND || type == LOGICAL_OR || type == LOGICAL_XOR) {
            if (type == right->type) {
                if (type == left->type) {
                    right = rptr->left;
                    rptr->left = left;
                    left = rptr;
                }
                else if (left->left == 0 && left->right == 0) {
                    right = lptr;
                    left = rptr;
                }
            }
            else if (left->isTerminal() && ! right->isTerminal()) {
                right = lptr;
                left = rptr;
            }
        }
    }
    if (left && !(left->isTerminal()))
        left->adjust();
    if (right && !(right->isTerminal()))
        right->adjust();
} // ibis::qExpr::adjust

/// After reordering, the lightest weight is one the left side of a group
/// of commutable operators.
double ibis::qExpr::reorder(const ibis::qExpr::weight& wt) {
    double ret = 0.0;
    if (directEval()) {
        ret = wt(this);
        return ret;
    }

    if (ibis::gVerbose > 5) {
        ibis::util::logger lg;
        lg() << "qExpr::reorder -- input: ";
        if (ibis::gVerbose > 7)
            printFull(lg());
        else
            print(lg());
    }

    adjust(); // to make sure the evaluation tree is a chain
    std::vector<ibis::qExpr*> terms;
    std::vector<double> wgt;
    ibis::qExpr* ptr;
    if (type == LOGICAL_AND || type == LOGICAL_OR || type == LOGICAL_XOR) {
        uint32_t i, j, k;
        double tmp;
        if (right->directEval()) {
            ret = wt(right);
        } // if (right->directEval())
        else {
            ret = right->reorder(wt);
        }
        terms.push_back(right);
        wgt.push_back(ret);

        ptr = left;
        while (ptr->type == type) {
            // loop for left child of the same type
            if (ptr->right->directEval()) {
                tmp = wt(ptr->right);
                LOGGER(ibis::gVerbose > 8)
                    << "qExpr::reorder -- adding term " << *(ptr->right)
                    << " with weight " << tmp;
            }
            else {
                tmp = ptr->right->reorder(wt);
                LOGGER(ibis::gVerbose > 8)
                    << "qExpr::reorder -- adding subexpression "
                    << static_cast<const void*>(ptr->right)
                    << " with weight " << tmp;
            }
            terms.push_back(ptr->right);
            wgt.push_back(tmp);
            ptr = ptr->left;
            ret += tmp;
        }

        // left child is no longer the same type
        if (ptr->directEval()) {
            tmp = wt(ptr);
        }
        else {
            tmp = ptr->reorder(wt);
        }
        terms.push_back(ptr);
        wgt.push_back(tmp);
        ret += tmp;

        // all node connected by the same operator are collected together in
        // terms.  Next, separate the terminal nodes from the others
        i = 0;
        j = terms.size() - 1;
        while (i < j) {
            if (terms[i]->directEval()) {
                ++ i;
            }
            else if (terms[j]->directEval()) {
                ptr = terms[i];
                terms[i] = terms[j];
                terms[j] = ptr;
                -- j;
                ++ i;
            }
            else {
                -- j;
            }
        }
        if (terms[i]->directEval())
            ++ i;

        // sort the array terms[i,...] according to wgt -- the heaviest
        // elements are ordered first because they are copied first back
        // into the tree structure as the right nodes, when the tree is
        // travesed in-order(and left-to-right), this results in the
        // lightest elements being evaluated first
        k = terms.size() - 1; // terms.size() >= 2
        for (i = 0; i < k; ++i) {
            j = i + 1;
            // find the one with largest weight in [i+1, ...)
            for (uint32_t i0 = i+2; i0 <= k; ++ i0) {
                if ((wgt[i0] > wgt[j]) ||
                    (wgt[i0] == wgt[j] && terms[i0]->directEval() &&
                     ! (terms[j]->directEval())))
                    j = i0;
            }

            if (wgt[i] < wgt[j] ||
                (wgt[i] == wgt[j] && terms[j]->directEval() &&
                 ! (terms[i]->directEval()))) {
                // term i is not the largest, or term i can not be directly
                // evaluated
                ptr = terms[i];
                terms[i] = terms[j];
                terms[j] = ptr;
                tmp = wgt[i];
                wgt[i] = wgt[j];
                wgt[j] = tmp;
            }
            else { // term i is the largest, term j must be second largest
                ++ i;
                if (j > i) {
                    ptr = terms[i];
                    terms[i] = terms[j];
                    terms[j] = ptr;
                    tmp = wgt[i];
                    wgt[i] = wgt[j];
                    wgt[j] = tmp;
                }
            }
        }

        //#if DEBUG+0 > 0 || _DEBUG+0 > 0
        if (ibis::gVerbose > 4) {
            ibis::util::logger lg(4);
            lg() << "DEBUG -- qExpr::reorder(" << *this
                 << ") -- (expression:weight,...)\n";
            for (i = 0; i < terms.size(); ++ i)
                lg() << *(terms[i]) << ":" << wgt[i] << ", ";
        }
        //#endif

        // populate the tree -- copy the heaviest nodes first to the right
        ptr = this;
        for (i = 0; i < k; ++ i) {
            ptr->right = terms[i];
            if (i+1 < k)
                ptr = ptr->left;
        }
        ptr->left = terms[k];
    } // if (type == LOGICAL_AND...
    else if (type == LOGICAL_MINUS) {
        ret = left->reorder(wt);
        ret += right->reorder(wt);
    } // else if (type == LOGICAL_MINUS)
    else { // fallback, see if the weight operator could do something
        ret = wt(this);
    }

    if (ibis::gVerbose > 4) {
        ibis::util::logger lg;
        lg() << "qExpr::reorder -- output (" << ret << ", @"
             << static_cast<const void*>(this) << "): ";
        if (ibis::gVerbose > 7)
            printFull(lg());
        else
            print(lg());
    }
    return ret;
} // ibis::qExpr::reorder

/// The terms that are simply range conditions are placed in simple, and
/// the remaining conditions are left in tail.
/// It returns 0 if there is a mixture of simple and complex conditions.
/// In this case, both simple and tail would be non-nil.  The return value
/// is -1 if all conditions are complex and 1 if all conditions are simple.
/// In these two cases, both simple and tail are nil.
int ibis::qExpr::separateSimple(ibis::qExpr *&simple,
                                ibis::qExpr *&tail) const {
    if (ibis::gVerbose > 12) {
        ibis::util::logger lg;
        lg() << "qExpr::separateSimple -- input: ";
        print(lg());
    }

    int ret = INT_MAX;
    std::vector<const ibis::qExpr*> terms;
    const ibis::qExpr* ptr;
    if (type == LOGICAL_AND) {
        uint32_t i, j;
        // after adjust only one term is on the right-hand side
        terms.push_back(right);

        ptr = left;
        while (ptr->type == type) {
            // loop for left child of the same type
            terms.push_back(ptr->right);
            ptr = ptr->left;
        }

        // left child is no longer the same type
        terms.push_back(ptr);

        // all node connected by the same operator are collected together in
        // terms.  Next, separate the simple nodes from the others
        i = 0;
        j = terms.size() - 1;
        while (i < j) {
            if (terms[i]->isSimple()) {
                ++ i;
            }
            else if (terms[j]->isSimple()) {
                ptr = terms[i];
                terms[i] = terms[j];
                terms[j] = ptr;
                -- j;
                ++ i;
            }
            else {
                -- j;
            }
        }
        if (terms[i]->isSimple())
            ++ i;

#if DEBUG+0 > 0 || _DEBUG+0 > 0
        if (ibis::gVerbose > 0) {
            ibis::util::logger lg(4);
            lg() << "qExpr::separateSimple -- terms joined with AND\n";
            for (i=0; i<terms.size(); ++i)
                lg() << *(terms[i]) << "\n";
        }
#endif

        if (i > 1 && i < terms.size()) {
            // more than one term, need AND operators
            simple = new ibis::qExpr(ibis::qExpr::LOGICAL_AND,
                                     terms[0]->dup(), terms[1]->dup());
            for (j = 2; j < i; ++ j)
                simple = new ibis::qExpr(ibis::qExpr::LOGICAL_AND,
                                         simple, terms[j]->dup());
        }
        else if (i == 1) {
            simple = terms[0]->dup();
        }
        else { // no simple conditions, or all simple conditions
            simple = 0;
        }
        if (i == 0 || i >= terms.size()) {
            // no complex conditions, or only complex conditions
            tail = 0;
        }
        else if (terms.size() > i+1) { // more than one complex terms
            tail = new ibis::qExpr(ibis::qExpr::LOGICAL_AND,
                                   terms[i]->dup(), terms[i+1]->dup());
            for (j = i+2; j < terms.size(); ++ j)
                tail = new ibis::qExpr(ibis::qExpr::LOGICAL_AND,
                                       tail, terms[j]->dup());
        }
        else { // only one complex term
            tail = terms[i]->dup();
        }
        if (i == 0) // nothing simple
            ret = -1;
        else if (i < terms.size()) // mixed simple and complex
            ret = 0;
        else // all simple
            ret = 1;
    } // if (type == LOGICAL_AND)
    else if (isSimple()) {
        simple = 0;
        tail = 0;
        ret = 1;
    }
    else {
        simple = 0;
        tail = 0;
        ret = -1;
    }

    if (ibis::gVerbose > 12) {
        ibis::util::logger lg;
        switch (ret) {
        default:
        case 0:
            if (simple) {
                lg() << "qExpr::separateSimple -- simple  "
                    "conditions: ";
                simple->print(lg());
                lg() << "\n";
            }
            if (tail) {
                lg()<< "qExpr::separateSimple -- complex "
                    "conditions: ";
                tail->print(lg());
                lg() << "\n";
            }
            break;
        case -1:
            lg() << "qExpr::separateSimple -- no simple terms";
            break;
        case 1:
            lg() << "qExpr::separateSimple -- all simple terms";
            break;
        }
    }
    return ret;
} // ibis::qExpr::separateSimple

ibis::qRange* ibis::qExpr::findRange(const char *vname) {
    ibis::qRange *ret = 0;
    if (type == RANGE || type == DRANGE) {
        ret = reinterpret_cast<ibis::qRange*>(this);
        if (stricmp(vname, ret->colName()) != 0)
            ret = 0;
        return ret;
    }
    else if (type == LOGICAL_AND) {
        if (left)
            ret = left->findRange(vname);
        if (ret == 0) {
            if (right)
                ret = right->findRange(vname);
        }
        return ret;
    }
    else {
        return ret;
    }
} // ibis::qExpr::findRange

/// Extract the top-level conjunctive terms.  If the top-most operator is
/// not the AND operator, the whole expression tree is considered one term.
/// Because this function may be called recursively, the argument ttl is
/// not cleared by this function.  The caller needs to make sure it is
/// cleared on input.
void ibis::qExpr::getConjunctiveTerms(ibis::qExpr::termTableList& ttl) const {
    // extract all top-level terms
    if (type == ibis::qExpr::LOGICAL_AND) {
        if (left != 0)
            left->getConjunctiveTerms(ttl);
        if (right != 0)
            right->getConjunctiveTerms(ttl);
    }
    else {
        TTN tmp;
        tmp.term = this;
        getTableNames(tmp.tnames);
        ttl.push_back(tmp);
    }
} // ibis::qExpr::getConjunctiveTerms

/// Extract the data partition name from the column name cn.  It looks for
/// the first period '.' in the column name.  If a period is found, the
/// characters before the period is returned as a string, otherwise, an
/// empty string is returned.  @note The data partition name will be
/// outputed in lowercase characters.
std::string ibis::qExpr::extractTableName(const char* cn) {
    std::string ret;
    if (cn != 0) {
        const char *period = strchr(cn, '.');
        for (const char *chr = cn; chr < period; ++ chr)
            ret += std::tolower(*chr);
    }
    return ret;
} // ibis::qExpr::extractTableName

/// Split the incoming name into data partition name and column name.  It
/// looks for the first period '.' in the incoming name.  If a period is
/// found, the characters before the period is returned as pn, and the
/// characters after the period is returned as cn.  If no period is found,
/// pn will be a blank string and cn will be a copy of inm.
///
/// @note Both output names will be in lower case only.
void ibis::qExpr::splitColumnName(const char* inm, std::string& pn,
                                  std::string& cn) {
    pn.clear();
    cn.clear();
    if (inm != 0) {
        const char *period = strchr(inm, '.');
        if (period > inm) { // found a period in the input name
            for (const char *ptr = inm; ptr < period; ++ ptr)
                pn += std::tolower(*ptr);
            for (const char *ptr = period+1; *ptr != 0; ++ ptr)
                cn += std::tolower(*ptr);
        }
        else {
            cn = inm;
        }
    }
} // ibis::qExpr::splitColumnName

/// Return the list of data partition names in a set.  It records a '*' for
/// the variables without explicit partition names.
void ibis::qExpr::getTableNames(std::set<std::string>& plist) const {
    if (left != 0)
        left->getTableNames(plist);
    if (right != 0)
        right->getTableNames(plist);
} // ibis::qExpr::getTableNames

/// Extract conjunctive terms of the deprecated joins.
void ibis::qExpr::extractDeprecatedJoins
(std::vector<const deprecatedJoin*>& terms) const {
    if (type == LOGICAL_AND) {
        if (left != 0)
            left->extractDeprecatedJoins(terms);
        if (right != 0)
            right->extractDeprecatedJoins(terms);
    }
    else if (type == DEPRECATEDJOIN) {
        terms.push_back(reinterpret_cast<const deprecatedJoin*>(this));
    }
} // ibis::qExpr::extractDeprecatedJoins

/// Construct a qRange directly from a string representation of the constants.
ibis::qContinuousRange::qContinuousRange
(const char *lstr, qExpr::COMPARE lop, const char* prop,
 qExpr::COMPARE rop, const char *rstr)
    : qRange(RANGE), name(ibis::util::strnewdup(prop)),
      left_op(lop), right_op(rop) {
    // first convert the values from string format to double format
    if (lstr)
        lower = (*lstr)?strtod(lstr, 0):(-DBL_MAX);
    else
        lower = -DBL_MAX;
    if (rstr)
        upper = (*rstr)?strtod(rstr, 0):(DBL_MAX);
    else
        upper = DBL_MAX;
#if DEBUG+0 > 0 || _DEBUG+0 > 0
    LOGGER(ibis::gVerbose >= 0)
        << "column: " << name << "\n"
        << "left string: \"" << (lstr?lstr:"<NULL>")
        << "\", right string: \"" << (rstr?rstr:"<NULL>") << "\"\n"
        << lower << ", " << name << ", " << upper;
#endif

    // make show the left operator is OP_LE and the right one is OP_LT
    if (left_op == qExpr::OP_LT) { // change open left boundary to close one
        left_op = qExpr::OP_LE;
        lower = ibis::util::incrDouble(lower);
    }
    else if (left_op == qExpr::OP_EQ) {
        right_op = qExpr::OP_UNDEFINED;
        upper = lower;
    }
    if (right_op == qExpr::OP_LE) { // change closed right boundary to open
        right_op = qExpr::OP_LT;
        upper = ibis::util::incrDouble(upper);
    }
    else if (right_op == qExpr::OP_EQ) {
        left_op = qExpr::OP_UNDEFINED;
        lower = upper;
    }
} // constructor of qContinuousRange

void ibis::qContinuousRange::restrictRange(double left, double right) {
    if ((left_op == OP_GT || left_op == OP_GE) &&
        (right_op == OP_GT || right_op == OP_GE)) { // swap left and right
        if (left_op == OP_GT)
            left_op = OP_LT;
        else
            left_op = OP_LE;
        if (right_op == OP_GT)
            right_op = OP_LT;
        else
            right_op = OP_LE;

        double tmp = lower;
        lower = upper;
        upper = tmp;
    }
    if (((left_op == OP_LT || left_op == OP_LE) && lower < left) ||
        (left_op == OP_UNDEFINED &&
         (right_op == OP_LT || right_op == OP_LE))) {
        lower = left;
        left_op = OP_LE;
    }
    if (((right_op == OP_LT || right_op == OP_LE) && upper > right) ||
        ((left_op == OP_LT || left_op == OP_LE) &&
         right_op == OP_UNDEFINED)) {
        upper = right;
        right_op = OP_LE;
    }
    if ((left_op == OP_EQ && right_op == OP_UNDEFINED &&
         (lower < left || lower > right)) ||
        (left_op == OP_UNDEFINED && right_op == OP_EQ &&
         (upper < left || upper > right))) { // empty range
        left_op = OP_EQ;
        right_op = OP_EQ;
        lower = left;
        upper = (right > left ? right : left + 1.0);
    }
} // ibis::qContinuousRange::restrictRange

bool ibis::qContinuousRange::empty() const {
    if ((left_op == OP_LT || left_op == OP_LE) &&
        (right_op == OP_LT || right_op == OP_LE)) {
        return (lower > upper || (lower == upper &&
                                  (left_op != OP_LE || right_op != OP_LE)));
    }
    else if (left_op == OP_EQ && right_op == OP_EQ) {
        return (lower != upper);
    }
    else if ((left_op == OP_GT || left_op == OP_GE) &&
             (right_op == OP_GT || right_op == OP_GE)) {
        return (upper > lower ||
                (lower == upper && (left_op != OP_GE || right_op != OP_GE)));
    }
    else {
        return false;
    }
} // ibis::qContinuousRange::empty

void ibis::qContinuousRange::print(std::ostream& out) const {
    if (name == 0 || *name == 0) {
        out << "ILL-DEFINED-RANGE";
        return;
    }
    if (left_op == OP_UNDEFINED && right_op == OP_UNDEFINED) {
        out << name << " NOT NULL";
        return;
    }

    switch (left_op) {
    case OP_EQ: {
        out << lower << " == ";
        break;
    }
    case OP_LT: {
        out << lower << " < ";
        break;
    } // case OP_LT
    case OP_LE: {
        out << lower << " <= ";
        break;
    } // case OP_LE
    case OP_GT: {
        out << lower << " > ";
        break;
    } // case OP_GT
    case OP_GE: {
        out << lower << " >= ";
        break;
    } // case OP_GE
    default:
        break;
    } // switch (left_op)
    out << name;
    switch (right_op) {
    case OP_EQ:
        out << " == " << upper;
        break;
    case OP_LT:
        out << " < " << upper;
        break;
    case OP_LE:
        out << " <= " << upper;
        break;
    case OP_GT:
        out << " > " << upper;
        break;
    case OP_GE:
        out << " >= " << upper;
        break;
    default:
        break;
    } // end of switch right_op
} // ibis::qContinuousRange::print

void ibis::qContinuousRange::printFull(std::ostream& out) const {
    if (name == 0 || *name == 0) {
        out << "ILL-DEFINED-RANGE";
        return;
    }
    if (left_op == OP_UNDEFINED && right_op == OP_UNDEFINED) {
        out << name << " NOT NULL";
        return;
    }

    switch (left_op) {
    case OP_EQ: {
        out << std::setprecision(16) << lower << " == ";
        break;
    }
    case OP_LT: {
        out << std::setprecision(16) << lower << " < ";
        break;
    } // case OP_LT
    case OP_LE: {
        out << std::setprecision(16) << lower << " <= ";
        break;
    } // case OP_LE
    case OP_GT: {
        out << std::setprecision(16) << lower << " > ";
        break;
    } // case OP_GT
    case OP_GE: {
        out << std::setprecision(16) << lower << " >= ";
        break;
    } // case OP_GE
    default:
        break;
    } // switch (left_op)
    out << name;
    switch (right_op) {
    case OP_EQ:
        out << " == " << std::setprecision(16) << upper;
        break;
    case OP_LT:
        out << " < " << std::setprecision(16) << upper;
        break;
    case OP_LE:
        out << " <= " << std::setprecision(16) << upper;
        break;
    case OP_GT:
        out << " > " << std::setprecision(16) << upper;
        break;
    case OP_GE:
        out << " >= " << std::setprecision(16) << upper;
        break;
    default:
        break;
    } // end of switch right_op
} // ibis::qContinuousRange::printFull

/// Is val in the specified range?  Return true if the incoming value is in
/// the specified range.
bool ibis::qContinuousRange::inRange(double val) const {
    volatile bool res0 = true; 
    volatile bool res1 = true; 
    switch (left_op) {
    case OP_LT: res0 = (lower < val); break;
    case OP_LE: res0 = (lower <= val); break;
    case OP_GT: res0 = (lower > val); break;
    case OP_GE: res0 = (lower >= val); break;
    case OP_EQ: res0 = (lower == val); break;
    default: break;
    } // switch (left_op)
    switch (right_op) {
    case OP_LT: res1 = (val < upper); break;
    case OP_LE: res1 = (val <= upper); break;
    case OP_GT: res1 = (val > upper); break;
    case OP_GE: res1 = (val >= upper); break;
    case OP_EQ: res1 = (val == upper); break;
    default:    break;
    }
    return res0 && res1;
} // ibis::qContinuousRange::inRange

// Does the given range overlap with the query range?  Returns true for
// yes, false for no.  The arguements lo and hi are both included in the
// range specified.  This is consistent with how the the two arguments to
// the SQL clause "v between a and b."
bool ibis::qContinuousRange::overlap(double lo, double hi) const {
    if (! (lo <= hi)) { // invalue (lo, hi) pair, assume overlap
        return true;
    }

    bool ret = false;
    switch (left_op) {
    case OP_LT:
        if (lower < hi) {
            switch (right_op) {
            case OP_LT:
                ret = (lo < upper);
                break;
            case OP_LE:
                ret = (lo <= upper);
                break;
            case OP_GT:
                ret = (hi > upper);
                break;
            case OP_GE:
                ret = (hi >= upper);
                break;
            case OP_EQ:
                ret = (lower < upper);
                break;
            default:
                ret = true;
                break;
            }
        }
        break;
    case OP_LE:
        if (lower <= hi) {
            switch (right_op) {
            case OP_LT:
                ret = (lo < upper);
                break;
            case OP_LE:
                ret = (lo <= upper);
                break;
            case OP_GT:
                ret = (hi > upper);
                break;
            case OP_GE:
                ret = (hi >= upper);
                break;
            case OP_EQ:
                ret = (lower <= upper);
                break;
            default:
                ret = true;
                break;
            }
        }
        break;
    case OP_GT:
        if (lower > lo) {
            switch (right_op) {
            case OP_LT:
                ret = (lo < upper);
                break;
            case OP_LE:
                ret = (lo <= upper);
                break;
            case OP_GT:
                ret = (hi > upper);
                break;
            case OP_GE:
                ret = (hi >= upper);
                break;
            case OP_EQ:
                ret = (lower > upper);
                break;
            default:
                ret = true;
                break;
            }
        }
        break;
    case OP_GE:
        if (lower >= lo) {
            switch (right_op) {
            case OP_LT:
                ret = (lo < upper);
                break;
            case OP_LE:
                ret = (lo <= upper);
                break;
            case OP_GT:
                ret = (hi > upper);
                break;
            case OP_GE:
                ret = (hi >= upper);
                break;
            case OP_EQ:
                ret = (lower >= upper);
                break;
            default:
                ret = true;
                break;
            }
        }
        break;
    case OP_EQ:
        if (lower >= lo && lower <= hi) {
            switch (right_op) {
            case OP_LT:
                ret = (lower < upper);
                break;
            case OP_LE:
                ret = (lower <= upper);
                break;
            case OP_GT:
                ret = (lower > upper);
                break;
            case OP_GE:
                ret = (lower >= upper);
                break;
            case OP_EQ:
                ret = (lower == upper);
                break;
            default:
                ret = true;
                break;
            }
        }
        break;
    default:
        switch (right_op) {
        case OP_LT:
            ret = (lo < upper);
            break;
        case OP_LE:
            ret = (lo <= upper);
            break;
        case OP_GT:
            ret = (hi > upper);
            break;
        case OP_GE:
            ret = (hi >= upper);
            break;
        case OP_EQ:
            ret = (lo <= upper && hi >= upper);
            break;
        default:
            break;
        }
        break;
    }

    return ret;
} // ibis::qContinuousRange::overlap

/// The constructor of qString.  For convenience of inputting patterns,
/// this function allows the back slash to be used as escape characters in
/// the second argument.  It attempts to remove the back slashes before
/// passing the second argument to later operations.
ibis::qString::qString(const char* ls, const char* rs) :
    qExpr(ibis::qExpr::STRING), lstr(ibis::util::strnewdup(ls)) {
    // attempt to remove the back slash as escape characters
    rstr = new char[1+std::strlen(rs)];
    const char* cptr = rs;
    char* dptr = rstr;
    while (*cptr != 0) {
        if (*cptr != '\\') {
            *dptr = *cptr;
        }
        else {
            ++cptr;
            *dptr = *cptr;
        }
        ++cptr; ++dptr;
    }
    *dptr = 0; // terminate rstr with the NULL character
}

void ibis::qString::print(std::ostream& out) const {
    if (lstr && rstr)
        out << lstr << " == \"" << rstr << "\"";
}

void ibis::qString::getTableNames(std::set<std::string>& plist) const {
    if (lstr != 0) {
        const std::string tn = ibis::qExpr::extractTableName(lstr);
        if (! tn.empty())
            plist.insert(tn);
    }
} // ibis::qString::getTableNames

/// Constructor.  For convenience of inputting patterns, this function
/// allows the back slash to be used as escape characters for the second
/// argument, and attempts to remove them before passing the pattern
/// expression to later operations.
ibis::qLike::qLike(const char* ls, const char* rs) :
    qExpr(ibis::qExpr::LIKE), lstr(ibis::util::strnewdup(ls)) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
    LOGGER(ibis::gVerbose > 5)
        << "qLike::ctor(\"" << ls << "\", \"" << rs << "\") ...";
#endif
    // attempt to remove back slash used as escape characters
    rpat = new char[1+std::strlen(rs)];
    const char* cptr = rs;
    char* dptr = rpat;
    while (*cptr != 0) {
        if (*cptr != '\\') {
            *dptr = *cptr;
        }
        else {
            ++cptr;
            *dptr = *cptr;
        }
        ++cptr; ++dptr;
    }
    *dptr = 0; // terminate rpat with the NULL character
}

void ibis::qLike::print(std::ostream& out) const {
    if (lstr && rpat)
        out << lstr << " LIKE " << rpat;
}

void ibis::qLike::getTableNames(std::set<std::string>& plist) const {
    if (lstr != 0) {
        const std::string& tn = ibis::qExpr::extractTableName(lstr);
        if (! tn.empty())
            plist.insert(tn);
    }
} // ibis::qLike::getTableNames

// append the name=vlaue pair.
void ibis::math::variable::addDecoration(const char *nm, const char *val) {
    if (nm != 0 && val != 0 && *nm != 0 && *val != 0) {
        decor += nm;
        decor += " = ";
        decor += val;
    }
} // ibis::math::variable::addDecoration

void ibis::math::variable::printFull(std::ostream &out) const {
    ibis::resource::vList nv;
    if (! decor.empty())
        ibis::resource::parseNameValuePairs(decor.c_str(), nv);
    const char *fmt = nv["FORMAT_UNIXTIME_GMT"];
    if (fmt == 0 || *fmt == 0)
        fmt = nv["FORMAT_UNIXTIME_UTC"];
    if (fmt != 0 && *fmt != 0) {
        out << "FORMAT_UNIXTIME_GMT(" << name << ", " << fmt << ')';
        return;
    }

    fmt = nv["FORMAT_UNIXTIME_LOCAL"];
    if (fmt != 0 && *fmt != 0) {
        out << "FORMAT_UNIXTIME_LOCAL(" << name << ", " << fmt << ')';
        return;
    }

    fmt = nv["FORMAT_UNIXTIME"];
    if (fmt == 0 || *fmt == 0)
        fmt = nv["FORMAT_DATE"];
    if (fmt == 0 || *fmt == 0)
        fmt = nv["DATE_FORMAT"];
    if (fmt != 0 && *fmt != 0) {
        const char *tz = nv["tzname"];
        if (tz == 0 || *tz == 0)
            tz = nv["timezone"];
        if (tz != 0 && (*tz == 'g' || *tz == 'G' || *tz == 'u' || *tz == 'U')) {
            out << "FORMAT_UNIXTIME_GMT(";
        }
        else {
            out << "FORMAT_UNIXTIME_LOCAL(";
        }
        out << name << ", " << fmt << ')';
    }
    else {
        out << name;
    }
} // ibis::math::variable::printFull

void ibis::math::variable::getTableNames(std::set<std::string>& plist) const {
    if (name != 0) {
        const std::string& tn = ibis::qExpr::extractTableName(name);
        if (! tn.empty())
            plist.insert(tn);
    }
} // ibis::math::variable::getTableNames

/// Record the specified name.  Return the number that is to be used
/// in later functions for retrieving the variable name and
/// its value.
uint32_t ibis::math::barrel::recordVariable(const char* name) {
    uint32_t ind = varmap.size();
    termMap::const_iterator it = varmap.find(name);
    if (it == varmap.end()) {
        varmap[name] = ind;
        namelist.push_back(name);
        varvalues.push_back(0.0);
    }
    else {
        ind = (*it).second;
    }
    return ind;
} // ibis::math::barrel::recordVariable

/// Record the variable names appear in the query expression.  It records
/// all variables in the expression recursively.
void ibis::math::barrel::recordVariable(const ibis::qExpr* const t) {
    if (t == 0) return;

    switch (t->getType()) {
    default:
        if (t->getLeft() != 0) {
            recordVariable(t->getLeft());
        }
        if (t->getRight() != 0) {
            recordVariable(t->getRight());
        }
        break;
    case ibis::qExpr::EXISTS:
        break;
    case ibis::qExpr::RANGE:
    case ibis::qExpr::DRANGE:
    case ibis::qExpr::INTHOD:
    case ibis::qExpr::UINTHOD:
        recordVariable(static_cast<const ibis::qRange*>(t)->colName());
        break;
    case ibis::qExpr::STRING:
        recordVariable(static_cast<const ibis::qString*>(t)->leftString());
        break;
    case ibis::qExpr::ANYSTRING:
        recordVariable(static_cast<const ibis::qAnyString*>(t)->colName());
        break;
    case ibis::qExpr::KEYWORD:
        recordVariable(static_cast<const ibis::qKeyword*>(t)->colName());
        break;
    case ibis::qExpr::ALLWORDS:
        recordVariable(static_cast<const ibis::qAllWords*>(t)->colName());
        break;
    case ibis::qExpr::LIKE:
        recordVariable(static_cast<const ibis::qLike*>(t)->colName());
        break;
    case ibis::qExpr::COMPRANGE: {
        const ibis::compRange &cr = *static_cast<const ibis::compRange*>(t);
        if (cr.getLeft())
            recordVariable(static_cast<const ibis::math::term*>(cr.getLeft()));
        if (cr.getRight())
            recordVariable(static_cast<const ibis::math::term*>(cr.getRight()));
        if (cr.getTerm3())
            recordVariable(static_cast<const ibis::math::term*>(cr.getTerm3()));
        break;}
    case ibis::qExpr::MATHTERM:
        recordVariable(static_cast<const ibis::math::term*>(t));
        break;
    case ibis::qExpr::DEPRECATEDJOIN: {
        const ibis::deprecatedJoin &dj =
            *static_cast<const ibis::deprecatedJoin*>(t);
        recordVariable(dj.getName1());
        recordVariable(dj.getName2());
        recordVariable(dj.getRange());
        break;}
        // case ibis::qExpr::ANYANY: {
        //      const char *pref = static_cast<const ibis::qAnyAny*>(t)->getPrefix();
        //      const int len = std::strlen(pref);
        //      for (unsigned j = 0; j < part0.nColumns(); ++ j) {
        //          if (strnicmp(part0.getColumn(j)->name(), pref, len) == 0) {
        //              recordVariable(part0.getColumn(j)->name());
        //          }
        //      }
        //      break;}
    }
} // ibis::math::barrel::recordVariable

/// Record the variable names appear in the @c term.  It records all
/// variables in the math term recursively.
void ibis::math::barrel::recordVariable(const ibis::math::term* const t) {
    if (t != 0) {
        if (t->termType() == ibis::math::VARIABLE) {
            static_cast<const ibis::math::variable*>(t)
                ->recordVariable(*this);
        }
        else {
            if (t->getLeft() != 0)
                recordVariable(static_cast<const ibis::math::term*>
                               (t->getLeft()));
            if (t->getRight() != 0)
                recordVariable(static_cast<const ibis::math::term*>
                               (t->getRight()));
        }
    }
} // ibis::math::barrel::recordVariable

/// Return true if the two @c barrels contain the same set of variables,
/// otherwise false.
bool
ibis::math::barrel::equivalent(const ibis::math::barrel& rhs) const {
    if (varmap.size() != rhs.varmap.size()) return false;

    bool ret = true;
    termMap::const_iterator ilhs = varmap.begin();
    termMap::const_iterator irhs = rhs.varmap.begin();
    while (ilhs != varmap.end() && ret) {
        ret = (0 == stricmp((*ilhs).first, (*irhs).first));
        ++ ilhs;
        ++ irhs;
    }
    return ret;
} // ibis::math::barrel::equivalent

/// Evaluate an operator.
double ibis::math::bediener::eval() const {
    double lhs, rhs;
    double ret = 0.0; // initialize the return value to zero
    switch (operador) {
    default:
    case ibis::math::UNKNOWN:
        break;
    case ibis::math::NEGATE: {
        if (getRight() != 0)
            ret = -static_cast<const ibis::math::term*>(getRight())->eval();
        else if (getLeft() != 0)
            ret = -static_cast<const ibis::math::term*>(getLeft())->eval();
        else
            ibis::util::setNaN(ret);
        break;
    }
    case ibis::math::BITOR: {
        uint64_t i1 = (uint64_t) static_cast<const ibis::math::term*>
            (getLeft())->eval();
        uint64_t i2 = (uint64_t) static_cast<const ibis::math::term*>
            (getRight())->eval();
        ret = static_cast<double>(i1 | i2);
        break;
    }
    case ibis::math::BITAND: {
        uint64_t i1 = (uint64_t) static_cast<const ibis::math::term*>
            (getLeft())->eval();
        uint64_t i2 = (uint64_t) static_cast<const ibis::math::term*>
            (getRight())->eval();
        ret = static_cast<double>(i1 & i2);
        break;
    }
    case ibis::math::PLUS: {
        lhs = static_cast<const ibis::math::term*>(getLeft())->eval();
        rhs = static_cast<const ibis::math::term*>(getRight())->eval();
        ret = lhs + rhs;
        break;
    }
    case ibis::math::MINUS: {
        lhs = static_cast<const ibis::math::term*>(getLeft())->eval();
        rhs = static_cast<const ibis::math::term*>(getRight())->eval();
        ret = lhs - rhs;
        break;
    }
    case ibis::math::MULTIPLY: {
        lhs = static_cast<const ibis::math::term*>(getLeft())->eval();
        rhs = static_cast<const ibis::math::term*>(getRight())->eval();
        ret = lhs * rhs;
        break;
    }
    case ibis::math::DIVIDE: {
        lhs = static_cast<const ibis::math::term*>(getLeft())->eval();
        if (lhs != 0.0) {
            rhs = static_cast<const ibis::math::term*>(getRight())
                ->eval();
            if (rhs != 0.0)
                ret = lhs / rhs;
        }
        break;
    }
    case ibis::math::REMAINDER: {
        lhs = static_cast<const ibis::math::term*>(getLeft())->eval();
        if (lhs != 0.0) {
            rhs = static_cast<const ibis::math::term*>(getRight())
                ->eval();
            if (rhs != 0.0)
                ret = fmod(lhs, rhs);
        }
        break;
    }
    case ibis::math::POWER: {
        lhs = static_cast<const ibis::math::term*>(getLeft())->eval();
        if (lhs != 0.0) {
            rhs = static_cast<const ibis::math::term*>(getRight())
                ->eval();
            if (rhs != 0.0)
                ret = pow(lhs, rhs);
            else
                ret = 1.0;
        }
        break;
    }
    }
    return ret;
} // ibis::math::bediener::eval

// constructors of concrete terms in ibis::compRange
ibis::math::stdFunction1::stdFunction1(const char* name) {
    if (0 == stricmp(name, "ACOS"))
        ftype = ibis::math::ACOS;
    else if (0 == stricmp(name, "ASIN"))
        ftype = ibis::math::ASIN;
    else if (0 == stricmp(name, "ATAN"))
        ftype = ibis::math::ATAN;
    else if (0 == stricmp(name, "CEIL"))
        ftype = ibis::math::CEIL;
    else if (0 == stricmp(name, "COS"))
        ftype = ibis::math::COS;
    else if (0 == stricmp(name, "COSH"))
        ftype = ibis::math::COSH;
    else if (0 == stricmp(name, "EXP"))
        ftype = ibis::math::EXP;
    else if (0 == stricmp(name, "FABS") || 0 == stricmp(name, "ABS"))
        ftype = ibis::math::FABS;
    else if (0 == stricmp(name, "FLOOR"))
        ftype = ibis::math::FLOOR;
    else if (0 == stricmp(name, "IS_ZERO"))
        ftype = ibis::math::IS_ZERO;
    else if (0 == stricmp(name, "IS_NONZERO"))
        ftype = ibis::math::IS_NONZERO;
    else if (0 == stricmp(name, "FREXP"))
        ftype = ibis::math::FREXP;
    else if (0 == stricmp(name, "LOG10"))
        ftype = ibis::math::LOG10;
    else if (0 == stricmp(name, "LOG"))
        ftype = ibis::math::LOG;
    else if (0 == stricmp(name, "MODF"))
        ftype = ibis::math::MODF;
    else if (0 == stricmp(name, "ROUND"))
        ftype = ibis::math::ROUND;
    else if (0 == stricmp(name, "TRUNC"))
        ftype = ibis::math::TRUNC;
    else if (0 == stricmp(name, "SIN"))
        ftype = ibis::math::SIN;
    else if (0 == stricmp(name, "SINH"))
        ftype = ibis::math::SINH;
    else if (0 == stricmp(name, "SQRT"))
        ftype = ibis::math::SQRT;
    else if (0 == stricmp(name, "TAN"))
        ftype = ibis::math::TAN;
    else if (0 == stricmp(name, "TANH"))
        ftype = ibis::math::TANH;
    else if (0 == stricmp(name, "INT_FROM_DICT"))
        ftype = ibis::math::ROUND;
    else {
        LOGGER(ibis::gVerbose >= 0)
            << "math::stdFunction1::stdFunction1(" << name
            << ") UNKNOWN (one-argument) function name";
        throw "math::stdFunction1::ctor failed due to a unknown function name"
            IBIS_FILE_LINE;
    }
} // constructor of ibis::math::stdFunction1

ibis::math::term* ibis::math::stdFunction1::reduce() {
#if _DEBUG+0>1 || DEBUG+0>1
    LOGGER(ibis::gVerbose > 4)
        << "DEBUG -- stdFunction1::reduce  input:  " << *this;
#endif
    ibis::math::term *lhs =
        static_cast<ibis::math::term*>(getLeft());
    if (lhs->termType() == ibis::math::OPERATOR ||
        lhs->termType() == ibis::math::STDFUNCTION1 ||
        lhs->termType() == ibis::math::STDFUNCTION2) {
        ibis::math::term *tmp = lhs->reduce();
        if (tmp != lhs) { // replace LHS with the new term
            setLeft(tmp);
            lhs = tmp;
        }
    }

    ibis::math::term *ret = this;
    if (lhs->termType() == ibis::math::NUMBER) {
        double arg = lhs->eval();
        switch (ftype) {
        case ACOS: ret = new ibis::math::number(acos(arg)); break;
        case ASIN: ret = new ibis::math::number(asin(arg)); break;
        case ATAN: ret = new ibis::math::number(atan(arg)); break;
        case CEIL: ret = new ibis::math::number(ceil(arg)); break;
        case COS: ret = new ibis::math::number(cos(arg)); break;
        case COSH: ret = new ibis::math::number(cosh(arg)); break;
        case EXP: ret = new ibis::math::number(exp(arg)); break;
        case FABS: ret = new ibis::math::number(fabs(arg)); break;
        case FLOOR: ret = new ibis::math::number(floor(arg)); break;
        case IS_ZERO: ret = new ibis::math::number((arg==0)); break;
        case IS_NONZERO: ret = new ibis::math::number((0!=arg)); break;
        case FREXP: {int expptr;
                ret = new ibis::math::number(frexp(arg, &expptr)); break;}
        case LOG10: ret = new ibis::math::number(log10(arg)); break;
        case LOG: ret = new ibis::math::number(log(arg)); break;
        case MODF: {double intptr;
                ret = new ibis::math::number(modf(arg, &intptr)); break;}
        case ROUND: ret = new ibis::math::number(floor(arg+0.5)); break;
        case SIN: ret = new ibis::math::number(sin(arg)); break;
        case SINH: ret = new ibis::math::number(sinh(arg)); break;
        case SQRT: ret = new ibis::math::number(sqrt(arg)); break;
        case TAN: ret = new ibis::math::number(tan(arg)); break;
        case TANH: ret = new ibis::math::number(tanh(arg)); break;
        default: break;
        }
    }
    else if (ftype == ACOS && lhs->termType() ==
             ibis::math::STDFUNCTION1) {
        ibis::math::stdFunction1 *tmp =
            reinterpret_cast<ibis::math::stdFunction1*>(lhs);
        if (tmp->ftype == COS) {
            ret = static_cast<ibis::math::term*>(tmp->getLeft());
            tmp->getLeft() = 0;
        }
    }
    else if (ftype == COS && lhs->termType() ==
             ibis::math::STDFUNCTION1) {
        ibis::math::stdFunction1 *tmp =
            reinterpret_cast<ibis::math::stdFunction1*>(lhs);
        if (tmp->ftype == ACOS) {
            ret = static_cast<ibis::math::term*>(tmp->getLeft());
            tmp->getLeft() = 0;
        }
    }
    else if (ftype == ASIN && lhs->termType() ==
             ibis::math::STDFUNCTION1) {
        ibis::math::stdFunction1 *tmp =
            reinterpret_cast<ibis::math::stdFunction1*>(lhs);
        if (tmp->ftype == SIN) {
            ret = static_cast<ibis::math::term*>(tmp->getLeft());
            tmp->getLeft() = 0;
        }
    }
    else if (ftype == SIN && lhs->termType() ==
             ibis::math::STDFUNCTION1) {
        ibis::math::stdFunction1 *tmp =
            reinterpret_cast<ibis::math::stdFunction1*>(lhs);
        if (tmp->ftype == ASIN) {
            ret = static_cast<ibis::math::term*>(tmp->getLeft());
            tmp->getLeft() = 0;
        }
    }
    else if (ftype == ATAN && lhs->termType() ==
             ibis::math::STDFUNCTION1) {
        ibis::math::stdFunction1 *tmp =
            reinterpret_cast<ibis::math::stdFunction1*>(lhs);
        if (tmp->ftype == TAN) {
            ret = static_cast<ibis::math::term*>(tmp->getLeft());
            tmp->getLeft() = 0;
        }
    }
    else if (ftype == TAN && lhs->termType() ==
             ibis::math::STDFUNCTION1) {
        ibis::math::stdFunction1 *tmp =
            reinterpret_cast<ibis::math::stdFunction1*>(lhs);
        if (tmp->ftype == ATAN) {
            ret = static_cast<ibis::math::term*>(tmp->getLeft());
            tmp->getLeft() = 0;
        }
    }
    else if (ftype == EXP && lhs->termType() ==
             ibis::math::STDFUNCTION1) {
        ibis::math::stdFunction1 *tmp =
            reinterpret_cast<ibis::math::stdFunction1*>(lhs);
        if (tmp->ftype == LOG) {
            ret = static_cast<ibis::math::term*>(tmp->getLeft());
            tmp->getLeft() = 0;
        }
    }
    else if (ftype == LOG && lhs->termType() ==
             ibis::math::STDFUNCTION1) {
        ibis::math::stdFunction1 *tmp =
            reinterpret_cast<ibis::math::stdFunction1*>(lhs);
        if (tmp->ftype == EXP) {
            ret = static_cast<ibis::math::term*>(tmp->getLeft());
            tmp->getLeft() = 0;
        }
    }
#if _DEBUG+0>1 || DEBUG+0>1
    LOGGER(ibis::gVerbose > 4)
        << "DEBUG -- stdFunction1::reduce output:  " << *ret;
#endif
    return ret;
} // ibis::math::stdfunction1::reduce

/// Evaluate one-argument standard functions from math.h.  The functions
/// modf and frexp take two argument, but only one is an input argument,
/// only the return value of these functions are returned.
double ibis::math::stdFunction1::eval() const {
    double arg =
        static_cast<const ibis::math::term*>(getLeft())->eval();
    switch (ftype) {
    case ibis::math::ACOS: arg = acos(arg); break;
    case ibis::math::ASIN: arg = asin(arg); break;
    case ibis::math::ATAN: arg = atan(arg); break;
    case ibis::math::CEIL: arg = ceil(arg); break;
    case ibis::math::COS: arg = cos(arg); break;
    case ibis::math::COSH: arg = cosh(arg); break;
    case ibis::math::EXP: arg = exp(arg); break;
    case ibis::math::FABS: arg = fabs(arg); break;
    case ibis::math::FLOOR: arg = floor(arg); break;
    case ibis::math::IS_ZERO: arg = (double)(0 == arg); break;
    case ibis::math::IS_NONZERO: arg = (double)(0 != arg); break;
    case ibis::math::FREXP: {int expptr; arg = frexp(arg, &expptr); break;}
    case ibis::math::LOG10: arg = log10(arg); break;
    case ibis::math::LOG: arg = log(arg); break;
    case ibis::math::MODF: {double intptr; arg = modf(arg, &intptr); break;}
    case ibis::math::ROUND: arg = floor(arg+0.5); break;
    case ibis::math::SIN: arg = sin(arg); break;
    case ibis::math::SINH: arg = sinh(arg); break;
    case ibis::math::SQRT: arg = sqrt(arg); break;
    case ibis::math::TAN: arg = tan(arg); break;
    case ibis::math::TANH: arg = tanh(arg); break;
    case ibis::math::TRUNC: arg = trunc(arg); break;
    default:
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- unknown 1-argument function, "
            "returning the argument";
        break;
    }
    return arg;
} // ibis::math::stdfunction1::eval

ibis::math::stdFunction2::stdFunction2(const char* name) {
    if (0 == stricmp(name, "ATAN2"))
        ftype = ibis::math::ATAN2;
    else if (0 == stricmp(name, "FMOD"))
        ftype = ibis::math::FMOD;
    else if (0 == stricmp(name, "LDEXP"))
        ftype = ibis::math::LDEXP;
    else if (0 == stricmp(name, "POW") || 0 == stricmp(name, "POWER"))
        ftype = ibis::math::POW;
    else if (0 == stricmp(name, "ROUND2") || 0 == stricmp(name, "ROUND") ||
             0 == stricmp(name, "TRUNC"))
        ftype = ibis::math::ROUND2;
    else if (0 == stricmp(name, "IS_EQL"))
        ftype = ibis::math::IS_EQL;
    else if (0 == stricmp(name, "IS_GTE"))
        ftype = ibis::math::IS_GTE;
    else if (0 == stricmp(name, "IS_LTE"))
        ftype = ibis::math::IS_LTE;
    else {
        LOGGER(ibis::gVerbose >= 0)
            << "math::stdFunction2::stdFunction2(" << name
            << ") UNKNOWN (two-argument) function name";
        throw "math::stdFunction2::ctor failed due to a unknown function name"
            IBIS_FILE_LINE;
    }
} // constructor of ibis::math::stdFunction2

ibis::math::term* ibis::math::stdFunction2::reduce() {
    ibis::math::term *lhs =
        static_cast<ibis::math::term*>(getLeft());
    ibis::math::term *rhs =
        static_cast<ibis::math::term*>(getRight());
    if (lhs->termType() == ibis::math::OPERATOR ||
        lhs->termType() == ibis::math::STDFUNCTION1 ||
        lhs->termType() == ibis::math::STDFUNCTION2) {
        ibis::math::term *tmp = lhs->reduce();
        if (tmp != lhs) { // replace LHS with the new term
            setLeft(tmp);
            lhs = tmp;
        }
    }
    if (rhs->termType() == ibis::math::OPERATOR ||
        rhs->termType() == ibis::math::STDFUNCTION1 ||
        rhs->termType() == ibis::math::STDFUNCTION2) {
        ibis::math::term *tmp = rhs->reduce();
        if (tmp != rhs) { // replace RHS with the new term
            setRight(tmp);
            rhs = tmp;
        }
    }

    ibis::math::term *ret = this;
    if (lhs->termType() == ibis::math::NUMBER &&
        rhs->termType() == ibis::math::NUMBER) {
        switch (ftype) {
        case ATAN2:
            ret = new ibis::math::number(atan2(lhs->eval(), rhs->eval()));
            break;
        case FMOD:
            ret = new ibis::math::number(fmod(lhs->eval(), rhs->eval()));
            break;
        case LDEXP:
            ret = new ibis::math::number
                (ldexp(lhs->eval(), static_cast<int>(rhs->eval())));
            break;
        case POW:
            ret = new ibis::math::number(pow(lhs->eval(), rhs->eval()));
            break;
        case ROUND2: {
            double scale = floor(0.5+rhs->eval());
            scale = (scale > 0 ? pow(1.0e1, scale) : 1.0);
            ret = new ibis::math::number(floor(0.5+lhs->eval()*scale)/scale);
            break;}
        case IS_EQL:
            if (lhs->eval() == rhs->eval()) {
                ret = new ibis::math::number ((double)1.0);
            } else {
                ret = new ibis::math::number ((double)0.0);
            }
            break;
        case IS_GTE:
            if (lhs->eval() >= rhs->eval()) {
                ret = new ibis::math::number ((double)1.0);
            } else {
                ret = new ibis::math::number ((double)0.0);
            }
            break;
        case IS_LTE:
            if (lhs->eval() <= rhs->eval()) {
                ret = new ibis::math::number ((double)1.0);
            } else {
                ret = new ibis::math::number ((double)0.0);
            }
            break;
        default: break;
        }
    }
    return ret;
} // ibis::math::stdfunction2::reduce

/// Evaluate the 2-argument standard functions.
double ibis::math::stdFunction2::eval() const {
    double lhs =
        static_cast<const ibis::math::term*>(getLeft())->eval();
    double rhs =
        static_cast<const ibis::math::term*>(getRight())->eval();
    switch (ftype) {
    case ibis::math::ATAN2: lhs = atan2(lhs, rhs); break;
    case ibis::math::FMOD: lhs = fmod(lhs, rhs); break;
    case ibis::math::LDEXP: lhs = ldexp(lhs, static_cast<int>(rhs)); break;
    case ibis::math::POW: lhs = pow(lhs, rhs); break;
    case ROUND2: {
        const double scale = pow(1.0e1, floor(0.5+rhs));
        lhs = floor(0.5 + lhs * scale) / scale;
        break;}
    case ibis::math::IS_EQL:
        if (lhs == rhs)
            lhs = (double)1.0;
        else
            lhs = (double)0.0;
        break;
    case ibis::math::IS_GTE:
        if (lhs >= rhs)
            lhs = (double)1.0;
        else
            lhs = (double)0.0;
        break;
    case ibis::math::IS_LTE:
        if (lhs <= rhs)
            lhs = (double)1.0;
        else
            lhs = (double)0.0;
        break;
    default:
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- unknown 2-argument function, "
            "returning the 1st argument";
        break;
    }
    return lhs;
} // ibis::math::stdfunction2::eval

void ibis::math::bediener::print(std::ostream& out) const {
    switch (operador) {
    case ibis::math::NEGATE:
        out << "(-";
        getRight()->print(out);
        out << ')';
        break;
    case ibis::math::UNKNOWN:
        out << "unknown operator ?";
        break;
    default:
        out << "(";
        getLeft()->print(out);
        out << " " << operator_name[operador] << " ";
        getRight()->print(out);
        out << ")";
    }
} // ibis::math::bediener::print

ibis::math::term* ibis::math::bediener::reduce() {
    reorder(); // reorder the expression for easier reduction

#if _DEBUG+0>1 || DEBUG+0>1
    LOGGER(ibis::gVerbose > 4)
        << "DEBUG -- bediener::reduce  input:  " << *this;
#endif
    ibis::math::term *lhs =
        reinterpret_cast<ibis::math::term*>(getLeft());
    ibis::math::term *rhs =
        reinterpret_cast<ibis::math::term*>(getRight());
    if (lhs != 0 && (lhs->termType() == ibis::math::OPERATOR ||
                     lhs->termType() == ibis::math::STDFUNCTION1 ||
                     lhs->termType() == ibis::math::STDFUNCTION2)) {
        ibis::math::term *tmp = lhs->reduce();
        if (tmp != lhs) { // replace LHS with the new term
            setLeft(tmp);
            lhs = tmp;
        }
    }
    if (rhs != 0 && (rhs->termType() == ibis::math::OPERATOR ||
                     rhs->termType() == ibis::math::STDFUNCTION1 ||
                     rhs->termType() == ibis::math::STDFUNCTION2)) {
        ibis::math::term *tmp = rhs->reduce();
        if (tmp != rhs) { // replace RHS with the new term
            setRight(tmp);
            rhs = tmp;
        }
    }
    if (lhs == 0 && rhs == 0) return this;

    ibis::math::term *ret = this;
    switch (operador) {
    default:
    case ibis::math::UNKNOWN:
        break;
    case ibis::math::NEGATE: {
        if (rhs != 0 && rhs->termType() == ibis::math::NUMBER)
            ret = new ibis::math::number(- rhs->eval());
        else if (lhs != 0 && lhs->termType() == ibis::math::NUMBER)
            ret = new ibis::math::number(- lhs->eval());
        break;}
    case ibis::math::BITOR: {
        if (lhs->termType() == ibis::math::NUMBER &&
            rhs->termType() == ibis::math::NUMBER) {
            uint64_t i1 = (uint64_t) lhs->eval();
            uint64_t i2 = (uint64_t) rhs->eval();
            ret = new ibis::math::number(static_cast<double>(i1 | i2));
        }
        break;}
    case ibis::math::BITAND: {
        if (lhs->termType() == ibis::math::NUMBER &&
            rhs->termType() == ibis::math::NUMBER) {
            uint64_t i1 = (uint64_t) lhs->eval();
            uint64_t i2 = (uint64_t) rhs->eval();
            ret = new ibis::math::number(static_cast<double>(i1 & i2));
        }
        break;}
    case ibis::math::PLUS: {
        if (lhs->termType() == ibis::math::NUMBER &&
            rhs->termType() == ibis::math::NUMBER) {
            // both sides are numbers
            ret = new ibis::math::number(lhs->eval() + rhs->eval());
        }
        else if (lhs->termType() == ibis::math::NUMBER &&
                 lhs->eval() == 0.0) {
            // 0 + A ==> A
            ret = static_cast<ibis::math::term*>(getRight());
            getRight() = 0;
        }
        else if (rhs->termType() == ibis::math::NUMBER &&
                 rhs->eval() == 0.0) {
            // A + 0 ==> A
            ret = static_cast<ibis::math::term*>(getLeft());
            getLeft() = 0;
        }
        else if (lhs->termType() == ibis::math::VARIABLE &&
                 rhs->termType() == ibis::math::VARIABLE &&
                 std::strcmp(static_cast<ibis::math::variable*>
                             (lhs)->variableName(),
                             static_cast<ibis::math::variable*>
                             (rhs)->variableName()) == 0) {
            // A + A ==> 2*A
            number *ntmp = new number(2.0);
            bediener *btmp = new bediener(MULTIPLY);
            btmp->getLeft() = ntmp;
            btmp->getRight() = getRight();
            getRight() = 0;
            ret = btmp;
        }
        else if (lhs->termType() == ibis::math::OPERATOR &&
                 rhs->termType() == ibis::math::OPERATOR &&
                 static_cast<ibis::math::term*>(lhs->getLeft())->termType()
                 == ibis::math::NUMBER &&
                 static_cast<ibis::math::term*>(rhs->getLeft())->termType()
                 == ibis::math::NUMBER &&
                 static_cast<ibis::math::term*>
                 (lhs->getRight())->termType() == ibis::math::VARIABLE &&
                 static_cast<ibis::math::term*>
                 (rhs->getRight())->termType() == ibis::math::VARIABLE &&
                 std::strcmp(static_cast<ibis::math::variable*>
                             (lhs->getRight())->variableName(),
                             static_cast<ibis::math::variable*>
                             (rhs->getRight())->variableName()) == 0) {
            // x*A + y*A ==> (x+y)*A
            ret = lhs->dup();
            static_cast<ibis::math::number*>(ret->getLeft())->val +=
                static_cast<ibis::math::term*>(rhs->getLeft())->eval();
        }
        break;}
    case ibis::math::MINUS: {
        if (lhs->termType() == ibis::math::NUMBER &&
            rhs->termType() == ibis::math::NUMBER) {
            ret = new ibis::math::number(lhs->eval() - rhs->eval());
        }
        else if (rhs->termType() == ibis::math::NUMBER &&
                 rhs->eval() == 0.0) {
            ret = static_cast<ibis::math::term*>(getLeft());
            getLeft() = 0;
        }
        else if (lhs->termType() == ibis::math::VARIABLE &&
                 rhs->termType() == ibis::math::VARIABLE &&
                 std::strcmp(static_cast<ibis::math::variable*>
                             (lhs)->variableName(),
                             static_cast<ibis::math::variable*>
                             (rhs)->variableName()) == 0) {
            // both sides are the same variable name
            ret = new number(0.0);
        }
        else if (lhs->termType() == ibis::math::OPERATOR &&
                 rhs->termType() == ibis::math::OPERATOR &&
                 static_cast<ibis::math::term*>(lhs->getLeft())->termType()
                 == ibis::math::NUMBER &&
                 static_cast<ibis::math::term*>(rhs->getLeft())->termType()
                 == ibis::math::NUMBER &&
                 static_cast<ibis::math::term*>
                 (lhs->getRight())->termType() == ibis::math::VARIABLE &&
                 static_cast<ibis::math::term*>
                 (rhs->getRight())->termType() == ibis::math::VARIABLE &&
                 std::strcmp(static_cast<ibis::math::variable*>
                             (lhs->getRight())->variableName(),
                             static_cast<ibis::math::variable*>
                             (rhs->getRight())->variableName()) == 0) {
            ret = lhs->dup();
            static_cast<ibis::math::number*>(ret->getLeft())->val -=
                static_cast<ibis::math::term*>(rhs->getLeft())->eval();
        }
        break;}
    case ibis::math::MULTIPLY: {
        if (lhs->termType() == ibis::math::NUMBER &&
            lhs->eval() == 0.0) {
            ret = new ibis::math::number(0.0);
        }
        else if (rhs->termType() == ibis::math::NUMBER &&
                 rhs->eval() == 0.0) {
            ret = new ibis::math::number(0.0);
        }
        else if (lhs->termType() == ibis::math::NUMBER &&
                 rhs->termType() == ibis::math::NUMBER) {
            ret = new ibis::math::number(lhs->eval() * rhs->eval());
        }
        else if (lhs->termType() == ibis::math::NUMBER &&
                 lhs->eval() == 1.0) { // multiply by one
            ret = static_cast<ibis::math::term*>(getRight());
            getRight() = 0;
        }
        else if (rhs->termType() == ibis::math::NUMBER &&
                 rhs->eval() == 1.0) { // multiply by one
            ret = static_cast<ibis::math::term*>(getLeft());
            getLeft() = 0;
        }
        else if (lhs->termType() == ibis::math::NUMBER &&
                 rhs->termType() == ibis::math::OPERATOR &&
                 static_cast<ibis::math::bediener*>
                 (rhs->getLeft())->operador == ibis::math::MULTIPLY &&
                 static_cast<ibis::math::term*>
                 (rhs->getLeft())->termType() == ibis::math::NUMBER) {
            ret = static_cast<ibis::math::term*>(getRight());
            static_cast<ibis::math::number*>(ret->getLeft())->val *=
                lhs->eval();
            getRight() = 0;
        }
        else if (rhs->termType() == ibis::math::NUMBER &&
                 lhs->termType() == ibis::math::OPERATOR &&
                 static_cast<ibis::math::bediener*>
                 (lhs->getLeft())->operador == ibis::math::MULTIPLY &&
                 static_cast<ibis::math::term*>
                 (lhs->getLeft())->termType() == ibis::math::NUMBER) {
            ret = static_cast<ibis::math::term*>(getLeft());
            static_cast<ibis::math::number*>(ret->getLeft())->val *=
                rhs->eval();
            getLeft() = 0;
        }
        break;}
    case ibis::math::DIVIDE: {
        if (lhs->termType() == ibis::math::NUMBER &&
            lhs->eval() == 0.0) { // zero
            ret = new ibis::math::number(0.0);
        }
        else if (rhs->termType() == ibis::math::NUMBER &&
                 (rhs->eval() < -DBL_MAX || rhs->eval() > DBL_MAX)) {
            ret = new ibis::math::number(0.0);
        }
        else if (lhs->termType() == ibis::math::NUMBER &&
                 rhs->termType() == ibis::math::NUMBER) {
            ret = new ibis::math::number(lhs->eval() / rhs->eval());
        }
        else if (rhs->termType() == ibis::math::NUMBER &&
                 lhs->termType() == ibis::math::OPERATOR &&
                 static_cast<ibis::math::bediener*>
                 (lhs->getLeft())->operador == ibis::math::MULTIPLY &&
                 static_cast<ibis::math::term*>
                 (lhs->getLeft())->termType() == ibis::math::NUMBER) {
            ret = lhs->dup();
            static_cast<ibis::math::number*>(ret->getLeft())->val /=
                rhs->eval();
        }
        break;}
    case ibis::math::POWER: {
        if (rhs->termType() == ibis::math::NUMBER &&
            rhs->eval() == 0.0) { // zeroth power
            ret = new ibis::math::number(1.0);
        }
        else if (lhs->termType() == ibis::math::NUMBER &&
                 lhs->eval() == 0.0) { // zero raise to some power
            ret = new ibis::math::number(0.0);
        }
        else if (lhs->termType() == ibis::math::NUMBER &&
                 rhs->termType() == ibis::math::NUMBER) { // constant
            ret = new ibis::math::number
                (pow(lhs->eval(), rhs->eval()));
        }
        break;}
    }

    if (ret != this) {
        ibis::math::term *tmp = ret->reduce();
        if (tmp != ret) {
            delete ret;
            ret = tmp;
        }
    }
#if _DEBUG+0>1 || DEBUG+0>1
    LOGGER(ibis::gVerbose > 4)
        << "DEBUG -- bediener::reduce output:  " << *ret;
#endif
    return ret;
} // ibis::math::bediener::reduce

// For operators whose two operands can be exchanged, this function makes
// sure the constants are move to the part that can be evaluated first.
void ibis::math::bediener::reorder() {
    // reduce the use of operator - and operator /
    convertConstants();
#if _DEBUG+0 > 1 || DEBUG+0 > 1
    LOGGER(ibis::gVerbose > 4)
        << "DEBUG -- bediener::reorder  input:  " << *this;
#endif

    std::vector< ibis::math::term* > terms;
    if (operador == ibis::math::BITOR ||
        operador == ibis::math::BITAND ||
        operador == ibis::math::PLUS ||
        operador == ibis::math::MULTIPLY) {
        // first linearize -- put all terms to be rearranged in a list
        linearize(operador, terms);

        // make sure the numbers appears last in the list
        // there are at least two elements in terms
        uint32_t i = 0;
        uint32_t j = terms.size() - 1;
        while (i < j) {
            if (terms[j]->termType() == ibis::math::NUMBER) {
                -- j;
            }
            else if (terms[i]->termType() == ibis::math::NUMBER) {
                ibis::math::term *ptr = terms[i];
                terms[i] = terms[j];
                terms[j] = ptr;
                -- j;
                ++ i;
            }
            else {
                ++ i;
            }
        }

        // put the list of terms into a skewed tree
        ibis::math::term *ptr = this;
        j = terms.size() - 1;
        for (i = 0; i < j; ++ i) {
            ptr->setRight(terms[i]);
            if (i+1 < j) {
                if (reinterpret_cast<ibis::math::term*>
                    (ptr->getLeft())->termType() !=
                    ibis::math::OPERATOR ||
                    reinterpret_cast<ibis::math::bediener*>
                    (ptr->getLeft())->operador != operador)
                    ptr->setLeft(new ibis::math::bediener(operador));
                ptr = reinterpret_cast<ibis::math::term*>(ptr->getLeft());
            }
        }
        ptr->setLeft(terms[j]);
    }
#if _DEBUG+0 > 1 || DEBUG+0 > 1
    LOGGER(ibis::gVerbose > 4)
        << "DEBUG -- bediener::reorder output:  " << *this;
#endif
} // ibis::math::bediener::reorder

void ibis::math::bediener::linearize
(const ibis::math::OPERADOR op,
 std::vector<ibis::math::term*>& terms) {
    if (operador == op) {
        ibis::math::term* rhs = reinterpret_cast<ibis::math::term*>
            (getRight());
        if (rhs->termType() == ibis::math::OPERATOR &&
            reinterpret_cast<ibis::math::bediener*>(rhs)->operador == op)
            reinterpret_cast<ibis::math::bediener*>(rhs)
                ->linearize(op, terms);
        else
            terms.push_back(rhs->dup());

        ibis::math::term* lhs = reinterpret_cast<ibis::math::term*>
            (getLeft());
        if (lhs->termType() == ibis::math::OPERATOR &&
            reinterpret_cast<ibis::math::bediener*>(lhs)->operador == op)
            reinterpret_cast<ibis::math::bediener*>(lhs)
                ->linearize(op, terms);
        else
            terms.push_back(lhs->dup());
    }
} // ibis::math::bediener::linearize

// if the right operand is a number, there are two cases where we can
// change the operators
void ibis::math::bediener::convertConstants() {
#if _DEBUG+0 > 1 || DEBUG+0 > 1
    LOGGER(ibis::gVerbose > 8)
        << "DEBUG -- bediener::convertConstants  input:  " << *this;
#endif
    ibis::math::term* rhs = reinterpret_cast<ibis::math::term*>
        (getRight());
    if (rhs->termType() == ibis::math::NUMBER) {
        if (operador == ibis::math::MINUS) {
            reinterpret_cast<ibis::math::number*>(rhs)->negate();
            operador = ibis::math::PLUS;

            ibis::math::term* lhs =
                reinterpret_cast<ibis::math::term*>(getLeft());
            if (lhs->termType() == ibis::math::OPERATOR)
                reinterpret_cast<ibis::math::bediener*>(lhs)
                    ->convertConstants();
        }
        else if (operador == ibis::math::DIVIDE) {
            reinterpret_cast<ibis::math::number*>(rhs)->invert();
            operador = ibis::math::MULTIPLY;

            ibis::math::term* lhs =
                reinterpret_cast<ibis::math::term*>(getLeft());
            if (lhs->termType() == ibis::math::OPERATOR)
                reinterpret_cast<ibis::math::bediener*>(lhs)
                    ->convertConstants();
        }
    }
#if _DEBUG+0 > 1 || DEBUG+0 > 1
    LOGGER(ibis::gVerbose > 8)
        << "DEBUG -- bediener::convertConstants output:  " << *this;
#endif
} // ibis::math::convertConstants

void ibis::math::stdFunction1::print(std::ostream& out) const {
    out << stdfun1_name[ftype] << '(';
    getLeft()->print(out);
    out << ')';
} // ibis::math::stdFunction1::print

void ibis::math::stdFunction2::print(std::ostream& out) const {
    out << stdfun2_name[ftype] << '(';
    getLeft()->print(out);
    out << ", ";
    getRight()->print(out);
    out << ')';
} // ibis::math::stdFunction2::print

void ibis::compRange::print(std::ostream& out) const {
    switch (op12) {
    case OP_EQ:
        getLeft()->print(out);
        out << " == ";
        break;
    case OP_LT:
        getLeft()->print(out);
        out << " < ";
        break;
    case OP_LE:
        getLeft()->print(out);
        out << " <= ";
        break;
    case OP_GT:
        getLeft()->print(out);
        out << " > ";
        break;
    case OP_GE:
        getLeft()->print(out);
        out << " >= ";
        break;
    default: break;
    }
    getRight()->print(out);
    if (expr3) {
        switch (op23) {
        case OP_EQ:
            out << " == ";
            expr3->print(out);
            break;
        case OP_LT:
            out << " < ";
            expr3->print(out);
            break;
        case OP_LE:
            out << " <= ";
            expr3->print(out);
            break;
        case OP_GT:
            out << " > ";
            expr3->print(out);
            break;
        case OP_GE:
            out << " >= ";
            expr3->print(out);
            break;
        default: break;
        }
    }
} // ibis::compRange::print

void ibis::qRange::getTableNames(std::set<std::string>& plist) const {
    const char *cn = colName();
    if (cn != 0) {
        const std::string& tn = ibis::qExpr::extractTableName(cn);
        if (! tn.empty())
            plist.insert(tn);
    }
} // ibis::qRange::getTableNames

// convert to a simple range stored as ibis::qContinuousRange
// attempt to replace the operators > and >= with < and <=
ibis::qContinuousRange* ibis::compRange::simpleRange() const {
    ibis::qContinuousRange* res = 0;
    if (expr3 == 0) {
        if (reinterpret_cast<const ibis::math::term*>(getLeft())->
            termType()==ibis::math::VARIABLE &&
            reinterpret_cast<const ibis::math::term*>(getRight())->
            termType()==ibis::math::NUMBER) {
            res = new ibis::qContinuousRange
                (reinterpret_cast<const ibis::math::variable*>
                 (getLeft())->variableName(), op12,
                 reinterpret_cast<const ibis::math::term*>
                 (getRight())->eval());
        }
        else if (reinterpret_cast<const ibis::math::term*>(getLeft())->
                 termType()==ibis::math::NUMBER &&
                 reinterpret_cast<const ibis::math::term*>(getRight())->
                 termType()==ibis::math::VARIABLE) {
            switch (op12) {
            case ibis::qExpr::OP_LT:
                res = new ibis::qContinuousRange
                    (reinterpret_cast<const ibis::math::variable*>
                     (getRight())->variableName(), ibis::qExpr::OP_GT,
                     reinterpret_cast<const ibis::math::term*>
                     (getLeft())->eval());
                break;
            case ibis::qExpr::OP_LE:
                res = new ibis::qContinuousRange
                    (reinterpret_cast<const ibis::math::variable*>
                     (getRight())->variableName(), ibis::qExpr::OP_GE,
                     reinterpret_cast<const ibis::math::term*>
                     (getLeft())->eval());
                break;
            case ibis::qExpr::OP_GT:
                res = new ibis::qContinuousRange
                    (reinterpret_cast<const ibis::math::variable*>
                     (getRight())->variableName(), ibis::qExpr::OP_LT,
                     reinterpret_cast<const ibis::math::term*>
                     (getLeft())->eval());
                break;
            case ibis::qExpr::OP_GE:
                res = new ibis::qContinuousRange
                    (reinterpret_cast<const ibis::math::variable*>
                     (getRight())->variableName(), ibis::qExpr::OP_LE,
                     reinterpret_cast<const ibis::math::term*>
                     (getLeft())->eval());
                break;
            default:
                res = new ibis::qContinuousRange
                    (reinterpret_cast<const ibis::math::variable*>
                     (getRight())->variableName(), op12,
                     reinterpret_cast<const ibis::math::term*>
                     (getLeft())->eval());
                break;
            }
        }
    }
    else if (expr3->termType() == ibis::math::NUMBER &&
             reinterpret_cast<const ibis::math::term*>(getLeft())->
             termType()==ibis::math::NUMBER &&
             reinterpret_cast<const ibis::math::term*>(getRight())->
             termType()==ibis::math::VARIABLE) {
        const char* vname =
            reinterpret_cast<const ibis::math::variable*>
            (getRight())->variableName();
        double val0 = reinterpret_cast<const ibis::math::number*>
            (getLeft())->eval();
        double val1 = expr3->eval();
        switch (op12) {
        case ibis::qExpr::OP_LT:
            switch (op23) {
            case ibis::qExpr::OP_LT:
            case ibis::qExpr::OP_LE:
                res = new ibis::qContinuousRange
                    (val0, op12, vname, op23, val1);
                break;
            case ibis::qExpr::OP_GT:
                if (val0 >= val1)
                    res = new ibis::qContinuousRange
                        (vname, ibis::qExpr::OP_GT, val0);
                else
                    res = new ibis::qContinuousRange
                        (vname, ibis::qExpr::OP_GT, val1);
                break;
            case ibis::qExpr::OP_GE:
                if (val0 >= val1)
                    res = new ibis::qContinuousRange
                        (vname, ibis::qExpr::OP_GT, val0);
                else
                    res = new ibis::qContinuousRange
                        (vname, ibis::qExpr::OP_GE, val1);
                break;
            case ibis::qExpr::OP_EQ:
                if (val1 > val0)
                    res = new ibis::qContinuousRange(vname, op23, val1);
                else
                    res = new ibis::qContinuousRange;
                break;
            default:
                res = new ibis::qContinuousRange;
                break;
            }
            break;
        case ibis::qExpr::OP_LE:
            switch (op23) {
            case ibis::qExpr::OP_LT:
            case ibis::qExpr::OP_LE:
                res = new ibis::qContinuousRange
                    (val0, op12, vname, op23, val1);
                break;
            case ibis::qExpr::OP_GT:
                if (val0 > val1)
                    res = new ibis::qContinuousRange
                        (vname, ibis::qExpr::OP_GE, val0);
                else
                    res = new ibis::qContinuousRange
                        (vname, ibis::qExpr::OP_GT, val1);
                break;
            case ibis::qExpr::OP_GE:
                if (val0 >= val1)
                    res = new ibis::qContinuousRange
                        (vname, ibis::qExpr::OP_GE, val0);
                else
                    res = new ibis::qContinuousRange
                        (vname, ibis::qExpr::OP_GE, val1);
                break;
            case ibis::qExpr::OP_EQ:
                if (val1 >= val0)
                    res = new ibis::qContinuousRange(vname, op23, val1);
                else
                    res = new ibis::qContinuousRange;
                break;
            default:
                res = new ibis::qContinuousRange;
                break;
            }
            break;
        case ibis::qExpr::OP_GT:
            switch (op23) {
            case ibis::qExpr::OP_LT:
                if (val0 >= val1)
                    res = new ibis::qContinuousRange
                        (vname, ibis::qExpr::OP_LT, val1);
                else
                    res = new ibis::qContinuousRange
                        (vname, ibis::qExpr::OP_LT, val0);
                break;
            case ibis::qExpr::OP_LE:
                if (val0 >= val1)
                    res = new ibis::qContinuousRange
                        (vname, ibis::qExpr::OP_LT, val0);
                else
                    res = new ibis::qContinuousRange
                        (vname, ibis::qExpr::OP_LE, val1);
                break;
            case ibis::qExpr::OP_GT:
                res = new ibis::qContinuousRange
                    (val1, ibis::qExpr::OP_LT, vname,
                     ibis::qExpr::OP_LT, val0);
                break;
            case ibis::qExpr::OP_GE:
                res = new ibis::qContinuousRange
                    (val1, ibis::qExpr::OP_LE, vname,
                     ibis::qExpr::OP_LT, val0);
                break;
            case ibis::qExpr::OP_EQ:
                if (val1 < val0)
                    res = new ibis::qContinuousRange(vname, op23, val1);
                else
                    res = new ibis::qContinuousRange;
                break;
            default:
                res = new ibis::qContinuousRange;
                break;
            }
            break;
        case ibis::qExpr::OP_GE:
            switch (op23) {
            case ibis::qExpr::OP_LT:
                if (val0 >= val1)
                    res = new ibis::qContinuousRange
                        (vname, ibis::qExpr::OP_LT, val1);
                else
                    res = new ibis::qContinuousRange
                        (vname, ibis::qExpr::OP_LE, val0);
                break;
            case ibis::qExpr::OP_LE:
                if (val0 >= val1)
                    res = new ibis::qContinuousRange
                        (vname, ibis::qExpr::OP_LE, val0);
                else
                    res = new ibis::qContinuousRange
                        (vname, ibis::qExpr::OP_LE, val1);
                break;
            case ibis::qExpr::OP_GT:
                res = new ibis::qContinuousRange
                    (val1, ibis::qExpr::OP_LT, vname,
                     ibis::qExpr::OP_LE, val0);
                break;
            case ibis::qExpr::OP_GE:
                res = new ibis::qContinuousRange
                    (val1, ibis::qExpr::OP_LE, vname,
                     ibis::qExpr::OP_LE, val0);
                break;
            case ibis::qExpr::OP_EQ:
                if (val1 <= val0)
                    res = new ibis::qContinuousRange(vname, op23, val1);
                else
                    res = new ibis::qContinuousRange;
                break;
            default:
                res = new ibis::qContinuousRange;
                break;
            }
            break;
        case ibis::qExpr::OP_EQ:
            switch (op23) {
            case ibis::qExpr::OP_LT:
                if (val0 < val1)
                    res = new ibis::qContinuousRange(vname, op12, val0);
                else
                    res = new ibis::qContinuousRange;
                break;
            case ibis::qExpr::OP_LE:
                if (val0 <= val1)
                    res = new ibis::qContinuousRange(vname, op12, val0);
                else
                    res = new ibis::qContinuousRange;
                break;
            case ibis::qExpr::OP_GT:
                if (val1 < val0)
                    res = new ibis::qContinuousRange(vname, op12, val0);
                else
                    res = new ibis::qContinuousRange;
                break;
            case ibis::qExpr::OP_GE:
                if (val1 <= val0)
                    res = new ibis::qContinuousRange(vname, op12, val0);
                else
                    res = new ibis::qContinuousRange;
                break;
            case ibis::qExpr::OP_EQ:
                if (val1 == val0)
                    res = new ibis::qContinuousRange(vname, op12, val0);
                else
                    res = new ibis::qContinuousRange;
                break;
            default:
                res = new ibis::qContinuousRange;
                break;
            }
            break;
        default:
            res = new ibis::qContinuousRange;
            break;
        }
    }
    return res;
} // ibis::compRange::simpleRange

/// Create a constant expression that always evaluates to true.
ibis::compRange* ibis::compRange::makeConstantTrue() {
    ibis::math::number *t1 = new ibis::math::number(0.0);
    ibis::math::number *t2 = new ibis::math::number(0.0);
    return new ibis::compRange(t1, ibis::qExpr::OP_EQ, t2);
} // ibis::compRange::makeConstantTrue

/// Create a constant expression that always evaluates to false.
ibis::compRange* ibis::compRange::makeConstantFalse() {
    ibis::math::number *one = new ibis::math::number(1.0);
    ibis::math::number *two = new ibis::math::number(2.0);
    return new ibis::compRange(one, ibis::qExpr::OP_EQ, two);
} // ibis::compRange::makeConstantFalse

void ibis::compRange::getTableNames(std::set<std::string>& plist) const {
    if (getLeft() != 0)
        getLeft()->getTableNames(plist);
    if (getRight() != 0)
        getRight()->getTableNames(plist);
    if (expr3 != 0)
        expr3->getTableNames(plist);
} // ibis::compRange::getTableNames

/// Construct a discrete range from two strings.  Used by the parser.
ibis::qDiscreteRange::qDiscreteRange(const char *col, const char *nums)
    : ibis::qRange(ibis::qExpr::DRANGE) {
    if (col == 0 || *col == 0) return;
    name = col;
    if (nums == 0 || *nums == 0) return;
    // use a std::set to temporarily hold the values and eliminate
    // duplicates
    std::set<double> dset;
    const char *str = nums;
    while (*str != 0) {
        char *stmp;
        double dtmp = strtod(str, &stmp);
        if (stmp > str) {// get a value, maybe HUGE_VAL, INF, NAN
            if (dtmp < DBL_MAX && dtmp > -DBL_MAX)
                dset.insert(dtmp);
            str = stmp + strspn(stmp, "\n\v\t, ");
        }
        else { // invalid value, skip to next space
            const char* st = strpbrk(str, "\n\v\t, ");
            if (st != 0)
                str = st + strspn(st, "\n\v\t, ");
            else
                str = st;
        }
    }
    if (! dset.empty()) {
        values.reserve(dset.size());
        for (std::set<double>::const_iterator it = dset.begin();
             it != dset.end(); ++ it)
            values.push_back(*it);
    }
} // qDiscreteRange ctor

/// Construct a qDiscreteRange object from a vector of unsigned 32-bit
/// integers.  Initially used to convert qAnyString to qDiscreteRange,
/// but made visible to public upon user request.
ibis::qDiscreteRange::qDiscreteRange(const char *col,
                                     const std::vector<uint32_t>& val)
    : ibis::qRange(ibis::qExpr::DRANGE) {
    if (col == 0 || *col == 0) return;
    name = col;
    if (val.empty()) return;
    if (val.size() == 1) {
        values.resize(1);
        values[0] = val[0];
        return;
    }

#if 0
    { // use a std::set to temporarily hold the values and eliminate
      // duplicates
        std::set<uint32_t> dset;
        for (std::vector<uint32_t>::const_iterator it = val.begin();
             it != val.end(); ++ it)
            dset.insert(*it);

        if (! dset.empty()) {
            values.reserve(dset.size());
            for (std::set<uint32_t>::const_iterator it = dset.begin();
                 it != dset.end(); ++ it)
                values.push_back(*it);
        }
    }
#else
    { // copy the incoming numbers into array tmp and sort them before
      // passing them to values
        std::vector<uint32_t> tmp(val);
        std::sort(tmp.begin(), tmp.end());
        uint32_t j = 0;
        for (uint32_t i = 1; i < tmp.size(); ++ i) {
            if (tmp[i] > tmp[j]) {
                ++ j;
                tmp[j] = tmp[i];
            }
        }
        tmp.resize(j+1);
        values.resize(tmp.size());
        std::copy(tmp.begin(), tmp.end(), values.begin());
    }
#endif
    if (values.size() < val.size() && ibis::gVerbose > 1) {
        unsigned j = val.size() - values.size();
        ibis::util::logger lg;
        lg() << "qDiscreteRange::ctor accepted incoming int array with "
             << val.size() << " elements, removed " << j
             << " duplicate value" << (j > 1 ? "s" : "");
    }
} // qDiscreteRange ctor

/// Construct a qDiscreteRange from an array of 32-bit integers.
/// @note The incoming array is modified by this funciton.  On return, it
/// will be sorted and contains only unique values.
ibis::qDiscreteRange::qDiscreteRange(const char *col,
                                     ibis::array_t<uint32_t>& val)
    : ibis::qRange(ibis::qExpr::DRANGE) {
    if (col == 0 || *col == 0) return;
    name = col;
    if (val.empty()) return;
    if (val.size() == 1) {
        values.resize(1);
        values[0] = val[0];
        return;
    }

    // sort the incoming values
    std::sort(val.begin(), val.end());
    uint32_t j = 0;
    for (uint32_t i = 1; i < val.size(); ++ i) { // copy unique values
        if (val[i] > val[j]) {
            ++ j;
            val[j] = val[i];
        }
    }
    val.resize(j+1);
    values.resize(j+1);
    std::copy(val.begin(), val.end(), values.begin());

    if (values.size() < val.size() && ibis::gVerbose > 1) {
        unsigned j = val.size() - values.size();
        ibis::util::logger lg;
        lg() << "qDiscreteRange::ctor accepted incoming int array with "
             << val.size() << " elements, removed " << j
             << " duplicate value" << (j > 1 ? "s" : "");
    }
} // qDiscreteRange ctor

/// Construct a qDiscreteRange object from a vector of double values.
ibis::qDiscreteRange::qDiscreteRange(const char *col,
                                     const std::vector<double>& val)
    : ibis::qRange(ibis::qExpr::DRANGE), name(col), values(val) {
    if (val.size() <= 1U) return;
    values.deduplicate();
    LOGGER(values.size() < val.size() && ibis::gVerbose > 1)
        << "qDiscreteRange::ctor accepted incoming double array with "
        << val.size() << " elements as an array with " << values.size()
        << " unique value" << (values.size() > 1 ? "s" : "");
} // ibis::qDiscreteRange::qDiscreteRange

/// Construct a qDiscreteRange object from an array of double values.
/// @note The incoming values are sorted and only the unique ones are kept
/// on returning from this function.
ibis::qDiscreteRange::qDiscreteRange(const char *col,
                                     ibis::array_t<double>& val)
    : ibis::qRange(ibis::qExpr::DRANGE), name(col) {
    if (val.empty()) return;
    val.deduplicate();
    values.copy(val);
} // ibis::qDiscreteRange::qDiscreteRange

void ibis::qDiscreteRange::print(std::ostream& out) const {
    out << name << " IN (";
    //     std::copy(values.begin(), values.end(),
    //        std::ostream_iterator<double>(out, ", "));
    if (values.size() > 0) {
        uint32_t prt = ((values.size() >> ibis::gVerbose) > 1) ?
            (1U << ibis::gVerbose) : values.size();
        if (prt == 0)
            prt = 1;
        else if (prt+prt >= values.size())
            prt = values.size();
        out << values[0];
        for (uint32_t i = 1; i < prt; ++ i)
            out << ", " << values[i];
        if (prt < values.size())
            out << " ... " << values.size()-prt << " omitted";
    }
    out << ')';
} // ibis::qDiscreteRange::print

/// Convert to a sequence of qContinuousRange.
ibis::qExpr* ibis::qDiscreteRange::convert() const {
    if (name.empty()) return 0;
    if (values.empty()) { // an empty range
        ibis::qContinuousRange *ret = new ibis::qContinuousRange
            (0.0, OP_LE, name.c_str(), OP_LT, -1.0);
        return ret;
    }

    ibis::qExpr *ret = new ibis::qContinuousRange
        (name.c_str(), ibis::qExpr::OP_EQ, values[0]);
    for (uint32_t i = 1; i < values.size(); ++ i) {
        ibis::qExpr *rhs = new ibis::qContinuousRange
            (name.c_str(), ibis::qExpr::OP_EQ, values[i]);
        ibis::qExpr *op = new ibis::qExpr(ibis::qExpr::LOGICAL_OR);
        op->setRight(rhs);
        op->setLeft(ret);
        ret = op;
    }
    return ret;
} // ibis::qDiscreteRange::convert

void ibis::qDiscreteRange::restrictRange(double left, double right) {
    if (left > right)
        return;
    uint32_t start = 0;
    uint32_t size = values.size();
    while (start < size && values[start] < left)
        ++ start;

    uint32_t sz;
    if (start > 0) { // need to copy values
        for (sz = 0; sz+start < size && values[sz+start] <= right; ++ sz)
            values[sz] = values[sz+start];
    }
    else {
        for (sz = 0; sz < size && values[sz] <= right; ++ sz);
    }
    values.resize(sz);
} // ibis::qDiscreteRange::restrictRange

// Does the given range overlap with the query range?  Returns true for
// yes, false for no.  The arguements lo and hi are both included in the
// range specified.  This is consistent with how the the two arguments to
// the SQL clause "v between a and b."
bool ibis::qDiscreteRange::overlap(double lo, double hi) const {
    if (! (lo <= hi)) { // invalid (lo, hi) pair, assume overlap true
        return true;
    }
    else if (lo == hi) {
        return inRange(lo);
    }

    if (values.empty()) return false;
    return (lo <= values.back() && hi >= values.front());
} // ibis::qDiscreteRange::overlap

/// Constructor.  Take a single number.
ibis::qIntHod::qIntHod(const char* col, int64_t v1)
    : ibis::qRange(ibis::qExpr::INTHOD), name(col), values(1) {
    values[0] = v1;
} // ibis::qIntHod::qIntHod

/// Constructor.  Take two numbers.
ibis::qIntHod::qIntHod(const char* col, int64_t v1, int64_t v2)
    : ibis::qRange(ibis::qExpr::INTHOD), name(col), values(2) {
    if (v1 == v2) {
        values.resize(1);
        values[0] = v1;
    }
    else if (v1 < v2) {
        values[0] = v1;
        values[1] = v2;
    }
    else {
        values[0] = v2;
        values[1] = v1;
    }
} // ibis::qIntHod::qIntHod

/// Constructor.  This Constructor takes a list of values in a string.  The
/// values are extracted using the function ibis::util::readInt.
ibis::qIntHod::qIntHod(const char* col, const char* nums)
    : ibis::qRange(ibis::qExpr::INTHOD), name(col) {
    int ierr;
    int64_t tmp;
    while (nums != 0 && *nums != 0) {
        // skip delimiters
        const char* str = nums + strspn(nums, ibis::util::delimiters);
        nums = str;
        // extract the integer
        ierr = ibis::util::readInt(tmp, nums);
        if (ierr == 0) {
            values.push_back(tmp);
        }
        else if (ibis::gVerbose > 0) {
            ibis::util::logger lg;
            lg() << "Warning -- qIntHod::ctor failed to parse \"";
            while (str < nums) {
                lg() << *str;
                ++ str;
            }
            lg() << "\" into an integer, ibis::util::readInt returned "
                 << ierr;
        }
    }
    values.deduplicate();
} // ibis::qIntHod::qIntHod

/// Constructor.  The incoming values are sorted to ascending order and
/// duplciates are removed.
ibis::qIntHod::qIntHod(const char* col, const std::vector<int64_t>& nums)
    : ibis::qRange(ibis::qExpr::INTHOD), name(col), values(nums.size()) {
    std::copy(nums.begin(), nums.end(), values.begin());
    values.deduplicate();
} // ibis::qIntHod::qIntHod

/// Constructor.  The incoming values are sorted to the ascending order and
/// duplicates are removed.
ibis::qIntHod::qIntHod(const char* col, const ibis::array_t<int64_t>& nums)
    : ibis::qRange(ibis::qExpr::INTHOD), name(col), values(nums) {
    values.deduplicate();
} // ibis::qIntHod::qIntHod

void ibis::qIntHod::restrictRange(double left, double right) {
    if (left > right)
        return;
    uint32_t start = 0;
    uint32_t size = values.size();
    while (start < size && values[start] < left)
        ++ start;

    uint32_t sz;
    if (start > 0) { // need to copy values
        for (sz = 0; sz+start < size && values[sz+start] <= right; ++ sz)
            values[sz] = values[sz+start];
    }
    else {
        for (sz = 0; sz < size && values[sz] <= right; ++ sz);
    }
    values.resize(sz);
} // ibis::qIntHod::restrictRange

/// Print a short version of the query expression.
void ibis::qIntHod::print(std::ostream& out) const {
    out << name << " IN (";
    if (values.size() > 0) {
        uint32_t prt = ((values.size() >> ibis::gVerbose) > 1) ?
            (1U << ibis::gVerbose) : values.size();
        if (prt == 0)
            prt = 1;
        else if (prt+prt >= values.size())
            prt = values.size();
        out << values[0];
        for (uint32_t i = 1; i < prt; ++ i)
            out << "LL, " << values[i];
        out << "LL";
        if (prt < values.size())
            out << " ... " << values.size()-prt << " omitted";
    }
    out << ')';
} // ibis::qIntHod::print

/// Print the full list of values.  The number are follwed by suffix 'LL'
/// to ensure the resulting string can be parsed back as the same
/// expression.
void ibis::qIntHod::printFull(std::ostream& out) const {
    out << name << " IN (";
    // std::copy(values.begin(), values.end(),
    //        std::ostream_iterator<int64_t>(out, "LL, "));
    if (values.size() > 0) {
        out << values[0];
        for (size_t j = 1; j < values.size(); ++ j)
            out << "LL, " << values[j];
        out << "LL";
    }
    out << ')';
} // ibis::qIntHod::printFull

/// Constructor.  Take a single number.
ibis::qUIntHod::qUIntHod(const char* col, uint64_t v1)
    : ibis::qRange(ibis::qExpr::INTHOD), name(col), values(1) {
    values[0] = v1;
} // ibis::qUIntHod::qUIntHod

/// Constructor.  Take two numbers.
ibis::qUIntHod::qUIntHod(const char* col, uint64_t v1, uint64_t v2)
    : ibis::qRange(ibis::qExpr::INTHOD), name(col), values(2) {
    if (v1 == v2) {
        values.resize(1);
        values[0] = v1;
    }
    else if (v1 < v2) {
        values[0] = v1;
        values[1] = v2;
    }
    else {
        values[0] = v2;
        values[1] = v1;
    }
} // ibis::qUIntHod::qUIntHod

/// Constructor.  This Constructor takes a list of values in a string.  The
/// values are extracted using the function ibis::util::readUInt.
ibis::qUIntHod::qUIntHod(const char* col, const char* nums)
    : ibis::qRange(ibis::qExpr::UINTHOD), name(col) {
    int ierr;
    uint64_t tmp;
    while (nums != 0 && *nums != 0) {
        // skip delimiters
        const char* str = nums + strspn(nums, ibis::util::delimiters);
        nums = str;
        // extract the integer
        ierr = ibis::util::readUInt(tmp, nums);
        if (ierr == 0) {
            values.push_back(tmp);
        }
        else if (ibis::gVerbose > 0) {
            ibis::util::logger lg;
            lg() << "Warning -- qUIntHod::ctor failed to parse \"";
            while (str < nums) {
                lg() << *str;
                ++ str;
            }
            lg() << "\" into an integer, ibis::util::readUInt returned "
                 << ierr;
        }
    }
    values.deduplicate();
} // ibis::qUIntHod::qUIntHod

/// Constructor.  The incoming values are sorted into ascending order and
/// duplicates are removed.
ibis::qUIntHod::qUIntHod(const char* col, const std::vector<uint64_t>& nums)
    : ibis::qRange(ibis::qExpr::UINTHOD), name(col), values(nums.size()) {
    std::copy(nums.begin(), nums.end(), values.begin());
    values.deduplicate();
} // ibis::qUIntHod::qUIntHod

/// Constructor.  The incoming values are sorted into ascending order and
/// duplicates are removed.
ibis::qUIntHod::qUIntHod(const char* col, const ibis::array_t<uint64_t>& nums)
    : ibis::qRange(ibis::qExpr::UINTHOD), name(col), values(nums) {
    values.deduplicate();
} // ibis::qUIntHod::qUIntHod

void ibis::qUIntHod::restrictRange(double left, double right) {
    if (left > right)
        return;
    uint32_t start = 0;
    uint32_t size = values.size();
    while (start < size && values[start] < left)
        ++ start;

    uint32_t sz;
    if (start > 0) { // need to copy values
        for (sz = 0; sz+start < size && values[sz+start] <= right; ++ sz)
            values[sz] = values[sz+start];
    }
    else {
        for (sz = 0; sz < size && values[sz] <= right; ++ sz);
    }
    values.resize(sz);
} // ibis::qUIntHod::restrictRange

/// Print a short version of the expression.
void ibis::qUIntHod::print(std::ostream& out) const {
    out << name << " IN (";
    if (values.size() > 0) {
        uint32_t prt = ((values.size() >> ibis::gVerbose) > 1) ?
            (1U << ibis::gVerbose) : values.size();
        if (prt == 0)
            prt = 1;
        else if (prt+prt >= values.size())
            prt = values.size();
        out << values[0];
        for (uint32_t i = 1; i < prt; ++ i)
            out << "ULL, " << values[i];
        out << "ULL";
        if (prt < values.size())
            out << " ... " << values.size()-prt << " omitted";
    }
    out << ')';
} // ibis::qUIntHod::print

/// Print a long version of the expression.  This function appends 'ULL' to
/// each number so that they are guaranteed to be translated to the same
/// type query expression is the output is sent back to the parser again.
void ibis::qUIntHod::printFull(std::ostream& out) const {
    out << name << " IN (";
    // std::copy(values.begin(), values.end(),
    //        std::ostream_iterator<uint64_t>(out, "ULL, "));
    if (values.size() > 0) {
        out << values[0];
        for (size_t j = 1; j < values.size(); ++ j)
            out << "ULL, " << values[j];
        out << "ULL";
    }
    out << ')';
} // ibis::qUIntHod::printFull

void ibis::qExists::print(std::ostream& out) const {
    if (name.empty()) return;
    out << "EXISTS(" << name << ')';
} // ibis::qExists::print

void ibis::qExists::printFull(std::ostream& out) const {
    if (name.empty()) return;
    out << "EXISTS(" << name << ')';
} // ibis::qExists::printFull

ibis::qAnyString::qAnyString(const char *col, const char *sval)
    : ibis::qExpr(ibis::qExpr::ANYSTRING) {
    if (col == 0 || *col == 0) return;
    name = col;
    if (sval == 0 || *sval == 0) return;
    std::set<std::string> sset; // use it to sort and remove duplicate
    while (*sval != 0) {
        std::string tmp;
        tmp.erase();
        while (*sval && isspace(*sval)) ++ sval; // skip leading space
        if (*sval == '\'') { // single quoted string
            ++ sval;
            while (*sval) {
                if (*sval != '\'')
                    tmp += *sval;
                else if (tmp.size() > 0 && tmp[tmp.size()-1] == '\\')
                    // escaped quote
                    tmp[tmp.size()-1] = '\'';
                else {// found the end quote
                    ++ sval; // skip the closing quote
                    break;
                }
                ++ sval;
            }
            if (! tmp.empty())
                sset.insert(tmp);
        }
        else if (*sval == '"') { // double quoted string
            ++ sval;
            while (*sval) {
                if (*sval != '"')
                    tmp += *sval;
                else if (tmp.size() > 0 && tmp[tmp.size()-1] == '\\')
                    // escaped quote
                    tmp[tmp.size()-1] = '"';
                else {
                    ++ sval; // skip the closing quote
                    break;
                }
                ++ sval;
            }
            if (! tmp.empty())
                sset.insert(tmp);
        }
        else { // space and comma delimited string
            while (*sval) {
                if (*sval != ',' && ! isspace(*sval))
                    tmp += *sval;
                else if (tmp[tmp.size()-1] == '\\')
                    tmp[tmp.size()-1] = *sval;
                else
                    break;
                ++ sval;
            }
            if (! tmp.empty())
                sset.insert(tmp);
        }
        if (*sval != 0)
            sval += strspn(sval, "\n\v\t, ");
    }

    if (! sset.empty()) {
        values.reserve(sset.size());
        for (std::set<std::string>::const_iterator it = sset.begin();
             it != sset.end(); ++ it)
            values.push_back(*it);
    }
} // ibis::qAnyString ctor

void ibis::qAnyString::print(std::ostream& out) const {
    if (name.empty()) return;
    out << name << " IN (";
    //     std::copy(values.begin(), values.end(),
    //        std::ostream_iterator<std::string>(out, ", "));
    if (values.size() > 0) {
        out << values[0];
        for (uint32_t i = 1; i < values.size(); ++ i)
            out << ", " << values[i];
    }
    out << ')';
} // ibis::qAnyString::print

ibis::qExpr* ibis::qAnyString::convert() const {
    if (name.empty()) return 0;
    if (values.empty()) return 0;

    ibis::qExpr *ret = new ibis::qString(name.c_str(), values[0].c_str());
    for (uint32_t i = 1; i < values.size(); ++ i) {
        ibis::qExpr *rhs = new ibis::qString(name.c_str(), values[i].c_str());
        ibis::qExpr *op = new ibis::qExpr(ibis::qExpr::LOGICAL_OR);
        op->setRight(rhs);
        op->setLeft(ret);
        ret = op;
    }
    return ret;
} // ibis::qAnyString::convert

void ibis::qAnyString::getTableNames(std::set<std::string>& plist) const {
    if (! name.empty()) {
        const std::string& tn = ibis::qExpr::extractTableName(name.c_str());
        if (! tn.empty())
            plist.insert(tn);
    }
} // ibis::qAnyString::getTableNames

void ibis::deprecatedJoin::print(std::ostream& out) const {
    out << "join(" << name1 << ", " << name2;
    if (expr)
        out << ", " << *expr;
    out << ')';
} // ibis::deprecatedJoin::print

/// Constructing a qAnyAny object from a string and a floating-point value.
ibis::qAnyAny::qAnyAny(const char *pre, const double dbl)
    : ibis::qExpr(ibis::qExpr::ANYANY), prefix(pre) {
    values.resize(1);
    values[0] = dbl;
}

/// Constructing an object of type qAnyAny from two strings.  The second
/// string is expected to be a list of numbers separated by coma and space.
ibis::qAnyAny::qAnyAny(const char *pre, const char *val)
    : ibis::qExpr(ibis::qExpr::ANYANY), prefix(pre) {
    // use a std::set to temporarily hold the values and eliminate
    // duplicates
    std::set<double> dset;
    const char *str = val + (*val=='(');
    while (*str != 0) {
        char *stmp;
        double dtmp = strtod(str, &stmp);
        if (stmp > str) {// get a value, maybe HUGE_VAL, INF, NAN
            if (dtmp < DBL_MAX && dtmp > -DBL_MAX)
                dset.insert(dtmp);
            str = stmp + strspn(stmp, "\n\v\t, ");
        }
        else { // invalid value, skip to next space
            const char *st1 = strpbrk(str, "\n\v\t, ");
            if (st1 != 0)
                str = st1 + strspn(st1, "\n\v\t, ");
            else
                str = st1;
        }
    }
    if (! dset.empty()) {
        values.reserve(dset.size());
        for (std::set<double>::const_iterator it = dset.begin();
             it != dset.end(); ++ it)
            values.push_back(*it);
    }
} // ibis::qAnyAny

void ibis::qAnyAny::print(std::ostream& out) const {
    if (values.size() > 1) {
        out << "ANY(" << prefix << ") IN (";
        if (values.size() > 0) {
            out << values[0];
            for (uint32_t i = 1; i < values.size(); ++ i)
                out << ", " << values[i];
        }
        out << ')';
    }
    else if (values.size() == 1) {
        out << "ANY(" << prefix << ")==" << values.back();
    }
} // ibis::qAnyAny::print

void ibis::qAnyAny::getTableNames(std::set<std::string>& plist) const {
    if (! prefix.empty()) {
        const std::string& tn = ibis::qExpr::extractTableName(prefix.c_str());
        if (! tn.empty())
            plist.insert(tn);
    }
} // ibis::qAnyAny::getTableNames

ibis::qKeyword::qKeyword(const char* ls, const char* rs) :
    qExpr(ibis::qExpr::KEYWORD), name(ibis::util::strnewdup(ls)) {
    // attempt to remove the back slash as escape characters
    kword = new char[1+std::strlen(rs)];
    const char* cptr = rs;
    char* dptr = kword;
    while (*cptr != 0) {
        if (*cptr != '\\') {
            *dptr = *cptr;
        }
        else {
            ++cptr;
            *dptr = *cptr;
        }
        ++cptr; ++dptr;
    }
    *dptr = 0; // terminate kword with the NULL character
}

void ibis::qKeyword::print(std::ostream& out) const {
    if (name && kword)
        out << name << " CONTAINS \'" << kword << '\'';
}

void ibis::qKeyword::getTableNames(std::set<std::string>& plist) const {
    if (name != 0) {
        const std::string tn = ibis::qExpr::extractTableName(name);
        if (! tn.empty())
            plist.insert(tn);
    }
} // ibis::qKeyword::getTableNames

ibis::qAllWords::qAllWords(const char *sname, const char *s1, const char *s2)
    : ibis::qExpr(ibis::qExpr::ALLWORDS), name(sname) {
    if (s1 != 0 && *s1 != 0) {
        if (s2 != 0 && *s2 != 0) {
            if (
#if FASTBIT_CASE_SENSITIVE_COMPARE+0 == 0
                stricmp(s1, s2)
#else
                std::strcmp(s1, s2)
#endif
                <= 0) {
                values.push_back(s1);
                values.push_back(s2);
            }
            else {
                values.push_back(s2);
                values.push_back(s1);
            }
        }
        else {
            values.push_back(s1);
        }
    }
    else if (s2 != 0 && *s2 != 0) {
        values.push_back(s2);
    }
} // ibis::qAllWords::qAllWords

ibis::qAllWords::qAllWords(const char *sname, const char *sval)
    : ibis::qExpr(ibis::qExpr::ALLWORDS) {
    if (sname == 0 || sval == 0 || *sname == 0 || *sval == 0) return;
    name = sname;

    std::set<std::string> sset; // use it to sort and remove duplicate
    std::string tmp;
    while (*sval != 0) {
        tmp.erase();
        (void) ibis::util::readString(tmp, sval, ibis::util::delimiters);
        if (! tmp.empty()) {
#if FASTBIT_CASE_SENSITIVE_COMPARE+0 == 0
            for (size_t j = 0; j < tmp.size(); ++ j)
                tmp[j] = tolower(tmp[j]);
#endif
            sset.insert(tmp);
        }
    }

    if (! sset.empty()) {
        values.reserve(sset.size());
        for (std::set<std::string>::const_iterator it = sset.begin();
             it != sset.end(); ++ it)
            values.push_back(*it);
    }
} // ibis::qAllWords ctor

void ibis::qAllWords::print(std::ostream& out) const {
    if (name.empty()) return;
    out << name << " CONTAINS (";
    if (values.size() > 0) {
        out << values[0];
        for (uint32_t i = 1; i < values.size(); ++ i)
            out << ", '" << values[i] << '\'';
    }
    out << ')';
} // ibis::qAllWords::print

ibis::qExpr* ibis::qAllWords::convert() const {
    if (name.empty()) {
        return 0;
    }
    else if (values.empty()) {
        return 0;
    }
    else if (values.size() == 0) {
        return new ibis::qKeyword(name.c_str(), values[0].c_str());
    }
    else {
        return const_cast<ibis::qAllWords*>(this);
    }
} // ibis::qAllWords::convert

void ibis::qAllWords::getTableNames(std::set<std::string>& plist) const {
    if (! name.empty()) {
        const std::string& tn = ibis::qExpr::extractTableName(name.c_str());
        if (! tn.empty())
            plist.insert(tn);
    }
} // ibis::qAllWords::getTableNames

void ibis::math::customFunction1::print(std::ostream& out) const {
    if (fun_ != 0) {
        fun_->printName(out);
        out << '(';
        if (getLeft() != 0) {
            getLeft()->print(out);
        }
        else if (getRight() != 0) {
            getRight()->print(out);
        }

        std::ostringstream oss;
        fun_->printDecoration(oss);
        if (oss.str().empty() == false) {
            if (getLeft() != 0 || getRight() != 0)
                out << ", ";
            out << oss.str();
        }
        out << ')';
    }
    else {
        out << "<Ill-formed one-argument function>";
    }
} // ibis::math::customFunction1::print

double ibis::math::customFunction1::eval() const {
    double arg =
        static_cast<const ibis::math::term*>(getLeft())->eval();
    if (fun_ != 0)
        return fun_->eval(arg);
    else
        return FASTBIT_DOUBLE_NULL;
} // ibis::math::customFunction1::eval

double ibis::math::fromUnixTime::eval(double val) const {
    if (fmt_.empty()) return val; // do nothing

    double res;
    char buf[80];
    const char *str;
    const time_t sec = static_cast<time_t>(val);
#if (defined(__MINGW__) || defined(__MINGW32__) || defined(__MINGW64__) || defined(_MSC_VER)) && defined(_WIN32)
    struct tm *mytm;
    if (tzname_[0] == 'g' || tzname_[0] == 'G' ||
        tzname_[0] == 'u' || tzname_[0] == 'U')
        mytm = gmtime(&sec);
    else
        mytm = localtime(&sec);
    (void) strftime(buf, 80, fmt_.c_str(), mytm);
#else
    struct tm mytm;
    if (tzname_[0] == 'g' || tzname_[0] == 'G' ||
        tzname_[0] == 'u' || tzname_[0] == 'U')
        (void) gmtime_r(&sec, &mytm);
    else
        (void) localtime_r(&sec, &mytm);
    (void) strftime(buf, 80, fmt_.c_str(), &mytm);
#endif
    str = buf;
    ibis::util::readDouble(res, str);
    LOGGER(*str != 0 && ibis::gVerbose > 1)
        << "Warning -- fromUnixTime::eval encountered a problem while "
        "attempting to convert " << fmt_ << " of " << sec
        << " into a double value";
    return res;
} // ibis::math::fromUnixTime::eval

void ibis::math::fromUnixTime::printName(std::ostream &out) const {
    out << "FROM_UNIXTIME_";
    if (tzname_.empty()) {
        out << "LOCAL";
    }
    else {
        out << tzname_;
    }
} // ibis::math::fromUnixTime::print

void ibis::math::fromUnixTime::printDecoration(std::ostream &out) const {
    out << '"' << fmt_ << '"';
} // ibis::math::fromUnixTime::printDecoration

double ibis::math::toUnixTime::eval(double val) const {
    double res;
    struct tm mytm;
    mytm.tm_year = val / 1E10;
    val -= mytm.tm_year * 1E10;
    mytm.tm_mon = val / 1E8;
    val -= mytm.tm_mon * 1E8;
    LOGGER(mytm.tm_mon > 11 && ibis::gVerbose > 3)
        << "Warning -- toUnixTime(" << val << ") -- month (" << mytm.tm_mon
        << ") is out of range";

    mytm.tm_mday = val / 1E6;
    val -= mytm.tm_mday * 1E6;
    LOGGER(mytm.tm_mday > 31 && ibis::gVerbose > 3)
        << "Warning -- toUnixTime(" << val << ") -- day of month ("
        << mytm.tm_mday << ") is out of range";

    mytm.tm_hour = val / 1E4;
    val -= mytm.tm_hour * 1E4;
    LOGGER(mytm.tm_hour > 23 && ibis::gVerbose > 3)
        << "Warning -- toUnixTime(" << val << ") -- hour of day ("
        << mytm.tm_hour << ") is out of range";

    mytm.tm_min = val / 1E2;
    val -= mytm.tm_min * 1E2;
    LOGGER(mytm.tm_min > 59 && ibis::gVerbose > 3)
        << "Warning -- toUnixTime(" << val << ") -- minute of hour ("
        << mytm.tm_min << ") is out of range";

    mytm.tm_sec = static_cast<int>(val);
    val -= mytm.tm_sec;
    LOGGER(mytm.tm_sec > 59 && ibis::gVerbose > 3)
        << "Warning -- toUnixTime(" << val << ") -- second of minute ("
        << mytm.tm_sec << ") is out of range";

    if (tzname_[0] == 'g' || tzname_[0] == 'G' ||
        tzname_[0] == 'u' || tzname_[0] == 'U') {
#if  (defined(HAVE_GETPWUID) || defined(HAVE_GETPWUID_R)) && !(defined(__MINGW__) || defined(__MINGW32__) || defined(__MINGW64__) || defined(__CYGWIN__))
        // timegm
        char *tz = getenv("TZ");
        tzset();
        res = mktime(&mytm);
        if (tz != 0)
            setenv("TZ", tz, 1);
        else
            unsetenv("TZ");
        tzset();
#else
        res = mktime(&mytm);
#endif
    }
    else {
        res = mktime(&mytm);
    }
    res += val; // add the fraction of second
    return res;
} // ibis::math::toUnixTime::eval

void ibis::math::toUnixTime::printName(std::ostream &out) const {
    out << "TO_UNIXTIME_";
    if (tzname_.empty()) {
        out << "LOCAL";
    }
    else {
        out << tzname_;
    }
} // ibis::math::toUnixTime::print

void ibis::math::toUnixTime::printDecoration(std::ostream &) const {
    return; // nothing to do
} // ibis::math::toUnixTime::printDecoration

void ibis::math::stringFunction1::print(std::ostream& out) const {
    if (fun_ != 0) {
        fun_->printName(out);
        out << '(';
        if (getLeft() != 0) {
            getLeft()->print(out);
        }
        else if (getRight() != 0) {
            getRight()->print(out);
        }

        std::ostringstream oss;
        fun_->printDecoration(oss);
        if (oss.str().empty() == false) {
            if (getLeft() != 0 || getRight() != 0)
                out << ", ";
            out << oss.str();
        }
        out << ')';
    }
    else {
        out << "<Ill-formed one-argument function>";
    }
} // ibis::math::stringFunction1::print

std::string ibis::math::stringFunction1::sval() const {
    double arg =
        static_cast<const ibis::math::term*>(getLeft())->eval();
    if (fun_ != 0)
        return fun_->eval(arg);
    else
        return std::string();
} // ibis::math::stringFunction1::sval

std::string ibis::math::formatUnixTime::eval(double val) const {
    if (fmt_.empty()) { // convert double to std::string
        std::ostringstream oss;
        oss << val;
        return oss.str();
    }

    char buf[80];
    const time_t sec = static_cast<time_t>(val);
#if (defined(__MINGW__) || defined(__MINGW32__) || defined(__MINGW64__) || defined(_MSC_VER)) && defined(_WIN32)
    struct tm *mytm;
    if (tzname_[0] == 'g' || tzname_[0] == 'G' ||
        tzname_[0] == 'u' || tzname_[0] == 'U')
        mytm = gmtime(&sec);
    else
        mytm = localtime(&sec);
    (void) strftime(buf, 80, fmt_.c_str(), mytm);
#else
    struct tm mytm;
    if (tzname_[0] == 'g' || tzname_[0] == 'G' ||
        tzname_[0] == 'u' || tzname_[0] == 'U')
        (void) gmtime_r(&sec, &mytm);
    else
        (void) localtime_r(&sec, &mytm);
    (void) strftime(buf, 80, fmt_.c_str(), &mytm);
#endif
    return std::string(buf);
} // ibis::math::formatUnixTime::eval

void ibis::math::formatUnixTime::printName(std::ostream &out) const {
    out << "FORMAT_UNIXTIME_";
    if (tzname_.empty()) {
        out << "LOCAL";
    }
    else {
        out << tzname_;
    }
} // ibis::math::formatUnixTime::print

void ibis::math::formatUnixTime::printDecoration(std::ostream &out) const {
    out << '"' << fmt_ << '"';
} // ibis::math::formatUnixTime::printDecoration

