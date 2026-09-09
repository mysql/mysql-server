#!/usr/bin/env python
#
# Copyright (c) 2005-2007 Niels Provos <provos@citi.umich.edu>
# Copyright (c) 2007-2012 Niels Provos and Nick Mathewson
# All rights reserved.
#
# Generates marshaling code based on libevent.

# pylint: disable=too-many-lines
# pylint: disable=too-many-branches
# pylint: disable=too-many-public-methods
# pylint: disable=too-many-statements
# pylint: disable=global-statement

# TODO:
# 1) propagate the arguments/options parsed by argparse down to the
#    instantiated factory objects.
# 2) move the globals into a class that manages execution, including the
#    progress outputs that go to stderr at the moment.
# 3) emit other languages.

import argparse
import re
import sys

_NAME = "event_rpcgen.py"
_VERSION = "0.1"

# Globals
LINE_COUNT = 0

CPPCOMMENT_RE = re.compile(r"\/\/.*$")
NONIDENT_RE = re.compile(r"\W")
PREPROCESSOR_DEF_RE = re.compile(r"^#define")
STRUCT_REF_RE = re.compile(r"^struct\[(?P<name>[a-zA-Z_][a-zA-Z0-9_]*)\]$")
STRUCT_DEF_RE = re.compile(r"^struct +[a-zA-Z_][a-zA-Z0-9_]* *{$")
WHITESPACE_RE = re.compile(r"\s+")

HEADER_DIRECT = []
CPP_DIRECT = []

QUIETLY = False


def declare(s):
    if not QUIETLY:
        print(s)


def TranslateList(mylist, mydict):
    return [x % mydict for x in mylist]


class RpcGenError(Exception):
    """An Exception class for parse errors."""

    def __init__(self, why): # pylint: disable=super-init-not-called
        self.why = why

    def __str__(self):
        return str(self.why)


# Holds everything that makes a struct
class Struct(object):
    def __init__(self, name):
        self._name = name
        self._entries = []
        self._tags = {}
        declare("  Created struct: %s" % name)

    def AddEntry(self, entry):
        if entry.Tag() in self._tags:
            raise RpcGenError(
                'Entry "%s" duplicates tag number %d from "%s" '
                "around line %d"
                % (entry.Name(), entry.Tag(), self._tags[entry.Tag()], LINE_COUNT)
            )
        self._entries.append(entry)
        self._tags[entry.Tag()] = entry.Name()
        declare("    Added entry: %s" % entry.Name())

    def Name(self):
        return self._name

    def EntryTagName(self, entry):
        """Creates the name inside an enumeration for distinguishing data
        types."""
        name = "%s_%s" % (self._name, entry.Name())
        return name.upper()

    @staticmethod
    def PrintIndented(filep, ident, code):
        """Takes an array, add indentation to each entry and prints it."""
        for entry in code:
            filep.write("%s%s\n" % (ident, entry))


