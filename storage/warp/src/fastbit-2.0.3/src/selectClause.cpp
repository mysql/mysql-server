// $Id$
// Author: John Wu <John.Wu at acm.org>
//      Lawrence Berkeley National Laboratory
// Copyright (c) 2007-2016 the Regents of the University of California
#include "part.h"
#include "qExpr.h"
#include "selectLexer.h"
#include "selectClause.h"

ibis::selectClause::selectClause(const char *cl) : lexer(0) {
    //#if defined(DEBUG) || defined(_DEBUG)
    LOGGER(ibis::gVerbose > 3)
        << "Constructing selectClause @ " << this;
    //#endif
    if (cl == 0 || *cl == 0) return;

    int ierr = parse(cl);
    LOGGER(ierr < 0 && ibis::gVerbose >= 0)
        << "Warning -- selectClause::ctor failed to parse \"" << cl
        << "\", function parse returned " << ierr;
} // ibis::selectClause::selectClause

ibis::selectClause::selectClause(const ibis::table::stringArray &sl)
    : lexer(0) {
    //#if defined(DEBUG) || defined(_DEBUG)
    LOGGER(ibis::gVerbose > 3)
        << "Constructing selectClause @ " << this;
    //#endif
    std::string tmp;
    for (size_t j = 0; j < sl.size(); ++ j) {
        if (sl[j] != 0 && *(sl[j]) != 0) {
            if (! tmp.empty())
                tmp += ", ";
            tmp += sl[j];
        }
    }
    if (tmp.empty()) return;

    int ierr = parse(tmp.c_str());
    LOGGER(ierr < 0 && ibis::gVerbose >= 0)
        << "Warning -- selectClause::ctor failed to parse \"" << tmp
        << "\", function parse returned " << ierr;
} // ibis::selectClause::selectClause

ibis::selectClause::selectClause(const ibis::selectClause& rhs)
    : atms_(rhs.atms_.size()), aggr_(rhs.aggr_), names_(rhs.names_),
      ordered_(rhs.ordered_), xtms_(rhs.xtms_.size()), xalias_(rhs.xalias_),
      xnames_(rhs.xnames_), clause_(rhs.clause_), lexer(0) {
    //#if defined(DEBUG) || defined(_DEBUG)
    if (ibis::gVerbose > 3) {
        ibis::util::logger lg;
        lg() << "Coping selectClause @ " << this << " from content @ "
             << static_cast<const void*>(&rhs);
    }
    //#endif
    for (uint32_t i = 0; i < rhs.atms_.size(); ++ i)
        atms_[i] = rhs.atms_[i]->dup();

    varMap vmap;
    for (uint32_t j = 0; j < rhs.xtms_.size(); ++ j) {
        xtms_[j] = rhs.xtms_[j]->dup();
        gatherVariables(vmap, xtms_[j]);
    }
    for (varMap::iterator it = vmap.begin(); it != vmap.end(); ++ it)
        it->second->updateReference(this);
} // ibis::selectClause::selectClause

ibis::selectClause::~selectClause() {
    //#if defined(DEBUG) || defined(_DEBUG)
    LOGGER(ibis::gVerbose > 3)
        << "Freeing selectClause @ " << this;
    //#endif
    clear();
}

void ibis::selectClause::clear() {
    ibis::util::clearVec(xtms_);
    ibis::util::clearVec(atms_);
    names_.clear();
    ordered_.clear();
    xalias_.clear();
    xnames_.clear();
    clause_.clear();
} // ibis::selectClause::clear

int ibis::selectClause::parse(const char *cl) {
    int ierr = 0;
    if (cl != 0 && *cl != 0) {
        clear();
        LOGGER(ibis::gVerbose > 5)
            << "selectClause::parse cleared existing content before parsing \""
            << cl << "\"";

        if (clause_.c_str() != cl)
            clause_ = cl;
        std::istringstream iss(clause_);
        ibis::util::logger lg;
        selectLexer lx(&iss, &(lg()));
        selectParser parser(*this);
        lexer = &lx;
#if DEBUG+0 > 2
        parser.set_debug_level(DEBUG-1);
#elif _DEBUG+0 > 2
        parser.set_debug_level(_DEBUG-1);
#endif
        parser.set_debug_stream(lg());
        ierr = parser.parse();
        lexer = 0;
        if (ierr == 0) {
            fillNames();
        }
        else {
            clear();
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- selectClause::parse failed to parse string \""
                << cl << "\"";
#ifdef FASTBIT_HALT_ON_PARSER_ERROR
            throw "selectClause failed to parse the incoming string"
                IBIS_FILE_LINE;
#endif
        }
    }
    return ierr;
} // ibis::selectClause::parse

