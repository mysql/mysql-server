//File: $Id$
// Author: John Wu <John.Wu at ACM.org>
// Copyright (c) 2000-2016 the Regents of the University of California
#include "dictionary.h"
#include "utilidor.h"

/// The header of dictionary files.  It has 20 bytes exactly.
static char _fastbit_dictionary_header[20] =
    {'#', 'I', 'B', 'I', 'S', ' ', 'D', 'i', 'c', 't',
     'i', 'o', 'n', 'a', 'r', 'y', 2, 0, 0, 0};

/// Default constructor.
ibis::dictionary::dictionary() {
} // default constructor

/// Copy constructor.  Places all the string in one contiguous buffer.
ibis::dictionary::dictionary(const ibis::dictionary& old)
    : raw_(old.raw_.size()), buffer_(1) {
    if (old.key_.empty()) {
        buffer_.clear();
        return;
    }

    const uint32_t nraw = old.raw_.size();
    // find out the size of the buffer to allocate
    size_t sz = 0;
    for (size_t i = 0; i < nraw; ++ i) {
        if (old.raw_[i] != 0) {
            sz += 1 + std::strlen(old.raw_[i]);
        }
    }

    // allocate memeory, record in buffer_
    char *str = new char[sz];
    buffer_[0] = str;

    // copy the string values and populate the hash map
    for (size_t i = 0; i < nraw; ++ i) {
        if (old.raw_[i] != 0) {
            raw_[i] = str;
            for (const char *t = old.raw_[i]; *t != 0; ++ t, ++ str)
                *str = *t;
            *str = 0;
            ++ str;
            key_[raw_[i]] = i;
        }
        else {
            raw_[i] = 0;
        }
    }
} // copy constructor

/// Compare whether this dicrionary and the other are equal in content.
/// The two dictionaries are considered same only if they have the same
/// keys in the same order.
bool ibis::dictionary::equal_to(const ibis::dictionary& other) const {
    if (key_.size() != other.key_.size())
        return false;

    for (MYMAP::const_iterator it = key_.begin(); it != key_.end(); ++ it) {
        MYMAP::const_iterator ot = other.key_.find(it->first);
        if (it->second != ot->second)
            return false;
    }
    return true;
} // ibis::dictionary::equal_to

/// Copy function.  Use copy constructor and swap the content.
void ibis::dictionary::copy(const ibis::dictionary& old) {
    ibis::dictionary tmp(old);
    swap(tmp);
} // ibis::dictionary::copy

/**
   Write the content of the dictionary to the named file.  The existing
   content in the named file is overwritten.  The content of the dictionary
   file is laid out as follows.

   \li Signature "#IBIS Dictionary " and version number (currently
   0x020000). (20 bytes)

   \li N = Number of strings in the file. (4 bytes)

   \li uint64_t[N+1]: the starting positions of the strings in this file.

   \li uint32_t[N]: The integer code corresponding to each string value.

   \li the string values packed one after the other with their nil
   terminators.
*/
int ibis::dictionary::write(const char* name) const {
    std::string evt = "dictionary::write";
    if (name == 0 || *name == 0) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- " << evt << " can not proceed with a "
            "null string as the file name";
        return -1;
    }
    if (ibis::gVerbose > 1) {
        evt += '(';
        evt += name;
        evt += ')';
    }
    if (key_.size() > raw_.size()) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- " << evt
            << " can not write an inconsistent dictionary, key_.size("
            << key_.size() << "), raw_.size(" << raw_.size() << ")";
        return -2;
    }

    ibis::util::timer mytimer(evt.c_str(), 4);
    FILE* fptr = fopen(name, "wb");
    if (fptr == 0) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- " << evt << " failed to open the file ... "
            << (errno ? strerror(errno) : "no free stdio stream");
        return -3;
    }

    IBIS_BLOCK_GUARD(fclose, fptr);
    int ierr = fwrite(_fastbit_dictionary_header, 1, 20, fptr);
    if (ierr != 20) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- " << evt
            << " failed to write the header, fwrite returned " << ierr;
        return -4;
    }

    const uint32_t nkeys = key_.size();
    ierr = fwrite(&nkeys, sizeof(nkeys), 1, fptr);
    if (ierr != 1) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- " << evt << " failed to write the size(" << nkeys
            << "), fwrite returned " << ierr;
        return -5;
    }
    if (nkeys == 0) // nothing else to write
        return 0;

    mergeBuffers();
    array_t<uint64_t> pos(nkeys+1);
    array_t<uint32_t> qos(nkeys);

    pos.clear();
    qos.clear();
    pos.push_back(0);
    if (buffer_.size() == 1) {
        for (uint32_t j = 0; j < raw_.size(); ++ j) {
            if (raw_[j] != 0) {
                pos.push_back(1U + strlen(raw_[j]));
                qos.push_back(j);
            }
        }
        ierr = writeBuffer(fptr, nkeys, pos, qos);
    }
    else {
        ierr = writeKeys(fptr, nkeys, pos, qos);
    }
    LOGGER(ibis::gVerbose > 1)
        << evt << " complete with ierr = " << ierr;
    return ierr;
} // ibis::dictionary::write