class StructCCode(Struct):
    """ Knows how to generate C code for a struct """

    def __init__(self, name):
        Struct.__init__(self, name)

    def PrintTags(self, filep):
        """Prints the tag definitions for a structure."""
        filep.write("/* Tag definition for %s */\n" % self._name)
        filep.write("enum %s_ {\n" % self._name.lower())
        for entry in self._entries:
            filep.write("  %s=%d,\n" % (self.EntryTagName(entry), entry.Tag()))
        filep.write("  %s_MAX_TAGS\n" % (self._name.upper()))
        filep.write("};\n\n")

    def PrintForwardDeclaration(self, filep):
        filep.write("struct %s;\n" % self._name)

    def PrintDeclaration(self, filep):
        filep.write("/* Structure declaration for %s */\n" % self._name)
        filep.write("struct %s_access_ {\n" % self._name)
        for entry in self._entries:
            dcl = entry.AssignDeclaration("(*%s_assign)" % entry.Name())
            dcl.extend(entry.GetDeclaration("(*%s_get)" % entry.Name()))
            if entry.Array():
                dcl.extend(entry.AddDeclaration("(*%s_add)" % entry.Name()))
            self.PrintIndented(filep, "  ", dcl)
        filep.write("};\n\n")

        filep.write("struct %s {\n" % self._name)
        filep.write("  struct %s_access_ *base;\n\n" % self._name)
        for entry in self._entries:
            dcl = entry.Declaration()
            self.PrintIndented(filep, "  ", dcl)
        filep.write("\n")
        for entry in self._entries:
            filep.write("  ev_uint8_t %s_set;\n" % entry.Name())
        filep.write("};\n\n")

        filep.write(
            """struct %(name)s *%(name)s_new(void);
struct %(name)s *%(name)s_new_with_arg(void *);
void %(name)s_free(struct %(name)s *);
void %(name)s_clear(struct %(name)s *);
void %(name)s_marshal(struct evbuffer *, const struct %(name)s *);
int %(name)s_unmarshal(struct %(name)s *, struct evbuffer *);
int %(name)s_complete(struct %(name)s *);
void evtag_marshal_%(name)s(struct evbuffer *, ev_uint32_t,
    const struct %(name)s *);
int evtag_unmarshal_%(name)s(struct evbuffer *, ev_uint32_t,
    struct %(name)s *);\n"""
            % {"name": self._name}
        )

        # Write a setting function of every variable
        for entry in self._entries:
            self.PrintIndented(
                filep, "", entry.AssignDeclaration(entry.AssignFuncName())
            )
            self.PrintIndented(filep, "", entry.GetDeclaration(entry.GetFuncName()))
            if entry.Array():
                self.PrintIndented(filep, "", entry.AddDeclaration(entry.AddFuncName()))

        filep.write("/* --- %s done --- */\n\n" % self._name)

    def PrintCode(self, filep):
        filep.write(
            """/*
 * Implementation of %s
 */
"""
            % (self._name)
        )

        filep.write(
            """
static struct %(name)s_access_ %(name)s_base__ = {
"""
            % {"name": self._name}
        )
        for entry in self._entries:
            self.PrintIndented(filep, "  ", entry.CodeBase())
        filep.write("};\n\n")

        # Creation
        filep.write(
            """struct %(name)s *
%(name)s_new(void)
{
  return %(name)s_new_with_arg(NULL);
}

struct %(name)s *
%(name)s_new_with_arg(void *unused)
{
  struct %(name)s *tmp;
  if ((tmp = malloc(sizeof(struct %(name)s))) == NULL) {
    event_warn("%%s: malloc", __func__);
    return (NULL);
  }
  tmp->base = &%(name)s_base__;

"""
            % {"name": self._name}
        )

        for entry in self._entries:
            self.PrintIndented(filep, "  ", entry.CodeInitialize("tmp"))
            filep.write("  tmp->%s_set = 0;\n\n" % entry.Name())

        filep.write(
            """  return (tmp);
}

"""
        )

        # Adding
        for entry in self._entries:
            if entry.Array():
                self.PrintIndented(filep, "", entry.CodeAdd())
            filep.write("\n")

        # Assigning
        for entry in self._entries:
            self.PrintIndented(filep, "", entry.CodeAssign())
            filep.write("\n")

        # Getting
        for entry in self._entries:
            self.PrintIndented(filep, "", entry.CodeGet())
            filep.write("\n")

        # Clearing
        filep.write(
            """void
%(name)s_clear(struct %(name)s *tmp)
{
"""
            % {"name": self._name}
        )
        for entry in self._entries:
            self.PrintIndented(filep, "  ", entry.CodeClear("tmp"))

        filep.write("}\n\n")

        # Freeing
        filep.write(
            """void
%(name)s_free(struct %(name)s *tmp)
{
"""
            % {"name": self._name}
        )

        for entry in self._entries:
            self.PrintIndented(filep, "  ", entry.CodeFree("tmp"))

        filep.write(
            """  free(tmp);
}

"""
        )

        # Marshaling
        filep.write(
            """void
%(name)s_marshal(struct evbuffer *evbuf, const struct %(name)s *tmp) {
"""
            % {"name": self._name}
        )
        for entry in self._entries:
            indent = "  "
            # Optional entries do not have to be set
            if entry.Optional():
                indent += "  "
                filep.write("  if (tmp->%s_set) {\n" % entry.Name())
            self.PrintIndented(
                filep,
                indent,
                entry.CodeMarshal(
                    "evbuf",
                    self.EntryTagName(entry),
                    entry.GetVarName("tmp"),
                    entry.GetVarLen("tmp"),
                ),
            )
            if entry.Optional():
                filep.write("  }\n")

        filep.write("}\n\n")

        # Unmarshaling
        filep.write(
            """int
%(name)s_unmarshal(struct %(name)s *tmp, struct evbuffer *evbuf)
{
  ev_uint32_t tag;
  while (evbuffer_get_length(evbuf) > 0) {
    if (evtag_peek(evbuf, &tag) == -1)
      return (-1);
    switch (tag) {

"""
            % {"name": self._name}
        )
        for entry in self._entries:
            filep.write("      case %s:\n" % (self.EntryTagName(entry)))
            if not entry.Array():
                filep.write(
                    """        if (tmp->%s_set)
          return (-1);
"""
                    % (entry.Name())
                )

            self.PrintIndented(
                filep,
                "        ",
                entry.CodeUnmarshal(
                    "evbuf",
                    self.EntryTagName(entry),
                    entry.GetVarName("tmp"),
                    entry.GetVarLen("tmp"),
                ),
            )

            filep.write(
                """        tmp->%s_set = 1;
        break;
"""
                % (entry.Name())
            )
        filep.write(
            """      default:
        return -1;
    }
  }

"""
        )
        # Check if it was decoded completely
        filep.write(
            """  if (%(name)s_complete(tmp) == -1)
    return (-1);
  return (0);
}
"""
            % {"name": self._name}
        )

        # Checking if a structure has all the required data
        filep.write(
            """
int
%(name)s_complete(struct %(name)s *msg)
{
"""
            % {"name": self._name}
        )
        for entry in self._entries:
            if not entry.Optional():
                code = [
                    """if (!msg->%(name)s_set)
    return (-1);"""
                ]
                code = TranslateList(code, entry.GetTranslation())
                self.PrintIndented(filep, "  ", code)

            self.PrintIndented(
                filep, "  ", entry.CodeComplete("msg", entry.GetVarName("msg"))
            )
        filep.write(
            """  return (0);
}
"""
        )

        # Complete message unmarshaling
        filep.write(
            """
int
evtag_unmarshal_%(name)s(struct evbuffer *evbuf, ev_uint32_t need_tag,
  struct %(name)s *msg)
{
  ev_uint32_t tag;
  int res = -1;

  struct evbuffer *tmp = evbuffer_new();

  if (evtag_unmarshal(evbuf, &tag, tmp) == -1 || tag != need_tag)
    goto error;

  if (%(name)s_unmarshal(msg, tmp) == -1)
    goto error;

  res = 0;

 error:
  evbuffer_free(tmp);
  return (res);
}
"""
            % {"name": self._name}
        )

        # Complete message marshaling
        filep.write(
            """
void
evtag_marshal_%(name)s(struct evbuffer *evbuf, ev_uint32_t tag,
    const struct %(name)s *msg)
{
  struct evbuffer *buf_ = evbuffer_new();
  assert(buf_ != NULL);
  %(name)s_marshal(buf_, msg);
  evtag_marshal_buffer(evbuf, tag, buf_);
  evbuffer_free(buf_);
}

"""
            % {"name": self._name}
        )


class Entry(object):
    def __init__(self, ent_type, name, tag):
        self._type = ent_type
        self._name = name
        self._tag = int(tag)
        self._ctype = ent_type
        self._optional = False
        self._can_be_array = False
        self._array = False
        self._line_count = -1
        self._struct = None
        self._refname = None

        self._optpointer = True
        self._optaddarg = True

    @staticmethod
    def GetInitializer():
        raise NotImplementedError("Entry does not provide an initializer")

    def SetStruct(self, struct):
        self._struct = struct

    def LineCount(self):
        assert self._line_count != -1
        return self._line_count

    def SetLineCount(self, number):
        self._line_count = number

    def Array(self):
        return self._array

    def Optional(self):
        return self._optional

    def Tag(self):
        return self._tag

    def Name(self):
        return self._name

    def Type(self):
        return self._type

    def MakeArray(self):
        self._array = True

    def MakeOptional(self):
        self._optional = True

    def Verify(self):
        if self.Array() and not self._can_be_array:
            raise RpcGenError(
                'Entry "%s" cannot be created as an array '
                "around line %d" % (self._name, self.LineCount())
            )
        if not self._struct:
            raise RpcGenError(
                'Entry "%s" does not know which struct it belongs to '
                "around line %d" % (self._name, self.LineCount())
            )
        if self._optional and self._array:
            raise RpcGenError(
                'Entry "%s" has illegal combination of optional and array '
                "around line %d" % (self._name, self.LineCount())
            )

    def GetTranslation(self, extradict=None):
        if extradict is None:
            extradict = {}
        mapping = {
            "parent_name": self._struct.Name(),
            "name": self._name,
            "ctype": self._ctype,
            "refname": self._refname,
            "optpointer": self._optpointer and "*" or "",
            "optreference": self._optpointer and "&" or "",
            "optaddarg": self._optaddarg and ", const %s value" % self._ctype or "",
        }
        for (k, v) in list(extradict.items()):
            mapping[k] = v

        return mapping

    def GetVarName(self, var):
        return "%(var)s->%(name)s_data" % self.GetTranslation({"var": var})

    def GetVarLen(self, _var):
        return "sizeof(%s)" % self._ctype

    def GetFuncName(self):
        return "%s_%s_get" % (self._struct.Name(), self._name)

    def GetDeclaration(self, funcname):
        code = [
            "int %s(struct %s *, %s *);" % (funcname, self._struct.Name(), self._ctype)
        ]
        return code

    def CodeGet(self):
        code = """int
%(parent_name)s_%(name)s_get(struct %(parent_name)s *msg, %(ctype)s *value)
{
  if (msg->%(name)s_set != 1)
    return (-1);
  *value = msg->%(name)s_data;
  return (0);
}"""
        code = code % self.GetTranslation()
        return code.split("\n")

    def AssignFuncName(self):
        return "%s_%s_assign" % (self._struct.Name(), self._name)

    def AddFuncName(self):
        return "%s_%s_add" % (self._struct.Name(), self._name)

    def AssignDeclaration(self, funcname):
        code = [
            "int %s(struct %s *, const %s);"
            % (funcname, self._struct.Name(), self._ctype)
        ]
        return code

    def CodeAssign(self):
        code = [
            "int",
            "%(parent_name)s_%(name)s_assign(struct %(parent_name)s *msg,"
            " const %(ctype)s value)",
            "{",
            "  msg->%(name)s_set = 1;",
            "  msg->%(name)s_data = value;",
            "  return (0);",
            "}",
        ]
        code = "\n".join(code)
        code = code % self.GetTranslation()
        return code.split("\n")

    def CodeClear(self, structname):
        code = ["%s->%s_set = 0;" % (structname, self.Name())]

        return code

    @staticmethod
    def CodeComplete(_structname, _var_name):
        return []

    @staticmethod
    def CodeFree(_name):
        return []

    def CodeBase(self):
        code = ["%(parent_name)s_%(name)s_assign,", "%(parent_name)s_%(name)s_get,"]
        if self.Array():
            code.append("%(parent_name)s_%(name)s_add,")

        code = "\n".join(code)
        code = code % self.GetTranslation()
        return code.split("\n")


