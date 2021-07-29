// File: $Id$
// Author: John Wu <John.Wu at ACM.org>
// Copyright (c) 2010-2016 the Regents of the University of California
#include "jrange.h"
#include "tab.h"        // ibis::tabula
#include "bord.h"       // ibis::bord, ibis::table::bufferArray
#include "category.h"   // ibis::category
#include "countQuery.h" // ibis::countQuery
#include "utilidor.h"   // ibis::util::sortMerge
#include "fromClause.h"
#include "selectClause.h"

#include <memory>       // std::unique_ptr
#include <stdexcept>    // std::exception
#include <typeinfo>     // std::typeid

/// Constructor.
ibis::jRange::jRange(const ibis::part& partr, const ibis::part& parts,
                     const ibis::column& colr, const ibis::column& cols,
                     double delta1, double delta2,
                     const ibis::qExpr* condr, const ibis::qExpr* conds,
                     const ibis::selectClause* sel, const ibis::fromClause* frm,
                     const char* desc)
    : sel_(sel ? new ibis::selectClause(*sel) : 0),
      frm_(frm ? new ibis::fromClause(*frm) : 0),
      partr_(partr), parts_(parts), colr_(colr), cols_(cols),
      delta1_(delta1), delta2_(delta2), orderr_(0), orders_(0),
      valr_(0), vals_(0), nrows(-1) {
    if (desc == 0 || *desc == 0) { // build a description string
        std::ostringstream oss;
        oss << "From " << partr.name() << " Join " << parts.name()
            << " On " << delta1 << " <= " << partr.name() << '.'
            << colr.name() << " - " << parts.name() << '.' << cols.name()
            << " <= " << delta2 << " Where ...";
        desc_ = oss.str();
    }
    else {
        desc_ = desc;
    }
    int ierr;
    if (condr != 0) {
        ibis::countQuery que(&partr);
        ierr = que.setWhereClause(condr);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jRange(" << desc_ << ") could apply "
                << condr << " on partition " << partr.name()
                << ", ierr = " << ierr;
            throw "jRange::ctor failed to apply conditions on partr"
                IBIS_FILE_LINE;
        }
        ierr = que.evaluate();
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jRange(" << desc_
                << ") could not evaluate " << que.getWhereClause()
                << " on partition " << partr.name() << ", ierr = " << ierr;
            throw "jRange::ctor failed to evaluate constraints on partr"
                IBIS_FILE_LINE;
        }
        maskr_.copy(*que.getHitVector());
    }
    else {
        colr.getNullMask(maskr_);
    }
    if (conds != 0) {
        ibis::countQuery que(&parts);
        ierr = que.setWhereClause(conds);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jRange(" << desc_ << ") could apply "
                << conds << " on partition " << parts.name()
                << ", ierr = " << ierr;
            throw "jRange::ctor failed to apply conditions on parts"
                IBIS_FILE_LINE;
        }
        ierr = que.evaluate();
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jRange(" << desc_
                << ") could not evaluate " << que.getWhereClause()
                << " on partition " << parts.name() << ", ierr = " << ierr;
            throw "jRange::ctor failed to evaluate constraints on parts"
                IBIS_FILE_LINE;
        }
        masks_.copy(*que.getHitVector());
    }
    else {
        cols.getNullMask(masks_);
    }
    LOGGER(ibis::gVerbose > 2)
        << "jRange(" << desc_ << ") construction complete";
} // ibis::jRange::jRange

/// Destructor.
ibis::jRange::~jRange() {
    delete orderr_;
    delete orders_;
    ibis::table::freeBuffer(valr_, colr_.type());
    ibis::table::freeBuffer(vals_, cols_.type());
    delete frm_;
    delete sel_;
    LOGGER(ibis::gVerbose > 4)
        << "jRange(" << desc_ << ") cleared";
}

/// Estimate the number of hits.  Nothing useful at this time.
void ibis::jRange::roughCount(uint64_t& nmin, uint64_t& nmax) const {
    nmin = 0;
    nmax = maskr_.cnt();
    nmax *= masks_.cnt();
} // ibis::jRange::roughCount

