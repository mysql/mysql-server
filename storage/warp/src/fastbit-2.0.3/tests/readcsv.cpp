// $Id$
// Author: John Wu <John.Wu@NERSC.gov> Lawrence Berkeley National Laboratory
// Copyright 2002-2016 the Regents of the University of California
/**   Read Comma-Separated Values.

   Command line options:

   csv-file-name [output-dir-name]

   This program takes one mandatory argument (csv-file-name ) and one
   optional argument (output-dir-name).  The mandatory argument is the name
   of the file containing Comma-Separated Values.  It must be the first
   argument.  The second optional argument is the name of the directory to
   store the output files.  If the second argument is not provided, the
   output directory will be 'tmp'.  The output files includes a file named
   -part.txt that describes the content of other files in the directory,
   and a list data files named after the columns in the input file.  The
   content of these data files are raw binary values.  If the output
   directory already contains files, files with the same names will be
   overwritten.

   The input file is expected to have comma-separated values.  Each line is
   to have the same number of fields and fields are separated by comma.
   The first line of the file must be a list of strings that are used as
   the names of the columns.  This first line must be preceeded by '#' or
   '--' so that it would be treated as comments by other CSV readers.

   The values are outputed as raw binary into a set of files, one per
   column.  The type of the columns are determined as follows.

   - As each line is read into memory, the type of each column is
     determined.

   - Three types are recognized, int, double and string.  The type checking
     follows the order of int, double and string.

   - An int must contain only decimal digits with an optional leading +/-
     sign.

   - A double contain decimal digits and either a decimal point
     ('.') or one of 'e' or 'E' followed by an integer.

   - Any column that is not recognized as an int or a double is treated as
     a string.

   - Comment lines starting with '#' (shell style comment) or '--' (SQL
     style comment) may be included in the input CSV file as comments.
     These comment lines are skipped.

   *** The type must be determined by the above rules correctly in the
   *** first row of the values.  If the types are not determined correctly
   *** in the row, the content of the binary file may be incorrect!
*/
#include <stdio.h>	// fopen, fgets
#include <stdlib.h>	// exit
#include <string.h>	// strlen
#include <ctype.h>	// isdigit
#include <time.h>	// time
#include <unistd.h>	// chdir
#if defined(__CYGWIN__)
#include <sys/stat.h>	// mkdir
#elif defined(__HOS_AIX__)
#include <sys/stat.h>	// mkdir
#elif defined(_WIN32)
#include <direct.h>	// _mkdir, _chdir
#elif defined(__linux__) || defined(__unix__) || defined(__APPLE__) || defined(__FreeBSD__)
#include <sys/stat.h>	// mkdir
#include <sys/types.h>
#endif

#include <iostream>	// std::cout, std::cerr, std::endl
#include <vector>	// std::vector
#include <string>	// std::string
#include <limits>	// std::numeric_limits
#include <cmath>	// std::pow

enum DATA_TYPE {INT, DOUBLE, STRING};

struct column {
    std::string	name;	// name of the column
    DATA_TYPE 	type;	// data type
    double	lo, hi;	// the smallest and the largest value encountered
    FILE*	file;	// pointer to output file

    // construct a column from a name.
    column(std::string &n) :
        name(n), type(INT), lo(1.79769e308), hi(-1.79769e308), file(0) {};
};

typedef std::vector<column> cList;
cList columns;	// a global list of columns

// /// Determine the type of the @arg val.  Use only one loop through the string.
// DATA_TYPE determineType(const char* val) {
//     DATA_TYPE type = INT;
//     if (val == 0) return type;

//     while (*val != 0 && isspace(*val)) ++ val;
//     if (*val == 0) return type;