class EntryBytes(Entry):
    def __init__(self, ent_type, name, tag, length):
        # Init base class
        super(EntryBytes, self).__init__(ent_type, name, tag)

        self._length = length
        self._ctype = "ev_uint8_t"

    @staticmethod
    def GetInitializer():
        return "NULL"

    def GetVarLen(self, _var):
        return "(%s)" % self._length

    @staticmethod
    def CodeArrayAdd(varname, _value):
        # XXX: copy here
        return ["%(varname)s = NULL;" % {"varname": varname}]

    def GetDeclaration(self, funcname):
        code = [
            "int %s(struct %s *, %s **);" % (funcname, self._struct.Name(), self._ctype)
        ]
        return code

    def AssignDeclaration(self, funcname):
        code = [
            "int %s(struct %s *, const %s *);"
            % (funcname, self._struct.Name(), self._ctype)
        ]
        return code

    def Declaration(self):
        dcl = ["ev_uint8_t %s_data[%s];" % (self._name, self._length)]

        return dcl

    def CodeGet(self):
        name = self._name
        code = [
            "int",
            "%s_%s_get(struct %s *msg, %s **value)"
            % (self._struct.Name(), name, self._struct.Name(), self._ctype),
            "{",
            "  if (msg->%s_set != 1)" % name,
            "    return (-1);",
            "  *value = msg->%s_data;" % name,
            "  return (0);",
            "}",
        ]
        return code

    def CodeAssign(self):
        name = self._name
        code = [
            "int",
            "%s_%s_assign(struct %s *msg, const %s *value)"
            % (self._struct.Name(), name, self._struct.Name(), self._ctype),
            "{",
            "  msg->%s_set = 1;" % name,
            "  memcpy(msg->%s_data, value, %s);" % (name, self._length),
            "  return (0);",
            "}",
        ]
        return code

    def CodeUnmarshal(self, buf, tag_name, var_name, var_len):
        code = [
            "if (evtag_unmarshal_fixed(%(buf)s, %(tag)s, "
            "%(var)s, %(varlen)s) == -1) {",
            '  event_warnx("%%s: failed to unmarshal %(name)s", __func__);',
            "  return (-1);",
            "}",
        ]
        return TranslateList(
            code,
            self.GetTranslation(
                {"var": var_name, "varlen": var_len, "buf": buf, "tag": tag_name}
            ),
        )

    @staticmethod
    def CodeMarshal(buf, tag_name, var_name, var_len):
        code = ["evtag_marshal(%s, %s, %s, %s);" % (buf, tag_name, var_name, var_len)]
        return code

    def CodeClear(self, structname):
        code = [
            "%s->%s_set = 0;" % (structname, self.Name()),
            "memset(%s->%s_data, 0, sizeof(%s->%s_data));"
            % (structname, self._name, structname, self._name),
        ]

        return code

    def CodeInitialize(self, name):
        code = [
            "memset(%s->%s_data, 0, sizeof(%s->%s_data));"
            % (name, self._name, name, self._name)
        ]
        return code

    def Verify(self):
        if not self._length:
            raise RpcGenError(
                'Entry "%s" needs a length '
                "around line %d" % (self._name, self.LineCount())
            )

        super(EntryBytes, self).Verify()


class EntryInt(Entry):
    def __init__(self, ent_type, name, tag, bits=32):
        # Init base class
        super(EntryInt, self).__init__(ent_type, name, tag)

        self._can_be_array = True
        if bits == 32:
            self._ctype = "ev_uint32_t"
            self._marshal_type = "int"
        if bits == 64:
            self._ctype = "ev_uint64_t"
            self._marshal_type = "int64"

    @staticmethod
    def GetInitializer():
        return "0"

    @staticmethod
    def CodeArrayFree(_var):
        return []

    @staticmethod
    def CodeArrayAssign(varname, srcvar):
        return ["%(varname)s = %(srcvar)s;" % {"varname": varname, "srcvar": srcvar}]

    @staticmethod
    def CodeArrayAdd(varname, value):
        """Returns a new entry of this type."""
        return ["%(varname)s = %(value)s;" % {"varname": varname, "value": value}]

    def CodeUnmarshal(self, buf, tag_name, var_name, _var_len):
        code = [
            "if (evtag_unmarshal_%(ma)s(%(buf)s, %(tag)s, &%(var)s) == -1) {",
            '  event_warnx("%%s: failed to unmarshal %(name)s", __func__);',
            "  return (-1);",
            "}",
        ]
        code = "\n".join(code) % self.GetTranslation(
            {"ma": self._marshal_type, "buf": buf, "tag": tag_name, "var": var_name}
        )
        return code.split("\n")

    def CodeMarshal(self, buf, tag_name, var_name, _var_len):
        code = [
            "evtag_marshal_%s(%s, %s, %s);"
            % (self._marshal_type, buf, tag_name, var_name)
        ]
        return code

    def Declaration(self):
        dcl = ["%s %s_data;" % (self._ctype, self._name)]

        return dcl

    def CodeInitialize(self, name):
        code = ["%s->%s_data = 0;" % (name, self._name)]
        return code