/// Write the dictionary one keyword at a time.  This version requires on
/// write call on each keyword, which can be time consuming when there are
/// many keywords.
int ibis::dictionary::writeKeys(FILE *fptr, uint32_t nkeys,
                                array_t<uint64_t> &pos,
                                array_t<uint32_t> &qos) const {
    int ierr = fseek(fptr, 8*(nkeys+1)+4*nkeys, SEEK_CUR);
    long int tmp = ftell(fptr);
    pos.clear();
    qos.clear();
    pos.push_back(tmp);
    for (uint32_t j = 0; j < raw_.size(); ++ j) {
        if (raw_[j] != 0) {
            const int len = 1 + std::strlen(raw_[j]);
            ierr = fwrite(raw_[j], 1, len, fptr);
            LOGGER(ierr != len && ibis::gVerbose > 1)
                << "Warning -- dictionary::writeKeys failed to write key["
                << j << "]; expected fwrite to return " << len
                << ", but got " << ierr;

            tmp += len;
            qos.push_back(j);
            pos.push_back(tmp);
        }
    }

    tmp = 0;
    // go back to write the offsets/positions
    ierr = fseek(fptr, 24, SEEK_SET);
    LOGGER(ierr != 0 && ibis::gVerbose > 1)
        << "Warning -- dictionary::writeKeys failed to seek to offset 24 "
        "to write the offsets";

    ierr = fwrite(pos.begin(), sizeof(uint64_t), nkeys+1, fptr);
    LOGGER(ierr != (int)(nkeys+1) && ibis::gVerbose > 1)
        << "Warning -- dictionary::writeKeys failed to write the offsets, "
        "expected fwrite to return " << nkeys+1 << ", but got " << ierr;
    tmp -= 7 * (ierr != (int)(nkeys+1));

    ierr = fwrite(qos.begin(), sizeof(uint32_t), nkeys, fptr);
    LOGGER(ierr != (int)(nkeys) && ibis::gVerbose > 1)
        << "Warning -- dictionary::writeKeys failed to write the keys, "
        "expected fwrite to return " << nkeys << ", but got " << ierr;
    tmp -= 8 * (ierr != (int)(nkeys));
    return tmp;
} // ibis::dictionary::writeKeys

/// Write the buffer out directly.
///
/// @ntoe This function is intended to be used by dictionary::write and
/// must satisfy the following conditions.  There must be only one buffer,
/// and the raw_ must be ordered in that buffer.  Under these conditions,
/// we can write the buffer using a single sequential write operations,
/// which should reduce the I/O time.  The easiest way to satisfy these
/// conditions is to invoke mergeBuffers.
int ibis::dictionary::writeBuffer(FILE *fptr, uint32_t nkeys,
                                  array_t<uint64_t> &pos,
                                  array_t<uint32_t> &qos) const {
    size_t ierr;
    pos[0] = 24 + 8 * (nkeys+1) + 4 * nkeys;
    for (unsigned j = 0; j < nkeys; ++ j)
        pos[j+1] += pos[j];

    ierr = fwrite(pos.begin(), sizeof(uint64_t), nkeys+1, fptr);
    LOGGER(ierr != (int)(nkeys+1) && ibis::gVerbose > 1)
        << "Warning -- dictionary::writeBuffer failed to write the offsets, "
        "expected fwrite to return " << nkeys+1 << ", but got " << ierr;

    ierr = fwrite(qos.begin(), sizeof(uint32_t), nkeys, fptr);
    LOGGER(ierr != (int)(nkeys) && ibis::gVerbose > 1)
        << "Warning -- dictionary::writeBuffer failed to write the keys, "
        "expected fwrite to return " << nkeys << ", but got " << ierr;

    const char *buff = buffer_[0];
    size_t sz = pos[nkeys] - pos[0];
    while (sz > 0) {  // a large buffer may need multuple fwrite calls
        ierr = fwrite(buff, 1, sz, fptr);
        if (ierr > 0U && ierr <= sz) {
            buff += ierr;
            sz -= ierr;
        }
        else {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- dictionary::writeBuffer failed to write the "
                "buffer, fwrite retruned 0";
            return -6;
        }
    }
    return 0;
} // ibis::dictionary::writeBuffer