//     unsigned len = (isdigit(*val) ? 1 : 0); // input string length
//     if (val[3] == 0 && ((val[0]=='N' || val[0]=='n') &&
//                         (val[1]=='A' || val[1]=='a') &&
//                         (val[2]=='N' || val[2]=='n'))) {
//         return DOUBLE;
//     }
//     else if (val[7] == 0 && ((val[0]=='I' || val[0]=='i') &&
//                              (val[1]=='N' || val[1]=='n') &&
//                              (val[2]=='F' || val[2]=='f') &&
//                              (val[3]=='I' || val[3]=='i') &&
//                              (val[4]=='N' || val[4]=='n') &&
//                              (val[5]=='I' || val[5]=='i') &&
//                              (val[6]=='T' || val[6]=='t'))) {
//         return DOUBLE;
//     }
//     else if (val[8] == 0 && ((val[0]=='+' || val[0]=='-') &&
//                              (val[1]=='I' || val[0]=='i') &&
//                              (val[2]=='N' || val[1]=='n') &&
//                              (val[3]=='F' || val[2]=='f') &&
//                              (val[4]=='I' || val[3]=='i') &&
//                              (val[5]=='N' || val[4]=='n') &&
//                              (val[6]=='I' || val[5]=='i') &&
//                              (val[7]=='T' || val[6]=='t'))) {
//         return DOUBLE;
//     }
//     else if (len == 0 && *val != '-' && *val != '+' && *val != '.') {
// 	return STRING;
//     }

//     bool seenExp = false;
//     for (++ val; *val != 0; ++ val) {
// 	++ len;
// 	if (isdigit(*val) != 0) {
// 	    continue;
// 	}
// 	else if (type == INT && *val == '.') {
// 	    type = DOUBLE;
// 	}
// 	else if (seenExp == false && (*val == 'e' || *val == 'E')) {
// 	    type = DOUBLE;
// 	    seenExp = true;
// 	    ++ val; // need to check the next character
// 	    if (*val == 0) {
// 		type = STRING;
// 		break;
// 	    }
// 	    else if (isdigit(*val) == 0 && *val != '-' && *val != '+') {
// 		type = STRING;
// 		break;
// 	    }
// 	}
// 	else {
// 	    type = STRING;
// 	    break;
// 	}
//     }

//     if (type == INT) { // the string might be too long for an int
// 	if (len > 16)
//             // integers with more than 16 digits are treated as strings
// 	    type = STRING;
// 	else if (len > 9)
//             // integers with more than 9 digits are treated as double
// 	    type = DOUBLE;
//     }
//     return type;
// } // determineType

/// Attempt to convert the incoming string into an integer.
/// Return zero (0) if all characters in @arg str are decimal integers and
/// the content does not overflow.
int readInt(char *&str, int& val) {
    int tmp = 0;
    val = 0;
    if (str == 0 || *str == 0) return 0;
    for (; isspace(*str); ++ str); // skip leading space

    const bool neg = (*str == '-');
    if (*str == '-' || *str == '+') ++ str;
    while (*str != 0 && isdigit(*str) != 0) {
	tmp = 10 * val + (*str - '0');
	if (tmp > val) {
	    val = tmp;
	}
	else if (val > 0) {
	    return -1; // overflow
	}
	++ str;
    }
    if (neg) val = -val;
    for (; isspace(*str); ++ str);
    if (*str == 0 || *str == ',') return 0;
    else return 1;
} // readInt