class EntryString(Entry):
    def __init__(self, ent_type, name, tag):
        # Init base class
        super(EntryString, self).__init__(ent_type, name, tag)

        self._can_be_array = True
        self._ctype = "char *"

    @staticmethod
    def GetInitializer():
        return "NULL"

    @staticmethod
    def CodeArrayFree(varname):
        code = ["if (%(var)s != NULL) free(%(var)s);"]

        return TranslateList(code, {"var": varname})

    @staticmethod
    def CodeArrayAssign(varname, srcvar):
        code = [
            "if (%(var)s != NULL)",
            "  free(%(var)s);",
            "%(var)s = strdup(%(srcvar)s);",
            "if (%(var)s == NULL) {",
            '  event_warnx("%%s: strdup", __func__);',
            "  return (-1);",
            "}",
        ]

        return TranslateList(code, {"var": varname, "srcvar": srcvar})

    @staticmethod
    def CodeArrayAdd(varname, value):
        code = [
            "if (%(value)s != NULL) {",
            "  %(var)s = strdup(%(value)s);",
            "  if (%(var)s == NULL) {",
            "    goto error;",
            "  }",
            "} else {",
            "  %(var)s = NULL;",
            "}",
        ]

        return TranslateList(code, {"var": varname, "value": value})

    def GetVarLen(self, var):
        return "strlen(%s)" % self.GetVarName(var)

    @staticmethod
    def CodeMakeInitalize(varname):
        return "%(varname)s = NULL;" % {"varname": varname}

    def CodeAssign(self):
        code = """int
%(parent_name)s_%(name)s_assign(struct %(parent_name)s *msg,
    const %(ctype)s value)
{
  if (msg->%(name)s_data != NULL)
    free(msg->%(name)s_data);
  if ((msg->%(name)s_data = strdup(value)) == NULL)
    return (-1);
  msg->%(name)s_set = 1;
  return (0);
}""" % (
            self.GetTranslation()
        )

        return code.split("\n")

    def CodeUnmarshal(self, buf, tag_name, var_name, _var_len):
        code = [
            "if (evtag_unmarshal_string(%(buf)s, %(tag)s, &%(var)s) == -1) {",
            '  event_warnx("%%s: failed to unmarshal %(name)s", __func__);',
            "  return (-1);",
            "}",
        ]
        code = "\n".join(code) % self.GetTranslation(
            {"buf": buf, "tag": tag_name, "var": var_name}
        )
        return code.split("\n")

    @staticmethod
    def CodeMarshal(buf, tag_name, var_name, _var_len):
        code = ["evtag_marshal_string(%s, %s, %s);" % (buf, tag_name, var_name)]
        return code

    def CodeClear(self, structname):
        code = [
            "if (%s->%s_set == 1) {" % (structname, self.Name()),
            "  free(%s->%s_data);" % (structname, self.Name()),
            "  %s->%s_data = NULL;" % (structname, self.Name()),
            "  %s->%s_set = 0;" % (structname, self.Name()),
            "}",
        ]

        return code

    def CodeInitialize(self, name):
        code = ["%s->%s_data = NULL;" % (name, self._name)]
        return code

    def CodeFree(self, name):
        code = [
            "if (%s->%s_data != NULL)" % (name, self._name),
            "    free (%s->%s_data);" % (name, self._name),
        ]

        return code

    def Declaration(self):
        dcl = ["char *%s_data;" % self._name]

        return dcl


class EntryStruct(Entry):
    def __init__(self, ent_type, name, tag, refname):
        # Init base class
        super(EntryStruct, self).__init__(ent_type, name, tag)

        self._optpointer = False
        self._can_be_array = True
        self._refname = refname
        self._ctype = "struct %s*" % refname
        self._optaddarg = False

    def GetInitializer(self):
        return "NULL"

    def GetVarLen(self, _var):
        return "-1"

    def CodeArrayAdd(self, varname, _value):
        code = [
            "%(varname)s = %(refname)s_new();",
            "if (%(varname)s == NULL)",
            "  goto error;",
        ]

        return TranslateList(code, self.GetTranslation({"varname": varname}))

    def CodeArrayFree(self, var):
        code = ["%(refname)s_free(%(var)s);" % self.GetTranslation({"var": var})]
        return code

    def CodeArrayAssign(self, var, srcvar):
        code = [
            "int had_error = 0;",
            "struct evbuffer *tmp = NULL;",
            "%(refname)s_clear(%(var)s);",
            "if ((tmp = evbuffer_new()) == NULL) {",
            '  event_warn("%%s: evbuffer_new()", __func__);',
            "  had_error = 1;",
            "  goto done;",
            "}",
            "%(refname)s_marshal(tmp, %(srcvar)s);",
            "if (%(refname)s_unmarshal(%(var)s, tmp) == -1) {",
            '  event_warnx("%%s: %(refname)s_unmarshal", __func__);',
            "  had_error = 1;",
            "  goto done;",
            "}",
            "done:",
            "if (tmp != NULL)",
            "  evbuffer_free(tmp);",
            "if (had_error) {",
            "  %(refname)s_clear(%(var)s);",
            "  return (-1);",
            "}",
        ]

        return TranslateList(code, self.GetTranslation({"var": var, "srcvar": srcvar}))

    def CodeGet(self):
        name = self._name
        code = [
            "int",
            "%s_%s_get(struct %s *msg, %s *value)"
            % (self._struct.Name(), name, self._struct.Name(), self._ctype),
            "{",
            "  if (msg->%s_set != 1) {" % name,
            "    msg->%s_data = %s_new();" % (name, self._refname),
            "    if (msg->%s_data == NULL)" % name,
            "      return (-1);",
            "    msg->%s_set = 1;" % name,
            "  }",
            "  *value = msg->%s_data;" % name,
            "  return (0);",
            "}",
        ]
        return code

    def CodeAssign(self):
        code = (
            """int
%(parent_name)s_%(name)s_assign(struct %(parent_name)s *msg,
    const %(ctype)s value)
{
   struct evbuffer *tmp = NULL;
   if (msg->%(name)s_set) {
     %(refname)s_clear(msg->%(name)s_data);
     msg->%(name)s_set = 0;
   } else {
     msg->%(name)s_data = %(refname)s_new();
     if (msg->%(name)s_data == NULL) {
       event_warn("%%s: %(refname)s_new()", __func__);
       goto error;
     }
   }
   if ((tmp = evbuffer_new()) == NULL) {
     event_warn("%%s: evbuffer_new()", __func__);
     goto error;
   }
   %(refname)s_marshal(tmp, value);
   if (%(refname)s_unmarshal(msg->%(name)s_data, tmp) == -1) {
     event_warnx("%%s: %(refname)s_unmarshal", __func__);
     goto error;
   }
   msg->%(name)s_set = 1;
   evbuffer_free(tmp);
   return (0);
 error:
   if (tmp != NULL)
     evbuffer_free(tmp);
   if (msg->%(name)s_data != NULL) {
     %(refname)s_free(msg->%(name)s_data);
     msg->%(name)s_data = NULL;
   }
   return (-1);
}"""
            % self.GetTranslation()
        )
        return code.split("\n")

    def CodeComplete(self, structname, var_name):
        code = [
            "if (%(structname)s->%(name)s_set && "
            "%(refname)s_complete(%(var)s) == -1)",
            "  return (-1);",
        ]

        return TranslateList(
            code, self.GetTranslation({"structname": structname, "var": var_name})
        )

    def CodeUnmarshal(self, buf, tag_name, var_name, _var_len):
        code = [
            "%(var)s = %(refname)s_new();",
            "if (%(var)s == NULL)",
            "  return (-1);",
            "if (evtag_unmarshal_%(refname)s(%(buf)s, %(tag)s, ",
            "    %(var)s) == -1) {",
            '  event_warnx("%%s: failed to unmarshal %(name)s", __func__);',
            "  return (-1);",
            "}",
        ]
        code = "\n".join(code) % self.GetTranslation(
            {"buf": buf, "tag": tag_name, "var": var_name}
        )
        return code.split("\n")

    def CodeMarshal(self, buf, tag_name, var_name, _var_len):
        code = [
            "evtag_marshal_%s(%s, %s, %s);" % (self._refname, buf, tag_name, var_name)
        ]
        return code

    def CodeClear(self, structname):
        code = [
            "if (%s->%s_set == 1) {" % (structname, self.Name()),
            "  %s_free(%s->%s_data);" % (self._refname, structname, self.Name()),
            "  %s->%s_data = NULL;" % (structname, self.Name()),
            "  %s->%s_set = 0;" % (structname, self.Name()),
            "}",
        ]

        return code

    def CodeInitialize(self, name):
        code = ["%s->%s_data = NULL;" % (name, self._name)]
        return code

    def CodeFree(self, name):
        code = [
            "if (%s->%s_data != NULL)" % (name, self._name),
            "    %s_free(%s->%s_data);" % (self._refname, name, self._name),
        ]

        return code

    def Declaration(self):
        dcl = ["%s %s_data;" % (self._ctype, self._name)]

        return dcl