/// Read the content of the named file.  The file content is read into the
/// buffer in one-shot and then digested.
///
/// This function determines the version of the dictionary and invokes the
/// necessary reading function to perform the actual reading operations.
/// Currently there are three possible version of dictioanries
/// 0x02000000 - the version produced by the current write function,
/// 0x01000000 - the version with 64-bit offsets, consecutive kyes, strings
///              are stored in key order
/// 0x00000000 - the version 32-bit offsets and stores strings in
///              sorted order.
/// unmarked   - the version without a header, only has the bare strings in
///              the code order.
int ibis::dictionary::read(const char* name) {
    if (name == 0 || *name == 0) return -1;
    std::string evt = "dictionary::read";
    if (ibis::gVerbose > 1) {
        evt += '(';
        evt += name;
        evt += ')';
    }

    // open the file to read
    int ierr = 0;
    FILE* fptr = fopen(name, "rb");
    if (fptr == 0) {
        LOGGER(ibis::gVerbose > 3)
            << "Warning -- " << evt << " failed to open the file ... "
            << (errno ? strerror(errno) : "no free stdio stream");
        return -2;
    }

    ibis::util::timer mytimer(evt.c_str(), 4);
    IBIS_BLOCK_GUARD(fclose, fptr);
    ierr = fseek(fptr, 0, SEEK_END); // to the end
    if (ierr != 0) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- " << evt << " failed to seek to the end of the file";
        return -3;
    }

    uint32_t version = 0xFFFFFFFFU;
    long int sz = ftell(fptr); // file size
    if (sz > 24) {
        char header[20];
        ierr = fseek(fptr, 0, SEEK_SET);
        if (ierr != 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- " << evt << " failed to seek to the beginning "
                "of the file";
            return -4;
        }

        ierr = fread(header, 1, 20, fptr);
        if (ierr != 20) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- " << evt << " failed to read the 20-byte header";
            return -5;
        }
        if (header[0] == _fastbit_dictionary_header[0] &&
            header[1] == _fastbit_dictionary_header[1] &&
            header[2] == _fastbit_dictionary_header[2] &&
            header[3] == _fastbit_dictionary_header[3] &&
            header[4] == _fastbit_dictionary_header[4] &&
            header[5] == _fastbit_dictionary_header[5] &&
            header[6] == _fastbit_dictionary_header[6] &&
            header[7] == _fastbit_dictionary_header[7] &&
            header[8] == _fastbit_dictionary_header[8] &&
            header[9] == _fastbit_dictionary_header[9] &&
            header[10] == _fastbit_dictionary_header[10] &&
            header[11] == _fastbit_dictionary_header[11] &&
            header[12] == _fastbit_dictionary_header[12] &&
            header[13] == _fastbit_dictionary_header[13] &&
            header[14] == _fastbit_dictionary_header[14] &&
            header[15] == _fastbit_dictionary_header[15]) {
            version = (header[16] << 24 | header[17] << 16 |
                       header[18] << 8 | header[19]);
            LOGGER(ibis::gVerbose > 3)
                << evt << " detected dictionary version 0x" << std::hex
                << version << std::dec;
        }
        else {
            LOGGER(ibis::gVerbose > 2)
                << evt << " did not find the expected header, assume "
                "to have no header (oldest version of dictioinary)";
        }
    }

    // invoke the actual reader based on version number
    switch (version) {
    case 0x02000000:
            ierr = readKeys2(evt.c_str(), fptr);
            break;
    case 0x01000000:
            ierr = readKeys1(evt.c_str(), fptr);
            break;
    case 0x00000000:
            ierr = readKeys0(evt.c_str(), fptr);
            break;
    default:
            ierr = readRaw(evt.c_str(), fptr);
            break;
    }
    if (ibis::gVerbose > 3) {
        ibis::util::logger lg;
        lg() << evt << " completed with ";
        toASCII(lg());
    }
    return ierr;
} // ibis::dictionary::read

/// Read the raw strings.  This is for the oldest style dictionary that
/// contains the raw strings.  There is no header in the dictionary file,
/// therefore this function has rewind back to the beginning of the file.
/// On successful completion, this function returns 0.
int ibis::dictionary::readRaw(const char *evt, FILE *fptr) {
    int ierr = fseek(fptr, 0, SEEK_END);
    if (ierr != 0) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- " << evt << " failed to seek to the end of the file";
        return -11;
    }
    clear();
    long int sz = ftell(fptr); // file size
    ierr = sz;
    if (ierr != sz) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- " << evt << " can not proceed because the "
            "dictionary file size (" << sz
            << ") does not fit into a 32-bit integer";
        return -12;
    }

    buffer_.resize(1);
    buffer_[0] = new char[sz];
    ierr = fseek(fptr, 0, SEEK_SET);
    if (ierr != 0) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- " << evt << " failed to seek to the beginning of "
            "the file";
        return -13;
    }
    ierr = fread(buffer_[0], 1, sz, fptr);
    if (ierr != sz) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- " << evt << " failed to read " << sz << " byte"
            << (sz>1?"s":"") << ", fread returned " << ierr;
        delete [] buffer_[0];
        buffer_.clear();
        return -14;
    }

    const char *str = buffer_[0];
    const char *end = buffer_[0] + ierr;
    do {
        key_[str] = raw_.size();
        raw_.push_back(str);
        while (*str != 0 && str < end) ++ str;
        if (*str == 0) {
            ++ str;
        }
    } while (str < end);

#if DEBUG+0 > 2 || _DEBUG+0 > 2
    ibis::util::logger lg;
    lg() << "DEBUG -- " << evt << " got the following keys";
    for (MYMAP::const_iterator it = key_.begin(); it != key_.end(); ++ it)
        lg() << "\n\t" << it->second << ": " << it->first;
#endif
    return 0;
} // ibis::dictionary::readRaw

/// Read the string values.  This function processes the data produced by
/// version 0x00000000 of the write function.  On successful completion, it
/// returns 0.
///
/// Note that this function assume the 20-byte header has been read
/// already.
int ibis::dictionary::readKeys0(const char *evt, FILE *fptr) {
    uint32_t nkeys;
    int ierr = fread(&nkeys, 4, 1, fptr);
    if (ierr != 1) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- " << evt
            << " failed to read the number of keys, fread returned " << ierr;
        return -6;
    }

    clear();
    array_t<uint32_t> codes(nkeys);
    ierr = fread(codes.begin(), 4, nkeys, fptr);
    if (ierr != (long)nkeys) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- " << evt << " failed to read " << nkeys
            << ", fread returned " << ierr;
        return -7;
    }

    array_t<uint32_t> offsets(nkeys+1);
    ierr = fread(offsets.begin(), 4, nkeys+1, fptr);
    if (ierr != (int)(1+nkeys)) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- " << evt << " failed to read the string positions, "
            "expected fread to return " << nkeys+1 << ", but got " << ierr;
        return -8;
    }

    buffer_.resize(1);
    buffer_[0] = new char[offsets.back()-offsets.front()];
    ierr = fread(buffer_[0], 1, offsets.back()-offsets.front(), fptr);
    if (ierr != (int)(offsets.back()-offsets.front())) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- " << evt << " failed to read the strings, "
            "expected fread to return " << (offsets.back()-offsets.front())
            << ", but got " << ierr;
        return -9;
    }
    raw_.resize(nkeys+1);
    raw_[0] = 0;
    // some implementation of unordered_map does have function reserve
    // key_.reserve(nkeys+nkeys);
    for (unsigned j = 0; j < nkeys; ++ j) {
        const uint32_t ik = codes[j];
        if (ik > 0 && ik <= nkeys) {
            raw_[ik] = buffer_[0] + (offsets[j] - offsets[0]);
            key_[raw_[ik]] = ik;
        }
        else {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- " << evt << " encounter code " << ik
                << " which is out of the expected range [0, " << nkeys << ']';
        }
#if DEBUG+0 > 2 || _DEBUG+0 > 2
        LOGGER(ibis::gVerbose > 0)
            << "DEBUG -- " << evt << " raw_[" << ik << "] = \"" << raw_[ik]
            << '"';
#endif
    }

