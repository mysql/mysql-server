#! /usr/bin/python

# Copyright (C) 2016 and later: Unicode, Inc. and others.
# License & terms of use: http://www.unicode.org/copyright.html

# Copyright (C) 2009-2011, International Business Machines Corporation, Google and Others.
# All rights reserved.

#
#  Script to check and fix svn property settings for ICU source files.
#  Also check for the correct line endings on files with svn:eol-style = native
#
#  THIS SCRIPT DOES NOT WORK ON WINDOWS
#     It only works correctly on platforms where the native line ending is a plain \n
#
#  usage:
#     icu-svnprops-check.py  [options]
#
#  options:
#     -f | --fix     Fix any problems that are found
#     -h | --help    Print a usage line and exit.
#
#  The tool operates recursively on the directory from which it is run.
#  Only files from the svn repository are checked.
#  No changes are made to the repository; only the working copy will be altered.

import sys
import os
import os.path
import re
import getopt


# file_types:  The parsed form of the svn auto-props specification.
#              A list of file types - .cc, .cpp, .txt, etc.
#              each element is a [type, proplist]
#              "type" is a regular expression string that will match a file name
#              prop list is another list, one element per property.
#              Each property item is a two element list, [prop name, prop value]
file_types = list()

def parse_auto_props():
    aprops = svn_auto_props.splitlines()
    for propline in aprops:
        if re.match("\s*(#.*)?$", propline):         # Match comment and blank lines
            continue
        if re.match("\s*\[auto-props\]", propline):  # Match the [auto-props] line.
            continue
        if not re.match("\s*[^\s]+\s*=", propline):  # minimal syntax check for <file-type> =
            print "Bad line from autoprops definitions: " + propline
            continue
        file_type, string_proplist = propline.split("=", 1)

        #transform the file type expression from autoprops into a normal regular expression.
        #  e.g.  "*.cpp"  ==>  ".*\.cpp$"
        file_type = file_type.strip()
        file_type = file_type.replace(".", "\.")
        file_type = file_type.replace("*", ".*")
        file_type = file_type + "$"

        # example string_proplist at this point: " svn:eol-style=native;svn:executable"
        # split on ';' into a list of properties.  The negative lookahead and lookbehind
        # in the split regexp are to prevent matching on ';;', which is an escaped ';'
        # within a property value.
        string_proplist = re.split("(?<!;);(?!;)", string_proplist)
        proplist = list()
        for prop in string_proplist:
            if prop.find("=") >= 0:
                prop_name, prop_val = prop.split("=", 1)
            else:
                # properties with no explicit value, e.g. svn:executable
                prop_name, prop_val = prop, ""
            prop_name = prop_name.strip()
            prop_val = prop_val.strip()
            # unescape any ";;" in a property value, e.g. the mime-type from
            #    *.java = svn:eol-style=native;svn:mime-type=text/plain;;charset=utf-8
            prop_val = prop_val.replace(";;", ";");
            proplist.append((prop_name, prop_val))

        file_types.append((file_type, proplist))
    # print file_types

        
def runCommand(cmd):
    output_file = os.popen(cmd);
    output_text = output_file.read();
    exit_status = output_file.close();
    if exit_status:
        print >>sys.stderr, '"', cmd, '" failed.  Exiting.'
        sys.exit(exit_status)
    return output_text

svn_auto_props = runCommand("svn propget svn:auto-props http://source.icu-project.org/repos/icu")

def usage():
    print "usage: " + sys.argv[0] + " [-f | --fix] [-h | --help]"

    
#
#  UTF-8 file check.   For text files with svn:mime-type=text/anything, check the specified charset
#    file_name:        name of a text file.
#    base_mime_type:   svn:mime-type property from the auto-props settings for this file type.
#    actual_mime_type: existing svn:mime-type property value for the file.
#    return:           The correct svn:mime-type property value,
#                         either the original, if it looks OK, otherwise the value from auto-props
#
def check_utf8(file_name, base_mime_type, actual_mime_type):

    f = open(file_name, 'r')
    bytes = f.read()
    f.close()
    file_is_utf8 = True
    try:
        bytes.decode("UTF-8")
    except UnicodeDecodeError:
        file_is_utf8 = False

    if not file_is_utf8 and actual_mime_type.find("utf-8") >= 0:
        print "Error: %s is not valid utf-8, but has a utf-8 mime type." % file_name
        return actual_mime_type

    if file_is_utf8 and actual_mime_type.find("charset") >=0 and actual_mime_type.find("utf-8") < 0:
        print "Warning: %s is valid utf-8, but has a mime-type of %s." % (file_name, actual_mime_type)

    if ord(bytes[0]) == 0xef:
        if not file_name.endswith(".txt"):
            print "Warning: file %s contains a UTF-8 BOM: " % file_name

    # If the file already has a charset in its mime-type, don't make any change.

    if actual_mime_type.find("charset=") >= 0:
        return actual_mime_type;

    return base_mime_type


def main(argv):
    fix_problems = False;
    try:
        opts, args = getopt.getopt(argv, "fh", ("fix", "help"))
    except getopt.GetoptError:
        print "unrecognized option: " + argv[0]
        usage()
        sys.exit(2)
    for opt, arg in opts:
        if opt in ("-h", "--help"):
            usage()
            sys.exit()
        if opt in ("-f", "--fix"):
            fix_problems = True
    if args:
        print "unexpected command line argument"
        usage()
        sys.exit()

    parse_auto_props()
    output = runCommand("svn ls -R ");
    file_list = output.splitlines()

    for f in file_list:
        if os.path.isdir(f):
            # print "Skipping dir " + f
            continue
        if not os.path.isfile(f):
            print "Repository file not in working copy: " + f
            continue;

        for file_pattern, props in file_types:
            if re.match(file_pattern, f):
                # print "doing " + f
                for propname, propval in props:
                    actual_propval = runCommand("svn propget --strict " + propname + " " + f)
                    #print propname + ": " + actual_propval
                    if propname == "svn:mime-type" and propval.find("text/") == 0:
                        # check for UTF-8 text files, should have svn:mime-type=text/something; charset=utf8
                        propval = check_utf8(f, propval, actual_propval)
                    if not (propval == actual_propval or (propval == "" and actual_propval == "*")):
                        print "svn propset %s '%s' %s" % (propname, propval, f)
                        if fix_problems:
                            os.system("svn propset %s '%s' %s" % (propname, propval, f))


if __name__ == "__main__":
    main(sys.argv[1:])