class EntryVarBytes(Entry):
    def __init__(self, ent_type, name, tag):
        # Init base class
        super(EntryVarBytes, self).__init__(ent_type, name, tag)

        self._ctype = "ev_uint8_t *"

    @staticmethod
    def GetInitializer():
        return "NULL"

    def GetVarLen(self, var):
        return "%(var)s->%(name)s_length" % self.GetTranslation({"var": var})

    @staticmethod
    def CodeArrayAdd(varname, _value):
        # xxx: copy
        return ["%(varname)s = NULL;" % {"varname": varname}]

    def GetDeclaration(self, funcname):
        code = [
            "int %s(struct %s *, %s *, ev_uint32_t *);"
            % (funcname, self._struct.Name(), self._ctype)
        ]
        return code

    def AssignDeclaration(self, funcname):
        code = [
            "int %s(struct %s *, const %s, ev_uint32_t);"
            % (funcname, self._struct.Name(), self._ctype)
        ]
        return code

    def CodeAssign(self):
        name = self._name
        code = [
            "int",
            "%s_%s_assign(struct %s *msg, "
            "const %s value, ev_uint32_t len)"
            % (self._struct.Name(), name, self._struct.Name(), self._ctype),
            "{",
            "  if (msg->%s_data != NULL)" % name,
            "    free (msg->%s_data);" % name,
            "  msg->%s_data = malloc(len);" % name,
            "  if (msg->%s_data == NULL)" % name,
            "    return (-1);",
            "  msg->%s_set = 1;" % name,
            "  msg->%s_length = len;" % name,
            "  memcpy(msg->%s_data, value, len);" % name,
            "  return (0);",
            "}",
        ]
        return code

    def CodeGet(self):
        name = self._name
        code = [
            "int",
            "%s_%s_get(struct %s *msg, %s *value, ev_uint32_t *plen)"
            % (self._struct.Name(), name, self._struct.Name(), self._ctype),
            "{",
            "  if (msg->%s_set != 1)" % name,
            "    return (-1);",
            "  *value = msg->%s_data;" % name,
            "  *plen = msg->%s_length;" % name,
            "  return (0);",
            "}",
        ]
        return code

    def CodeUnmarshal(self, buf, tag_name, var_name, var_len):
        code = [
            "if (evtag_payload_length(%(buf)s, &%(varlen)s) == -1)",
            "  return (-1);",
            # We do not want DoS opportunities
            "if (%(varlen)s > evbuffer_get_length(%(buf)s))",
            "  return (-1);",
            "if ((%(var)s = malloc(%(varlen)s)) == NULL)",
            "  return (-1);",
            "if (evtag_unmarshal_fixed(%(buf)s, %(tag)s, %(var)s, "
            "%(varlen)s) == -1) {",
            '  event_warnx("%%s: failed to unmarshal %(name)s", __func__);',
            "  return (-1);",
            "}",
        ]
        code = "\n".join(code) % self.GetTranslation(
            {"buf": buf, "tag": tag_name, "var": var_name, "varlen": var_len}
        )
        return code.split("\n")

    @staticmethod
    def CodeMarshal(buf, tag_name, var_name, var_len):
        code = ["evtag_marshal(%s, %s, %s, %s);" % (buf, tag_name, var_name, var_len)]
        return code

    def CodeClear(self, structname):
        code = [
            "if (%s->%s_set == 1) {" % (structname, self.Name()),
            "  free (%s->%s_data);" % (structname, self.Name()),
            "  %s->%s_data = NULL;" % (structname, self.Name()),
            "  %s->%s_length = 0;" % (structname, self.Name()),
            "  %s->%s_set = 0;" % (structname, self.Name()),
            "}",
        ]

        return code

    def CodeInitialize(self, name):
        code = [
            "%s->%s_data = NULL;" % (name, self._name),
            "%s->%s_length = 0;" % (name, self._name),
        ]
        return code

    def CodeFree(self, name):
        code = [
            "if (%s->%s_data != NULL)" % (name, self._name),
            "    free(%s->%s_data);" % (name, self._name),
        ]

        return code

    def Declaration(self):
        dcl = [
            "ev_uint8_t *%s_data;" % self._name,
            "ev_uint32_t %s_length;" % self._name,
        ]

        return dcl