#if DEBUG+0 > 2 || _DEBUG+0 > 2
    ibis::util::logger lg;
    lg() << "DEBUG -- " << evt << " got the following keys\n\t";
    for (MYMAP::const_iterator it = key_.begin(); it != key_.end(); ++ it)
        lg() << "\n\t" << it->second << ": " << it->first;
#endif
    return 0;
} // ibis::dictionary::readKeys0

/// Read the string values.  This function processes the data produced by
/// version 0x01000000 of the write function.  On successful completion, it
/// returns 0.
int ibis::dictionary::readKeys1(const char *evt, FILE *fptr) {
    uint32_t nkeys;
    int ierr = fread(&nkeys, 4, 1, fptr);
    if (ierr != 1) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- " << evt
            << " failed to read the number of keys, fread returned " << ierr;
        return -6;
    }

    clear();

    array_t<uint64_t> offsets(nkeys+1);
    ierr = fread(offsets.begin(), 8, nkeys+1, fptr);
    if (ierr != (int)(1+nkeys)) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- " << evt << " failed to read the string positions, "
            "expected fread to return " << nkeys+1 << ", but got " << ierr;
        return -8;
    }

    buffer_.resize(1);
    buffer_[0] = new char[offsets.back()-offsets.front()];
    ierr = fread(buffer_[0], 1, offsets.back()-offsets.front(), fptr);
    if (ierr != (int)(offsets.back()-offsets.front())) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- " << evt << " failed to read the strings, "
            "expected fread to return " << (offsets.back()-offsets.front())
            << ", but got " << ierr;
        return -9;
    }
    raw_.resize(nkeys+1);
    raw_[0] = 0;
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ > 8)
    key_.reserve(nkeys+nkeys); // older version does not have reserve
#endif
    for (unsigned j = 0; j < nkeys; ++ j) {
        raw_[j+1] = buffer_[0] + (offsets[j] - offsets[0]);
        key_[raw_[j+1]] = j+1;
#if DEBUG+0 > 2 || _DEBUG+0 > 2
        LOGGER(ibis::gVerbose > 0)
            << "DEBUG -- " << evt << " raw_[" << j+1 << "] = \"" << raw_[j+1]
            << '"';
#endif
    }

#if DEBUG+0 > 2 || _DEBUG+0 > 2
    ibis::util::logger lg;
    lg() << "DEBUG -- " << evt << " got the following keys";
    for (MYMAP::const_iterator it = key_.begin(); it != key_.end(); ++ it)
        lg() << "\n\t" << it->second << ": " << it->first;
#endif
    return 0;
} // ibis::dictionary::readKeys1

/// Read the string values.  This function processes the data produced by
/// version 0x01000000 of the write function.  On successful completion, it
/// returns 0.
int ibis::dictionary::readKeys2(const char *evt, FILE *fptr) {
    uint32_t nkeys;
    int ierr = fread(&nkeys, 4, 1, fptr);
    if (ierr != 1) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- " << evt
            << " failed to read the number of keys, fread returned " << ierr;
        return -6;
    }

    clear();

    array_t<uint32_t> codes(nkeys);
    array_t<uint64_t> offsets(nkeys+1);
    ierr = fread(offsets.begin(), 8, nkeys+1, fptr);
    if (ierr != (int)(1+nkeys)) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- " << evt << " failed to read the string positions, "
            "expected fread to return " << nkeys+1 << ", but got " << ierr;
        return -7;
    }

    ierr = fread(codes.begin(), 4, nkeys, fptr);
    if (ierr != (int)(nkeys)) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- " << evt << " failed to read the string keys, "
            "expected fread to return " << nkeys << ", but got " << ierr;
        return -8;
    }
    uint32_t maxcode = 0;
    for (size_t j = 0; j < nkeys; ++ j) {
        if (maxcode < codes[j])
            maxcode = codes[j];
    }

    buffer_.resize(1);
    buffer_[0] = new char[offsets.back()-offsets.front()];
    ierr = fread(buffer_[0], 1, offsets.back()-offsets.front(), fptr);
    if (ierr != (int)(offsets.back()-offsets.front())) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- " << evt << " failed to read the strings, "
            "expected fread to return " << (offsets.back()-offsets.front())
            << ", but got " << ierr;
        return -9;
    }

    raw_.resize(maxcode+1);
    for (size_t j = 0; j <= maxcode; ++ j)
        raw_[j] = 0;
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ > 8)
    key_.reserve(nkeys+nkeys); // older version does not have reserve