/// Write the string form of an aggregator and artithmetic expression
/// combination.
std::string ibis::selectClause::aggDescription
(ibis::selectClause::AGREGADO ag, const ibis::math::term *tm) const {
    if (tm == 0) return std::string();

    std::ostringstream oss;
    switch (ag) {
    default:
        oss << *(tm);
        break;
    case AVG:
        oss << "AVG(" << *(tm) << ')';
        break;
    case CNT:
        oss << "COUNT(" << *(tm) << ')';
        break;
    case MAX:
        oss << "MAX(" << *(tm) << ')';
        break;
    case MIN:
        oss << "MIN(" << *(tm) << ')';
        break;
    case SUM:
        oss << "SUM(" << *(tm) << ')';
        break;
    case CONCAT:
        oss << "GROUP_CONCAT(" << *(tm) << ')';
        break;
    case DISTINCT:
        oss << "COUNTDISTINCT(" << *(tm) << ')';
        break;
    case VARPOP:
        oss << "VARPOP(" << *(tm) << ')';
        break;
    case VARSAMP:
        oss << "VAR(" << *(tm) << ')';
        break;
    case STDPOP:
        oss << "STDPOP(" << *(tm) << ')';
        break;
    case STDSAMP:
        oss << "STD(" << *(tm) << ')';
        break;
    case MEDIAN:
        oss << "MEDIAN(" << *(tm) << ')';
        break;
    }
    return oss.str();
} // ibis::selectClause::aggDescription

/// Fill array names_ and xnames_.  An alias for an aggregation operation
/// is used as the external name for the whole term.  This function
/// resolves all external names first to establish all aliases, and then
/// resolve the names of the arguments to the aggregation functions.  The
/// arithmetic expressions without external names are given names of the
/// form "_hhh", where "hhh" is a hexadecimal number.
void ibis::selectClause::fillNames() {
    names_.clear();
    xnames_.clear();
    if (atms_.empty()) return;

    names_.resize(atms_.size());
    xnames_.resize(xtms_.size());

    // go through the aliases first before making up names
    for (StringToInt::const_iterator it = xalias_.begin();
         it != xalias_.end(); ++ it)
        xnames_[it->second] = it->first;

    // fill the external names
    for (uint32_t j = 0; j < xtms_.size(); ++ j) {
        if (xnames_[j].empty() &&
            xtms_[j]->termType() == ibis::math::VARIABLE) {
            const char *vn = static_cast<const ibis::math::variable*>
                (xtms_[j])->variableName();
            uint64_t jv = atms_.size();
            if (vn[0] == '_' && vn[1] == '_')
                if (0 > ibis::util::decode16(jv, vn+2))
                    jv = atms_.size();
            if (jv < names_.size() && !names_[jv].empty())
                xnames_[j] = names_[jv];
            else
                xnames_[j] = vn;

            // size_t pos = xnames_[j].rfind('.');
            // if (pos < xnames_[j].size())
            //  xnames_[j].erase(0, pos+1);
        }

        if (xnames_[j].empty()) {
            std::ostringstream oss;
            oss << "_" << std::hex << j;
            xnames_[j] = oss.str();
        }
        else {
            if (isalpha(xnames_[j][0]) == 0 && xnames_[j][0] != '_')
                xnames_[j][0] = 'A' + (xnames_[j][0] % 26);
            for (unsigned i = 1; i < xnames_[j].size(); ++ i)
                if (isalnum(xnames_[j][i]) == 0)
                    xnames_[j][i] = '_';
        }
    }

    // fill the argument name
    for (uint32_t j = 0; j < atms_.size(); ++ j) {
        if (! names_[j].empty()) continue; // have a name already

        if (atms_[j]->termType() == ibis::math::VARIABLE &&
            aggr_[j] == ibis::selectClause::NIL_AGGR) {
            names_[j] = static_cast<const ibis::math::variable*>(atms_[j])
                ->variableName();

            // size_t pos = names_[j].rfind('.');
            // if (pos < names_[j].size())
            //  names_[j].erase(0, pos+1);
        }
        if (names_[j].empty()) {
            std::ostringstream oss;
            oss << "__" << std::hex << j;
            names_[j] = oss.str();
        }
        else {
            if (isalpha(names_[j][0]) == 0 && names_[j][0] != '_')
                names_[j][0] = 'A' + (names_[j][0] % 26);
            for (unsigned i = 1; i < names_[j].size(); ++ i)
                if (isalnum(names_[j][i]) == 0)
                    names_[j][i] = '_';
        }
    }

    if (ibis::gVerbose > 2) {
        ibis::util::logger lg;
        lg() << "selectClause::fillNames -- ";
        printDetails(lg());
    }
} // ibis::selectClause::fillNames

