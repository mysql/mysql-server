// File: $Id$
// Author: John Wu <John.Wu at ACM.org>
// Copyright (c) 2008-2016 the Regents of the University of California
#include "jnatural.h"   // ibis::jNatural
#include "tab.h"        // ibis::tabula
#include "bord.h"       // ibis::bord, ibis::table::bufferArray
#include "category.h"   // ibis::category
#include "countQuery.h" // ibis::countQuery
#include "utilidor.h"   // ibis::util::sortMerge
#include "fromClause.h"
#include "selectClause.h"

#include <memory>       // std::unique_ptr
#include <stdexcept>    // std::exception

/// Constructor.  This constructor handles a join expression equivalent to
/// one of the following SQL statements
///
///@code
/// From partr Join parts On colr = cols where condr and conds;
/// From partr, parts where partr.colr = parts.cols and condr and conds;
///@endcode
///
/// Note that this function processes the selection conditions on partr and
/// parts immediately and therefore does not actually remember the
/// conditions condr and conds.  To preserve those conditions, it is
/// recommended to keep the original query string as the description desc.
ibis::jNatural::jNatural(const ibis::part* partr, const ibis::part* parts,
                         const ibis::column* colr, const ibis::column* cols,
                         const ibis::qExpr* condr, const ibis::qExpr* conds,
                         const ibis::selectClause* sel,
                         const ibis::fromClause* frm,
                         const char* desc)
    : sel_(sel ? new ibis::selectClause(*sel) : 0),
      frm_(frm ? new ibis::fromClause(*frm) : 0), R_(*partr), S_(*parts),
      colR_(*colr), colS_(*cols), orderR_(0), orderS_(0),
      valR_(0), valS_(0), nrows(-1) {
    int ierr;
    if (desc == 0 || *desc == 0) { // build a description string
        desc_ = "From ";
        desc_ += partr->name();
        desc_ += " Join ";
        desc_ += parts->name();
        desc_ += " On ";
        desc_ += partr->name();
        desc_ += '.';
        desc_ += colr->name();
        desc_ += " = ";
        desc_ += parts->name();
        desc_ += '.';
        desc_ += cols->name();
        desc_ += " Where ...";
    }
    else {
        desc_ = desc;
    }
    if (condr != 0) {
        ibis::countQuery que(partr);
        ierr = que.setWhereClause(condr);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jNatural(" << desc_
                << ") could not assign " << *condr << " on partition "
                << R_.name() << ", ierr = " << ierr;
            throw std::invalid_argument("jNatural::ctor failed to parse "
                                        "constraints on R_" IBIS_FILE_LINE);
        }
        ierr = que.evaluate();
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jNatural(" << desc_
                << ") could not evaluate " << que.getWhereClause()
                << " on partition " << R_.name() << ", ierr = " << ierr;
            throw std::invalid_argument("jNatural::ctor failed to evaluate "
                                        "constraints on R_" IBIS_FILE_LINE);
        }
        maskR_.copy(*que.getHitVector());
    }
    else {
        colR_.getNullMask(maskR_);
    }
    if (conds != 0) {
        ibis::countQuery que(parts);
        ierr = que.setWhereClause(conds);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jNatural(" << desc_
                << ") could not parse " << conds << " on partition "
                << S_.name() << ", ierr = " << ierr;
            throw std::invalid_argument("jNatural::ctor failed to parse "
                                        "constraints on S_" IBIS_FILE_LINE);
        }
        ierr = que.evaluate();
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jNatural(" << desc_
                << ") could not evaluate " << que.getWhereClause()
                << " on partition " << S_.name() << ", ierr = " << ierr;
            throw std::invalid_argument("jNatural::ctor failed to evaluate "
                                        "constraints on S_" IBIS_FILE_LINE);
        }
        maskS_.copy(*que.getHitVector());
    }
    else {
        colS_.getNullMask(maskS_);
    }
} // constructor