#endif
    for (unsigned j = 0; j < nkeys; ++ j) {
        const char* tmp = buffer_[0] + (offsets[j] - offsets[0]);
        raw_[codes[j]] = tmp;
        key_[tmp] = codes[j];
#if DEBUG+0 > 2 || _DEBUG+0 > 2
        LOGGER(ibis::gVerbose > 0)
            << "DEBUG -- " << evt << " raw_[" << codes[j] << "] = \"" << tmp
            << '"';
#endif
    }

#if DEBUG+0 > 2 || _DEBUG+0 > 2
    ibis::util::logger lg;
    lg() << "DEBUG -- " << evt << " got the following keys\n\t";
    for (MYMAP::const_iterator it = key_.begin(); it != key_.end(); ++ it)
        lg() << "\n\t" << it->second << ": " << it->first;
#endif
    return 0;
} // ibis::dictionary::readKeys2

/// Output the current content in ASCII format.  Each non-empty entry is
/// printed in the format of "number: string".
void ibis::dictionary::toASCII(std::ostream &out) const {
    out << "-- dictionary @" << static_cast<const void*>(this) << " with "
        << key_.size() << " entr" << (key_.size()>1?"ies":"y");
    for (unsigned j = 0; j < raw_.size(); ++ j)
        if (raw_[j] != 0)
            out << "\n" << j << ": \"" << raw_[j] << '"';
} // ibis::dictionary::toASCII

/// Read the ASCII formatted disctionary.  This is meant to be the reverse
/// of toASCII, where each line of the input stream contains a positve
/// integer followed by a string value, with an optioinal ':' (plus white
/// space) as separators.
///
/// The new entries read from the incoming I/O stream are merged with the
/// existing dictioinary.  If the string has already been assigned a code,
/// the existing code will be used.  If the given code has been used for
/// another string, the incoming string will be assined a new code.
/// Warning messages will be printed to the logging channel when such a
/// conflict is encountered.
int ibis::dictionary::fromASCII(std::istream &in) {
    ibis::fileManager::buffer<char> linebuf(MAX_LINE);
    if (! in) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- dictionary::fromASCII can not proceed because "
            "the input I/O stream is in an error state";
        return -1;
    }

    int ierr;
    const char *str;
    bool more = true;
    const char *delim = ":,; \t\v";
    while (more) {
        std::streampos linestart = in.tellg();
        // read the next line
        while (! in.getline(linebuf.address(), linebuf.size())) {
            if (in.eof()) { // end of file, no more to read
                *(linebuf.address()) = 0;
                more  = false;
                break;
            }
            // more to read, linebuf needs to be increased
            const size_t nold =
                (linebuf.size() > 0 ? linebuf.size() : MAX_LINE);
            if (nold+nold != linebuf.resize(nold+nold)) {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- dictionary::fromASCII failed to allocate "
                    "linebuf of " << nold+nold << " bytes";
                more = false;
                return -2;
            }
            in.clear(); // clear the error bit
            // go back to the beginning of the line
            if (! in.seekg(linestart, std::ios::beg)) {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- dictionary::fromASCII failed to seek back "
                    "to the beginning of a line of text";
                *(linebuf.address()) = 0;
                more = false;
                return -3;
            }
        }
        // got a line of text
        str = linebuf.address();
        if (str == 0) break;
        while (*str != 0 && isspace(*str)) ++ str; // skip leading space
        if (*str == 0 || *str == '#' || (*str == '-' && str[1] == '-')) {
            // skip comment line (shell style comment and SQL style comments)
            continue;
        }

        uint64_t posi;
        ierr = ibis::util::readUInt(posi, str, delim);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 3)
                << "Warning -- dictionary::fromASCII could not extract a "
                "number from \"" << linebuf.address() << '"';
            posi = 0;
        }
        str += strspn(str, delim); // skip delimiters
        if (posi > 0 && posi < 0X7FFFFFFF) {
            insert(ibis::util::getString(str), static_cast<uint32_t>(posi));
        }
        else {
            LOGGER(ibis::gVerbose > 3 && posi > 0)
                << "Warning -- dictionary::fromASCII can not use a code ("
                << posi << ") that is larger than 2^31";
            insert(ibis::util::getString(str));
        }
    } // while (more)
    return 0;
} // ibis::dictionary::fromASCII

/// Clear the allocated memory.
void ibis::dictionary::clear() {
    for (size_t i = 0; i < buffer_.size(); ++ i)
        delete [] buffer_[i];
    buffer_.clear();
    key_.clear();
    raw_.clear();
} // ibis::dictionary::clear