/// Attempt to convert the incoming string into a double.
int readDouble(char *&str, double& val) {
    val = 0;
    if (str == 0 || *str == 0) return 0;
    for (; isspace(*str); ++ str); // skip leading space
    if (str[3] == 0 && ((str[0]=='N' || str[0]=='n') &&
                        (str[1]=='A' || str[1]=='a') &&
                        (str[2]=='N' || str[2]=='n'))) {
        val = std::numeric_limits<double>::quiet_NaN();
        str += 4;
        return 0;
    }
    else if (str[7] == 0 && ((str[0]=='I' || str[0]=='i') &&
                             (str[1]=='N' || str[1]=='n') &&
                             (str[2]=='F' || str[2]=='f') &&
                             (str[3]=='I' || str[3]=='i') &&
                             (str[4]=='N' || str[4]=='n') &&
                             (str[5]=='I' || str[5]=='i') &&
                             (str[6]=='T' || str[6]=='t'))) {
        val = std::numeric_limits<double>::infinity();
        str += 8;
        return 0;
    }
    else if (str[8] == 0 && ((str[0]=='+' || str[0]=='-') &&
                             (str[1]=='I' || str[0]=='i') &&
                             (str[2]=='N' || str[1]=='n') &&
                             (str[3]=='F' || str[2]=='f') &&
                             (str[4]=='I' || str[3]=='i') &&
                             (str[5]=='N' || str[4]=='n') &&
                             (str[6]=='I' || str[5]=='i') &&
                             (str[7]=='T' || str[6]=='t'))) {
        val = (str[0]=='+' ? std::numeric_limits<double>::infinity() :
               -std::numeric_limits<double>::infinity());
        str += 9;
        return 0;
    }

    double tmp;
    const char *s0 = str;
    const bool neg = (*str == '-');
    const unsigned width =  16;
    if (*str == '-' || *str == '+') ++ str;
    // limit the number of digits in the integer portion to 16
    while (*str != 0 && str <= s0+width && isdigit(*str) != 0) {
	val = 10 * val + static_cast<double>(*str - '0');
	++ str;
    }

    if (*str == '.') { // values after the decimal point
	tmp = 0.1;
	// constrain the effective accuracy ??? (str-s0) < width+1 &&
	for (++ str; isdigit(*str); ++ str) {
	    val += tmp * static_cast<double>(*str - '0');
	    tmp *= 0.1;
	}
    }

    if (*str == 'e' || *str == 'E') {
	++ str;
	int ex, ierr;
	ierr = readInt(str, ex);
	if (ierr == 0) {
	    val *= std::pow(1e1, static_cast<double>(ex));
	}
	else {
	    return ierr;
	}
    }

    for (; isspace(*str); ++ str);
    if (*str == 0 || *str == ',') {
	if (neg) val = - val;
	return 0;
    }
    else { // incorrect format
	return 2;
    }
} // readDouble

// Attempt to extract a string from the input pointer.  A string can be
// either double quoted, single quoted or unquoted.  A unquoted string is
// assumed tobe surrended by while space as defined in isspace.
int readString(char *&tmp, std::string &str) {
    str.clear();

    // skip the leading space
    while (isspace(*tmp)) ++ tmp;

    if (*tmp == '"') { // double quoted string
	++ tmp;
	while (*tmp != 0 && (*tmp != '"' ||
			     (*tmp == '"' && tmp[-1] == '\\'))) {
	    if (*tmp != '"')
		str += *tmp;
	    else
		str[str.size()-1] = '"';
	    ++ tmp;
	}
	++ tmp; // skip over the end quote
    }
    else if (*tmp == '\'') { // single quoted string
	++ tmp;
	while (*tmp != 0 && (*tmp != '\'' ||
			     (*tmp == '\'' && tmp[-1] == '\\'))) {
	    if (*tmp != '\'')
		str += *tmp;
	    else
		str[str.size()-1] = '\'';
	    ++ tmp;
	}
	++ tmp; // skip over the end quote
    }
    else { // read till the next comma or end of string
	while (*tmp != 0 && *tmp != ',') {
	    str += *tmp;
	    ++ tmp;
	}
	if (str.size() > 0) { // remove trailing space
	    unsigned len = str.size();
	    for (; len > 0 && isspace(str[len-1]); -- len);
	    str.erase(len);
	}
    }
    return 0;
} // readString