int64_t ibis::jRange::count() const {
    if (nrows >= 0) return nrows; // already have done this
    if (maskr_.cnt() == 0 || masks_.cnt() == 0) {
        return 0;
    }

    std::string mesg;
    mesg = "jRange::count(";
    mesg += desc_;
    mesg += ")";
    ibis::util::timer tm(mesg.c_str(), 1);
    // allocate space for ordering arrays
    orderr_ = new array_t<uint32_t>;
    orders_ = new array_t<uint32_t>;

    // Retrieve and sort the values
    switch (colr_.type()) {
    default:
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- jRange[" << desc_
            << "] cann't handle join column of type " << colr_.type();
        return -2;
    case ibis::BYTE: {
        valr_ = colr_.selectBytes(maskr_);
        if (valr_ == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jRange::count(" << desc_
                << ") call to " << colr_.name() << "->selectBytes("
                << maskr_.cnt() << ") failed";
            return -3;
        }
        vals_ = cols_.selectBytes(masks_);
        if (vals_ == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jRange::count(" << desc_
                << ") call to " << cols_.name() << "->selectBytes("
                << masks_.cnt() << ") failed";
            return -4;
        }
        nrows = ibis::util::sortMerge
            (*static_cast<array_t<signed char>*>(valr_), *orderr_,
             *static_cast<array_t<signed char>*>(vals_), *orders_,
             delta1_, delta2_);
        break;}
    case ibis::UBYTE: {
        valr_ = colr_.selectUBytes(maskr_);
        if (valr_ == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jRange::count(" << desc_
                << ") call to " << colr_.name() << "->selectUBytes("
                << maskr_.cnt() << ") failed";
            return -3;
        }
        vals_ = cols_.selectUBytes(masks_);
        if (vals_ == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jRange::count(" << desc_
                << ") call to " << cols_.name() << "->selectUBytes("
                << masks_.cnt() << ") failed";
            return -4;
        }
        nrows = ibis::util::sortMerge
            (*static_cast<array_t<unsigned char>*>(valr_),
             *orderr_,
             *static_cast<array_t<unsigned char>*>(vals_),
             *orders_, delta1_, delta2_);
        break;}
    case ibis::SHORT: {
        valr_ = colr_.selectShorts(maskr_);
        if (valr_ == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jRange::count(" << desc_
                << ") call to " << colr_.name() << "->selectShorts("
                << maskr_.cnt() << ") failed";
            return -3;
        }
        vals_ = cols_.selectShorts(masks_);
        if (vals_ == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jRange::count(" << desc_
                << ") call to " << cols_.name() << "->selectShorts("
                << masks_.cnt() << ") failed";
            return -4;
        }
        nrows = ibis::util::sortMerge
            (*static_cast<array_t<int16_t>*>(valr_), *orderr_,
             *static_cast<array_t<int16_t>*>(vals_), *orders_,
             delta1_, delta2_);
        break;}
    case ibis::USHORT: {
        valr_ = colr_.selectUShorts(maskr_);
        if (valr_ == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jRange::count(" << desc_
                << ") call to " << colr_.name() << "->selectUShorts("
                << maskr_.cnt() << ") failed";
            return -3;
        }
        vals_ = cols_.selectUShorts(masks_);
        if (vals_ == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jRange::count(" << desc_
                << ") call to " << cols_.name() << "->selectUShorts("
                << masks_.cnt() << ") failed";
            return -4;
        }
        nrows = ibis::util::sortMerge
            (*static_cast<array_t<uint16_t>*>(valr_), *orderr_,
             *static_cast<array_t<uint16_t>*>(vals_), *orders_,
             delta1_, delta2_);
        break;}
    case ibis::INT: {
        valr_ = colr_.selectInts(maskr_);
        if (valr_ == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jRange::count(" << desc_
                << ") call to " << colr_.name() << "->selectInts("
                << maskr_.cnt() << ") failed";
            return -3;
        }
        vals_ = cols_.selectInts(masks_);
        if (vals_ == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jRange::count(" << desc_
                << ") call to " << cols_.name() << "->selectInts("
                << masks_.cnt() << ") failed";
            return -4;
        }
        nrows = ibis::util::sortMerge
            (*static_cast<array_t<int32_t>*>(valr_), *orderr_,
             *static_cast<array_t<int32_t>*>(vals_), *orders_,
             delta1_, delta2_);
        break;}
    case ibis::UINT: {
        valr_ = colr_.selectUInts(maskr_);
        if (valr_ == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jRange::count(" << desc_
                << ") call to " << colr_.name() << "->selectUInts("
                << maskr_.cnt() << ") failed";
            return -3;
        }
        vals_ = cols_.selectUInts(masks_);
        if (vals_ == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jRange::count(" << desc_
                << ") call to " << cols_.name() << "->selectUInts("
                << masks_.cnt() << ") failed";
            return -4;
        }
        nrows = ibis::util::sortMerge
            (*static_cast<array_t<uint32_t>*>(valr_), *orderr_,
             *static_cast<array_t<uint32_t>*>(vals_), *orders_,
             delta1_, delta2_);
        break;}
    case ibis::LONG: {
        valr_ = colr_.selectLongs(maskr_);
        if (valr_ == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jRange::count(" << desc_
                << ") call to " << colr_.name() << "->selectLongs("
                << maskr_.cnt() << ") failed";
            return -3;
        }
        vals_ = cols_.selectLongs(masks_);
        if (vals_ == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jRange::count(" << desc_
                << ") call to " << cols_.name() << "->selectLongs("
                << masks_.cnt() << ") failed";
            return -4;
        }
        nrows = ibis::util::sortMerge
            (*static_cast<array_t<int64_t>*>(valr_), *orderr_,
             *static_cast<array_t<int64_t>*>(vals_), *orders_,
             delta1_, delta2_);
        break;}
    case ibis::ULONG: {
        valr_ = colr_.selectULongs(maskr_);
        if (valr_ == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jRange::count(" << desc_
                << ") call to " << colr_.name() << "->selectULongs("
                << maskr_.cnt() << ") failed";
            return -3;
        }
        vals_ = cols_.selectULongs(masks_);
        if (vals_ == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jRange::count(" << desc_
                << ") call to " << cols_.name() << "->selectULongs("
                << masks_.cnt() << ") failed";
            return -4;
        }
        nrows = ibis::util::sortMerge
            (*static_cast<array_t<uint64_t>*>(valr_), *orderr_,
             *static_cast<array_t<uint64_t>*>(vals_), *orders_,
             delta1_, delta2_);
        break;}
    case ibis::FLOAT: {
        valr_ = colr_.selectFloats(maskr_);
        if (valr_ == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jRange::count(" << desc_
                << ") call to " << colr_.name() << "->selectFloats("
                << maskr_.cnt() << ") failed";
            return -3;
        }
        vals_ = cols_.selectFloats(masks_);
        if (vals_ == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jRange::count(" << desc_
                << ") call to " << cols_.name() << "->selectFloats("
                << masks_.cnt() << ") failed";
            return -4;
        }
        nrows = ibis::util::sortMerge
            (*static_cast<array_t<float>*>(valr_), *orderr_,
             *static_cast<array_t<float>*>(vals_), *orders_, delta1_, delta2_);
        break;}
    case ibis::DOUBLE: {
        valr_ = colr_.selectDoubles(maskr_);
        if (valr_ == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jRange::count(" << desc_
                << ") call to " << colr_.name() << "->selectDoubles("
                << maskr_.cnt() << ") failed";
            return -3;
        }
        vals_ = cols_.selectDoubles(masks_);
        if (vals_ == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jRange::count(" << desc_
                << ") call to " << cols_.name() << "->selectDoubles("
                << masks_.cnt() << ") failed";
            return -4;
        }
        nrows = ibis::util::sortMerge
            (*static_cast<array_t<double>*>(valr_), *orderr_,
             *static_cast<array_t<double>*>(vals_), *orders_, delta1_, delta2_);
        break;}
    }
    LOGGER(ibis::gVerbose > 2)
        << "jRange::count(" << desc_ << ") found " << nrows
        << " hit" << (nrows>1?"s":"");
    return nrows;
} // ibis::jRange::count

ibis::table* ibis::jRange::select() const {
    if (nrows < 0) {
        int64_t ierr = count();
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- jRange::count failed with error code"
                << ierr;
            return 0;
        }
    }
    if (sel_ == 0 || sel_->empty()) { // default
        std::string tn = ibis::util::shortName(desc_.c_str());
        return new ibis::tabula(tn.c_str(), desc_.c_str(), nrows);
    }

    uint32_t features=0; // 1: arithmetic operation, 2: aggregation
    // use a barrel to collect all unique names
    ibis::math::barrel brl;
    for (uint32_t j = 0; j < sel_->aggSize(); ++ j) {
        const ibis::math::term* t = sel_->aggExpr(j);
        brl.recordVariable(t);
        if (t->termType() != ibis::math::VARIABLE &&
            t->termType() != ibis::math::NUMBER &&
            t->termType() != ibis::math::STRING) {
            features |= 1; // arithmetic operation
        }
        if (sel_->getAggregator(j) != ibis::selectClause::NIL_AGGR) {
            features |= 2; // aggregation
        }
    }
    // convert the barrel into a stringArray for processing
    ibis::table::stringArray sl;
    sl.reserve(brl.size());
    for (unsigned j = 0; j < brl.size(); ++ j) {
        const char *str = brl.name(j);
        if (*str != 0) {
            if (str[0] != '_' || str[1] != '_')
                sl.push_back(str);
        }
    }

    std::unique_ptr<ibis::table> res1(select(sl));
    if (res1.get() == 0 || res1->nRows() == 0 || res1->nColumns() == 0 ||
        features == 0)
        return res1.release();

    if (ibis::gVerbose > 2) {
        ibis::util::logger lg;
        lg() << "jRange::select(" << *sel_ << ", " << desc_
             << ") produced the first intermediate table:\n";
        res1->describe(lg());
    }

    if ((features & 1) != 0) { // arithmetic computations
        res1.reset(static_cast<const ibis::bord*>(res1.get())->evaluateTerms
                   (*sel_, desc_.c_str()));
        if (res1.get() != 0) {
            if (ibis::gVerbose > 2) {
                ibis::util::logger lg;
                lg() << "jRange::select(" << *sel_ << ", " << desc_
                     << ") produced the second intermediate table:\n";
                res1->describe(lg());
            }
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- jRange::select(" << *sel_
                << ") failed to evaluate the arithmetic expressions";
            return 0;
        }
    }

    if ((features & 2) != 0) { // aggregation operations
        res1.reset(static_cast<const ibis::bord*>(res1.get())->groupby(*sel_));
        if (res1.get() != 0) {
            if (ibis::gVerbose > 2) {
                ibis::util::logger lg;
                lg() << "jRange::select(" << *sel_ << ", " << desc_
                     << ") produced the third intermediate table:\n";
                res1->describe(lg());
            }
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- jRange::select(" << *sel_
                << ") failed to evaluate the aggregations";
        }
    }
    return res1.release();
} // ibis::jRange::select

ibis::table* ibis::jRange::select(const char *sstr) const {
    if (nrows < 0) {
        int64_t ierr = count();
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- jRange::count failed with error code"
                << ierr;
            return 0;
        }
    }
    if (sstr == 0 || *sstr == 0) { // default
        std::string tn = ibis::util::shortName(desc_.c_str());
        return new ibis::tabula(tn.c_str(), desc_.c_str(), nrows);
    }

    ibis::selectClause sel(sstr);
    uint32_t features=0; // 1: arithmetic operation, 2: aggregation
    // use a barrel to collect all unique names
    ibis::math::barrel brl;
    for (uint32_t j = 0; j < sel.aggSize(); ++ j) {
        const ibis::math::term* t = sel.aggExpr(j);
        brl.recordVariable(t);
        if (t->termType() != ibis::math::VARIABLE &&
            t->termType() != ibis::math::NUMBER &&
            t->termType() != ibis::math::STRING) {
            features |= 1; // arithmetic operation
        }
        if (sel.getAggregator(j) != ibis::selectClause::NIL_AGGR) {
            features |= 2; // aggregation
        }
    }
    // convert the barrel into a stringArray for processing
    ibis::table::stringArray sl;
    sl.reserve(brl.size());
    for (unsigned j = 0; j < brl.size(); ++ j) {
        const char* str = brl.name(j);
        if (*str != 0) {
            if (str[0] != '_' || str[1] != '_')
                sl.push_back(str);
        }
    }

    std::unique_ptr<ibis::table> res1(select(sl));
    if (res1.get() == 0 || res1->nRows() == 0 || res1->nColumns() == 0 ||
        features == 0)
        return res1.release();

    if (ibis::gVerbose > 2) {
        ibis::util::logger lg;
        lg() << "jRange::select(" << sstr << ", " << desc_
             << ") produced the first intermediate table:\n";
        res1->describe(lg());
    }

    if ((features & 1) != 0) { // arithmetic computations
        res1.reset(static_cast<const ibis::bord*>(res1.get())->evaluateTerms
                   (sel, desc_.c_str()));
        if (res1.get() != 0) {
            if (ibis::gVerbose > 2) {
                ibis::util::logger lg;
                lg() << "jRange::select(" << sel << ", " << desc_
                     << ") produced the second intermediate table:\n";
                res1->describe(lg());
            }
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- jRange::select(" << sel
                << ") failed to evaluate the arithmetic expressions";
            return 0;
        }
    }

    if ((features & 2) != 0) { // aggregation operations
        res1.reset(static_cast<const ibis::bord*>(res1.get())->groupby(sel));
        if (res1.get() != 0) {
            if (ibis::gVerbose > 2) {
                ibis::util::logger lg;
                lg() << "jRange::select(" << *sel_ << ", " << desc_
                     << ") produced the third intermediate table:\n";
                res1->describe(lg());
            }
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- jRange::select(" << *sel_
                << ") failed to evaluate the aggregations";
        }
    }
    return res1.release();
} // ibis::jRange::select

ibis::table*
ibis::jRange::select(const ibis::table::stringArray& colnames) const {
    ibis::table *res = 0;
    if (nrows < 0) {
        int64_t ierr = count();
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- jRange::count failed with error code"
                << ierr;
            return res;
        }
    }
    if (valr_ == 0 || orderr_ == 0 || vals_ == 0 || orders_ == 0 ||
        orderr_->size() != maskr_.cnt() || orders_->size() != masks_.cnt()) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- jRange::select failed to evaluate the join";
        return res;
    }
    if (colnames.empty() || nrows == 0) {
        std::string nm = ibis::util::shortName(desc_);
        res = new ibis::tabula(nm.c_str(), desc_.c_str(), nrows);
        return res;
    }

    const uint32_t ncols = colnames.size();
    std::string evt;
    evt = "select ";
    evt += colnames[0];
    for (uint32_t j = 1; j < ncols; ++ j) {
        evt += ", ";
        evt += colnames[j];
    }
    if ((desc_[0] != 'F' && desc_[0] != 'f') ||
        (desc_[1] != 'R' && desc_[1] != 'r') ||
        (desc_[2] != 'O' && desc_[2] != 'o') ||
        (desc_[3] != 'M' && desc_[3] != 'm'))
        evt += " for ";
    else
        evt += ' ';
    evt += desc_;
    ibis::util::timer mytimer(evt.c_str());
    std::map<const char*, uint32_t, ibis::lessi> namesToPos;
    std::vector<uint32_t> ipToPos(colnames.size());
    std::vector<const ibis::column*> ircol, iscol;
    std::vector<const ibis::dictionary*> cats(colnames.size(), 0);
    // identify the names from the two data partitions
    for (uint32_t j = 0; j < ncols; ++ j) {
        ipToPos[j] = ncols+1;
        const char* cn = colnames[j];
        std::string tname;
        while (*cn != 0 && *cn != '.') {
            tname += *cn;
            ++ cn;
        }
        if (*cn == '.') {
            ++ cn;
        }
        else { // did not find '.'
            tname.erase();
            cn = colnames[j];
        }
        int match = -1; // 0 ==> partr_, 1 ==> parts_
        if (! tname.empty()) {
            match = frm_->position(tname.c_str());
            if (match >= static_cast<long>(frm_->size())) {
                if (stricmp(tname.c_str(), partr_.name()) == 0) {
                    match = 0;
                }
                else if (stricmp(tname.c_str(), parts_.name()) == 0) {
                    match = 1;
                }
            }
        }

        if (match == 0) {
            const ibis::column *col = partr_.getColumn(cn);
            if (col != 0) {
                namesToPos[colnames[j]] = j;
                ipToPos[j] = ircol.size();
                ircol.push_back(col);
                if (col->type() == ibis::CATEGORY) {
                    const ibis::category *cat =
                        static_cast<const ibis::category*>(col);
                    cats[j] = cat->getDictionary();
                }
                else if (col->type() == ibis::UINT) {
                    const ibis::bord::column *bc =
                        dynamic_cast<const ibis::bord::column*>(col);
                    if (bc != 0) {
                        cats[j] = bc->getDictionary();
                    }
                }
            }
            else {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt << " can not find column named \""
                    << colnames[j] << "\" in data partition \"" << partr_.name()
                    << "\"";
                return res;
            }
        }
        else if (match == 1) {
            const ibis::column *col = parts_.getColumn(cn);
            if (col != 0) {
                namesToPos[colnames[j]] = j;
                ipToPos[j] = ncols - iscol.size();
                iscol.push_back(col);
                if (col->type() == ibis::CATEGORY) {
                    const ibis::category *cat =
                        static_cast<const ibis::category*>(col);
                    cats[j] = cat->getDictionary();
                }
                else if (col->type() == ibis::UINT) {
                    const ibis::bord::column *bc =
                        dynamic_cast<const ibis::bord::column*>(col);
                    if (bc != 0) {
                        cats[j] = bc->getDictionary();
                    }
                }
            }
            else {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt << " can not find column named \""
                    << colnames[j] << "\" in data partition \""
                    << parts_.name() << "\"";
                return res;
            }
        }
        else { // not prefixed with a data partition name
            cn = colnames[j];
            const ibis::column* col = partr_.getColumn(cn);
            if (col != 0) {
                ipToPos[j] = ircol.size();
                ircol.push_back(col);
                if (col->type() == ibis::CATEGORY) {
                    const ibis::category *cat =
                        static_cast<const ibis::category*>(col);
                    cats[j] = cat->getDictionary();
                }
                else if (col->type() == ibis::UINT) {
                    const ibis::bord::column *bc =
                        dynamic_cast<const ibis::bord::column*>(col);
                    if (bc != 0) {
                        cats[j] = bc->getDictionary();
                    }
                }
                LOGGER(ibis::gVerbose > 3)
                    << evt << " encountered a column name ("
                    << colnames[j] << ") that does not start with a data "
                    "partition name, assume it is for \"" << partr_.name()
                    << "\"";
            }
            else {
                col = parts_.getColumn(cn);
                if (col != 0) {
                    ipToPos[j] = ncols - iscol.size();
                    iscol.push_back(col);
                    if (col->type() == ibis::CATEGORY) {
                        const ibis::category *cat =
                            static_cast<const ibis::category*>(col);
                        cats[j] = cat->getDictionary();
                    }
                    else if (col->type() == ibis::UINT) {
                        const ibis::bord::column *bc =
                            dynamic_cast<const ibis::bord::column*>(col);
                        if (bc != 0) {
                            cats[j] = bc->getDictionary();
                        }
                    }
                    LOGGER(ibis::gVerbose > 1)
                        << evt << " encountered a column name (" << colnames[j]
                        << ") that does not start with a data partition name, "
                        "assume it is for \"" << parts_.name() << "\"";
                }
                else {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- " << evt << " encountered a name ("
                        << colnames[j] << ") that does not start with a data "
                        "partition name";
                    return res;
                }
            }
        }
    } // for (uint32_t j = 0; j < ncols;

    LOGGER(ibis::gVerbose > 3)
        << evt << " -- found " << ircol.size()
        << " column" << (ircol.size() > 1 ? "s" : "") << " from "
        << partr_.name() << " and " << iscol.size() << " column"
        << (iscol.size() > 1 ? "s" : "") << " from " << parts_.name();

    // change Pos values for columns in S to have offset ircol.size()
    for (uint32_t j = 0; j < ncols; ++j) {
        if (ipToPos[j] <= ncols && ipToPos[j] >= ircol.size())
            ipToPos[j] = (ncols - ipToPos[j]) + ircol.size();
    }
    ibis::table::typeArray   rtypes(ircol.size(), ibis::UNKNOWN_TYPE);
    ibis::table::bufferArray rbuff(ircol.size(), 0);
    IBIS_BLOCK_GUARD(ibis::table::freeBuffers, ibis::util::ref(rbuff),
                     ibis::util::ref(rtypes));
    ibis::table::typeArray   stypes(iscol.size(), ibis::UNKNOWN_TYPE);
    ibis::table::bufferArray sbuff(iscol.size(), 0);
    IBIS_BLOCK_GUARD(ibis::table::freeBuffers, ibis::util::ref(sbuff),
                     ibis::util::ref(stypes));
    bool sane = true;

    // retrieve values from r_
    for (uint32_t j = 0; sane && j < ircol.size(); ++ j) {
        rtypes[j] = ircol[j]->type();
        switch (ircol[j]->type()) {
        case ibis::BYTE:
            rbuff[j] = ircol[j]->selectBytes(maskr_);
            if (rbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<array_t<signed char>*>(rbuff[j]),
                     *orderr_);
            else
                sane = false;
            break;
        case ibis::UBYTE:
            rbuff[j] = ircol[j]->selectUBytes(maskr_);
            if (rbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<array_t<unsigned char>*>(rbuff[j]),
                     *orderr_);
            else
                sane = false;
            break;
        case ibis::SHORT:
            rbuff[j] = ircol[j]->selectShorts(maskr_);
            if (rbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<array_t<int16_t>*>(rbuff[j]),
                     *orderr_);
            else
                sane = false;
            break;
        case ibis::USHORT:
            rbuff[j] = ircol[j]->selectUShorts(maskr_);
            if (rbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<array_t<uint16_t>*>(rbuff[j]),
                     *orderr_);
            else
                sane = false;
            break;
        case ibis::INT:
            rbuff[j] = ircol[j]->selectInts(maskr_);
            if (rbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<array_t<int32_t>*>(rbuff[j]),
                     *orderr_);
            else
                sane = false;
            break;
        case ibis::UINT:
            rbuff[j] = ircol[j]->selectUInts(maskr_);
            if (rbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<array_t<uint32_t>*>(rbuff[j]),
                     *orderr_);
            else
                sane = false;
            break;
        case ibis::LONG:
            rbuff[j] = ircol[j]->selectLongs(maskr_);
            if (rbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<array_t<int64_t>*>(rbuff[j]),
                     *orderr_);
            else
                sane = false;
            break;
        case ibis::ULONG:
            rbuff[j] = ircol[j]->selectULongs(maskr_);
            if (rbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<array_t<uint64_t>*>(rbuff[j]),
                     *orderr_);
            else
                sane = false;
            break;
        case ibis::FLOAT:
            rbuff[j] = ircol[j]->selectFloats(maskr_);
            if (rbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<array_t<float>*>(rbuff[j]),
                     *orderr_);
            else
                sane = false;
            break;
        case ibis::DOUBLE:
            rbuff[j] = ircol[j]->selectDoubles(maskr_);
            if (rbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<array_t<double>*>(rbuff[j]),
                     *orderr_);
            else
                sane = false;
            break;
        case ibis::TEXT:
        case ibis::CATEGORY:
            rbuff[j] = ircol[j]->selectStrings(maskr_);
            if (rbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<std::vector<std::string>*>(rbuff[j]),
                     *orderr_);
            else
                sane = false;
            break;
        default:
            sane = false;
            rbuff[j] = 0;
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jRange::select does not support column "
                "type " << ibis::TYPESTRING[(int)ircol[j]->type()]
                << " (name = " << partr_.name() << "." << ircol[j]->name()
                << ")";
            break;
        }
    }
    if (! sane) {
        return res;
    }

    // retrieve values from parts_
    for (uint32_t j = 0; sane && j < iscol.size(); ++ j) {
        stypes[j] = iscol[j]->type();
        switch (iscol[j]->type()) {
        case ibis::BYTE:
            sbuff[j] = iscol[j]->selectBytes(masks_);
            if (sbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<array_t<signed char>*>(sbuff[j]),
                     *orders_);
            else
                sane = false;
            break;
        case ibis::UBYTE:
            sbuff[j] = iscol[j]->selectUBytes(masks_);
            if (sbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<array_t<unsigned char>*>(sbuff[j]),
                     *orders_);
            else
                sane = false;
            break;
        case ibis::SHORT:
            sbuff[j] = iscol[j]->selectShorts(masks_);
            if (sbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<array_t<int16_t>*>(sbuff[j]),
                     *orders_);
            else
                sane = false;
            break;
        case ibis::USHORT:
            sbuff[j] = iscol[j]->selectUShorts(masks_);
            if (sbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<array_t<uint16_t>*>(sbuff[j]),
                     *orders_);
            else
                sane = false;
            break;
        case ibis::INT:
            sbuff[j] = iscol[j]->selectInts(masks_);
            if (sbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<array_t<int32_t>*>(sbuff[j]),
                     *orders_);
            else
                sane = false;
            break;
        case ibis::UINT:
            sbuff[j] = iscol[j]->selectUInts(masks_);
            if (sbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<array_t<uint32_t>*>(sbuff[j]),
                     *orders_);
            else
                sane = false;
            break;
        case ibis::LONG:
            sbuff[j] = iscol[j]->selectLongs(masks_);
            if (sbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<array_t<int64_t>*>(sbuff[j]),
                     *orders_);
            else
                sane = false;
            break;
        case ibis::ULONG:
            sbuff[j] = iscol[j]->selectULongs(masks_);
            if (sbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<array_t<uint64_t>*>(sbuff[j]),
                     *orders_);
            else
                sane = false;
            break;
        case ibis::FLOAT:
            sbuff[j] = iscol[j]->selectFloats(masks_);
            if (sbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<array_t<float>*>(sbuff[j]),
                     *orders_);
            else
                sane = false;
            break;
        case ibis::DOUBLE:
            sbuff[j] = iscol[j]->selectDoubles(masks_);
            if (sbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<array_t<double>*>(sbuff[j]),
                     *orders_);
            else
                sane = false;
            break;
        case ibis::TEXT:
        case ibis::CATEGORY:
            sbuff[j] = iscol[j]->selectStrings(masks_);
            if (sbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<std::vector<std::string>*>(sbuff[j]),
                     *orders_);
            else
                sane = false;
            break;
        default:
            sane = false;
            sbuff[j] = 0;
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jRange::select does not support column "
                "type " << ibis::TYPESTRING[(int)iscol[j]->type()]
                << " (name = " << parts_.name() << "." << iscol[j]->name()
                << ")";
            break;
        }
    }
    if (! sane) {
        return res;
    }

    /// fill the in-memory buffer
    switch (colr_.type()) {
    case ibis::BYTE:
        res = fillResult
            (nrows, delta1_, delta2_, evt,
             *static_cast<array_t<signed char>*>(valr_), rtypes, rbuff,
             *static_cast<array_t<signed char>*>(vals_), stypes, sbuff,
             colnames, ipToPos);
        break;
    case ibis::UBYTE:
        res = fillResult
            (nrows, delta1_, delta2_, evt,
             *static_cast<array_t<unsigned char>*>(valr_), rtypes, rbuff,
             *static_cast<array_t<unsigned char>*>(vals_), stypes, sbuff,
             colnames, ipToPos);
        break;
    case ibis::SHORT:
        res = fillResult
            (nrows, delta1_, delta2_, evt,
             *static_cast<array_t<int16_t>*>(valr_), rtypes, rbuff,
             *static_cast<array_t<int16_t>*>(vals_), stypes, sbuff,
             colnames, ipToPos);
        break;
    case ibis::USHORT:
        res = fillResult
            (nrows, delta1_, delta2_, evt,
             *static_cast<array_t<uint16_t>*>(valr_), rtypes, rbuff,
             *static_cast<array_t<uint16_t>*>(vals_), stypes, sbuff,
             colnames, ipToPos);
        break;
    case ibis::INT:
        res = fillResult
            (nrows, delta1_, delta2_, evt,
             *static_cast<array_t<int32_t>*>(valr_), rtypes, rbuff,
             *static_cast<array_t<int32_t>*>(vals_), stypes, sbuff,
             colnames, ipToPos);
        break;
    case ibis::UINT:
        res = fillResult
            (nrows, delta1_, delta2_, evt,
             *static_cast<array_t<uint32_t>*>(valr_), rtypes, rbuff,
             *static_cast<array_t<uint32_t>*>(vals_), stypes, sbuff,
             colnames, ipToPos);
        break;
    case ibis::LONG:
        res = fillResult
            (nrows, delta1_, delta2_, evt,
             *static_cast<array_t<int64_t>*>(valr_), rtypes, rbuff,
             *static_cast<array_t<int64_t>*>(vals_), stypes, sbuff,
             colnames, ipToPos);
        break;
    case ibis::ULONG:
        res = fillResult
            (nrows, delta1_, delta2_, evt,
             *static_cast<array_t<uint64_t>*>(valr_), rtypes, rbuff,
             *static_cast<array_t<uint64_t>*>(vals_), stypes, sbuff,
             colnames, ipToPos);
        break;
    case ibis::FLOAT:
        res = fillResult
            (nrows, delta1_, delta2_, evt,
             *static_cast<array_t<float>*>(valr_), rtypes, rbuff,
             *static_cast<array_t<float>*>(vals_), stypes, sbuff,
             colnames, ipToPos);
        break;
    case ibis::DOUBLE:
        res = fillResult
            (nrows, delta1_, delta2_, evt,
             *static_cast<array_t<double>*>(valr_), rtypes, rbuff,
             *static_cast<array_t<double>*>(vals_), stypes, sbuff,
             colnames, ipToPos);
        break;
    default:
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " can not handle join column of type "
            << ibis::TYPESTRING[(int)colr_.type()];
    }

    for (unsigned j = 0; j < cats.size(); ++ j) {
        if (cats[j] != 0) {
            ibis::bord::column *bc = dynamic_cast<ibis::bord::column*>
                (static_cast<ibis::bord*>(res)->getColumn(j));
            if (bc != 0)
                bc->setDictionary(cats[j]);
        }
    }
    return res;
} // ibis::jRange::select

/// Generate a table representing a range-join in memory.  The input to
/// this function are values to go into the resulting table; it only needs
/// to match the rows and fill the output table.
///
/// @note This implementation is for elementary numberical data types only.
template <typename T>
ibis::table*
ibis::jRange::fillResult(size_t nrows, double delta1, double delta2,
                         const std::string &desc,
                         const ibis::array_t<T>& rjcol,
                         const ibis::table::typeArray& rtypes,
                         const ibis::table::bufferArray& rbuff,
                         const ibis::array_t<T>& sjcol,
                         const ibis::table::typeArray& stypes,
                         const ibis::table::bufferArray& sbuff,
                         const ibis::table::stringArray& tcname,
                         const std::vector<uint32_t>& tcnpos) {
    if (nrows > (rjcol.size() * sjcol.size()) ||
        rtypes.size() != rbuff.size() || stypes.size() != sbuff.size() ||
        tcname.size() != rtypes.size() + stypes.size() ||
        tcnpos.size() != tcname.size()) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- jRange::fillResult can not proceed due "
            "to invalid arguments";
        return 0;
    }
    std::string tn = ibis::util::shortName(desc.c_str());
    if (nrows == 0 || rjcol.empty() || sjcol.empty() ||
        (stypes.empty() && rtypes.empty()))
        return new ibis::tabula(tn.c_str(), desc.c_str(), nrows);

    ibis::table::bufferArray tbuff(tcname.size());
    ibis::table::typeArray   ttypes(tcname.size());
    IBIS_BLOCK_GUARD(ibis::table::freeBuffers, ibis::util::ref(tbuff),
                     ibis::util::ref(ttypes));
    try {
        // allocate enough space for the output table
        for (size_t j = 0; j < tcname.size(); ++ j) {
            if (tcnpos[j] < rtypes.size()) {
                ttypes[j] = rtypes[tcnpos[j]];
                tbuff[j] = ibis::table::allocateBuffer
                    (rtypes[tcnpos[j]], nrows);
            }
            else if (tcnpos[j] < rtypes.size()+stypes.size()) {
                ttypes[j] = stypes[tcnpos[j]-rtypes.size()];
                tbuff[j] = ibis::table::allocateBuffer
                    (stypes[tcnpos[j]-rtypes.size()], nrows);
            }
            else { // tcnpos is out of valid range
                ttypes[j] = ibis::UNKNOWN_TYPE;
                tbuff[j] = 0;
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- jRange::fillResult detects an "
                    "invalid tcnpos[" << j << "] = " << tcnpos[j]
                    << ", should be less than " << rtypes.size()+stypes.size();
                return 0;
            }
        }
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- jRange::fillResult failed to allocate "
            "sufficient memory for " << nrows << " row" << (nrows>1?"s":"")
            << " and " << rtypes.size()+stypes.size()
            << " column" << (rtypes.size()+stypes.size()>1?"s":"");
        return 0;
    }

    size_t tind = 0; // row index into the resulting table
    uint32_t ir0 = 0;
    uint32_t ir1 = 0;
    uint32_t is = 0;
    const uint32_t nr = rjcol.size();
    const uint32_t ns = sjcol.size();
    while (ir0 < nr && is < ns) {
        while (ir0 < nr && rjcol[ir0] < sjcol[is]+delta1)
            ++ ir0;
        ir1 = (ir1>=ir0?ir1:ir0);
        while (ir1 < nr && rjcol[ir1] <= sjcol[is]+delta2)
            ++ ir1;
        if (ir1 > ir0) { // found matches
            size_t is0 = is;
            while (is < ns && sjcol[is] == sjcol[is0])
                ++ is;
            LOGGER(ibis::gVerbose > 5)
                << "DEBUG -- jRange::fillResult: ir0=" << ir0 << ", ir1="
                << ir1 << ", is0=" << is0 << ", is1=" << is << ", rjcol["
                << ir0 << "]=" << rjcol[ir0] << ", rjcol[" << ir1 << "]="
                << rjcol[ir1] << ", sjcol[" << is0 << "]=" << sjcol[is0]
                << ", sjcol[" << is << "]=" << sjcol[is];
            for (size_t jr = ir0; jr < ir1; ++ jr) {
                for (size_t js = is0; js < is; ++ js) {
                    for (size_t jt = 0; jt < tcnpos.size(); ++ jt) {
                        if (tcnpos[jt] < rbuff.size()) {
                            ibis::bord::copyValue(rtypes[tcnpos[jt]],
                                                  tbuff[jt], tind,
                                                  rbuff[tcnpos[jt]], jr);
                        }
                        else {
                            ibis::bord::copyValue
                                (stypes[tcnpos[jt]-rtypes.size()],
                                 tbuff[jt], tind,
                                 sbuff[tcnpos[jt]-rtypes.size()], js);
                        }
                    } // jt
                    ++ tind;
                } // js
            } // jr
        }
        else {
            ++ is;
        }
    } // while ...
    if (tind != nrows) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- jRange::fillResult expected to produce "
            << nrows << " row" << (nrows>1?"s":"") << ", but produced "
            << tind << " instead";
        return 0;
    }

    LOGGER(ibis::gVerbose > 3)
        << "jRange(" << desc << ")::fillResult produced " << tind
        << " row" << (tind>1?"s":"") << " for \"" << typeid(T).name()
        << '[' << rjcol.size() << "] - " << typeid(T).name()
        << '[' << sjcol.size() << "] between " << delta1 << " and " << delta2
        << '\"';
    return new ibis::bord(tn.c_str(), desc.c_str(), nrows,
                          tbuff, ttypes, tcname);
} // ibis::jRange::fillResult
