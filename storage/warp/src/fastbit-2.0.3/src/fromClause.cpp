// $Id$
// Author: John Wu <John.Wu at acm.org>
//      Lawrence Berkeley National Laboratory
// Copyright (c) 2009-2016 the Regents of the University of California
#include "part.h"
#include "qExpr.h"
#include "fromLexer.h"
#include "fromClause.h"

ibis::fromClause::fromClause(const char *cl) : jcond_(0), lexer(0) {
    if (cl == 0 || *cl == 0) return;
    LOGGER(ibis::gVerbose > 5)
        << "fromClause::ctor creating a new from clause with \"" << cl
        << "\"";
    parse(cl);
} // ibis::fromClause::fromClause

ibis::fromClause::fromClause(const ibis::table::stringArray &sl)
    : jcond_(0), lexer(0) {
    for (size_t j = 0; j < sl.size(); ++ j) {
        if (sl[j] != 0 && *(sl[j]) != 0) {
            if (! clause_.empty())
                clause_ += ", ";
            clause_ += sl[j];
        }
    }
    if (clause_.empty()) return;
    LOGGER(ibis::gVerbose > 5)
        << "fromClause::ctor creating a new from clause with \"" << clause_
        << "\"";
    parse(clause_.c_str());
} // ibis::fromClause::fromClause

/// Copy constructor.  Deep copy.
ibis::fromClause::fromClause(const ibis::fromClause& rhs)
    : names_(rhs.names_), aliases_(rhs.aliases_),
      jcond_(0), clause_(rhs.clause_), lexer(0) {
    if (rhs.jcond_ != 0)
        jcond_ = static_cast<ibis::compRange*>(rhs.jcond_->dup());
    for (size_t j = 0; j < names_.size(); ++ j) {
        if (! names_[j].empty() &&
            ordered_.find(names_[j].c_str()) == ordered_.end())
            ordered_[names_[j].c_str()] = j;
        if (! aliases_[j].empty() &&
            ordered_.find(aliases_[j].c_str()) == ordered_.end())
            ordered_[aliases_[j].c_str()] = j;
    }
    std::map<const char*, size_t, ibis::lessi>::const_iterator it0;
    std::map<const char*, size_t, ibis::lessi>::iterator it1;
    for (it0 = rhs.ordered_.begin(), it1 = ordered_.begin();
         it0 != rhs.ordered_.end() && it1 != ordered_.end();
         ++ it0, ++ it1)
        it1->second = it0->second;
} // ibis::fromClause::fromClause

ibis::fromClause::~fromClause() {
    clear();
}

/// Remove the current content.
void ibis::fromClause::clear() {
    names_.clear();
    ordered_.clear();
    aliases_.clear();
    delete jcond_;
    jcond_ = 0;
    clause_.clear();
} // ibis::fromClause::clear

/// Parse a new string.  Clear the existing content.  A minimal amount of
/// sanity check is also performed.
int ibis::fromClause::parse(const char *cl) {
    int ierr = 0;
    if (cl != 0 && *cl != 0) {
        if (cl != clause_.c_str()) {
            clear();
            clause_ = cl;
        }
        std::istringstream iss(clause_);
        ibis::util::logger lg;
        fromLexer lx(&iss, &(lg()));
        fromParser parser(*this);
        lexer = &lx;
#if DEBUG+0 > 2
        parser.set_debug_level(DEBUG-1);
#elif _DEBUG+0 > 2
        parser.set_debug_level(_DEBUG-1);
#endif
        parser.set_debug_stream(lg());
        ierr = parser.parse();
        lexer = 0;
    }
    if (ierr == 0) {
        // if (jcond_ != 0)
        //     ibis::qExpr::simplify(jcond_);
        if (jcond_ != 0 &&
            (names_.size() != 2 || aliases_.size() != 2)) {
            ierr = -300;
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- fromClause expects no more than two table "
                "names, but got " << names_.size() << " table name"
                << (names_.size()>1?"s":"") << " and " << aliases_.size()
                << " alias" << (aliases_.size()>1?"es":"");
        }
    }
    else {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- fromClause::parse failed to parse string \""
            << clause_ << "\"";
    }
    if (ierr < 0) clear();
    for (size_t j = 0; j < names_.size(); ++ j) {
        if (! names_[j].empty() &&
            ordered_.find(names_[j].c_str()) == ordered_.end())
            ordered_[names_[j].c_str()] = j;
        if (! aliases_[j].empty()) {
            if (ordered_.find(aliases_[j].c_str()) == ordered_.end()) {
                ordered_[aliases_[j].c_str()] = j;
            }
            else {
                LOGGER(ibis::gVerbose >= 0)
                    << "Warning -- fromClause::parse(" << cl
                    << ") detected duplicate alias " << aliases_[j]
                    << ", only the first one will be in effective";
            }
        }
    }
    return ierr;
} // ibis::fromClause::parse

/// Fill the array nms with the known names.  It returns both actual table
/// names and their aliases in alphabetic order.
void ibis::fromClause::getNames(ibis::table::stringArray& nms) const {
    nms.clear();
    nms.reserve(ordered_.size());
    for (std::map<const char*, size_t, ibis::lessi>::const_iterator it
             = ordered_.begin();
         it != ordered_.end(); ++ it)
        nms.push_back(it->first);
} // ibis::fromClause::getNames