// Read a line of the input CSV file.
int readALine(FILE *fptr, char*& buf, unsigned& lbuf, bool skipcomment=true) {
    long start = ftell(fptr); // start position
    int retry;
    if (buf == 0) { // allocate a buffer of 1024 characters
	buf = new char[1024];
	lbuf = 1024;
    }
    else if (lbuf < 128) { // buffer too small, enlarge to 1024
	delete [] buf;
	buf = new char[1024];
	lbuf = 1024;
    }

    do {
	char *tmp = fgets(buf, lbuf, fptr);
	if (feof(fptr)) {
	    buf[0] = 0;
	    return -1;
	}
	if (tmp != buf) {
	    std::cerr << "readALine failed to read the next line at position "
		      << start << "\n" << std::endl;
	    buf[0] = 0;
	    return -2;
	}
	if (ferror(fptr) != 0) {
	    std::cerr << "readALine encountered an error while reading "
		"the next line at position "
		      << start << "\n" << std::endl;
	    buf[0] = 0;
	    return -3;
	}

        retry = 0;
	buf[lbuf-1] = 0;
	unsigned int len = strlen(buf);
	if (len+1 < lbuf) {
	    buf[len-1] = 0;
	    if (skipcomment && (buf[0] == '#' ||
				(buf[0] == '-' && buf[1] == '-'))) {
		// comment line, try the next line
		retry = 1;
	    }
	    else {
		return len;
	    }
	}
	else if (buf[len-1] != '\n' && feof(fptr) == 0 && ferror(fptr) == 0) {
	    // did not read the whole line, double the buffer space
	    lbuf <<= 1;
	    delete [] buf;
	    buf = new char[lbuf];
	    if (buf == 0) {
		std::cerr << "readALine failed to allocate a buf of char["
			  << lbuf << "], can not continue" << std::endl;
		return -4;
	    }
	    fseek(fptr, start, SEEK_SET);
	    retry = 2;
	}
	else { // got a line of text in buf
	    while (len > 1 && isspace(buf[len-1]) != 0) {
                buf[len-1] = 0;
                -- len;
            }
            if (len > 0)
                return len;
            else
                retry = 3;
	}
    } while (retry > 0);
    return 0;
} // readALine

// read the first line of the input file to retrieve the names of the
// columns.
void readColumnNames(FILE *fptr, char*& buf, unsigned& lbuf) {
    int ierr = readALine(fptr, buf, lbuf, false);
    if (ierr <= 0) return;

    char* tmp = buf;
    tmp += strspn(tmp, "#- \t"); // skip leading space and comment characters
    columns.clear();	// clear the list of columns
    while (*tmp != 0) {
	std::string str;
	(void) readString(tmp, str);
	for (; isspace(*tmp); ++ tmp); // skip trailing space
	if (*tmp == ',') ++ tmp; // skip delimiter
	if (! str.empty()) {// record string
	    columns.push_back(str);

	    // change the type of IDs to STRING
	    if (str.size()>2 && str[str.size()-1] == 'D' &&
		str[str.size()-2] == 'I')
		columns.back().type = STRING;
	}
    } // while (*tmp != 0)
} // readColumnNames