/// Find all codes that matches the SQL LIKE pattern.
/// If the pattern is null or empty, matches is not changed.
void ibis::dictionary::patternSearch(const char* pat,
                                     array_t<uint32_t>& matches) const {
    if (pat == 0) return;
    //if (*pat == 0) return;//empty string is allowed
    if (key_.size() == 0) return;
    if (key_.size() > raw_.size()) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- dictionary::patternSearch(" << pat
            << ") can not proceed because the member variables have "
            "inconsistent sizes: raw_.size(" << raw_.size() << ", key_.size("
            << key_.size() << ')';
        return;
    }

#if FASTBIT_CASE_SENSITIVE_COMPARE+0 == 0
    for (char *ptr = const_cast<char*>(pat); *ptr != 0; ++ ptr) {
        *ptr = toupper(*ptr);
    }
#endif
    // extract longest constant prefix to restrict range
    size_t pos;
    bool esc = false;
    bool meta = false;
    std::string prefix;
    const size_t len = std::strlen(pat);
    for (pos = 0; pos < len && !meta; ++pos) {
        const char c = *(pat + pos);
        if (esc) {
            prefix.append(1, c);
            esc = false;
        }
        else {
            switch (c) {
            case STRMATCH_META_ESCAPE:
                esc = true;
                break;
            case STRMATCH_META_CSH_ANY:
            case STRMATCH_META_CSH_ONE:
            case STRMATCH_META_SQL_ANY:
            case STRMATCH_META_SQL_ONE:
                meta = true;
                break;
            default:
                prefix.append(1, c);
                break;
            }
        }
    }

    // if there is no meta char, find the string already
    if (!meta) {
        uint32_t code = operator[](prefix.c_str());
        if (code < raw_.size()) {
            matches.push_back(code);
        }
        return;
    }

    // match values in the range
    for (MYMAP::const_iterator j = key_.begin();
         j != key_.end(); ++ j) {
        if (ibis::util::strMatch(j->first, pat)) {
            matches.push_back(j->second);
        }
    }

#if DEBUG+0 > 2 || _DEBUG+0 > 2
    ibis::util::logger lg;
    lg() << "DEBUG -- dictionary::patternSearch(" << pat << ") found "
         << matches.size() << " matching value" << (matches.size()>1?"s":"")
         << ":\t";
    for (unsigned j = 0; j < matches.size(); ++ j)
        lg() << matches[j] << ' ';
#endif
} // ibis::dictionary::patternSearch

/// Convert a string to its integer code.  Returns 0xFFFFFFFFU for null
/// strings, 0:size()-1 for strings in the dictionary, and
/// dictionary::size() for unknown values.
uint32_t ibis::dictionary::operator[](const char* str) const {
    if (str == 0) return 0xFFFFFFFFU;
#ifdef FASTBIT_EMPTY_STRING_AS_NULL
    if (*str == 0) return 0xFFFFFFFFU;
#endif
#if FASTBIT_CASE_SENSITIVE_COMPARE+0 == 0
    for (char *ptr = const_cast<char*>(str); *ptr != 0; ++ ptr) {
        *ptr = toupper(*ptr);
    }
#endif

#if DEBUG+0 > 2 || _DEBUG+0 > 2
    {
        ibis::util::logger lg;
        lg() << "DEBUG -- dictionary has the following keys\n\t";
        for (MYMAP::const_iterator it = key_.begin(); it != key_.end(); ++ it)
            lg() << '"' << it->first << '"' << ' ';
    }
#endif
    MYMAP::const_iterator it = key_.find(str);
    if (it != key_.end()) {
#if DEBUG+0 > 2 || _DEBUG+0 > 2
        LOGGER(ibis::gVerbose > 0)
            << "DEBUG -- dictionary::operator[] found code " << it->second
            << " for string \"" << str << '"';
#endif
        return it->second;
    }
    else {
#if DEBUG+0 > 2 || _DEBUG+0 > 2
        LOGGER(ibis::gVerbose > 0)
            << "DEBUG -- dictionary::operator[] could NOT find a code for "
            "string \"" << str << '"';
#endif
        return raw_.size();
    }
} // string to int

/// Insert a string to the specified position.  Returns the integer value
/// assigned to the string.  A copy of the string is stored in the
/// dictionary object.
///
/// If the incoming string value is already in the dictionary, the existing
/// entry is erased and a new entry is inserted.
/// If the specified position is already occupied, the existing entry is
/// erased and a new entry is inserted.  This is meant for user to update a
/// dictionary, however, it may cause two existing entries to be erased.
/// These erased enteries could invalidate dependent data structures such
/// indexes and .int files.
///
/// @warning Use this function only to build a new dictioinary.
uint32_t ibis::dictionary::insert(const char* str, uint32_t pos) {
    if (str == 0) return 0xFFFFFFFFU;
#ifdef FASTBIT_EMPTY_STRING_AS_NULL
    if (*str == 0) return 0xFFFFFFFFU;
#endif
#if FASTBIT_CASE_SENSITIVE_COMPARE+0 == 0
    for (char *ptr = const_cast<char*>(str); *ptr != 0; ++ ptr) {
        *ptr = toupper(*ptr);
    }
#endif
    if (pos < raw_.size() && raw_[pos] != 0) {
        if (strcmp(str, raw_[pos]) == 0) // same string value
            return pos;

        const char *old = raw_[pos];
        MYMAP::iterator jt = key_.find(old);
        raw_[jt->second] = 0;
        key_.erase(jt);
        raw_[pos] = 0;
        LOGGER(ibis::gVerbose > 0)
            << "dictionary::insert(" << str << ", " << pos
            << ") removed existing entry with code " << pos << " (" << old
            << ")";
    }
    if (key_.find(str) != key_.end()) {
        MYMAP::iterator jt = key_.find(str);
        if (jt->second != pos) {
            LOGGER(ibis::gVerbose > 0)
                << "dictionary::insert(" << str << ", " << pos
                << ") modifying the code for \"" << str << "\" from "
                << jt->second << " to " << pos;
            raw_[jt->second] = 0;
            raw_[pos] = jt->first;
            jt->second = pos;
        }
        return pos;
    }
    if (pos < raw_.size()) {
        if (raw_[pos] != 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- dictionary::insert(" << str << ", " << pos
                << ") found another string (" << raw_[pos]
                << ") at the position, try to find the next available position";
            for (pos = 0; pos < raw_.size(); ++ pos) {
                if (raw_[pos] == 0)
                    break;
            }
        }
        if (pos < raw_.size()) {
            char *copy = ibis::util::strnewdup(str);
            buffer_.push_back(copy);
            key_[copy] = pos;
            raw_[pos] = copy;
            return pos;
        }
    }
    if (pos == 0xFFFFFFFFU) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- dictionary::insert can not use code 0xFFFFFFFFU "
            "because it is reserved for NULL strings";
        return 0xFFFFFFFFU;
    }
    if (pos >= raw_.size()) {
        // need to expand raw_
        const uint32_t nk = raw_.size();
        if (raw_.capacity() < pos+1) {
            // attempt to double the storage to reduce the amountized cost
            raw_.reserve(nk+nk>pos+1?nk+nk:pos+1);
        }
        raw_.resize(pos+1);
        for (unsigned j = nk; j < pos; ++ j)
            raw_[j] = 0;
        char *copy = ibis::util::strnewdup(str);
        buffer_.push_back(copy);
        raw_[pos] = copy;
        key_[copy] = pos;
#if DEBUG+0 > 2 || _DEBUG+0 > 2
        LOGGER(ibis::gVerbose > 0)
            << "DEBUG -- dictionary::insert(" << raw_.back()
            << ") return a new code " << pos;
#endif
    }
    return pos;
} // ibis::dictionary::insert