class EntryArray(Entry):
    _index = None

    def __init__(self, entry):
        # Init base class
        super(EntryArray, self).__init__(entry._type, entry._name, entry._tag)

        self._entry = entry
        self._refname = entry._refname
        self._ctype = self._entry._ctype
        self._optional = True
        self._optpointer = self._entry._optpointer
        self._optaddarg = self._entry._optaddarg

        # provide a new function for accessing the variable name
        def GetVarName(var_name):
            return "%(var)s->%(name)s_data[%(index)s]" % self._entry.GetTranslation(
                {"var": var_name, "index": self._index}
            )

        self._entry.GetVarName = GetVarName

    def GetInitializer(self):
        return "NULL"

    def GetVarName(self, var):
        return var

    def GetVarLen(self, _var_name):
        return "-1"

    def GetDeclaration(self, funcname):
        """Allows direct access to elements of the array."""
        code = [
            "int %(funcname)s(struct %(parent_name)s *, int, %(ctype)s *);"
            % self.GetTranslation({"funcname": funcname})
        ]
        return code

    def AssignDeclaration(self, funcname):
        code = [
            "int %s(struct %s *, int, const %s);"
            % (funcname, self._struct.Name(), self._ctype)
        ]
        return code

    def AddDeclaration(self, funcname):
        code = [
            "%(ctype)s %(optpointer)s "
            "%(funcname)s(struct %(parent_name)s *msg%(optaddarg)s);"
            % self.GetTranslation({"funcname": funcname})
        ]
        return code

    def CodeGet(self):
        code = """int
%(parent_name)s_%(name)s_get(struct %(parent_name)s *msg, int offset,
    %(ctype)s *value)
{
  if (!msg->%(name)s_set || offset < 0 || offset >= msg->%(name)s_length)
    return (-1);
  *value = msg->%(name)s_data[offset];
  return (0);
}
""" % (
            self.GetTranslation()
        )

        return code.splitlines()

    def CodeAssign(self):
        code = [
            "int",
            "%(parent_name)s_%(name)s_assign(struct %(parent_name)s *msg, int off,",
            "  const %(ctype)s value)",
            "{",
            "  if (!msg->%(name)s_set || off < 0 || off >= msg->%(name)s_length)",
            "    return (-1);",
            "",
            "  {",
        ]
        code = TranslateList(code, self.GetTranslation())

        codearrayassign = self._entry.CodeArrayAssign(
            "msg->%(name)s_data[off]" % self.GetTranslation(), "value"
        )
        code += ["    " + x for x in codearrayassign]

        code += TranslateList(["  }", "  return (0);", "}"], self.GetTranslation())

        return code

    def CodeAdd(self):
        codearrayadd = self._entry.CodeArrayAdd(
            "msg->%(name)s_data[msg->%(name)s_length - 1]" % self.GetTranslation(),
            "value",
        )
        code = [
            "static int",
            "%(parent_name)s_%(name)s_expand_to_hold_more("
            "struct %(parent_name)s *msg)",
            "{",
            "  int tobe_allocated = msg->%(name)s_num_allocated;",
            "  %(ctype)s* new_data = NULL;",
            "  tobe_allocated = !tobe_allocated ? 1 : tobe_allocated << 1;",
            "  new_data = (%(ctype)s*) realloc(msg->%(name)s_data,",
            "      tobe_allocated * sizeof(%(ctype)s));",
            "  if (new_data == NULL)",
            "    return -1;",
            "  msg->%(name)s_data = new_data;",
            "  msg->%(name)s_num_allocated = tobe_allocated;",
            "  return 0;",
            "}",
            "",
            "%(ctype)s %(optpointer)s",
            "%(parent_name)s_%(name)s_add(struct %(parent_name)s *msg%(optaddarg)s)",
            "{",
            "  if (++msg->%(name)s_length >= msg->%(name)s_num_allocated) {",
            "    if (%(parent_name)s_%(name)s_expand_to_hold_more(msg)<0)",
            "      goto error;",
            "  }",
        ]

        code = TranslateList(code, self.GetTranslation())

        code += ["  " + x for x in codearrayadd]

        code += TranslateList(
            [
                "  msg->%(name)s_set = 1;",
                "  return %(optreference)s(msg->%(name)s_data["
                "msg->%(name)s_length - 1]);",
                "error:",
                "  --msg->%(name)s_length;",
                "  return (NULL);",
                "}",
            ],
            self.GetTranslation(),
        )

        return code

    def CodeComplete(self, structname, var_name):
        self._index = "i"
        tmp = self._entry.CodeComplete(structname, self._entry.GetVarName(var_name))
        # skip the whole loop if there is nothing to check
        if not tmp:
            return []

        translate = self.GetTranslation({"structname": structname})
        code = [
            "{",
            "  int i;",
            "  for (i = 0; i < %(structname)s->%(name)s_length; ++i) {",
        ]

        code = TranslateList(code, translate)

        code += ["    " + x for x in tmp]

        code += ["  }", "}"]

        return code

    def CodeUnmarshal(self, buf, tag_name, var_name, _var_len):
        translate = self.GetTranslation(
            {
                "var": var_name,
                "buf": buf,
                "tag": tag_name,
                "init": self._entry.GetInitializer(),
            }
        )
        code = [
            "if (%(var)s->%(name)s_length >= %(var)s->%(name)s_num_allocated &&",
            "    %(parent_name)s_%(name)s_expand_to_hold_more(%(var)s) < 0) {",
            '  puts("HEY NOW");',
            "  return (-1);",
            "}",
        ]

        # the unmarshal code directly returns
        code = TranslateList(code, translate)

        self._index = "%(var)s->%(name)s_length" % translate
        code += self._entry.CodeUnmarshal(
            buf,
            tag_name,
            self._entry.GetVarName(var_name),
            self._entry.GetVarLen(var_name),
        )

        code += ["++%(var)s->%(name)s_length;" % translate]

        return code

    def CodeMarshal(self, buf, tag_name, var_name, _var_len):
        code = ["{", "  int i;", "  for (i = 0; i < %(var)s->%(name)s_length; ++i) {"]

        self._index = "i"
        code += self._entry.CodeMarshal(
            buf,
            tag_name,
            self._entry.GetVarName(var_name),
            self._entry.GetVarLen(var_name),
        )
        code += ["  }", "}"]

        code = "\n".join(code) % self.GetTranslation({"var": var_name})

        return code.split("\n")

    def CodeClear(self, structname):
        translate = self.GetTranslation({"structname": structname})
        codearrayfree = self._entry.CodeArrayFree(
            "%(structname)s->%(name)s_data[i]"
            % self.GetTranslation({"structname": structname})
        )

        code = ["if (%(structname)s->%(name)s_set == 1) {"]

        if codearrayfree:
            code += [
                "  int i;",
                "  for (i = 0; i < %(structname)s->%(name)s_length; ++i) {",
            ]

        code = TranslateList(code, translate)

        if codearrayfree:
            code += ["    " + x for x in codearrayfree]
            code += ["  }"]

        code += TranslateList(
            [
                "  free(%(structname)s->%(name)s_data);",
                "  %(structname)s->%(name)s_data = NULL;",
                "  %(structname)s->%(name)s_set = 0;",
                "  %(structname)s->%(name)s_length = 0;",
                "  %(structname)s->%(name)s_num_allocated = 0;",
                "}",
            ],
            translate,
        )

        return code

    def CodeInitialize(self, name):
        code = [
            "%s->%s_data = NULL;" % (name, self._name),
            "%s->%s_length = 0;" % (name, self._name),
            "%s->%s_num_allocated = 0;" % (name, self._name),
        ]
        return code

    def CodeFree(self, structname):
        code = self.CodeClear(structname)

        code += TranslateList(
            ["free(%(structname)s->%(name)s_data);"],
            self.GetTranslation({"structname": structname}),
        )

        return code

    def Declaration(self):
        dcl = [
            "%s *%s_data;" % (self._ctype, self._name),
            "int %s_length;" % self._name,
            "int %s_num_allocated;" % self._name,
        ]

        return dcl


