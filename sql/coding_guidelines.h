/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is also distributed with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have included with MySQL.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @page GENERAL_DEVELOPMENT_GUIDELINES General Development Guidelines

  We use Git for source management.

  You should use the TRUNK source tree (currently called
  "mysql-trunk") for all new development. To download and set
  up the public development branch, use these commands:

  ~~~~~~~~~~~~~~~~
  shell> git clone https://github.com/mysql/mysql-server.git mysql-trunk
  shell> cd mysql-trunk
  shell> git branch mysql-trunk
  ~~~~~~~~~~~~~~~~
  Before making big design decisions, please begin by posting a
  summary of what you want to do, why you want to do it, and
  how you plan to do it. This way we can easily provide you
  with feedback and also discuss it thoroughly. Perhaps another
  developer can assist you.

  - @subpage CODING_GUIDELINES_OF_SERVER
  - @subpage INDENTATION_SPACING
  - @subpage NAMING_CONVENTION
  - @subpage COMMENTING_CODE
  - @subpage HEADER_FILE
  - @subpage SUGGESTED_MODE_IN_EMACS
  - @subpage BASIC_VIM_SETUP
  - @subpage ANOTHER_VIM_SETUP
  - @subpage EXAMPLE_SETUP_FOR_CTAGS
*/


/**
  @page CODING_GUIDELINES_OF_SERVER C/C++ Coding Guidelines of MySQL Server

  This section covers guidelines for C/C++ code for the MySQL
  server. The guidelines do not necessarily apply for other
  projects such as MySQL Connector/J or Connector/ODBC.
*/