/// Insert a string to the dictionary.  Returns the integer value assigned
/// to the string.  A copy of the string is stored internally.
uint32_t ibis::dictionary::insert(const char* str) {
    if (str == 0) return 0xFFFFFFFFU;
#ifdef FASTBIT_EMPTY_STRING_AS_NULL
    if (*str == 0) return 0xFFFFFFFFU;
#endif
#if FASTBIT_CASE_SENSITIVE_COMPARE+0 == 0
    for (char *ptr = const_cast<char*>(str); *ptr != 0; ++ ptr) {
        *ptr = toupper(*ptr);
    }
#endif
    MYMAP::const_iterator it = key_.find(str);
    if (it != key_.end()) {
#if DEBUG+0 > 2 || _DEBUG+0 > 2
        LOGGER(ibis::gVerbose > 0)
            << "DEBUG -- dictionary::insert(" << str
            << ") return an existing code " << it->second;
#endif
        return it->second;
    }
    else {
        // incoming string is a new entry
        const uint32_t nk = raw_.size();
        char *copy = ibis::util::strnewdup(str);
        buffer_.push_back(copy);
        raw_.push_back(copy);
        key_[copy] = nk;
#if DEBUG+0 > 2 || _DEBUG+0 > 2
        LOGGER(ibis::gVerbose > 0)
            << "DEBUG -- dictionary::insert(" << raw_.back()
            << ") return a new code " << nk;
#endif
        return nk;
    }
} // ibis::dictionary::insert

/// Append a string to the dictionary.  Returns the integer value assigned
/// to the string.  A copy of the string is stored internally.
///
/// This function assumes the incoming string is ordered after all known
/// strings to this dictionary object.  In other word, this function
/// expects the strings to be given in the sorted (ascending) order.  It
/// does not attempt to check that the incoming string is indeed ordered.
/// What this function relies on is that the incoming string is not a
/// repeat of any existing strings.
uint32_t ibis::dictionary::appendOrdered(const char* str) {
    if (str == 0) return 0;
#ifdef FASTBIT_EMPTY_STRING_AS_NULL
    if (*str == 0) return 0;
#endif
#if FASTBIT_CASE_SENSITIVE_COMPARE+0 == 0
    for (char *ptr = const_cast<char*>(str); *ptr != 0; ++ ptr) {
        *ptr = toupper(*ptr);
    }
#endif

    const uint32_t ind = key_.size();
    char *copy = ibis::util::strnewdup(str);
    buffer_.push_back(copy);
    raw_.push_back(copy);
    key_[copy] = ind;
    return ind;
} // ibis::dictionary::appendOrdered

/// Non-copying insertion.  Do not make a copy of the input string.
/// Transfers the ownership of @c str to the dictionary.  Caller needs to
/// check whether it is a new word in the dictionary.  If it is not a new
/// word in the dictionary, the dictionary does not take ownership of the
/// string argument.
uint32_t ibis::dictionary::insertRaw(char* str) {
    if (str == 0) return 0xFFFFFFFFU;
#ifdef FASTBIT_EMPTY_STRING_AS_NULL
    if (*str == 0) return 0xFFFFFFFFU;
#endif
#if FASTBIT_CASE_SENSITIVE_COMPARE+0 == 0
    for (char *ptr = const_cast<char*>(str); *ptr != 0; ++ ptr) {
        *ptr = toupper(*ptr);
    }
#endif
    MYMAP::const_iterator it = key_.find(str);
    if (it != key_.end()) {
        return it->second;
    }
    else {
        // incoming string is a new entry
        const uint32_t nk = raw_.size();
        buffer_.push_back(str);
        raw_.push_back(str);
        key_[str] = nk;
        return nk;
    }
} // ibis::dictionary::insertRaw