// read a line from the input file and write out the values 
int readValues(FILE *fptr, char*&buf, unsigned& lbuf) {
    int ierr = readALine(fptr, buf, lbuf);
    if (ierr <= 0) return ierr;

    ierr = 0;
    char *tmp = buf;
    for (unsigned i = 0; i < columns.size(); ++ i) {
	switch (columns[i].type) {
	default:
	case INT:
	    if (*tmp != 0) {
		int vint;
		char *s0 = tmp;
		int jerr = readInt(tmp, vint);
		if (jerr == 0) {
		    if (vint < columns[i].lo)
			columns[i].lo = vint;
		    if (vint > columns[i].hi)
			columns[i].hi = vint;
		    (void) fwrite(&vint, sizeof(vint), 1, columns[i].file);
		}
		else {
		    ++ ierr;
		    tmp = s0;
		    double dbl;
		    jerr = readDouble(tmp, dbl);
		    if (jerr == 0) {
			columns[i].type = DOUBLE;
			if (dbl < columns[i].lo)
			    columns[i].lo = dbl;
			if (dbl > columns[i].hi)
			    columns[i].hi = dbl;
			(void) fwrite(&dbl, sizeof(dbl), 1, columns[i].file);
		    }
		    else {
			tmp = s0;
			std::string str;
			columns[i].type = STRING;
			jerr = readString(tmp, str);
			(void) fwrite(str.c_str(), 1, str.size()+1,
				      columns[i].file);
		    }
		}
	    }
	    else { // default int value is zero
		int vint = 0;
		fwrite(&vint, sizeof(vint), 1, columns[i].file);
	    }
	    break;
	case DOUBLE:
	    if (*tmp != 0) {
		double dbl;
		char *s0 = tmp;
		int jerr = readDouble(tmp, dbl);
		if (jerr == 0) {
		    if (dbl < columns[i].lo)
			columns[i].lo = dbl;
		    if (dbl > columns[i].hi)
			columns[i].hi = dbl;
		    (void) fwrite(&dbl, sizeof(dbl), 1, columns[i].file);
		}
		else {
		    ++ ierr;
		    tmp = s0;
		    std::string str;
		    columns[i].type = STRING;
		    jerr = readString(tmp, str);
		    (void) fwrite(str.c_str(), 1, str.size()+1,
				  columns[i].file);
		}
	    }
	    else { // default double value is zero
		double dbl = 0;
		(void) fwrite(&dbl, sizeof(dbl), 1, columns[i].file);
	    }
	    break;
	case STRING:
	    {
		std::string str;
		readString(tmp, str);
		(void) fwrite(str.c_str(), 1, str.size()+1, columns[i].file);
	    }
	    break;
	}

	if (isspace(*tmp)) { // skip trailing space
	    for (++ tmp; isspace(*tmp); ++ tmp);
	}
	if (*tmp == ',') // skip delimitor
	    ++ tmp;
    } // for (unsigned i = 0;...

#ifdef DEBUG
    std::clog << "DEBUG -- readValues parsed: " << buf << std::endl;
#endif
    return ierr;
} // readValues