/**
  @page INDENTATION_SPACING Indentation and Spacing

  - For indentation, use space characters, not tab (\\t)
    characters. See the editor configuration tips at the end
    of this section for instructions on configuring a vim or
    emacs editor to use spaces instead of tabs.

  - Avoid trailing whitespace, in code and comments.

  Correct:
  ~~~~~~~~~~~~~~~~
  if (a)
  ~~~~~~~~~~~~~~~~

  Incorrect:
  ~~~~~~~~~~~~~~~~
  if (a)<SP><SP><TAB><SP>
  ~~~~~~~~~~~~~~~~

  Remove trailing spaces if you are already changing a line,
  otherwise leave existing code intact.

  - Use line feed (\\n) for line breaks. Do not use carriage
    return + line feed (\\r\\n); that can cause problems for
    other users and for builds. This rule is particularly
    important if you use a Windows editor.

  - To begin indenting, add two spaces. To end indenting,
    subtract two spaces. For example:

  ~~~~~~~~~~~~~~~~
  {
    code, code, code
    {
      code, code, code
    }
  }
  ~~~~~~~~~~~~~~~~


  - An exception to the preceding rule: namespaces (named or
    unnamed) do not introduce a new level of indentation.
    Example:

  ~~~~~~~~~~~~~~~~
  namespace foo
  {
  class Bar
  {
    Bar();
  };
  }  // namespace foo
  ~~~~~~~~~~~~~~~~

  - The maximum line width is 80 characters. If you are
    writing a longer line, try to break it at a logical point
    and continue on the next line with the same indenting.
    Use of backslash is okay; however, multi-line literals
    might cause less confusion if they are defined before the
    function start.

  - You may use empty lines (two line breaks in a row)
    wherever it seems helpful for readability. But never use
    two or more empty lines in a row. The only exception is
    after a function definition (see below).

  - To separate two functions, use three line breaks (two
    empty lines). To separate a list of variable declarations
    from executable statements, use two line breaks (one
    empty line). For example:

  ~~~~~~~~~~~~~~~~
  int function_1()
  {
    int i;
    int j;

    function0();
  }


  int function2()
  {
    return;
  }
  ~~~~~~~~~~~~~~~~


  - Align matching '{}' (left and right braces) in the
    same column; that is, the closing '}' should be directly
    below the opening '{'. Do not put any non-space
    characters on the same line as a brace, not even a
    comment. Indent within braces. Exception: if there is
    nothing between two braces, that is, '{}', they should
    appear together. For example:

  ~~~~~~~~~~~~~~~~
  if (code, code, code)
  {
    code, code, code;
  }


  for (code, code, code)
  {}
  ~~~~~~~~~~~~~~~~

  - Indent switch like this:

  ~~~~~~~~~~~~~~~~
  switch (condition)
  {
  case XXX:
    statements;
  case YYY:
    {
      statements;
    }
  }
  ~~~~~~~~~~~~~~~~

  - Align variable declarations like this:

  ~~~~~~~~~~~~~~~~
  Type      value;
  int       var2;
  ulonglong var3;
  ~~~~~~~~~~~~~~~~


  - Assignment: For new projects, follow Google style.
    Traditional assignment rules for MySQL are
    listed here. For old projects/components, use the old MySQL
    style for the time being.

  - When assigning to a variable, put zero spaces after the
    target variable name, then the assignment operator
    ('=', '+=', etc.), then space(s). For single assignments,
    there should be only one space after the equal sign. For
    multiple assignments, add additional spaces so that the
    source values line up. For example:

  ~~~~~~~~~~~~~~~~
  a/= b;
  return_value= my_function(arg1);
  ...
  int x=          27;
  int new_var=    18;
  ~~~~~~~~~~~~~~~~

  Align assignments from one structure to another, like this:

  ~~~~~~~~~~~~~~~~
  foo->member=      bar->member;
  foo->name=        bar->name;
  foo->name_length= bar->name_length;
  ~~~~~~~~~~~~~~~~

  - Put separate statements on separate lines. This applies
    for both variable declarations and executable statements.
    For example, this is wrong:

  ~~~~~~~~~~~~~~~~
  int x= 11; int y= 12;

  z= x; y+= x;
  ~~~~~~~~~~~~~~~~

  This is right:
  ~~~~~~~~~~~~~~~~
  int x= 11;
  int y= 12;

  z= x;
  y+= x;
  ~~~~~~~~~~~~~~~~

  - Put spaces both before and after binary comparison
    operators ('>', '==', '>=', etc.), binary arithmetic
    operators ('+', etc.), and binary Boolean operators ('||',
    etc.). Do not put spaces around unary operators like '!'
    or '++'. Do not put spaces around [de-]referencing
    operators like '->' or '[]'. Do not put space after '*'
    when '*' introduces a pointer. Do not put spaces after
    '('. Put one space after ')' if it ends a condition, but
    not if it ends a list of function arguments. For example:

  ~~~~~~~~~~~~~~~~
  int *var;

  if ((x == y + 2) && !param->is_signed)
    function_call();
  ~~~~~~~~~~~~~~~~

  - When a function has multiple arguments separated by
    commas (','), put one space after each comma. For
    example:

  ~~~~~~~~~~~~~~~~
  ln= mysql_bin_log.generate_name(opt_bin_logname, "-bin", 1, buf);
  ~~~~~~~~~~~~~~~~

  - Put one space after a keyword which introduces a
    condition, such as if or for or while.

  - After if or else or while, when there is only one
    instruction after the condition, braces are not necessary
    and the instruction goes on the next line, indented.

  ~~~~~~~~~~~~~~~~
  if (sig != MYSQL_KILL_SIGNAL && sig != 0)
    unireg_abort(1);
  else
    unireg_end();
  while (*val && my_isspace(mysqld_charset, *val))
    *val++;
  ~~~~~~~~~~~~~~~~

  - In function declarations and invocations: There is no
    space between function name and '('. There is no space or
    line break between '(' and the first argument. If the
    arguments do not fit on one line, align them.
    Examples:

  ~~~~~~~~~~~~~~~~
  Return_value_type *Class_name::method_name(const char *arg1,
                                             size_t arg2, Type *arg3)
  return_value= function_name(argument1, argument2, long_argument3,
                              argument4,
                              function_name2(long_argument5,
                                             long_argument6));
  return_value=
    long_long_function_name(long_long_argument1, long_long_argument2,
                            long_long_long_argument3,
                            long_long_argument4,
                            long_function_name2(long_long_argument5,
                                                long_long_argument6));
  Long_long_return_value_type *
  Long_long_class_name::
  long_long_method_name(const char *long_long_arg1, size_t long_long_arg2,
                        Long_long_type *arg3)
  ~~~~~~~~~~~~~~~~
  (You may but need not split Class_name::method_name into
  two lines.) When arguments do not fit on one line, consider
  renaming them.

  - Format constructors in the following way:

  ~~~~~~~~~~~~~~~~
  Item::Item(int a_arg, int b_arg, int c_arg)
    :a(a_arg), b(b_arg), c(c_arg)
  {}
  ~~~~~~~~~~~~~~~~
  But keep lines short to make them more readable:
  ~~~~~~~~~~~~~~~~
  Item::Item(int longer_arg, int more_longer_arg)
    :longer(longer_arg),
    more_longer(more_longer_arg)
  {}
  ~~~~~~~~~~~~~~~~

  If a constructor can fit into one line:
  ~~~~~~~~~~~~~~~~
  Item::Item(int a_arg) :a(a_arg) {}
  ~~~~~~~~~~~~~~~~
*/