/// Constructor.  This constructor handles a join equivalent to the
/// following SQL statement
/// @code
/// From partr Join parts Using(colname) Where condr And conds
/// @endcode
///
/// Note that conditions specified in condr is for partr only, and conds is
/// for parts only.  When the column names in these conditions contain
/// table names, the table names in them are ignored.  If no conditions are
/// specified, all valid records in the partition will participate in the
/// natural join.  This constructor avoids the need of specifying an alias
/// when performing self-join, however, it also makes it impossible to
/// distingush the column names in the select clause.
ibis::jNatural::jNatural(const ibis::part* partr, const ibis::part* parts,
                         const char* colname, const char* condr,
                         const char* conds, const char* sel)
    : sel_(new ibis::selectClause(sel)), frm_(0), R_(*partr), S_(*parts),
      colR_(*(partr->getColumn(colname))), colS_(*(parts->getColumn(colname))),
      orderR_(0), orderS_(0), valR_(0), valS_(0), nrows(-1) {
    if (colname == 0 || *colname == 0) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- jNatural must have a valid string for colname";
        throw "jNatural::ctor must have a valid colname as join columns"
            IBIS_FILE_LINE;
    }

    if (colR_.type() != colS_.type()) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- jNatural detects the join columns with "
            << "mismatching types: " << R_.name() << "." << colname
            << " (" << ibis::TYPESTRING[colR_.type()] << "), "
            << S_.name()   << "." << colname << " ("
            << ibis::TYPESTRING[colS_.type()] << ")";

        throw std::invalid_argument("jNatural join columns missing "
                                    "or having different types" IBIS_FILE_LINE);
    }
    desc_ = "From ";
    desc_ += R_.name();
    desc_ += " Join ";
    desc_ += S_.name();
    desc_ += " Using(";
    desc_ += colname;
    desc_ += ")";
    if ((condr != 0 && *condr != 0) || (condr != 0 && *condr != 0)) {
        desc_ += " Where ...";
    }

    int ierr;
    if (condr != 0 && *condr != 0) {
        ibis::countQuery que(partr);
        ierr = que.setWhereClause(condr);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jNatural(" << desc_
                << ") could not parse " << condr << " on partition "
                << R_.name() << ", ierr = " << ierr;
            throw "jNatural::ctor failed to parse constraints on R_"
                IBIS_FILE_LINE;
        }
        ierr = que.evaluate();
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jNatural(" << desc_
                << ") could not evaluate " << que.getWhereClause()
                << " on partition " << R_.name() << ", ierr = " << ierr;
            throw "jNatural::ctor failed to evaluate constraints on R_"
                IBIS_FILE_LINE;
        }
        maskR_.copy(*que.getHitVector());
    }
    else {
        colR_.getNullMask(maskR_);
    }
    if (conds != 0 && *conds != 0) {
        ibis::countQuery que(parts);
        ierr = que.setWhereClause(conds);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jNatural(" << desc_
                << ") could not parse " << conds << " on partition "
                << S_.name() << ", ierr = " << ierr;
            throw "jNatural::ctor failed to parse constraints on S_"
                IBIS_FILE_LINE;
        }
        ierr = que.evaluate();
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jNatural(" << desc_
                << ") could not evaluate " << que.getWhereClause()
                << " on partition " << S_.name() << ", ierr = " << ierr;
            throw "jNatural::ctor failed to evaluate constraints on S_"
                IBIS_FILE_LINE;
        }
        maskS_.copy(*que.getHitVector());
    }
    else {
        colS_.getNullMask(maskS_);
    }

    LOGGER(ibis::gVerbose > 2)
        << "jNatural(" << desc_ << ") construction complete";
} // ibis::jNatural::jNatural

ibis::jNatural::~jNatural() {
    delete orderR_;
    delete orderS_;
    ibis::table::freeBuffer(valR_, colR_.type());
    ibis::table::freeBuffer(valS_, colS_.type());
    delete frm_;
    delete sel_;
    LOGGER(ibis::gVerbose > 4)
        << "jNatural(" << desc_ << ") cleared";
} // ibis::jNatural::~jNatural

/// Estimate the number of hits.  Don't do much right now, may change later.
void ibis::jNatural::roughCount(uint64_t& nmin, uint64_t& nmax) const {
    nmin = 0;
    nmax = (uint64_t)maskR_.cnt() * maskS_.cnt();
} // ibis::jNatural::roughCount