/// Map internal column names to external column names.  The key of the map
/// is the internal column names, i.e., the column names used by the
/// ibis::bord object generated with a selct clause.  If that ibis::bord
/// object does not go through any aggregation operation, then the columns
/// need to be renamed using this information.
///
/// It returns the number of changes needed.  A negative number is used to
/// indicate error.
int ibis::selectClause::getAliases(ibis::selectClause::nameMap& nmap) const {
    nmap.clear();
    for (unsigned j = 0; j < xtms_.size(); ++ j) {
        if (xtms_[j]->termType() == ibis::math::VARIABLE) {
            const char* vn = static_cast<const ibis::math::variable*>
                (xtms_[j])->variableName();
            if (stricmp(vn, xnames_[j].c_str()) != 0)
                nmap[vn] = xnames_[j].c_str();
        }
    }
    return nmap.size();
} // ibis::selectClause::getAliases
 
/// Record an aggregation function.  Return a math term of the type
/// variable to the caller so the caller can continue to build up a larger
/// expression.  For simplicity, the variable name is simply "__hhh", where
/// "hhh" is the size of aggr_ in hexadecimal.
///
/// @note This function takes charge of expr.  It will free the object if
/// the object is not passed on to other operations.  This can happen when
/// the particular variable appeared already in the select clause.
ibis::math::variable*
ibis::selectClause::addAgregado(ibis::selectClause::AGREGADO agr,
                                ibis::math::term *expr) {
    if (agr != ibis::selectClause::NIL_AGGR &&
        hasAggregation(expr)) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- selectClause can not have aggregations inside "
            "another aggregation operation (" << *expr << ')';
        throw "selectClause::addAgregado failed due to nested aggregations"
            IBIS_FILE_LINE;
    }

    const unsigned end = atms_.size();
    LOGGER(ibis::gVerbose > 5)
        << "selectClause::addAgregado  -- adding term " << end << ": "
        << aggDescription(agr, expr);
    if (expr->termType() != ibis::math::VARIABLE) {
        aggr_.push_back(agr);
        atms_.push_back(expr);
        std::ostringstream oss;
        oss << "__" << std::hex << end;
        ordered_[oss.str()] = end;
        return new ibis::selectClause::variable(oss.str().c_str(), this);
    }
    else {
        ibis::math::variable *var = static_cast<ibis::math::variable*>(expr);
        ibis::selectClause::StringToInt::const_iterator it =
            ordered_.find(var->variableName());
        if (it == ordered_.end()) { // no in the existing list
            aggr_.push_back(agr);
            atms_.push_back(expr);
            ordered_[var->variableName()] = end;
            if (agr != ibis::selectClause::NIL_AGGR) {
                std::ostringstream oss;
                oss << "__" << std::hex << end;
                ordered_[oss.str()] = end;
                return new ibis::selectClause::variable
                    (oss.str().c_str(), this);
            }
            else {
                return var->dup();
            }
        }
        else if (agr != aggr_[it->second]) { // new aggregation
            aggr_.push_back(agr);
            atms_.push_back(expr);
            if (agr != ibis::selectClause::NIL_AGGR) {
                std::ostringstream oss;
                oss << "__" << std::hex << end;
                ordered_[oss.str()] = end;
                return new ibis::selectClause::variable
                    (oss.str().c_str(), this);
            }
            else {
                ordered_[var->variableName()] = end;
                return var->dup();
            }
        }
        else { // the variable has appeared before
            delete expr;
            std::ostringstream oss;
            oss << "__" << std::hex << it->second;
            return new ibis::selectClause::variable(oss.str().c_str(), this);
        }
    }
    return 0;
} // ibis::selectClause::addAgregado

/// Number of terms without aggregation functions.  They are implicitly
/// used as sort keys for group by operations.  However, if the select
/// clause does not contain any aggregation function, the sorting operation
/// might be skipped.
uint32_t ibis::selectClause::numGroupbyKeys() const {
    uint32_t ret = (atms_.size() > aggr_.size() ?
                    atms_.size() - aggr_.size() : 0);
    for (uint32_t j = 0; j < aggr_.size(); ++j)
        ret += (aggr_[j] == NIL_AGGR);
    return ret;
} // ibis::selectClause::numGroupbyKeys