// the main of readcsv.cpp
int main(int argc, char** argv) {
    if (argc < 2) {
	std::cerr << *argv << " must be followed by the name of the "
	    "CSV file\n"
		  << "An optional second argument can specify the "
	    "destination of output files.\n" << std::endl;
	return -3;
    }

    // open the input file
    FILE *fptr = fopen(argv[1], "r");
    if (fptr == 0) {
	std::cerr << *argv << " failed to open file " << argv[1]
		  << " for reading\n" << std::endl;
	return -4;
    }

    // read the first line of the CSV file to extract column names
    unsigned lbuf = 10240;
    char *buf = new char[lbuf];
    readColumnNames(fptr, buf, lbuf);
    if (columns.empty()) {
	std::cerr << *argv << ": the first line of file " << argv[1]
		  << " does not contain any strings\n" << std::endl;
	fclose(fptr);
	return -5;
    }
    std::cout << "File " << argv[1] << " contains " << columns.size()
	      << " columns." << std::endl;

    // generate output file pointers
    int ierr = 0;
    const char *dest = (argc>2 ? argv[2] : "tmp");
#if defined(__CYGWIN__) || defined(__linux__) || defined(__unix__) || defined(__HOS_AIX__) || defined(__APPLE__) || defined(__FreeBSD__)
    mkdir(dest, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP);
    ierr = chdir(dest);
#elif defined(_WIN32)
    _mkdir(dest);
    ierr = _chdir(dest);
#else
    mkdir(dest);
    ierr = chdir(dest);
#endif
    if (ierr != 0) {
	std::cerr << *argv << ": failed to change to directory " << dest
		  << std::endl;
	return -6;
    }
    for (unsigned i = 0; i < columns.size(); ++ i) {
	columns[i].file = fopen(columns[i].name.c_str(), "wb");
	if (columns[i].file == 0) {
	    std::cerr << *argv << " unable to open output file "
		      << columns[i].name << " in directory " << dest
		      << std::endl;
	    ++ ierr;
	}
    }
    if (ierr != 0) {
	std::cerr << *argv << ": failed to open some output files. "
		  << "Make sure directory " << dest << " is accessible\n"
		  << std::endl;
	fclose(fptr);
	for (unsigned i = 0; i < columns.size(); ++ i)
	    if (columns[i].file != 0)
		fclose(columns[i].file);
	return -7;
    }

    // reading the bulk of the data
    unsigned long cnt = 0; // number of rows read
    const long endline1 = ftell(fptr); // end of the first line
    do {
	ierr = readValues(fptr, buf, lbuf);
	if (ierr > 0) {
	    if (cnt > 0) { // need to rewind
		cnt = 0;
		fseek(fptr, endline1, SEEK_SET);
		for (cList::iterator it = columns.begin();
		     it != columns.end();
		     ++ it)
		    rewind((*it).file);
	    }
	    else {
		cnt = 1;
	    }
	    ierr = 0;
	}
	else if (ierr == 0) {
	    ++ cnt;
	    if ((cnt % 10000) == 0)
		std::cout << "... " << cnt << "\n";
	}
    } while (ierr == 0);

    if (ierr < -1)
	std::cerr << *argv << " encountered an error (" << ierr
		  << ") after reading " << cnt << " rows from file "
		  << argv[1] << std::endl;
    else
	std::cout << *argv << " successfully read " << cnt
		  << " rows from file " << argv[1] << std::endl;
    fclose(fptr);
    for (cList::iterator it = columns.begin();
	 it != columns.end();
	 ++ it)
	fclose((*it).file);
    if (cnt > 0) {
	// write out the -part.txt file
	fptr = fopen("-part.txt", "w");
	if (fptr == 0) {
	    std::cerr << *argv << " unable to open file -part.txt in "
		      << dest << "\n" << std::endl;
	    return -8;
	}
	std::string dname = argv[1]; // data set name
	unsigned len = dname.rfind('/');
	if (len < dname.size())
	    dname.erase(0, len+1);
	len = dname.find('.');
	if (len < dname.size())
	    dname.erase(len);
	fprintf(fptr,
		"BEGIN HEADER\nDataSet.Name = \"%s\"\n"
		"DataSet.Description = \"%s %s\"\n"
		"Number_of_columns = %lu\n"
		"Number_of_rows = %lu\n"
		"Timestamp = %lu\nEND HEADER\n",
		dname.c_str(), argv[0], argv[1], columns.size(), cnt,
		static_cast<long unsigned int>(time(0)));
	for (cList::iterator it = columns.begin();
	     it != columns.end();
	     ++ it) {
	    switch ((*it).type) {
	    default:
	    case STRING: // ibis::CATEGORY
		fprintf(fptr,
			"\nBegin Column\nname = \"%s\"\n"
			"data_type = \"category\"\nEnd Column\n",
			(*it).name.c_str());
		break;
	    case DOUBLE: // ibis::DOUBLE
		if ((*it).lo <= (*it).hi)
		    fprintf(fptr,
			    "\nBegin Column\nname = \"%s\"\n"
			    "data_type = \"double\"\nminimum = %.15g\n"
			    "maximum = %.15g\nEnd Column\n",
			    (*it).name.c_str(), (*it).lo, (*it).hi);
		else
		    fprintf(fptr,
			    "\nBegin Column\nname = \"%s\"\n"
			    "data_type = \"double\"\nEnd Column\n",
			    (*it).name.c_str());
		break;
	    case INT: // ibis::INT
		if ((*it).lo <= (*it).hi)
		    fprintf(fptr,
			    "\nBegin Column\nname = \"%s\"\n"
			    "data_type = \"int\"\nminimum = %ld\n"
			    "maximum = %ld\nEnd Column\n",
			    (*it).name.c_str(),
			    static_cast<long>((*it).lo),
			    static_cast<long>((*it).hi));
		else
		    fprintf(fptr,
			    "\nBegin Column\nname = \"%s\"\n"
			    "data_type = \"int\"\nEnd Column\n",
			    (*it).name.c_str());
		break;
	    }
	}
	fclose(fptr);
	std::cout << *argv << " outputed " << cnt << " rows to directory "
		  << dest << std::endl;
    }
    return 0;
} // main