/// Print the content.  Write a string version of the from clause to the
/// specified output stream.
void ibis::fromClause::print(std::ostream& out) const {
    if (jcond_ == 0) { // no join condition, simply print the table names
        for (size_t j = 0; j < names_.size(); ++j) {
            if (j > 0)
                out << ", ";
            out << names_[j];
            if (! aliases_[j].empty())
                out << " as " << aliases_[j];
        }
    }
    else if (jcond_->getTerm3() != 0 && jcond_->getLeft() == 0 &&
             jcond_->getRight() == 0) { // join ... using(term 3)
        out << names_[0];
        if (! aliases_[0].empty())
            out << " as " << aliases_[0];
        out << " join " << names_[1];
        if (! aliases_[1].empty())
            out << " as " << aliases_[1];
        out << " using " << *(jcond_->getTerm3());
    }
    else if (jcond_->getLeft() == 0 &&
             jcond_->getRight() == 0) { // join (no explicit join column)
        out << names_[0];
        if (! aliases_[0].empty())
            out << " as " << aliases_[0];
        out << " join " << names_[1];
        if (! aliases_[1].empty())
            out << " as " << aliases_[1];       
    }
    else { // join ... on 
        out << names_[0];
        if (! aliases_[0].empty())
            out << " as " << aliases_[0];
        out << " join " << names_[1];
        if (! aliases_[1].empty())
            out << " as " << aliases_[1];
        out << " on " << *jcond_;
    }
} // ibis::fromClause::print

/// Given an alias find its real name.  The input string will be returned
/// if it is neither an alias nor an actual table name mentioned in the
/// from clause.
const char* ibis::fromClause::realName(const char* al) const {
    if (al == 0 || *al == 0) return 0;
    if (ordered_.empty()) return al;

    std::map<const char*, size_t, ibis::lessi>::const_iterator it
        = ordered_.find(al);
    if (it != ordered_.end()) {
        if (it->second < names_.size()) {
            return names_[it->second].c_str();
        }
        else {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- fromClause::realName(" << al << ") encountered "
                "an internal error, the name points to element "
                << it->second << ", but there only " << names_.size()
                << " name" << (names_.size() > 1 ? "s" : "");
            return al;
        }
    }
    else {
        LOGGER(ibis::gVerbose > 5)
            << "Warning -- fromClause::realName(" << al
            << ") finds no other name for " << al;
        return al;
    }
} // ibis::fromClause::realName

/// Given a name find its alias.  The incoming argument will be returned if
/// it is neither an alias nor an actual table name mentioned in the from
/// clause.
const char* ibis::fromClause::alias(const char* al) const {
    if (al == 0 || *al == 0) return 0;
    if (ordered_.empty()) return al;

    std::map<const char*, size_t, ibis::lessi>::const_iterator it
        = ordered_.find(al);
    if (it != ordered_.end()) {
        if (it->second < aliases_.size()) {
            return aliases_[it->second].c_str();
        }
        else if (it->second < names_.size()) {
            return names_[it->second].c_str();
        }
        else {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- fromClause::alias(" << al << ") encountered "
                "an internal error, the name points to element "
                << it->second << ", but there only " << aliases_.size()
                << " alias" << (names_.size() > 1 ? "es" : "");
            return al;
        }
    }
    else {
        LOGGER(ibis::gVerbose > 5)
            << "Warning -- fromClause::alias(" << al
            << ") finds no alias for " << al;
        return al;
    }
} // ibis::fromClause::alias

size_t ibis::fromClause::position(const char* al) const {
    if (al == 0 || *al == 0)
        return names_.size();
    std::map<const char*, size_t, ibis::lessi>::const_iterator it
        = ordered_.find(al);
    if (it != ordered_.end()) {
        return it->second;
    }
    else {
        return names_.size();
    }
} // ibis::fromClause::position

/// Reorder the table names.  The name matching nm0 will be placed first,
/// followed by the one matching nm1.
void ibis::fromClause::reorderNames(const char* nm0, const char* nm1) {
    if (nm0 == 0 || nm1 == 0 || *nm0 == 0 || *nm1 == 0) return;

    if (names_.empty()) { // insert new names as table names
        aliases_.resize(2);
        names_.resize(2);
        names_[0] = nm0;
        names_[1] = nm1;
        ordered_.clear();
        ordered_[names_[0].c_str()] = 0;
        ordered_[names_[1].c_str()] = 1;
    }
    else if (names_.size() == 1) {
        if (stricmp(nm0, aliases_[0].c_str()) == 0 &&
            stricmp(nm1, names_[0].c_str()) == 0) {
            aliases_.resize(2);
            names_.resize(2);
            names_[1] = nm1;
            ordered_.clear(); // because we have changed names_ and aliases_
            ordered_[aliases_[0].c_str()] = 0;
            ordered_[names_[1].c_str()] = 1;
        }
        else if (stricmp(nm1, aliases_[0].c_str()) == 0 &&
                 stricmp(nm0, names_[0].c_str()) == 0) {
            aliases_.resize(2);
            names_.resize(2);
            names_[1] = names_[0];
            aliases_[0].swap(aliases_[1]);
            ordered_.clear();
            ordered_[names_[0].c_str()] = 0;
            ordered_[aliases_[1].c_str()] = 1;
        }
        else {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- fromClause::reorderNames(" << nm0 << ", "
                << nm1 << ") expects the two input arguments to be "
                << aliases_[0] << " and " << names_[0];
        }
    }
    else if (names_.size() == 2) {
        if ((stricmp(nm0, names_[1].c_str()) == 0 ||
             stricmp(nm0, aliases_[1].c_str()) == 0) &&
            (stricmp(nm1, names_[0].c_str()) == 0 ||
             stricmp(nm1, aliases_[0].c_str()) == 0)) {
            aliases_[0].swap(aliases_[1]);
            names_[0].swap(names_[1]);

            for (std::map<const char*, size_t, ibis::lessi>::iterator it
                     = ordered_.begin();
                 it != ordered_.end();
                 ++ it) {
                it->second = (it->second == 0);
            }
        }
    }
} // ibis::fromClause::reorderNames