/**
  @page NAMING_CONVENTION Naming Conventions

  - For identifiers formed from multiple words, separate each
    component with underscore rather than capitalization.
    Thus, use my_var instead of myVar or MyVar.

  - Avoid capitalization except for class names; class names
    should begin with a capital letter.
    This exception from Google coding guidelines exists
    because the server has a history of using My_class. It will
    be confusing to mix the two (from a code-review perspective).

  ~~~~~~~~~~~~~~~~
  class Item;
  class Query_arena;
  class Log_event;
  ~~~~~~~~~~~~~~~~

  - Avoid function names, structure elements, or variables
    that begin or end with '_'.

  - Use long function and variable names in English. This
    makes your code easier to read for all developers.

  - We used to have the rule: "Structure types are typedef'ed
    to an all-upper-case identifier." That rule has been
    deprecated for C++ code. Do not add typedefs for
    structs/classes in C++

  - All \#define declarations should be in upper case.

  ~~~~~~~~~~~~~~~~
  #define MY_CONSTANT 15
  ~~~~~~~~~~~~~~~~

  - Enumeration names should begin with enum_.

  - Function declarations (forward declarations) have
    parameter names in addition to parameter types.

  - Member variable names: Do not use foo_. Instead, use
    m_foo (non-static) and s_foo (static), which
    are improvements over the Google style.
*/