/// Does the data partition need additional processing to process the
/// select clause?  If any of the (lower-level) names is not present in the
/// incoming data partition, then it is we presume additional evaluation is
/// needed.  That is, this function will return true.  If all the names are
/// present in the data partition, then this function returns false.
bool ibis::selectClause::needsEval(const ibis::part &prt) const {
    bool need = false;
    for (uint32_t j = 0; need == false && j < atms_.size(); ++ j) {
        need = (0 == prt.getColumn(names_[j].c_str()));
    }
    return need;
} // ibis::selectClause::needsEval

/// Can the select clause be evaluated in separate parts?  Return true if
/// there is at least one aggregator and all aggregation operations are
/// separable operations.  Otherwise return false.
bool ibis::selectClause::isSeparable() const {
    unsigned nplains = 0;
    bool separable = true;
    for (unsigned j = 0; separable && j < aggr_.size(); ++ j) {
        nplains += (aggr_[j] == NIL_AGGR);
        separable = (aggr_[j] == NIL_AGGR ||
                     aggr_[j] == CNT || aggr_[j] == SUM ||
                     aggr_[j] == MAX || aggr_[j] == MIN);
    }
    if (separable)
        separable = (nplains < aggr_.size());
    return separable;
} // ibis::selectClause::isSeparable

/// Is the select caluse univariate?  If yes, return the pointer to the
/// string value, otherwise return a nil pointer.
const char* ibis::selectClause::isUnivariate() const {
    ibis::math::barrel bar;
    for (size_t j = 0; j < atms_.size(); ++ j)
        bar.recordVariable(atms_[j]);
    ibis::table::stringArray sl;
    for (size_t j = 0; j < bar.size(); ++ j) {
        const char* str = bar.name(j);
        if (*str != 0) {
            if (str[0] != '_' || str[1] != '_') {
                sl.push_back(str);
                if (sl.size() > 1)
                    return static_cast<const char*>(0);
            }
        }
    }
    if (sl.size() == 1) {
        return sl[0];
    }
    else {
        return static_cast<const char*>(0);
    }
} // ibis::selectClause::isUnivariate

/// Determine if the name refers to a term in the list of aggregation
/// functions.  A name to a aggregation function will be named by
/// ibis::selctClause::addAgregado.  If the return value is less than the
/// size of atms_, then the name is considered referring to a aggregation
/// function, otherwise, it is a literal name from the user.
uint64_t ibis::selectClause::decodeAName(const char* nm) const {
    uint64_t ret = std::numeric_limits<uint64_t>::max();
    if (nm == 0) return ret;
    if (nm[0] != '_' || nm[1] != '_') return ret;

    int ierr = ibis::util::decode16(ret, nm+2);
    if (ierr < 0)
        return atms_.size();
    return ret;
} // ibis::selectClause::decodeAName