/// Use sort-merge join.  This function sorts the qualified values and
/// counts the number of results.
int64_t ibis::jNatural::count() const {
    if (nrows >= 0) return nrows; // already have done this
    if (maskR_.cnt() == 0 || maskS_.cnt() == 0) {
        return 0;
    }

    std::string mesg;
    mesg = "jNatural::count(";
    mesg += desc_;
    mesg += ")";
    ibis::util::timer tm(mesg.c_str(), 1);
    // allocate space for ordering arrays
    orderR_ = new array_t<uint32_t>;
    orderS_ = new array_t<uint32_t>;

    // Retrieve and sort the values
    switch (colR_.type()) {
    default:
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- jNatural[" << desc_
            << "] cann't handle join column of type " << colR_.type();
        return -2;
    case ibis::BYTE: {
        valR_ = colR_.selectBytes(maskR_);
        if (valR_ == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jNatural::count(" << desc_
                << ") call to " << colR_.name() << "->selectBytes("
                << maskR_.cnt() << ") failed";
            return -3;
        }
        valS_ = colS_.selectBytes(maskS_);
        if (valS_ == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jNatural::count(" << desc_
                << ") call to " << colS_.name() << "->selectBytes("
                << maskS_.cnt() << ") failed";
            return -4;
        }
        nrows = ibis::util::sortMerge
            (*static_cast<array_t<signed char>*>(valR_), *orderR_,
             *static_cast<array_t<signed char>*>(valS_), *orderS_);
        break;}
    case ibis::UBYTE: {
        valR_ = colR_.selectUBytes(maskR_);
        if (valR_ == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jNatural::count(" << desc_
                << ") call to " << colR_.name() << "->selectUBytes("
                << maskR_.cnt() << ") failed";
            return -3;
        }
        valS_ = colS_.selectUBytes(maskS_);
        if (valS_ == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jNatural::count(" << desc_
                << ") call to " << colS_.name() << "->selectUBytes("
                << maskS_.cnt() << ") failed";
            return -4;
        }
        nrows = ibis::util::sortMerge
            (*static_cast<array_t<unsigned char>*>(valR_),
             *orderR_,
             *static_cast<array_t<unsigned char>*>(valS_),
             *orderS_);
        break;}
    case ibis::SHORT: {
        valR_ = colR_.selectShorts(maskR_);
        if (valR_ == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jNatural::count(" << desc_
                << ") call to " << colR_.name() << "->selectShorts("
                << maskR_.cnt() << ") failed";
            return -3;
        }
        valS_ = colS_.selectShorts(maskS_);
        if (valS_ == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jNatural::count(" << desc_
                << ") call to " << colS_.name() << "->selectShorts("
                << maskS_.cnt() << ") failed";
            return -4;
        }
        nrows = ibis::util::sortMerge
            (*static_cast<array_t<int16_t>*>(valR_), *orderR_,
             *static_cast<array_t<int16_t>*>(valS_), *orderS_);
        break;}
    case ibis::USHORT: {
        valR_ = colR_.selectUShorts(maskR_);
        if (valR_ == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jNatural::count(" << desc_
                << ") call to " << colR_.name() << "->selectUShorts("
                << maskR_.cnt() << ") failed";
            return -3;
        }
        valS_ = colS_.selectUShorts(maskS_);
        if (valS_ == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jNatural::count(" << desc_
                << ") call to " << colS_.name() << "->selectUShorts("
                << maskS_.cnt() << ") failed";
            return -4;
        }
        nrows = ibis::util::sortMerge
            (*static_cast<array_t<uint16_t>*>(valR_), *orderR_,
             *static_cast<array_t<uint16_t>*>(valS_), *orderS_);
        break;}
    case ibis::INT: {
        valR_ = colR_.selectInts(maskR_);
        if (valR_ == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jNatural::count(" << desc_
                << ") call to " << colR_.name() << "->selectInts("
                << maskR_.cnt() << ") failed";
            return -3;
        }
        valS_ = colS_.selectInts(maskS_);
        if (valS_ == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jNatural::count(" << desc_
                << ") call to " << colS_.name() << "->selectInts("
                << maskS_.cnt() << ") failed";
            return -4;
        }
        nrows = ibis::util::sortMerge
            (*static_cast<array_t<int32_t>*>(valR_), *orderR_,
             *static_cast<array_t<int32_t>*>(valS_), *orderS_);
        break;}
    case ibis::UINT: {
        valR_ = colR_.selectUInts(maskR_);
        if (valR_ == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jNatural::count(" << desc_
                << ") call to " << colR_.name() << "->selectUInts("
                << maskR_.cnt() << ") failed";
            return -3;
        }
        valS_ = colS_.selectUInts(maskS_);
        if (valS_ == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jNatural::count(" << desc_
                << ") call to " << colS_.name() << "->selectUInts("
                << maskS_.cnt() << ") failed";
            return -4;
        }
        nrows = ibis::util::sortMerge
            (*static_cast<array_t<uint32_t>*>(valR_), *orderR_,
             *static_cast<array_t<uint32_t>*>(valS_), *orderS_);
        break;}
    case ibis::LONG: {
        valR_ = colR_.selectLongs(maskR_);
        if (valR_ == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jNatural::count(" << desc_
                << ") call to " << colR_.name() << "->selectLongs("
                << maskR_.cnt() << ") failed";
            return -3;
        }
        valS_ = colS_.selectLongs(maskS_);
        if (valS_ == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jNatural::count(" << desc_
                << ") call to " << colS_.name() << "->selectLongs("
                << maskS_.cnt() << ") failed";
            return -4;
        }
        nrows = ibis::util::sortMerge
            (*static_cast<array_t<int64_t>*>(valR_), *orderR_,
             *static_cast<array_t<int64_t>*>(valS_), *orderS_);
        break;}
    case ibis::ULONG: {
        valR_ = colR_.selectULongs(maskR_);
        if (valR_ == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jNatural::count(" << desc_
                << ") call to " << colR_.name() << "->selectULongs("
                << maskR_.cnt() << ") failed";
            return -3;
        }
        valS_ = colS_.selectULongs(maskS_);
        if (valS_ == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jNatural::count(" << desc_
                << ") call to " << colS_.name() << "->selectULongs("
                << maskS_.cnt() << ") failed";
            return -4;
        }
        nrows = ibis::util::sortMerge
            (*static_cast<array_t<uint64_t>*>(valR_), *orderR_,
             *static_cast<array_t<uint64_t>*>(valS_), *orderS_);
        break;}
    case ibis::FLOAT: {
        valR_ = colR_.selectFloats(maskR_);
        if (valR_ == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jNatural::count(" << desc_
                << ") call to " << colR_.name() << "->selectFloats("
                << maskR_.cnt() << ") failed";
            return -3;
        }
        valS_ = colS_.selectFloats(maskS_);
        if (valS_ == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jNatural::count(" << desc_
                << ") call to " << colS_.name() << "->selectFloats("
                << maskS_.cnt() << ") failed";
            return -4;
        }
        nrows = ibis::util::sortMerge
            (*static_cast<array_t<float>*>(valR_), *orderR_,
             *static_cast<array_t<float>*>(valS_), *orderS_);
        break;}
    case ibis::DOUBLE: {
        valR_ = colR_.selectDoubles(maskR_);
        if (valR_ == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jNatural::count(" << desc_
                << ") call to " << colR_.name() << "->selectDoubles("
                << maskR_.cnt() << ") failed";
            return -3;
        }
        valS_ = colS_.selectDoubles(maskS_);
        if (valS_ == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jNatural::count(" << desc_
                << ") call to " << colS_.name() << "->selectDoubles("
                << maskS_.cnt() << ") failed";
            return -4;
        }
        nrows = ibis::util::sortMerge
            (*static_cast<array_t<double>*>(valR_), *orderR_,
             *static_cast<array_t<double>*>(valS_), *orderS_);
        break;}
    case ibis::TEXT:
    case ibis::CATEGORY: {
        valR_ = colR_.selectStrings(maskR_);
        if (valR_ == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jNatural::count(" << desc_
                << ") call to " << colR_.name() << "->selectStrings("
                << maskR_.cnt() << ") failed";
            return -3;
        }
        valS_ = colS_.selectStrings(maskS_);
        if (valS_ == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jNatural::count(" << desc_
                << ") call to " << colS_.name() << "->selectStrings("
                << maskS_.cnt() << ") failed";
            return -4;
        }
        nrows = ibis::util::sortMerge
            (*static_cast<std::vector<std::string>* >(valR_), *orderR_,
             *static_cast<std::vector<std::string>* >(valS_), *orderS_);
        break;}
    }
    LOGGER(ibis::gVerbose > 2)
        << "jNatural::count(" << desc_ << ") found " << nrows
        << " hit" << (nrows>1?"s":"");
    return nrows;
} // ibis::jNatural::count

/// Generate a table representing an equi-join in memory.  The input to
/// this function are values to go into the resulting table.  It only needs
/// to match the rows and fill the output table.
template <typename T>
ibis::table*
ibis::jNatural::fillResult(size_t nrows,
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
            << "Warning -- jNatural::fillResult can not proceed due "
            "to invalid arguments";
        return 0;
    }
    std::string tn = ibis::util::shortName(desc.c_str());
    if (nrows == 0 || rjcol.empty() || sjcol.empty() ||
        (stypes.empty() && rtypes.empty()))
        return new ibis::tabula(tn.c_str(), desc.c_str(), nrows);

    ibis::table::bufferArray tbuff(tcname.size());
    ibis::table::typeArray   ttypes(tcname.size());
    IBIS_BLOCK_GUARD(ibis::table::freeBuffers,
                     ibis::util::ref(tbuff),
                     ibis::util::ref(ttypes));
    try {
        bool badpos = false;
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
                badpos = true;
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- jNatural::fillResult detects an "
                    "invalid tcnpos[" << j << "] = " << tcnpos[j]
                    << ", should be less than " << rtypes.size()+stypes.size();
            }
        }
        if (badpos) {
            return 0;
        }
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- jNatural::fillResult failed to allocate "
            "sufficient memory for " << nrows << " row" << (nrows>1?"s":"")
            << " and " << rtypes.size()+stypes.size()
            << " column" << (rtypes.size()+stypes.size()>1?"s":"");
        return 0;
    }

    size_t tind = 0; // row index into the resulting table
    for (size_t rind = 0, sind = 0;
         rind < rjcol.size() && sind < sjcol.size(); ) {
        while (rind < rjcol.size() && rjcol[rind] < sjcol[sind]) ++ rind;
        while (sind < sjcol.size() && sjcol[sind] < rjcol[rind]) ++ sind;
        if (rind < rjcol.size() && sind < sjcol.size() &&
            rjcol[rind] == sjcol[sind]) { // found matches
            size_t rind0 = rind;
            size_t rind1 = rind+1;
            while (rind1 < rjcol.size() && rjcol[rind1] == sjcol[sind])
                ++ rind1;
            size_t sind0 = sind;
            size_t sind1 = sind+1;
            while (sind1 < sjcol.size() && sjcol[sind1] == rjcol[rind])
                ++ sind1;
            for (rind = rind0; rind < rind1; ++ rind) {
                for (sind = sind0; sind < sind1; ++ sind) {
                    for (size_t j = 0; j < tcnpos.size(); ++ j) {
                        if (tcnpos[j] < rbuff.size()) {
                            ibis::bord::copyValue(rtypes[tcnpos[j]],
                                                  tbuff[j], tind,
                                                  rbuff[tcnpos[j]], rind);
                        }
                        else {
                            ibis::bord::copyValue
                                (stypes[tcnpos[j]-rtypes.size()],
                                 tbuff[j], tind,
                                 sbuff[tcnpos[j]-rtypes.size()], sind);
                        }
                    } // j
                    ++ tind;
                } // sind
            } // rind
        }
    } // for (size_t rind = 0, sind = 0; ...
    if (tind != nrows) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- jNatural::fillResult expected to produce "
            << nrows << " row" << (nrows>1?"s":"") << ", but produced "
            << tind << " instead";
        return 0;
    }

    std::unique_ptr<ibis::bord> res
        (new ibis::bord(tn.c_str(), desc.c_str(), nrows,
                        tbuff, ttypes, tcname));
    return res.release();
} // ibis::jNatural::fillResult

/// Form the joined table for string valued join columns.  The caller
/// provides all relevant values, this function only needs to join them to
/// produce the output data table.
ibis::table*
ibis::jNatural::fillResult(size_t nrows,
                           const std::string &desc,
                           const std::vector<std::string>& rjcol,
                           const ibis::table::typeArray& rtypes,
                           const ibis::table::bufferArray& rbuff,
                           const std::vector<std::string>& sjcol,
                           const ibis::table::typeArray& stypes,
                           const ibis::table::bufferArray& sbuff,
                           const ibis::table::stringArray& tcname,
                           const std::vector<uint32_t>& tcnpos) {
    if (rjcol.empty() || sjcol.empty() ||
        (nrows > rjcol.size() * sjcol.size()) ||
        (nrows < rjcol.size() && nrows < sjcol.size()) ||
        ((rtypes.empty() || rbuff.empty() || rtypes.size() != rbuff.size()) &&
         (stypes.empty() || sbuff.empty() || stypes.size() != sbuff.size())) ||
        tcname.size() != rtypes.size() + stypes.size() ||
        tcnpos.size() != tcname.size()) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- jNatural::fillResult can not proceed due "
            "to invalid arguments";
        return 0;
    }

    ibis::table::bufferArray tbuff(tcname.size());
    ibis::table::typeArray   ttypes(tcname.size());
    IBIS_BLOCK_GUARD(ibis::table::freeBuffers,
                     ibis::util::ref(tbuff),
                     ibis::util::ref(ttypes));
    try {
        // allocate enough space for the in-memory buffers
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
                    << "Warning -- jNatural::fillResult detects an "
                    "invalid tcnpos[" << j << "] = " << tcnpos[j]
                    << ", should be less than " << rtypes.size()+stypes.size();
                return 0;
            }
        }
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- jNatural::fillResult failed to allocate "
            "sufficient memory for " << nrows << " row" << (nrows>1?"s":"")
            << " and " << rtypes.size()+stypes.size()
            << " column" << (rtypes.size()+stypes.size()>1?"s":"");
        return 0;
    }

    size_t tind = 0; // row index into the resulting table
    for (size_t rind = 0, sind = 0;
         rind < rjcol.size() && sind < sjcol.size(); ) {
        while (rind < rjcol.size() && rjcol[rind] < sjcol[sind]) ++ rind;
        while (sind < sjcol.size() && sjcol[sind] < rjcol[rind]) ++ sind;
        if (rind < rjcol.size() && sind < sjcol.size() &&
            rjcol[rind] == sjcol[sind]) {
            size_t rind0 = rind;
            size_t rind1 = rind+1;
            while (rind1 < rjcol.size() && rjcol[rind1] == sjcol[sind])
                ++ rind1;
            size_t sind0 = sind;
            size_t sind1 = sind+1;
            while (sind1 < sjcol.size() && sjcol[sind1] == rjcol[rind])
                ++ sind1;
            for (rind = rind0; rind < rind1; ++ rind) {
                for (sind = sind0; sind < sind1; ++ sind) {
                    for (size_t j = 0; j < tcnpos.size(); ++ j) {
                        if (tcnpos[j] < rbuff.size()) {
                            const size_t j1 = tcnpos[j];
                            ibis::bord::copyValue
                                (rtypes[j1], tbuff[j], tind, rbuff[j1], rind);
                        }
                        else {
                            const size_t j1 = tcnpos[j-rtypes.size()];
                            ibis::bord::copyValue
                                (stypes[j1], tbuff[j], tind, sbuff[j1], sind);
                        }
                    } // j
                    ++ tind;
                } // sind
            } // rind
        }
    } // for (size_t rind = 0, sind = 0; ...
    if (tind != nrows) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- jNatural::fillResult expected to produce "
            << nrows << " row" << (nrows>1?"s":"") << ", but produced "
            << tind << " instead";
        return 0;
    }

    std::string tn = ibis::util::shortName(desc.c_str());
    std::unique_ptr<ibis::bord> res
        (new ibis::bord(tn.c_str(), desc.c_str(), nrows,
                        tbuff, ttypes, tcname));
    return res.release();
} // ibis::jNatural::fillResult

/// Select values for a list of column names.
///
/// @note The incoming argument MUST be a list of column names.  Can not be
/// any aggregation functions!
ibis::table*
ibis::jNatural::select(const ibis::table::stringArray& colnames) const {
    ibis::table *res = 0;
    if (nrows < 0) {
        int64_t ierr = count();
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- jNatural::count failed with error code "
                << ierr;
            return res;
        }
    }
    if (valR_ == 0 || orderR_ == 0 || valS_ == 0 || orderS_ == 0 ||
        orderR_->size() != maskR_.cnt() || orderS_->size() != maskS_.cnt()) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- jNatural::select can not proceed without properly "
            "initialized internal data strucutres";
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
        int match = -1; // 0 ==> R_, 1 ==> S_
        if (! tname.empty()) {
            const int nfrm = (frm_!=0 ? frm_->size() : 0);
            match = (frm_!=0 ? frm_->position(tname.c_str()) : 0);
            if (match >= nfrm) {
                if (stricmp(tname.c_str(), R_.name()) == 0) {
                    match = 0;
                }
                else if (stricmp(tname.c_str(), S_.name()) == 0) {
                    match = 1;
                }
            }
        }

        if (match == 0) {
            const ibis::column *col = R_.getColumn(cn);
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
                    << "Warning -- " << evt
                    << " can not find column named \""
                    << colnames[j] << "\" in data partition \""
                    << R_.name() << "\"";
                return res;
            }
        }
        else if (match == 1) {
            const ibis::column *col = S_.getColumn(cn);
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
                    << "Warning -- " << evt
                    << " can not find column named \""
                    << colnames[j] << "\" in data partition \""
                    << S_.name() << "\"";
                return res;
            }
        }
        else { // not prefixed with a data partition name
            cn = colnames[j];
            const ibis::column* col = R_.getColumn(cn);
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
                    "partition name, assume it is for \""
                    << R_.name() << "\"";
            }
            else {
                col = S_.getColumn(cn);
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
                        << evt << " encountered a column name ("
                        << colnames[j] << ") that does not start with a "
                        "data partition name, assume it is for \""
                        << S_.name() << "\"";
                }
                else {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- " << evt << " encountered a name ("
                        << colnames[j] << ") that doesnot start with a "
                        "data partition name";
                    return res;
                }
            }
        }
    } // for (uint32_t j = 0; j < ncols;

    LOGGER(ibis::gVerbose > 3)
        << evt << " -- found " << ircol.size()
        << " column" << (ircol.size() > 1 ? "s" : "") << " from "
        << R_.name() << " and " << iscol.size() << " column"
        << (iscol.size() > 1 ? "s" : "") << " from " << S_.name();

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

    // retrieve values from R_
    for (uint32_t j = 0; sane && j < ircol.size(); ++ j) {
        rtypes[j] = ircol[j]->type();
        switch (ircol[j]->type()) {
        case ibis::BYTE:
            rbuff[j] = ircol[j]->selectBytes(maskR_);
            if (rbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<array_t<signed char>*>(rbuff[j]),
                     *orderR_);
            else
                sane = false;
            break;
        case ibis::UBYTE:
            rbuff[j] = ircol[j]->selectUBytes(maskR_);
            if (rbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<array_t<unsigned char>*>(rbuff[j]),
                     *orderR_);
            else
                sane = false;
            break;
        case ibis::SHORT:
            rbuff[j] = ircol[j]->selectShorts(maskR_);
            if (rbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<array_t<int16_t>*>(rbuff[j]),
                     *orderR_);
            else
                sane = false;
            break;
        case ibis::USHORT:
            rbuff[j] = ircol[j]->selectUShorts(maskR_);
            if (rbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<array_t<uint16_t>*>(rbuff[j]),
                     *orderR_);
            else
                sane = false;
            break;
        case ibis::INT:
            rbuff[j] = ircol[j]->selectInts(maskR_);
            if (rbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<array_t<int32_t>*>(rbuff[j]),
                     *orderR_);
            else
                sane = false;
            break;
        case ibis::LONG:
            rbuff[j] = ircol[j]->selectLongs(maskR_);
            if (rbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<array_t<int64_t>*>(rbuff[j]),
                     *orderR_);
            else
                sane = false;
            break;
        case ibis::ULONG:
            rbuff[j] = ircol[j]->selectULongs(maskR_);
            if (rbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<array_t<uint64_t>*>(rbuff[j]),
                     *orderR_);
            else
                sane = false;
            break;
        case ibis::FLOAT:
            rbuff[j] = ircol[j]->selectFloats(maskR_);
            if (rbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<array_t<float>*>(rbuff[j]),
                     *orderR_);
            else
                sane = false;
            break;
        case ibis::DOUBLE:
            rbuff[j] = ircol[j]->selectDoubles(maskR_);
            if (rbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<array_t<double>*>(rbuff[j]),
                     *orderR_);
            else
                sane = false;
            break;
        case ibis::TEXT:
            rbuff[j] = ircol[j]->selectStrings(maskR_);
            if (rbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<std::vector<std::string>*>(rbuff[j]),
                     *orderR_);
            else
                sane = false;
            break;
        case ibis::UINT: {
            rbuff[j] = ircol[j]->selectUInts(maskR_);
            if (rbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<array_t<uint32_t>*>(rbuff[j]),
                     *orderR_);
            else
                sane = false;
            break;}
        case ibis::CATEGORY: {
            rtypes[j] = ibis::UINT;
            rbuff[j] = ircol[j]->selectUInts(maskR_);
            if (rbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<array_t<uint32_t>*>(rbuff[j]),
                     *orderR_);
            else
                sane = false;
            break;}
        default:
            sane = false;
            rbuff[j] = 0;
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jNatural::select does not support column "
                "type " << ibis::TYPESTRING[(int)ircol[j]->type()]
                << " (name = " << R_.name() << "." << ircol[j]->name() << ")";
            break;
        }
    }
    if (! sane) {
        return res;
    }

    // retrieve values from S_
    for (uint32_t j = 0; sane && j < iscol.size(); ++ j) {
        stypes[j] = iscol[j]->type();
        switch (iscol[j]->type()) {
        case ibis::BYTE:
            sbuff[j] = iscol[j]->selectBytes(maskS_);
            if (sbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<array_t<signed char>*>(sbuff[j]),
                     *orderS_);
            else
                sane = false;
            break;
        case ibis::UBYTE:
            sbuff[j] = iscol[j]->selectUBytes(maskS_);
            if (sbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<array_t<unsigned char>*>(sbuff[j]),
                     *orderS_);
            else
                sane = false;
            break;
        case ibis::SHORT:
            sbuff[j] = iscol[j]->selectShorts(maskS_);
            if (sbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<array_t<int16_t>*>(sbuff[j]),
                     *orderS_);
            else
                sane = false;
            break;
        case ibis::USHORT:
            sbuff[j] = iscol[j]->selectUShorts(maskS_);
            if (sbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<array_t<uint16_t>*>(sbuff[j]),
                     *orderS_);
            else
                sane = false;
            break;
        case ibis::INT:
            sbuff[j] = iscol[j]->selectInts(maskS_);
            if (sbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<array_t<int32_t>*>(sbuff[j]),
                     *orderS_);
            else
                sane = false;
            break;
        case ibis::LONG:
            sbuff[j] = iscol[j]->selectLongs(maskS_);
            if (sbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<array_t<int64_t>*>(sbuff[j]),
                     *orderS_);
            else
                sane = false;
            break;
        case ibis::ULONG:
            sbuff[j] = iscol[j]->selectULongs(maskS_);
            if (sbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<array_t<uint64_t>*>(sbuff[j]),
                     *orderS_);
            else
                sane = false;
            break;
        case ibis::FLOAT:
            sbuff[j] = iscol[j]->selectFloats(maskS_);
            if (sbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<array_t<float>*>(sbuff[j]),
                     *orderS_);
            else
                sane = false;
            break;
        case ibis::DOUBLE:
            sbuff[j] = iscol[j]->selectDoubles(maskS_);
            if (sbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<array_t<double>*>(sbuff[j]),
                     *orderS_);
            else
                sane = false;
            break;
        case ibis::TEXT:
            sbuff[j] = iscol[j]->selectStrings(maskS_);
            if (sbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<std::vector<std::string>*>(sbuff[j]),
                     *orderS_);
            else
                sane = false;
            break;
        case ibis::UINT:{
            sbuff[j] = iscol[j]->selectUInts(maskS_);
            if (sbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<array_t<uint32_t>*>(sbuff[j]),
                     *orderS_);
            else
                sane = false;
            break;}
        case ibis::CATEGORY: {
            stypes[j] = ibis::UINT;
            sbuff[j] = iscol[j]->selectUInts(maskS_);
            if (sbuff[j] != 0)
                ibis::util::reorder
                    (*static_cast<array_t<uint32_t>*>(sbuff[j]),
                     *orderS_);
            else
                sane = false;
            break;}
        default:
            sane = false;
            sbuff[j] = 0;
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- jNatural::select does not support column "
                "type " << ibis::TYPESTRING[(int)iscol[j]->type()]
                << " (name = " << S_.name() << "." << iscol[j]->name() << ')';
            break;
        }
    }
    if (! sane) {
        return res;
    }

    /// fill the in-memory buffer
    switch (colR_.type()) {
    case ibis::BYTE:
        res = fillResult
            (nrows, evt,
             *static_cast<array_t<signed char>*>(valR_), rtypes, rbuff,
             *static_cast<array_t<signed char>*>(valS_), stypes, sbuff,
             colnames, ipToPos);
        break;
    case ibis::UBYTE:
        res = fillResult
            (nrows, evt,
             *static_cast<array_t<unsigned char>*>(valR_), rtypes, rbuff,
             *static_cast<array_t<unsigned char>*>(valS_), stypes, sbuff,
             colnames, ipToPos);
        break;
    case ibis::SHORT:
        res = fillResult
            (nrows, evt,
             *static_cast<array_t<int16_t>*>(valR_), rtypes, rbuff,
             *static_cast<array_t<int16_t>*>(valS_), stypes, sbuff,
             colnames, ipToPos);
        break;
    case ibis::USHORT:
        res = fillResult
            (nrows, evt,
             *static_cast<array_t<uint16_t>*>(valR_), rtypes, rbuff,
             *static_cast<array_t<uint16_t>*>(valS_), stypes, sbuff,
             colnames, ipToPos);
        break;
    case ibis::INT:
        res = fillResult
            (nrows, evt,
             *static_cast<array_t<int32_t>*>(valR_), rtypes, rbuff,
             *static_cast<array_t<int32_t>*>(valS_), stypes, sbuff,
             colnames, ipToPos);
        break;
    case ibis::UINT:
        res = fillResult
            (nrows, evt,
             *static_cast<array_t<uint32_t>*>(valR_), rtypes, rbuff,
             *static_cast<array_t<uint32_t>*>(valS_), stypes, sbuff,
             colnames, ipToPos);
        break;
    case ibis::LONG:
        res = fillResult
            (nrows, evt,
             *static_cast<array_t<int64_t>*>(valR_), rtypes, rbuff,
             *static_cast<array_t<int64_t>*>(valS_), stypes, sbuff,
             colnames, ipToPos);
        break;
    case ibis::ULONG:
        res = fillResult
            (nrows, evt,
             *static_cast<array_t<uint64_t>*>(valR_), rtypes, rbuff,
             *static_cast<array_t<uint64_t>*>(valS_), stypes, sbuff,
             colnames, ipToPos);
        break;
    case ibis::FLOAT:
        res = fillResult
            (nrows, evt,
             *static_cast<array_t<float>*>(valR_), rtypes, rbuff,
             *static_cast<array_t<float>*>(valS_), stypes, sbuff,
             colnames, ipToPos);
        break;
    case ibis::DOUBLE:
        res = fillResult
            (nrows, evt,
             *static_cast<array_t<double>*>(valR_), rtypes, rbuff,
             *static_cast<array_t<double>*>(valS_), stypes, sbuff,
             colnames, ipToPos);
        break;
    case ibis::TEXT:
    case ibis::CATEGORY:
        res = fillResult
            (nrows, evt,
             *static_cast<std::vector<std::string>*>(valR_), rtypes, rbuff,
             *static_cast<std::vector<std::string>*>(valS_), stypes, sbuff,
             colnames, ipToPos);
        break;
    default:
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " cannot handle join column of type "
            << ibis::TYPESTRING[(int)colR_.type()];
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
} // ibis::jNatural::select

ibis::table* ibis::jNatural::select(const char* sstr) const {
    if (nrows < 0) {
        int64_t ierr = count();
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- jNatural::count failed with error code "
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
            if (*str != '_' || str[1] != '_')
                sl.push_back(str);
        }
    }

    std::unique_ptr<ibis::table> res1(select(sl));
    if (res1.get() == 0 || res1->nRows() == 0 || res1->nColumns() == 0 ||
        features == 0)
        return res1.release();

    if (ibis::gVerbose > 2) {
        ibis::util::logger lg;
        lg() << "jNatural::select(" << sstr << ", " << desc_
             << ") produced the first intermediate table:\n";
        res1->describe(lg());
    }

    if ((features & 1) != 0) { // arithmetic computations
        std::unique_ptr<ibis::table> 
            res2(static_cast<const ibis::bord*>(res1.get())->evaluateTerms
                 (sel, desc_.c_str()));
        if (res2.get() != 0) {
            if (ibis::gVerbose > 2) {
                ibis::util::logger lg;
                lg() << "jNatural::select(" << sstr << ", " << desc_
                     << ") produced the second intermediate table:\n";
                res2->describe(lg());
            }
            res1 = std::move(res2);
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- jNatural::select(" << sstr
                << ") failed to evaluate the arithmetic expressions";
            return res1.release();
        }
    }

    if ((features & 2) != 0) { // aggregation operations
        res1.reset(static_cast<const ibis::bord*>(res1.get())->groupby(sel));
        if (res1.get() != 0) {
            if (ibis::gVerbose > 2) {
                ibis::util::logger lg;
                lg() << "jNatural::select(" << sstr << ", " << desc_
                     << ") produced the third intermediate table:\n";
                res1->describe(lg());
            }
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- jNatural::select(" << sstr
                << ") failed to evaluate the aggregations";
        }
    }
    return res1.release();
} // ibis::jNatural::select

/// Evaluate the select clause specified in the constructor.
ibis::table* ibis::jNatural::select() const {
    if (nrows < 0) {
        int64_t ierr = count();
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- jNatural::count failed with error code "
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
        lg() << "jNatural::select(" << *sel_ << ", " << desc_
             << ") produced the first intermediate table:\n";
        res1->describe(lg());
    }

    if ((features & 1) != 0) { // arithmetic computations
        res1.reset(static_cast<const ibis::bord*>(res1.get())->evaluateTerms
                   (*sel_, desc_.c_str()));
        if (res1.get() != 0) {
            if (ibis::gVerbose > 2) {
                ibis::util::logger lg;
                lg() << "jNatural::select(" << *sel_ << ", " << desc_
                     << ") produced the second intermediate table:\n";
                res1->describe(lg());
            }
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- jNatural::select(" << *sel_
                << ") failed to evaluate the arithmetic expressions";
            return 0;
        }
    }

    if ((features & 2) != 0) { // aggregation operations
        res1.reset(static_cast<const ibis::bord*>(res1.get())->groupby(*sel_));
        if (res1.get() != 0) {
            if (ibis::gVerbose > 2) {
                ibis::util::logger lg;
                lg() << "jNatural::select(" << *sel_ << ", " << desc_
                     << ") produced the third intermediate table:\n";
                res1->describe(lg());
            }
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- jNatural::select(" << *sel_
                << ") failed to evaluate the aggregations";
        }
    }
    return res1.release();
} // ibis::jNatural::select