/**
  @page COMMENTING_CODE Commenting Code

  - Comment your code when you do something that someone else
    may think is not trivial.

  - Comments for pure virtual functions, documentation for
    API usage should precede (member, or
    non-member) function declarations. Descriptions of
    implementation details, algorithms, anything that does
    not impact usage, should precede the
    implementation. Please try to not duplicate information.
    Make a reference to the declaration from the
    implementation if necessary. If the implementation and
    usage are too interleaved, put a reference from the
    interface to the implementation, and keep the entire
    comment in a single place.

  - Class comments should precede the class
    declaration.

  - When writing multi-line comments please put the '<em>*</em>' and
    '<em>*</em>/' on their own lines, put the '<em>*</em>/' below the
    '/<em>*</em>', put a line break and a two-space indent after the
    '/<em>*</em>', do not use additional asterisks on the left of the comment.

  <div style="margin-left:30px">
  <table style="background-color:#E0E0E0"><tr><td style="width:670px"><pre>
  /<em>*</em>
    This is how a multi-line comment in the middle of code
    should look.  Note it is not Doxygen-style if it is not at the
    beginning of a code enclosure (function or class).
  <em>*</em>/

  /<em>* *********</em>This comment is bad. It's indented incorrectly, it has
  <em>*</em>            additional asterisks. Don't write this way.
  <em>*  **********</em>/</pre>
  </td></tr></table></div>

  - When writing single-line comments, the '/<em>*</em>' and '<em>*</em>/" are
    on the same line. For example:

  <div style="margin-left:30px">
  <table style="background-color:#E0E0E0"><tr><td style="width:670px">
  /<em>*</em> We must check if stack_size = Solaris 2.9 can return 0 here.
  <em>*</em>/</td></tr></table></div>

  - Single-line comments like this are okay in C++.

  <div style="margin-left:30px">
  <table style="background-color:#E0E0E0"><tr><td style="width:670px">
  /<em></em>/ We must check if stack_size = Solaris 2.9 can return 0 here.
  </td></tr></table></div>


  - For a short comment at the end of a line, you may use
    either /<em>*</em> ... *<em></em>/ or a // double slash. In C files or in
    header files used by C files, avoid // comments.

  - Align short side // or /<em>*</em> ... <em>*</em>/ comments by 48th column
    (start the comment in column 49).

  <div style="margin-left:30px">
  <table style="background-color:#E0E0E0"><tr><td style="width:670px">
  { qc*= 2; /<em>*</em> double the estimation <em>*</em>/ }
  </td></tr></table></div>

  - When commenting members of a structure or a class, align
    comments by 48th column. If a comment does not fit into
    one line, move it to a separate line. Do not create
    multiline comments aligned by 48th column.

  <div style="margin-left:30px">
  <table style="background-color:#E0E0E0"><tr><td style="width:670px"><pre>
  struct st_mysql_stmt
  {
  ...
    MYSQL_ROWS     *data_cursor;         /<em>**</em>< current row in cached result <em>*</em>/
    /<em>*</em> copy of mysql->affected_rows after statement execution <em>*</em>/
    my_ulonglong   affected_rows;
    my_ulonglong   insert_id;            /<em>**</em>< copy of mysql->insert_id *<em></em>/
    /<em>*</em>
      mysql_stmt_fetch() calls this function to fetch one row (it's different
      for buffered, unbuffered and cursor fetch).
    <em>*</em>/
    int            (*read_row_func)(struct st_mysql_stmt *stmt,
  ...
  };</pre>
  </td></tr></table></div>

  - All comments should be in English.

  - Each standalone comment must start with a Capital letter.

  - There is a '.' at the end of each statement in a comment
    paragraph, including the last one.

  <div style="margin-left:30px">
  <table style="background-color:#E0E0E0"><tr><td style="width:670px"><pre>
  /<em>*</em>
    This is a standalone comment. The comment is aligned to fit 79
    characters per line. There is a period at the end of each sentence.
    Including the last one.
  <em>*</em>/</pre>
  </td></tr></table></div>

 - Every structure, class, method or function should have a
   description unless it is very short and its purpose is
   obvious.

 - Use the following example as a template for function or
   method comments.

      + Please refer to the Doxygen Manual
        (http://www.stack.nl/~dimitri/doxygen/manual.html)
        for additional information.

      + Note the IN and OUT parameters. IN is implicit, and
        can (but usually should not) be specified with the
        \@param[in] tag. For OUT and INOUT parameters you should
        use \@param[out] and \@param[in,out] tags,
        respectively.

      + Parameter specifications in \@param section start
        with lowercase and are not terminated with a full
        stop/period.

      + Section headers are aligned at two spaces. This must
        be a sentence with a full stop/period at the end.
        If the sentence must express a subject that
        contains a full stop such that Doxygen would be
        fooled into stopping early, then use \@brief and
        \@details to explicitly mark them.

      + Align \@retval specifications at four spaces if they
        follow a \@return description. Else, align at two
        spaces.

      + Separate sections with an empty line. <br>

      + All function comments should be no longer than 79
        characters per line.

      + Put two line breaks (one empty line) between a
        function comment and its description. <br>

      + Doxygen comments: Use <em>/</em>** ... *<em>/</em> syntax and not ///

      + Doxygen command: Use '@' and not '\' for doxygen commands.

  <div style="margin-left:30px">
  <table style="background-color:#E0E0E0"><tr><td style="width:670px">
  /<em>**</em><pre>
    Initialize SHA1Context.

    Set initial values in preparation for computing a new SHA1 message digest.

    \@param[in,out]  context  the context to reset

    \@return Operation status
      \@retval SHA_SUCCESS      OK
      \@retval != SHA_SUCCESS   sha error Code
  <em>*</em>/

  int sha1_reset(SHA1_CONTEXT *context)
  {
    ...</pre>
  </td></tr></table></div>
*/