/// Add a top-level term.  It invokes ibis::selectClause::addRecursive to
/// do the actual work.  The final expression returned by addRecursive is
/// added to  xtms_.
void ibis::selectClause::addTerm(ibis::math::term *tm, const std::string* al) {
    if (tm == 0) return;
    ibis::math::term *xtm = addRecursive(tm);
    if (xtm != 0) {
        if (al != 0 && !al->empty())
            xalias_[*al] = xtms_.size();
        xtms_.push_back(xtm);
    }
    else {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- selectClause::addTerm(" << *tm
            << ") encountered an ill-formed arithmetic expression";
        throw "selectClause encountered an ill-formed expression"
            IBIS_FILE_LINE;
    }
} // ibis::selectClause::addTerm

/// Does the math expression contain any aggregation operations?
bool ibis::selectClause::hasAggregation(const ibis::math::term *tm) const {
    switch (tm->termType()) {
    default:
    case ibis::math::NUMBER:
    case ibis::math::STRING:
        return false;
    case ibis::math::VARIABLE:
        return (dynamic_cast<const ibis::selectClause::variable *>(tm) != 0);
    case ibis::math::STDFUNCTION1:
    case ibis::math::CUSTOMFUNCTION1:
    case ibis::math::STRINGFUNCTION1:
        return hasAggregation(reinterpret_cast<const ibis::math::term*>
                              (tm->getLeft()));
    case ibis::math::OPERATOR:
    case ibis::math::STDFUNCTION2:
    case ibis::math::CUSTOMFUNCTION2:
    case ibis::math::STRINGFUNCTION2:
        bool res = tm->getLeft() != 0 ?
            hasAggregation(reinterpret_cast<const ibis::math::term*>
                           (tm->getLeft())) : false;
        if (tm->getRight() != 0 && ! res)
            res = hasAggregation(reinterpret_cast<const ibis::math::term*>
                                 (tm->getRight()));
        return res;
    }
} // ibis::selectClause::hasAggregation

ibis::math::term* ibis::selectClause::addRecursive(ibis::math::term*& tm) {
    if (tm == 0) return tm;

    switch (tm->termType()) {
    default:
    case ibis::math::NUMBER:
    case ibis::math::STRING:
        break; // nothing to do
    case ibis::math::VARIABLE: {
        ibis::selectClause::variable *var =
            dynamic_cast<ibis::selectClause::variable *>(tm);
        if (var == 0) { // a bare variable
            const char* vname =
                static_cast<ibis::math::variable*>(tm)->variableName();
            if (ordered_.find(vname) == ordered_.end()) {
                const unsigned pos = atms_.size();
                aggr_.push_back(ibis::selectClause::NIL_AGGR);
                atms_.push_back(tm->dup());
                ordered_[vname] = pos;

                LOGGER(ibis::gVerbose > 5)
                    << "selectClause::addRecursive -- adding term "
                    << pos << ": " << vname;
            }
        }
        break;}
    case ibis::math::STDFUNCTION1:
    case ibis::math::CUSTOMFUNCTION1:
    case ibis::math::STRINGFUNCTION1: {
        ibis::math::term *nxt =
            reinterpret_cast<ibis::math::term*>(tm->getLeft());
        if (nxt == 0) {
            return nxt;
        }
        else if (hasAggregation(nxt)) {
            ibis::math::term *tmp = addRecursive(nxt);
            if (tmp != nxt)
                tm->getLeft() = tmp;
        }
        else {
            const unsigned pos = atms_.size();
            aggr_.push_back(ibis::selectClause::NIL_AGGR);
            atms_.push_back(tm);
            LOGGER(ibis::gVerbose > 5)
                << "selectClause::addRecursive -- adding term "
                << pos << ": " << aggDescription(pos);

            std::ostringstream oss;
            oss << "__" << std::hex << pos;
            ordered_[oss.str()] = pos;
            return new ibis::selectClause::variable(oss.str().c_str(), this);
        }
        break;}
    case ibis::math::OPERATOR:
    case ibis::math::STDFUNCTION2:
    case ibis::math::CUSTOMFUNCTION2:
    case ibis::math::STRINGFUNCTION2: {
        ibis::math::term *left =
            reinterpret_cast<ibis::math::term*>(tm->getLeft());
        ibis::math::term *right =
            reinterpret_cast<ibis::math::term*>(tm->getRight());
        if (left == 0) {
            if (right == 0) {
                return 0;
            }
            else if (dynamic_cast<ibis::selectClause::variable*>(right) == 0) {
                tm->getRight() = addRecursive(right);
            }
        }
        else if (dynamic_cast<ibis::selectClause::variable*>(left) != 0) {
            if (dynamic_cast<ibis::selectClause::variable*>(right) == 0) {
                tm->getRight() = addRecursive(right);
            }
        }
        else if (dynamic_cast<ibis::selectClause::variable*>(right) != 0) {
            tm->getLeft() = addRecursive(left);
        }
        else if (hasAggregation(tm)) {
            tm->getLeft() = addRecursive(left);
            tm->getRight() = addRecursive(right);
        }
        else {
            const unsigned pos = atms_.size();
            aggr_.push_back(ibis::selectClause::NIL_AGGR);
            atms_.push_back(tm);
            LOGGER(ibis::gVerbose > 5)
                << "selectClause::addRecursive -- adding term "
                << pos << ": " << aggDescription(pos);

            std::ostringstream oss;
            oss << "__" << std::hex << pos;
            ordered_[oss.str()] = pos;
            return new ibis::selectClause::variable(oss.str().c_str(), this);
        }
        break;}
    }
    return tm;
} // ibis::selectClause::addRecursive

/// Produce a string for the jth term of the select clause.  The string
/// shows the actual expression, not the alias.  To see the final name to
/// be used, call ibis::selectClause::termName(j).
std::string ibis::selectClause::termDescription(unsigned j) const {
    if (j < xtms_.size()) {
        std::ostringstream oss;
        oss << *(xtms_[j]);
        return oss.str();
    }
    else {
        return std::string();
    }
} // ibis::selectClause::termDescription

/// Gather the implicit group-by keys into a vector.
/// @note Uses std::vector<std::string> because the string values may not
/// existing inside the select clause, such as the string representation
/// for arithmetic experssions.
int ibis::selectClause::getGroupbyKeys(std::vector<std::string>& keys) const {
    keys.clear();
    for (unsigned j = 0; j < atms_.size(); ++ j) {
        if (j >= aggr_.size() || aggr_[j] == ibis::selectClause::NIL_AGGR) {
            std::ostringstream oss;
            oss << *(atms_[j]);
            keys.push_back(oss.str());
        }
    }
    return keys.size();
} // ibis::selectClause::getGroupbyKeys

/// Locate the position of the string.  Upon successful completion, it
/// returns the position of the term with the matching name, otherwise, it
/// returns -1.  The incoming argument may be an alias, a column name, or
/// the exact form of the arithmetic expression.  In case it is an
/// arithmetic expression, it must be exactly the same as the original term
/// passed to the constructor of this class including spaces.  The
/// comparison is done with case-insensitive string comparison.
int ibis::selectClause::find(const char* key) const {
    int ret = -1;
    if (key != 0 && *key != 0) {
        StringToInt::const_iterator it = xalias_.find(key);
        if (it != xalias_.end()) {
            ret = it->second;
        }
        else {
            // try to match names of the terms one at a time
            for (ret = 0; ret < static_cast<int>(names_.size()); ++ ret) {
                if (stricmp(xnames_[ret].c_str(), key) == 0)
                    break;
            }
            // try to match the string version of each arithmetic expression
            if (ret >= static_cast<int>(names_.size())) {
                for (unsigned int i = 0; i < atms_.size(); ++ i) {
                    std::ostringstream oss;
                    switch (aggr_[i]) {
                    default:
                        oss << *(atms_[i]);
                        break;
                    case AVG:
                        oss << "AVG(" << *(atms_[i]) << ')';
                        break;
                    case CNT:
                        oss << "COUNT(" << *(atms_[i]) << ')';
                        break;
                    case MAX:
                        oss << "MAX(" << *(atms_[i]) << ')';
                        break;
                    case MIN:
                        oss << "MIN(" << *(atms_[i]) << ')';
                        break;
                    case SUM:
                        oss << "SUM(" << *(atms_[i]) << ')';
                        break;
                    case VARPOP:
                        oss << "VARPOP(" << *(atms_[i]) << ')';
                        break;
                    case VARSAMP:
                        oss << "VARSAMP(" << *(atms_[i]) << ')';
                        break;
                    case STDPOP:
                        oss << "STDPOP(" << *(atms_[i]) << ')';
                        break;
                    case STDSAMP:
                        oss << "STDSAMP(" << *(atms_[i]) << ')';
                        break;
                    case CONCAT:
                        oss << "GROUP_CONCAT(" << *(atms_[i]) << ')';
                        break;
                    case DISTINCT:
                        oss << "COUNTDISTINCT(" << *(atms_[i]) << ')';
                        break;
                    }
                    if (stricmp(oss.str().c_str(), key) == 0) {
                        ret = i;
                        break;
                    }
                }
            }
            if (ret >= static_cast<int>(names_.size()))
                ret = -1;
        }
    }
    return ret;
} // ibis::selectClause::find

/// Write a string version of the select clause to the specified output stream.
void ibis::selectClause::print(std::ostream& out) const {
    if (!clause_.empty()) {
        out << clause_;
    }
    else {
        std::vector<const std::string*> aliases(xtms_.size(), 0);
        for (StringToInt::const_iterator it = xalias_.begin();
             it != xalias_.end(); ++ it) {
            aliases[(*it).second] = &(it->first);
        }

        for (uint32_t i = 0; i < xtms_.size(); ++ i) {
            if (i > 0)
                out << ", ";
            out << *(xtms_[i]);
            if (aliases[i] != 0)
                out << " AS " << *(aliases[i]);
        }
    }
} // ibis::selectClause::print

void ibis::selectClause::printDetails(std::ostream& out) const {
    out << "select clause internal details:\n low-level expressions (names_["
        << names_.size() << "], aggr_[" << aggr_.size() << "], atms_["
        << atms_.size() << "]):";
    for (size_t j = 0; j < atms_.size(); ++ j) {
        out << "\n  " << j << ":\t" << names_[j] << ",\t";
        switch (aggr_[j]) {
        default:
            out << *(atms_[j]);
            break;
        case AVG:
            out << "AVG(" << *(atms_[j]) << ')';
            break;
        case CNT:
            out << "COUNT(" << *(atms_[j]) << ')';
            break;
        case MAX:
            out << "MAX(" << *(atms_[j]) << ')';
            break;
        case MIN:
            out << "MIN(" << *(atms_[j]) << ')';
            break;
        case SUM:
            out << "SUM(" << *(atms_[j]) << ')';
            break;
        case CONCAT:
            out << "GROUP_CONCAT(" << *(atms_[j]) << ')';
            break;
        case DISTINCT:
            out << "COUNTDISTINCT(" << *(atms_[j]) << ')';
            break;
        case VARPOP:
            out << "VARPOP(" << *(atms_[j]) << ')';
            break;
        case VARSAMP:
            out << "VAR(" << *(atms_[j]) << ')';
            break;
        case STDPOP:
            out << "STDPOP(" << *(atms_[j]) << ')';
            break;
        case STDSAMP:
            out << "STD(" << *(atms_[j]) << ')';
            break;
        case MEDIAN:
            out << "MEDIAN(" << *(atms_[j]) << ')';
            break;
        }
    }
    out << "\n high-level expressions (xnames_[" << xnames_.size()
        << "], xtms_[" << xtms_.size() << "]):";
    for (size_t j = 0; j < xtms_.size(); ++ j)
        out << "\n  " << j << ":\t" << xnames_[j] << ",\t" << *(xtms_[j]);
} // ibis::selectClause::printDetails

void ibis::selectClause::getNullMask(const ibis::part& part0,
                                     ibis::bitvector& mask) const {
    if (atms_.size() > 0) {
        ibis::part::barrel bar(&part0);
        for (uint32_t j = 0; j < atms_.size(); ++ j)
            bar.recordVariable(atms_[j]);
        if (bar.size() > 0) {
            bar.getNullMask(mask);
        }
        else {
            part0.getNullMask(mask);
        }
    }
    else {
        part0.getNullMask(mask);
    }
} // ibis::selectClause::getNullMask

/// Verify the select clause is valid against the given data partition.
/// Returns the number of variables that are not in the data partition.
/// This function also simplifies the arithmetic expression if
/// ibis::math::preserveInputExpression is not set.
///
/// @note Simplifying the arithmetic expressions typically reduces the time
/// needed for evaluations, but may introduce a different set of round-off
/// erros in the evaluation process than the original expression.  Set the
/// variable ibis::math::preserveInputExpression to true to avoid this
/// change in error round-off property.
int ibis::selectClause::verify(const ibis::part& part0) const {
    int ierr = 0;
    for (uint32_t j = 0; j < atms_.size(); ++ j) {
        if (ibis::math::preserveInputExpressions == false) {
            ibis::math::term *tmp = atms_[j]->reduce();
            if (tmp != atms_[j]) {
                delete const_cast<ibis::math::term*>(atms_[j]);
                const_cast<mathTerms&>(atms_)[j] = tmp;
            }
        }
        ierr += verifyTerm(*(atms_[j]), part0, this);
    }

    if (ibis::gVerbose > 6) {
        ibis::util::logger lg;
        lg() << "selectClause -- after simplification, ";
        printDetails(lg());
    }
    return ierr;
} // ibis::selectClause::verify

/// Verify the selected terms.  Return the number of terms containing
/// unknown names.
int ibis::selectClause::verifySome(const std::vector<uint32_t>& touse,
                                   const ibis::part& part0) const {
    int ierr = 0;
    for (uint32_t j = 0; j < touse.size(); ++ j) {
        if (ibis::math::preserveInputExpressions == false) {
            ibis::math::term *tmp = atms_[touse[j]]->reduce();
            if (tmp != atms_[touse[j]]) {
                delete const_cast<ibis::math::term*>(atms_[touse[j]]);
                const_cast<mathTerms&>(atms_)[touse[j]] = tmp;
            }
        }
        ierr += verifyTerm(*(atms_[touse[j]]), part0, this);
    }

    if (ibis::gVerbose > 6) {
        ibis::util::logger lg;
        lg() << "selectClause -- after simplification, ";
        printDetails(lg());
    }
    return ierr;
} // ibis::selectClause::verifySome

/// Verify the specified term has valid column names.  It returns the
/// number of terms not in the given data partition.
int ibis::selectClause::verifyTerm(const ibis::math::term& xp0,
                                   const ibis::part& part0,
                                   const ibis::selectClause* sel0) {
    int ierr = 0;

    if (xp0.termType() == ibis::math::VARIABLE) {
        const ibis::math::variable& var =
            static_cast<const ibis::math::variable&>(xp0);
        if (*(var.variableName()) != '*') {
            if (part0.getColumn(var.variableName()) == 0) {
                bool alias = false;
                const char *vnm = strchr(var.variableName(), '_');
                if (vnm != 0) {
                    ++ vnm;
                    if (part0.getColumn(vnm) != 0)
                        return ierr;
                }
                if (sel0 != 0) {
                    int as = sel0->find(var.variableName());
                    if (as >= 0 && (unsigned)as < sel0->aggSize())
                        alias = (part0.getColumn(sel0->aggName(as)) != 0);
                }
                if (! alias) {
                    ++ ierr;
                    LOGGER(ibis::gVerbose > 2)
                        << "Warning -- selectClause::verifyTerm can NOT find "
                        "a column named " << var.variableName()
                        << " in data partition " << part0.name();
                }
            }
        }
    }
    else if (xp0.termType() == ibis::math::UNDEF_TERM) {
        ++ ierr;
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- selectClause::verifyTerm can not work with a "
            "math::term of undefined type";
    }
    else {
        if (xp0.getLeft() != 0)
            ierr += verifyTerm(*static_cast<const ibis::math::term*>
                               (xp0.getLeft()), part0, sel0);
        if (xp0.getRight() != 0)
            ierr += verifyTerm(*static_cast<const ibis::math::term*>
                               (xp0.getRight()), part0, sel0);
    }

    return ierr;
} // ibis::selectClause::verifyTerm

void ibis::selectClause::gatherVariables(ibis::selectClause::varMap &vmap,
                                         ibis::math::term *t) const {
    if (t == 0) return;

    switch (t->termType()) {
    default: break; // do nothing
    case ibis::math::VARIABLE: {
        ibis::selectClause::variable *var =
            dynamic_cast<ibis::selectClause::variable*>(t);
        if (var != 0)
            vmap[var->variableName()] = var;
        break;}
    case ibis::math::OPERATOR:
    case ibis::math::STDFUNCTION1:
    case ibis::math::STDFUNCTION2:
    case ibis::math::CUSTOMFUNCTION1:
    case ibis::math::CUSTOMFUNCTION2:
    case ibis::math::STRINGFUNCTION1:
    case ibis::math::STRINGFUNCTION2: {
        if (t->getLeft() != 0)
            gatherVariables(vmap, static_cast<ibis::math::term*>
                            (t->getLeft()));
        if (t->getRight() != 0)
            gatherVariables(vmap, static_cast<ibis::math::term*>
                            (t->getRight()));
        break;}
    }
} // ibis::selectClause::gatherVariables

void ibis::selectClause::variable::print(std::ostream& out) const {
    const uint64_t itrm = sc_->decodeAName(name);
    if (itrm >= sc_->atms_.size()) {
        out << name;
        return;
    }
    if (itrm >= sc_->aggr_.size()) {
        // assume to be a bare arithmetic expression
        out << *(sc_->atms_[itrm]);
        return;
    }

    switch (sc_->aggr_[itrm]) {
    default:
    case ibis::selectClause::NIL_AGGR:
        out << *(sc_->atms_[itrm]);
        break;
    case ibis::selectClause::AVG:
        out << "AVG(" << *(sc_->atms_[itrm]) << ')';
        break;
    case ibis::selectClause::CNT:
        out << "COUNT(" << *(sc_->atms_[itrm]) << ')';
        break;
    case ibis::selectClause::MAX:
        out << "MAX(" << *(sc_->atms_[itrm]) << ')';
        break;
    case ibis::selectClause::MIN:
        out << "MIN(" << *(sc_->atms_[itrm]) << ')';
        break;
    case ibis::selectClause::SUM:
        out << "SUM(" << *(sc_->atms_[itrm]) << ')';
        break;
    case ibis::selectClause::VARPOP:
        out << "VARPOP(" << *(sc_->atms_[itrm]) << ')';
        break;
    case ibis::selectClause::VARSAMP:
        out << "VAR(" << *(sc_->atms_[itrm]) << ')';
        break;
    case ibis::selectClause::STDPOP:
        out << "STDPOP(" << *(sc_->atms_[itrm]) << ')';
        break;
    case ibis::selectClause::STDSAMP:
        out << "STD(" << *(sc_->atms_[itrm]) << ')';
        break;
    case ibis::selectClause::CONCAT:
        out << "GROUP_CONCAT(" << *(sc_->atms_[itrm]) << ')';
        break;
    case ibis::selectClause::DISTINCT:
        out << "COUNTDISTINCT(" << *(sc_->atms_[itrm]) << ')';
        break;
    case ibis::selectClause::MEDIAN:
        out << "MEDIAN(" << *(sc_->atms_[itrm]) << ')';
        break;
    }
} // ibis::selectClause::variable::print