/// Reassign the integer values to the strings.  Upon successful completion
/// of this function, the integer values assigned to the strings will be in
/// ascending order.  In other word, string values that are lexigraphically
/// smaller will have smaller integer representations.
///
/// The argument to this function carrys the permutation information needed
/// to turn the previous integer assignments into the new ones.  If the
/// previous assignment was k, the new assignement will be o2n[k].  Note
/// that the name o2n is shorthand for old-to-new.
void ibis::dictionary::sort(ibis::array_t<uint32_t> &o2n) {
    const size_t nelm = raw_.size();
    ibis::array_t<uint32_t> n2o(nelm);
    for (size_t j = 0; j < nelm; ++ j)
        n2o[j] = j;
#if DEBUG+0 > 2 || _DEBUG+0 > 2
    {
        ibis::util::logger lg;
        lg() << "DEBUG -- dictionary::sort starts with\n\t";
        for (MYMAP::const_iterator it = key_.begin(); it != key_.end(); ++ it)
            lg() << '"' << it->first << '"' << "(" << it->second << ") ";
    }
#endif
    ibis::util::sortStrings(raw_, n2o);
    o2n.resize(nelm);
    for (size_t j = 0; j < nelm; ++ j)
        o2n[n2o[j]] = j;
    for (MYMAP::iterator it = key_.begin(); it != key_.end(); ++ it)
        it->second = o2n[it->second];
#if DEBUG+0 > 2 || _DEBUG+0 > 2
    {
        ibis::util::logger lg;
        lg() << "DEBUG -- dictionary::sort ends with";
        for (MYMAP::const_iterator it = key_.begin(); it != key_.end(); ++ it)
            lg() << "\n\t\"" << it->first << '"' << "(" << it->second << ": "
                 << raw_[it->second] << ") ";
        lg() << "\n\to2n(" << o2n.size() << "):";
        for (size_t j = 1; j < nelm; ++ j)
            lg() << "\n\to2n[" << j << "] = " << o2n[j] << ": " << raw_[o2n[j]];
    }
#endif
} // ibis::dictionary::sort

/// Merge the incoming dictionary with this one.  It produces a dictionary
/// that combines the words in both dictionaries.  Existing words in the
/// current dictionary will keep their current assignment.
///
/// Upon successful completion of this function, the return value will be
/// the new size of the dictionary.
int ibis::dictionary::merge(const ibis::dictionary& rhs) {
    const uint32_t nr = rhs.key_.size();
    if (nr == 0) {
        return key_.size();
    }

    for (size_t j = 1; j < rhs.raw_.size(); ++ j)
        (void) insert(rhs.raw_[j]);
    return key_.size();
} // ibis::dictionary::merge

/// Produce an array that maps the integers in old dictionary to the new
/// one.  The incoming dictionary represents the old dictionary, this
/// dictionary represents the new one.
///
/// Upon successful completion of this fuction, the array o2n will have
/// (old.size()+1) number of elements, where the new value for the old code
/// i is stored as o2n[i].
int ibis::dictionary::morph(const ibis::dictionary &old,
                            ibis::array_t<uint32_t> &o2n) const {
    const uint32_t nold = old.key_.size();
    const uint32_t nnew = key_.size();
    if (nold > nnew) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- dictionary::morph can not proceed because the "
            "new dictioanry is smaller than the old one";
        return -1;
    }

    o2n.resize(nold+1);
    o2n[0] = 0;
    if (nold == 0) return 0;

    for (uint32_t j0 = 1; j0 < old.raw_.size(); ++ j0)
        o2n[j0] = operator[](raw_[j0]);
    return nold;
} // ibis::dictioniary::morph

/// Merge all buffers into a single one.  New memory is allocated to store
/// the string values together if they are stored in different locations
/// currently.
///
/// @note Logically, this function does not change the content of the
/// dictionary, but it actually need to change a number of pointers.  The
/// implementation of the function uses the copy-swap idiom to take
/// advantage of the copy constructor.
void ibis::dictionary::mergeBuffers() const {
    if (buffer_.size() <= 1) return; // nothing to do

    try {
        ibis::dictionary tmp(*this); // consolidate buffers here
        const_cast<dictionary*>(this)->swap(tmp);
    }
    catch (...) {
        // it is fine to keep the original content
    }
} // ibis::dictionary::mergeBuffers


/// Hash function for C-style string values.  This is an adaptation of
/// MurMurHash3 that generates 32-bit hash values.
size_t std::hash<const char*>::operator()(const char* x) const {
    const int len = std::strlen(x);
    const int nblocks = len / 4;
    uint32_t h1 = 0;
    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;
    // bulk of the bytes
    const uint32_t * blocks = (const uint32_t *)(x + nblocks*4);
    for (int i = -nblocks; i; i++) {
        uint32_t k1 = blocks[i];

        k1 *= c1;
        k1 = FASTBIT_ROTL32(k1,15);
        k1 *= c2;

        h1 ^= k1;
        h1 = FASTBIT_ROTL32(h1,13);
        h1 = h1*5+0xe6546b64;
    }

    // tail
    const uint8_t * tail = (const uint8_t*)(x + nblocks*4);
    uint32_t k1 = 0;
    switch (len & 3) {
    case 3: k1 ^= tail[2] << 16;
    case 2: k1 ^= tail[1] << 8;
    case 1: k1 ^= tail[0];
        k1 *= c1;
        k1 = FASTBIT_ROTL32(k1,15);
        k1 *= c2;
        h1 ^= k1;
    };

    //----------
    // finalize
    h1 ^= len;
    h1 ^= h1 >> 16;
    h1 *= 0x85ebca6b;
    h1 ^= h1 >> 13;
    h1 *= 0xc2b2ae35;
    h1 ^= h1 >> 16;
    return h1;
} // std::hash<const char*>::operator()