/**
  @page HEADER_FILE Header Files

  - Use header guards. Put the header guard in the first
    line of the header, before the copyright. Use an
    all-uppercase name for the header guard. Derive the
    header guard name from the file base name, and append
    _INCLUDED to create a macro name. Example: sql_show.h ->
    SQL_SHOW_INCLUDED.

  - Include directives shall be first in the file. In a class
    implementation, include the header file containing the class
    declaration before all other header files, to ensure
    that the header is self-sufficient.

  - Every header file should be self-sufficient in the sense
    that for a header file my_header.h, the following should
    compile without errors:

  ~~~~~~~~~~~~~~~~
  #include "my_header.h"
  ~~~~~~~~~~~~~~~~

  An exception is made for generated files; for example, those
  generated by Yacc and Lex, because it is not possible to
  rewrite the generators to produce "correct" files.
*/


/**
  @page SUGGESTED_MODE_IN_EMACS Suggested Mode in emacs
  @verbatim
  (require 'font-lock)
  (require 'cc-mode)
  (setq global-font-lock-mode t) ;;colors in all buffers that support it
  (setq font-lock-maximum-decoration t) ;;maximum color
  (c-add-style "MY"
   '("K&R"
       (c-basic-offset . 2)
       (c-comment-only-line-offset . 0)
       (c-offsets-alist . ((statement-block-intro . +)
                           (knr-argdecl-intro . 0)
                           (substatement-open . 0)
                           (label . -)
                           (statement-cont . +)
                           (arglist-intro . c-lineup-arglist-intro-after-paren)
                           (arglist-close . c-lineup-arglist)
                           (innamespace . 0)
                           (inline-open . 0)
                           (statement-case-open . +)
                           ))
       ))

  (defun mysql-c-mode-hook ()
    (interactive)
    (require 'cc-mode)
    (c-set-style "MY")
    (setq indent-tabs-mode nil)
    (setq comment-column 48))

  (add-hook 'c-mode-common-hook 'mysql-c-mode-hook)
  @endverbatim
*/


/**
  @page BASIC_VIM_SETUP Basic vim Setup
  @verbatim
  set tabstop=8
  set shiftwidth=2
  set backspace=2
  set softtabstop
  set smartindent
  set cindent
  set cinoptions=g0:0t0c2C1(0f0l1
  set expandtab
  @endverbatim
*/


/**
  @page ANOTHER_VIM_SETUP Another vim Setup
  @verbatim
  set tabstop=8
  set shiftwidth=2
  set bs=2
  set et
  set sts=2
  set tw=78
  set formatoptions=cqroa1
  set cinoptions=g0:0t0c2C1(0f0l1
  set cindent

  function InsertShiftTabWrapper()
    let num_spaces = 48 - virtcol('.')
    let line = ' '
    while (num_spaces > 0)
      let line = line . ' '
      let num_spaces = num_spaces - 1
    endwhile
    return line
  endfunction
  " jump to 48th column by Shift-Tab - to place a comment there
  inoremap <S-tab> <c-r>=InsertShiftTabWrapper()<cr>
  " highlight trailing spaces as errors
  let c_space_errors=1
  @endverbatim
*/


/**
  @page EXAMPLE_SETUP_FOR_CTAGS Example Setup for ctags

  Put this configuration into your ~/.ctags file:
  @verbatim
  --c++-kinds=+p
  --fields=+iaS
  --extra=+q
  --langdef=errmsg
  --regex-errmsg=/^(ER_[A-Z0-9_]+)/\1/
  --langmap=errmsg:(errmsg*.txt),c:+.ic,yacc:+.yy
  @endverbatim
*/


/**
  @page DBUG_TAGS DBUG Tags

  <p>The full documentation of the DBUG library is in files dbug/user.* in the
  MySQL source tree. Here are some of the DBUG tags we now use:</p>

    - enter

      Arguments to the function.

    - exit

      Results from the function.

    - info

      Something that may be interesting.

    - warning

      When something does not go the usual route or may be wrong.

    - error

      When something went wrong.

    - loop

      Write in a loop, that is probably only useful when debugging the loop.
      These should normally be deleted when you are satisfied with the code
      and it has been in real use for a while.

  <br>
  Some tags specific to mysqld, because we want to watch these carefully:

    - trans

      Starting/stopping transactions.

    - quit

      info when mysqld is preparing to die.

    - query

      Print query.

*/