def NormalizeLine(line):

    line = CPPCOMMENT_RE.sub("", line)
    line = line.strip()
    line = WHITESPACE_RE.sub(" ", line)

    return line


ENTRY_NAME_RE = re.compile(r"(?P<name>[^\[\]]+)(\[(?P<fixed_length>.*)\])?")
ENTRY_TAG_NUMBER_RE = re.compile(r"(0x)?\d+", re.I)


def ProcessOneEntry(factory, newstruct, entry):
    optional = False
    array = False
    entry_type = ""
    name = ""
    tag = ""
    tag_set = None
    separator = ""
    fixed_length = ""

    for token in entry.split(" "):
        if not entry_type:
            if not optional and token == "optional":
                optional = True
                continue

            if not array and token == "array":
                array = True
                continue

        if not entry_type:
            entry_type = token
            continue

        if not name:
            res = ENTRY_NAME_RE.match(token)
            if not res:
                raise RpcGenError(
                    r"""Cannot parse name: "%s" around line %d""" % (entry, LINE_COUNT)
                )
            name = res.group("name")
            fixed_length = res.group("fixed_length")
            continue

        if not separator:
            separator = token
            if separator != "=":
                raise RpcGenError(
                    r'''Expected "=" after name "%s" got "%s"''' % (name, token)
                )
            continue

        if not tag_set:
            tag_set = 1
            if not ENTRY_TAG_NUMBER_RE.match(token):
                raise RpcGenError(r'''Expected tag number: "%s"''' % (entry))
            tag = int(token, 0)
            continue

        raise RpcGenError(r'''Cannot parse "%s"''' % (entry))

    if not tag_set:
        raise RpcGenError(r'''Need tag number: "%s"''' % (entry))

    # Create the right entry
    if entry_type == "bytes":
        if fixed_length:
            newentry = factory.EntryBytes(entry_type, name, tag, fixed_length)
        else:
            newentry = factory.EntryVarBytes(entry_type, name, tag)
    elif entry_type == "int" and not fixed_length:
        newentry = factory.EntryInt(entry_type, name, tag)
    elif entry_type == "int64" and not fixed_length:
        newentry = factory.EntryInt(entry_type, name, tag, bits=64)
    elif entry_type == "string" and not fixed_length:
        newentry = factory.EntryString(entry_type, name, tag)
    else:
        res = STRUCT_REF_RE.match(entry_type)
        if res:
            # References another struct defined in our file
            newentry = factory.EntryStruct(entry_type, name, tag, res.group("name"))
        else:
            raise RpcGenError('Bad type: "%s" in "%s"' % (entry_type, entry))

    structs = []

    if optional:
        newentry.MakeOptional()
    if array:
        newentry.MakeArray()

    newentry.SetStruct(newstruct)
    newentry.SetLineCount(LINE_COUNT)
    newentry.Verify()

    if array:
        # We need to encapsulate this entry into a struct
        newentry = factory.EntryArray(newentry)
        newentry.SetStruct(newstruct)
        newentry.SetLineCount(LINE_COUNT)
        newentry.MakeArray()

    newstruct.AddEntry(newentry)

    return structs


def ProcessStruct(factory, data):
    tokens = data.split(" ")

    # First three tokens are: 'struct' 'name' '{'
    newstruct = factory.Struct(tokens[1])

    inside = " ".join(tokens[3:-1])

    tokens = inside.split(";")

    structs = []

    for entry in tokens:
        entry = NormalizeLine(entry)
        if not entry:
            continue

        # It's possible that new structs get defined in here
        structs.extend(ProcessOneEntry(factory, newstruct, entry))

    structs.append(newstruct)
    return structs


C_COMMENT_START = "/*"
C_COMMENT_END = "*/"

C_COMMENT_START_RE = re.compile(re.escape(C_COMMENT_START))
C_COMMENT_END_RE = re.compile(re.escape(C_COMMENT_END))

C_COMMENT_START_SUB_RE = re.compile(r"%s.*$" % (re.escape(C_COMMENT_START)))
C_COMMENT_END_SUB_RE = re.compile(r"%s.*$" % (re.escape(C_COMMENT_END)))

C_MULTILINE_COMMENT_SUB_RE = re.compile(
    r"%s.*?%s" % (re.escape(C_COMMENT_START), re.escape(C_COMMENT_END))
)
CPP_CONDITIONAL_BLOCK_RE = re.compile(r"#(if( |def)|endif)")
INCLUDE_RE = re.compile(r'#include (".+"|<.+>)')


def GetNextStruct(filep):
    global CPP_DIRECT
    global LINE_COUNT

    got_struct = False
    have_c_comment = False

    data = ""

    while True:
        line = filep.readline()
        if not line:
            break

        LINE_COUNT += 1
        line = line[:-1]

        if not have_c_comment and C_COMMENT_START_RE.search(line):
            if C_MULTILINE_COMMENT_SUB_RE.search(line):
                line = C_MULTILINE_COMMENT_SUB_RE.sub("", line)
            else:
                line = C_COMMENT_START_SUB_RE.sub("", line)
                have_c_comment = True

        if have_c_comment:
            if not C_COMMENT_END_RE.search(line):
                continue
            have_c_comment = False
            line = C_COMMENT_END_SUB_RE.sub("", line)

        line = NormalizeLine(line)

        if not line:
            continue

        if not got_struct:
            if INCLUDE_RE.match(line):
                CPP_DIRECT.append(line)
            elif CPP_CONDITIONAL_BLOCK_RE.match(line):
                CPP_DIRECT.append(line)
            elif PREPROCESSOR_DEF_RE.match(line):
                HEADER_DIRECT.append(line)
            elif not STRUCT_DEF_RE.match(line):
                raise RpcGenError("Missing struct on line %d: %s" % (LINE_COUNT, line))
            else:
                got_struct = True
                data += line
            continue

        # We are inside the struct
        tokens = line.split("}")
        if len(tokens) == 1:
            data += " " + line
            continue

        if tokens[1]:
            raise RpcGenError("Trailing garbage after struct on line %d" % LINE_COUNT)

        # We found the end of the struct
        data += " %s}" % tokens[0]
        break

    # Remove any comments, that might be in there
    data = re.sub(r"/\*.*\*/", "", data)

    return data


def Parse(factory, filep):
    """
    Parses the input file and returns C code and corresponding header file.
    """

    entities = []

    while 1:
        # Just gets the whole struct nicely formatted
        data = GetNextStruct(filep)

        if not data:
            break

        entities.extend(ProcessStruct(factory, data))

    return entities


class CCodeGenerator(object):
    def __init__(self):
        pass

    @staticmethod
    def GuardName(name):
        # Use the complete provided path to the input file, with all
        # non-identifier characters replaced with underscores, to
        # reduce the chance of a collision between guard macros.
        return "EVENT_RPCOUT_%s_" % (NONIDENT_RE.sub("_", name).upper())

    def HeaderPreamble(self, name):
        guard = self.GuardName(name)
        pre = """
/*
 * Automatically generated from %s
 */

#ifndef %s
#define %s

""" % (
            name,
            guard,
            guard,
        )

        if HEADER_DIRECT:
            for statement in HEADER_DIRECT:
                pre += "%s\n" % statement
            pre += "\n"

        pre += """
#include <event2/util.h> /* for ev_uint*_t */
#include <event2/rpc.h>
"""

        return pre

    def HeaderPostamble(self, name):
        guard = self.GuardName(name)
        return "#endif  /* %s */" % (guard)

    @staticmethod
    def BodyPreamble(name, header_file):
        global _NAME
        global _VERSION

        slash = header_file.rfind("/")
        if slash != -1:
            header_file = header_file[slash + 1 :]

        pre = """
/*
 * Automatically generated from %(name)s
 * by %(script_name)s/%(script_version)s.  DO NOT EDIT THIS FILE.
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <event2/event-config.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/tag.h>

#if defined(EVENT__HAVE___func__)
# ifndef __func__
#  define __func__ __func__
# endif
#elif defined(EVENT__HAVE___FUNCTION__)
# define __func__ __FUNCTION__
#else
# define __func__ __FILE__
#endif

""" % {
            "name": name,
            "script_name": _NAME,
            "script_version": _VERSION,
        }

        for statement in CPP_DIRECT:
            pre += "%s\n" % statement

        pre += '\n#include "%s"\n\n' % header_file

        pre += "void event_warn(const char *fmt, ...);\n"
        pre += "void event_warnx(const char *fmt, ...);\n\n"

        return pre

    @staticmethod
    def HeaderFilename(filename):
        return ".".join(filename.split(".")[:-1]) + ".h"

    @staticmethod
    def CodeFilename(filename):
        return ".".join(filename.split(".")[:-1]) + ".gen.c"

    @staticmethod
    def Struct(name):
        return StructCCode(name)

    @staticmethod
    def EntryBytes(entry_type, name, tag, fixed_length):
        return EntryBytes(entry_type, name, tag, fixed_length)

    @staticmethod
    def EntryVarBytes(entry_type, name, tag):
        return EntryVarBytes(entry_type, name, tag)

    @staticmethod
    def EntryInt(entry_type, name, tag, bits=32):
        return EntryInt(entry_type, name, tag, bits)

    @staticmethod
    def EntryString(entry_type, name, tag):
        return EntryString(entry_type, name, tag)

    @staticmethod
    def EntryStruct(entry_type, name, tag, struct_name):
        return EntryStruct(entry_type, name, tag, struct_name)

    @staticmethod
    def EntryArray(entry):
        return EntryArray(entry)


class CommandLine(object):
    def __init__(self, argv=None):
        """Initialize a command-line to launch event_rpcgen, as if
           from a command-line with CommandLine(sys.argv).  If you're
           calling this directly, remember to provide a dummy value
           for sys.argv[0]
        """
        global QUIETLY

        self.filename = None
        self.header_file = None
        self.impl_file = None
        self.factory = CCodeGenerator()

        parser = argparse.ArgumentParser(
            usage="%(prog)s [options] rpc-file [[h-file] c-file]"
        )
        parser.add_argument("--quiet", action="store_true", default=False)
        parser.add_argument("rpc_file", type=argparse.FileType("r"))

        args, extra_args = parser.parse_known_args(args=argv)

        QUIETLY = args.quiet

        if extra_args:
            if len(extra_args) == 1:
                self.impl_file = extra_args[0].replace("\\", "/")
            elif len(extra_args) == 2:
                self.header_file = extra_args[0].replace("\\", "/")
                self.impl_file = extra_args[1].replace("\\", "/")
            else:
                parser.error("Spurious arguments provided")

        self.rpc_file = args.rpc_file

        if not self.impl_file:
            self.impl_file = self.factory.CodeFilename(self.rpc_file.name)

        if not self.header_file:
            self.header_file = self.factory.HeaderFilename(self.impl_file)

        if not self.impl_file.endswith(".c"):
            parser.error("can only generate C implementation files")
        if not self.header_file.endswith(".h"):
            parser.error("can only generate C header files")

    def run(self):
        filename = self.rpc_file.name
        header_file = self.header_file
        impl_file = self.impl_file
        factory = self.factory

        declare('Reading "%s"' % filename)

        with self.rpc_file:
            entities = Parse(factory, self.rpc_file)

        declare('... creating "%s"' % header_file)
        with open(header_file, "w") as header_fp:
            header_fp.write(factory.HeaderPreamble(filename))

            # Create forward declarations: allows other structs to reference
            # each other
            for entry in entities:
                entry.PrintForwardDeclaration(header_fp)
            header_fp.write("\n")

            for entry in entities:
                entry.PrintTags(header_fp)
                entry.PrintDeclaration(header_fp)
            header_fp.write(factory.HeaderPostamble(filename))

        declare('... creating "%s"' % impl_file)
        with open(impl_file, "w") as impl_fp:
            impl_fp.write(factory.BodyPreamble(filename, header_file))
            for entry in entities:
                entry.PrintCode(impl_fp)


def main(argv=None):
    try:
        CommandLine(argv=argv).run()
        return 0
    except RpcGenError as e:
        sys.stderr.write(e)
    except EnvironmentError as e:
        if e.filename and e.strerror:
            sys.stderr.write("%s: %s" % (e.filename, e.strerror))
        elif e.strerror:
            sys.stderr.write(e.strerror)
        else:
            raise
    return 1


if __name__ == "__main__":
    sys.exit(main(argv=sys.argv[1:]))
