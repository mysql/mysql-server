\page PageLibsMysqlStrconv Library: Strconv

<!---
Copyright (c) 2024, 2026, Oracle and/or its affiliates.
//
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.
//
This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.
//
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.
//
You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
-->

<!--
MySQL Library: Strconv
======================
-->

Code documentation: @ref GroupLibsMysqlStrconv.

## Overview

Library for string conversion. This contains the following functionality:

- `encode`: Framework to format objects as strings. An object author defines a
  single formatting function, and the framework provides multiple API entry
  points to convert objects to strings: into preallocated buffers or
  std::string; throwing or returning error; computing the result or merely
  computing the length of the result. The framework allows object authors to
  define multiple formats for any given object type, using format tags to
  distinguish them. It pre-defines text format, binary format, hex format, debug
  format, and escaped format; all capable of formatting strings and the first
  two capable of formatting integers. The goal is to provide an efficient and
  uniform API and uniform out-of-memory handling for any object types that
  implement the API. At the same time, object authors are given an API optimized
  for simplicity, which has no error cases, and makes it easy to reuse/combine
  existing object types.

- `decode`: Framework to parse strings into objects. An object author defines a
  single parsing function, and the framework provides multiple API entry points
  that parse strings into objects: from std::string or raw pointers; requesting
  that the object extends to the end of the input or allowing it to be a prefix;
  requesting that the object is found or allowing it to be absent. The framework
  defines the handling of parse errors and out-of-memory errors, and provides
  usable error messages. The framework allows object authors to define multiple
  formats for any given object type, using format tags to distinguish them. It
  pre-defines text format and binary format for strings and integral types. The
  goal is to provide an efficient and uniform API and uniform handling of parse
  errors and out-of-memory errors, for any object types that implement the API,
  while making it as easy as possible for object authors to define parsing of
  custom types.

- Output String Wrappers, String Targets, and `out_str_write`: these low-level
  helpers, used by `encode`, aid in implementing string producing functions.

  Output String Wrappers are objects that wrap the output buffer, and which
  accept buffers either as `std::string` or using raw pointers; either
  null-terminated or not; with a "resize policy" that either requires the
  producer to resize the buffer appropriately, or declares that the user has
  allocated a sufficiently large buffer. Users create Output String Wrappers by
  calling factory functions, and pass them to the string producer.

  String Targets are objects through which the string producer interacts with
  Output String Wrappers. They abstract the buffer handling and ensure the
  resize policy is followed. The string producer should take a String Target as
  parameter and invoke the `write`, `write_char`, or `write_raw` members. There
  are two String Target classes: `String_counter`, which counts the number of
  bytes written to it, and `String_writer`, which writes to an Output String
  Wrapper "back-end".

  The `out_str_write` function takes advantage of the two forms of String
  Targets by implementing the following pattern: (1) invoke the string producing
  function, passing a String Target object that computes the size of the output;
  (2) allocate the output; (3) invoke the string producing function again,
  passing a String Target object that writes to the allocated buffer. This
  pattern has two advantages, compared to appending to a `std::string` or
  `std::stringstream`: (1) it only uses one allocation, which may be more
  efficient; (2) any allocations occur outside the invocation of the string
  producing function. Therefore, the string producing function can never observe
  allocation failures, which usually means it has no error cases. This is makes
  it simpler and less error-prone.

  By implementing string producers using `out_str_write` and String Targets, the
  producer implementation can focus on formatting rather than buffer handling,
  while enabling an API allowing many different forms of output buffers.

## Choosing the most suitable library for the task

Use the right tool for the job! This section compares use cases for the current
alternative string conversion libraries.

Simple, specialized formats / mini-languages:

  - `mysql::strconv::encode`/`decode`:
    - You code the formatter and/or parser in C++.
    - Your code defines both the object model and the string format.
    - The framework helps you streamline error handling and memory managerment,
      so your code is more focused on the specifics of the format.
    - Very little CPU overhead, and minimizes allocations and copy operations.
    - The parser provides reasonable error messages.

  - `std::format`/`std::printf` + `std::regex_match`:
    - You define the string format using format strings.
    - Limited to very simple languages.
    - Minimal coding effort.
    - Minimal dependencies.
    - May be slow, and the quality of `std::regex_match` implementations varies
      between platforms (`re2` may be better, but then it is a new dependency).
    - No error message that can explain the reason for a mismatch.

  - Hand-coding without using a framework:
    - You can do anything.
    - No dependencies.
    - Hard to implement.
    - Error-prone.

Storing/transmitting data packets that evolve over time:

  - `mysql::serialization`:
    - You declare a schema programmatically.
    - You define an object model that conforms to the library's requirements.
    - The framework defines the string format, which is binary.
    - Space-efficient format that can preserve backward compatibility when a
      schema evolves over time.

  - `protobuf`:
    - You write a schema in a file, and then use a special compiler to generate
      code for the object model.
    - Both object model and serialized form are defined by the framework. The
      serialized form is a binary format. It can be converted to JSON.
    - Long-time, widely used library with many tools makes it good for
      interoperability. However, the format differs between protobuf versions.

  - `json`:
    - The framework provides a schema-agnostic object model and format.
    - You define the "schema" by how you organize data in the object model.
    - Human-readable text format.
    - Standard, stable format standard with rich tools around.
    - Space and CPU overhead for storing in text format.

Passing ABI-compatible data packets between shared objects (.so/.dll) within a
process:

  - `mysql::abi_helpers`:
    - The framework provides a schema-agnostic object model.
    - You define the "schema" programmatically by how you organize data in the
      object model.
    - Very little CPU and space overhead, because there is no encoding/decoding.
    - Packet definitions may evolve over time.

## The `encode` and `decode` APIs

There are two clients of this library: *authors* of types that need to be
formatted as strings and/or parsed from strings, and *users* that invoke those
formatter/parser functions.

For users, the framework tries to be:
- easy: it provides functions to string-format an object into/out of raw pointer
  buffers, std::string, std::string_view, or std::ostream. Also the size of the
  formatted string for an object can be computed without actually writing it
  anywhere.
- safe: it returns error states that the user must check (`[[nodiscard]]`).
- flexible, by allowing multiple formats of a given object type. The parser
  returns a string that explains the reason for the error.
- flexible: for the parser, users can allow the object description to be
  optional or required.
- fast: It uses static polymorphism to avoid virtual function calls and to allow
  the compiler to inline and optimize most function calls. The formatter
  pre-computes the length and then performs only one allocation and a minimum of
  copy operations.

For authors, the framework tries to be:
- easy: authors only need to implement one function to format and one function
  to parse each format. The framework uses the single formatting function first
  to compute the length without writing data, and then to actually write the
  data.
- safe: formatters never have error cases. For parsers, the framework defines
  non-throwing error handling in a way that tries to minimize the risk that
  authors forget it, while making it as out-of-the-way as possible of the
  implementation of a specific format.
- flexible: authors can define multiple formats for a given type: for example
  text format, binary format, multiple format versions, etc.
- flexible: it is easy to reuse one format when defining another format.

## Users: converting objects to strings

### Basic usage, text format

To convert `object` to a new `std::string` object, using text format, use:

```
  // Get a std::string using the throwing interface.
  try {
    std::string str = mysql::strconv::throwing::encode_text(object);
  } catch (std::bad_alloc &) { /* handle the exception */ }
```
or
```
  // Get a std::optional<std::string> using the non-throwing interface.
  std::optional<std::string> opt_str = mysql::strconv::encode_text(object);
  if (!opt_str.has_value()) { /* handle the oom error */ }
  std::string str = opt_str.value();
```

### Other formats

To convert `object` to a new `std::string` object, using other formats, use
`encode` instead of `encode_text`, and pass a format object for the first
parameter:

```
  // Get a std::string using the throwing interface.
  try {
    std::string str = mysql::strconv::throwing::encode(Binary_format{}, object);
  } catch (std::bad_alloc &) { /* handle the exception */ }
```
or
```
  // Get a std::optional<std::string> using the non-throwing interface.
  auto opt_str = mysql::strconv::encode(Hex_format{}, object);
  if (!opt_str.has_value()) { /* handle the oom error */ }
  std::string str = opt_str.value();
```

### Other representations of output strings, using output string wrappers

If you know a bound on the object size and don't want the cost of allocations
and error handling, pass an *output string wrapper* object before `obj`. First
an example:

```
  char buf[100];
  size_t size = 99; // reserve 1 char for the null-termination byte
  mysql::strconv::encode_text(mysql::strconv::out_str_fixed_z(buf, size), obj);
```

In the code above, `out_str_fixed_z` returns an output string wrapper: an object
that holds a reference to (but does not own) a string that can be written to. It
may be thought of as a modifiable version of `std::string_view`. Output strings
are constructed using the factory functions:
- `out_str_fixed(std::string &s)`: write to `s`, which already has reserved
  capacity for as many characters as we will write.
- `out_str_fixed_nz(char *buf, size_t &size)`: write a non-null-terminated
  string to `buf` which has capacity at least `size` (which we know is enough),
  and update `size` to the actual number of bytes written.
- `out_str_fixed_z(char *buf, size_t &size)`: same, but make it null-terminated.
  The actual capacity must be at least `size+1`, to accomodate the `\0`.

Alternatively, you may pass `char *&end` instead of `size_t &size`, to represent
the string length through and pointer. The buffer may be a pointer or an array,
and the character type may be `char`, `unsigned char`, or `std::byte`.

You may also use output string wrappers that allocate as much memory as needed.
To do so, replace `fixed` by `growable` in the name of the factory function:
`out_str_growable`, `out_str_growable_nz`, or `out_str_growable_z`. These
functions take a reference in the first argument, which will be updated to the
new string, if allocation is needed.

## Users: using `decode`

### Basic usage and error handling

The `decode` functions parse strings into objects. Here is an example of
how it can be used:

```
  Object parse_object(std::string text) {
    Object obj;
    auto result =
        mysql::strconv::decode(mysql::strconv::Text_format{}, text, obj);
    if (!result.is_ok()) {
      std::cout << mysql::strconv::encode_text(result); // produce error message
      std::terminate();
    }
    return obj;
  }
```

The returned `result` object is of type `Parser`. This can be queried for the
success status (`result.is_ok()`) and for an error message
(`encode_text(result)`).

## Simpler form for text format

When parsing `Text_format`, you may use the simpler form `decode_text(text, obj)`,
which is equivalent to `decode(Text_format{}, text, obj)`.

### Other representations of the input string.

If your input string uses raw pointers, just wrap it in a `std::string_view`.
Since `std::string_view` has implicit conversions from raw pointers, just use:

```
  // char *buf, size_t size
  auto result = mysql::strconv::decode(fmt, {buf, size}, obj);
```
or
```
  // char *buf, char *end
  auto result = mysql::strconv::decode(fmt, {buf, end}, obj);
```

### Other formats than Text

If you need a format other than `Text_format`, use the appropriate formatting
class instead: one of `Debug_format`, `Binary_format`, `Fixint_binary_format`,
`Escaped_format`, or `Hex_format` defined in this library, or whatever formats
the authors of Object-parsers may have defined.

### String-to-string conversion

In the special case of string-to-string conversion, i.e., the object is a string
type, you may pass an `std::string` for the output object, and it will grow as
needed. Alternatively, you may pass an output string wrapper (described above)
for the object type. Then it will produce a string into the buffer/string you
provide, using the string representation, resize policy (fixed or growable), and
null-termination policy you specify.

## Pre-defined object formats

This library pre-defines formatters and parsers for integral and string types:

- Any built-in integer (`std::integral`, both signed and unsigned) can be parsed
  or formatted using `Text_format` (as base-10 ascii), `Binary_format` (using
  the varint format defined in the `mysql::serialization` library), or
  `Fixint_binary_format` (as 8-byte little endian).

- Strings can be formatted but not parsed using `Text_format` (copying the
  string verbatim) or `Escaped_format` (escaping special characters), and can be
  both formatted and parsed using `Fixint_binary_format` (an integer length
  followed by the characters).

## Parse options

The first argument of encode is actually a set of *parse options*. As we saw
above, one kind of parse option is a Format object. Another kind of parse option
is a Repeat object, which declares that the same parse operation is executed
between N and M times. For example, with N=0 and M=1, the object is optional.
Repeat objects are constructed using factory functions such as
`Repeat::optional()` (0 to 1 repetitions), `Repeat::range(N, M)`, or
`Repeat::at_least(N)`, and combined with Format objects using the `|` operator.
Here is an example:

```
  auto result = mysql::strconv::decode(
      mysql::strconv::Text_format{} | mysql::strconv::Repeat::optional(),
      str, obj);
```

## Authors: implementing `encode` for custom types and formats

### Function prototype

Authors need to implement this:

```
  namespace mysql::strconv {
  void encode_impl(
      const /*Format*/ &format,
      mysql::strconv::Is_string_target auto &target,
      const /*Object*/ &object) {
    // write object to target in format
  }
  }
```

It is essential that the function is in the `mysql::strconv` namespace. The
parameters are as follows:

- `object`: the object to format.

- `target`: where the object shall be written. `Is_string_target` is a concept
  that is true for the `String_counter` and `String_writer` classes which are
  internal to the framework and provide a uniform interface for writing output
  or computing the length without writing. When the user calls `encode`, the
  framework will first invoke this function with a `String_counter` object,
  which will merely count the bytes; then it will resize the output buffer as
  needed, and finally call this function with a `String_writer` object that
  writes to the output string wrapper. Your `encode_impl` function, by using
  the concept `Is_string_target`, can be agnostic to whether it is invoked with
  a `String_counter` or a `String_writer` object.

- `format`: this serves two purposes. (1) its type is used to tag-dispatch the
  particular string format. It can either be one of `Text_format`,
  `Debug_format`, `Escaped_format`, `Hex_format`, `Binary_format`, or
  `Fixint_binary_format`, which are defined in this library; or any other format
  type that you as an author define. (2) the object `format` may hold additional
  format parameters, which may control any aspect of the output format that
  users should be able configure - for example, separator characters, size
  limitations, etc.

### Formatting data into a string

The `encode_impl` implementation should normally use the following
functions:

```
  target.write(format, object);
  target.write_raw(sv);
  target.write_char(ch);
```

Here, `target` is the `Is_string_target` object passed to the function. `write`
formats `value` (of any other type) using the format specified by `format`.
`write_raw` writes bytes to the output without formatting them; `sv` is a
`std::string_view`.

### Low-level functions to format data

In case your formatter needs different logic when computing the string length
versus writing the string, e.g. when the string length is faster to compute, use
the following pattern:

```
  template <mysql::strconv::Is_string_target Target_t>
  void encode_impl(
      const /*Format*/ &format,
      Target_t &target,
      const /*Object*/ &object) {
    if constexpr (Target_t::target_type == Target_type::counter) {
      // compute length of object
    } else {
      // write object
    }
  }
```

This function prototype is identical to the one in the `Function prototype`
section above. The function body has a compile-time `if` that executes the first
branch to compute the size and the second branch to write the data.

In the `true` branch, you may use `target.write`, `target.write_raw`, and
`target.write_char` as above, and also `target.advance(size_t)` which increments
the position by the given amount.

In the `false` branch, you may use `target.write`, `target.write_raw`, and
`target.write_char` as above. In addition you may use `target.pos()` to get the
raw `char *` pointer to the current position in the buffer. (Use `upos` and
`bpos` to get `unsigned char*` and `std::byte *`, respectively). After writing N
bytes through the raw pointer, you must call `target.advance(N)` to advance the
position accordingly.

(Detail: It is discouraged to implement this as two functions:
```
  // Do not do this!
  // void encode_impl(const /*Format*/ &format,
  //                  mysql::strconv::String_counter &target,
  //                  const /*Object*/ &object);
  // void encode_impl(const /*Format*/ &format,
  //                  mysql::strconv::String_writer &target,
  //                  const /*Object*/ &object);
```
The reason is that this prevents implementing `encode_impl` for types that
derive from `Object` with a single single function taking an `Is_string_target
auto` parameter: if doing so, calls to `encode_impl(Format, String_writer,
Object)` would be ambiguous, because both `encode_impl(Format, String_writer,
Object)` and `encode_impl(Format, Is_string_target, Derived_object)` would be
"viable" and none of them "better" than the other.)

## Authors: implementing `decode` for custom types and formats

### Function prototype

Authors need to implement this:

```
  namespace mysql::strconv {
  void decode_impl(
      const /*Format*/ &format,
      mysql::strconv::Parser &parser,
      /*Object*/ &object) {
    // read into object from parser using format; report any errors through parser
  }
  }
```

It is essential that the function is in the `mysql::strconv` namespace. The
parameters are as follows:

- `object`: the object into which the result should be stored.

- `parser`: parser object, which internally holds both the string to parse, a
  cursor into it, the success/failure status of the operation, and any
  associated error message. As an author, you use this object both to parse
  sub-objects which your object consists of, to check the status of those parse
  operations, and to report the status of the current parse operation to the
  user.

- `format`: this serves two purposes. (1) its type is used to tag-dispatch the
  particular string format. It can either be one of `Text_format`,
  `Debug_format`, `Binary_format`, or `Fixint_binary_format`, which are defined
  in this library; or any other type the user defines. (2) the object `format`
  may hold additional format parameters, which may control any aspect of the
  output format the author wants to make configurable.

### Parsing data from a string

The `decode_impl` function should usually use the following functions:

```
  parser.read(parse_options, obj);
  parser.skip(parse_options, sv);
```

Here, `obj` is any sub-object of your object and `sv` is a `std::string_view`.
`read` will invoke the corresponding `encode_impl` function. `skip` will
check that the substring starting at the cursor position starts with `sv`. Both
will:

- on success, advance the position, set the status to `ok` and return
  `mysql::utils::Return_status::ok`.

- on error, preserve the position, set the status to `error` and return
  `mysql::utils::Return_status::error`.

The parse_options should normally contain the format. The format may be combined
with a `Repeat` object, using the `|` operator. For `read`, it may also be
combined with a `Checker` object.

A `Repeat` defines the number of times the object may be parsed: by default
exactly once, but `Repeat::optional()` allows it to be parsed between 0 and 1
times, i.e., optionally; and `Repeat::range(N, M)` requires at least N
repetitions and allows up to M repetitions. The last M-N objects will be parsed
as long as doing so does not result in an error; a parse error occurring at this
point is replaced by an `ok` status.

A `Checker` contains a lambda function that validates the output object after
the parsing completed; on error, the position is restored.

Example:
```
  auto checker = mysql::strconv::Checker([&] {
    if (!obj.is_valid()) parser.set_parse_error("Invalid object");
  });
  if (parser.read(format | mysql::strconv::Repeat::optional() | checker, obj))
    return;
```

`read` and `skip` are declared as `[[nodiscard]]` (except `skip` when it takes a
`Repeat` object whose lower bound is 0, because then it cannot fail). Thus you
can't forget to check the status. Normally you want to propagate the error to
the caller (by leaving the status object unchanged and returning), as in the
following example:

```
  if (parser.read(format, obj) != mysql::utils::Return_status::ok) return;
```

### Reporting errors

If your `decode_impl` needs to check other error conditions than those that
sub-object check, or override an error reported by a sub-object with some other
error, use the following member functions:
- `parser.set_ok()`: success.
- `parser.set_parse_error(message)`: An error occurred while parsing the string.
  The message should be a string_view containing a sentence that describes was
  is wrong, for example, "Integer out of range". The sentence should start with
  a capital letter, but should not end with a period.
- `parser.oom()`: An out-of-memory error occurred (e.g. when inserting into
  `object`). This is equivalent to `set_store_error("Out of memory")`.
- `parser.set_store_error(message)`: This is a generalization of `oom`, and
  should be used for any other error that occurs while storing the object.
  `message` should be a string_view containing phrase that describes what is
  wrong.

The difference between parse errors and store errors are:
- For parse errors, a subsequent `encode(parser)` will produce a message that
  contains the position in the string. Store errors do not contain that, as the
  error is assumed to be unrelated to a particular position (e.g.
  out-of-memory).
- When a sub-object is parsed *optionally*, because `parser.read` was invoked
  with a `Repeat(N,M)` parameter and the object is one of the optional objects
  (at an index between `N` and `M`), a parse error is replaced by an OK status,
  whereas a store error always makes the caller fail.

### Auto-skipping whitespace

Some formats may allow whitespace around all or most tokens in a string. To make
parsing of such formats less repetitive/error-prone, define the following member
function of your format class:
```
  void before_token(Parser &parser) { skip_whitespace(parser); }
```
`skip_whitespace` is defined in this library and will advance the position to
the next non-whitespace character. Define a similar `after_token` function if
needed. If `before_token` and/or `after_token` functions are defined, they will
be invoked by the functions `parser.read`, `parser.read_optional`, `parser,skip`,
and `parser.skip_optional` before and after parsing the string.

### String-to-string parsing

For the special case where the object type is string, use the following
prototype:

```
  void decode_impl(
      const /*Format*/ &format,
      mysql::strconv::Parser &parser,
      mysql::strconv::Is_string_target auto &target);
```

Write to the target just like you would in a `encode_impl` function, i.e.,
normally using `target.write_raw(sv)` or `target.write_char(ch)` (and possibly
`target.write(Text_format{}, obj)`).

This enables the following API functions for users:
- `decode(format, in_str, /*output string wrapper*/)`: users may pass any
  output string writer, so they may use raw pointers and fixed-size buffers,
  just like with `encode`.
- `compute_decoded_length(format, in_str)`: computes the size of the
  output without writing the output.
- `test_decode(format, in_str)`: determines whether parsing would succeed
  or not.

## Formats

A "format" is a class that identifies a particular way to convert from string to
object or object to string. The *type* of the format class is used to
tag-dispatch the correct formatting algorithm: for example, a call to
`encode(Text_format, ...)` typically uses an entirely different algorithm
than a call to `encode(Binary_format, ...)`. *Members* of the format class
may be used as parameters to tune the formatting algorithm: for example, the
built-in `Escape_format` has members that specify e.g. the escape character.

### Built-in formats

This library defines the following alternative string formats for integers
(`std::integral`) and strings (`std::string`, `std::string_view`, `char[N]`,
`unsigned char[N]`, `std::byte[N]`):

- `Text_format`: integrals are base-10 ascii. String data is copied literally.
  `decode` is not defined for strings (because the end of the string cannot
  be known).

- `Escaped_format`: Based on `Text_format`, but escapes special characters when
  formatting strings.

- `Hex_format`: Formats/parses strings in hex format.

- `Debug_format`: same as text format, but authors may choose to include
  additional debug information that does not make sense to give to end users.
  Typically, this is only used for formatting and not parsing. If user calls
  `encode` for an object type that does not have
  `encode_impl(Debug_format,...)` implemented, it falls back to
  `Text_format`.

- `Binary_format`: format integrals using variable length integers: see
  `mysql::serialization::detail::write_varlen_bytes`. Format strings as a
  variable integer length followed by the string data.

- `Fixint_binary_format`: format integrals using fixed-length 8-byte
  little-endian integers. If users calls encode for an object type that does
  not have `encode_impl(Fixint_binary_format,...)` defined, it falls back to
  `Binary_format`.

- `Fixstr_binary_format`: format/parse strings of fixed length. The number of
  bytes is specified as a member of the Fixstr_binary_format object.

### Hierarchy of parent formats

The framework provides a mechanism called *parent format*, which allows a
specialized (child) format to use a more generic (parent) format as fall-back
for object types for which no specialized `encode_impl`/`decode_impl`
function is defined for the child format.

For example, `Debug_format` is defined to have `Text_format` as its parent. This
has the consequence that whenever a user invokes `encode(Debug_format, ...,
SomeType)`, the framework uses `encode_impl(Debug_format, ..., SomeType)` *if
that is defined*, and otherwise falls back to the parent format and uses
`encode_impl(Text_format, ..., SomeType)`.

The parent formats form a hierarchy: If no implementation is defined for the
parent, the framework checks the parent-of-the-parent, and so on. Here is the
hierarchy for the built-in formats:

```
  +-Text_format
  | +- Debug_format
  | +- Escaped_format
  +-Binary_format
  | +- Fixint_binary_format
  +-Hex_format
```

The parents are specified using member functions of the child format classes:
```
  auto Debug_format::parent() const { return Text_format{}; }
  auto Escaped_format::parent() const { return Text_format{}; }
  auto Fixint_binary_format::parent() const { return Binary_format{}; }
```

User-defined formats may insert themselves into the hierarchy by defining the
`parent` member analogously.

Parent formats are resolved at compile-time, so the format resolution algorithm
typically has no performance cost.

(Detail: This *could* have been implemented using inheritance, e.g.,
`Debug_format` could derive from `Text_format`, and `Fixint_binary_format` could
have derived from `Binary_format`. This would provide the same fall-back
mechanism, but could introduce ambiguity in overload resolution. For example,
suppose we have two concepts `C` and `D`, and implement `Binary_format`  for
classes satisfying `C` and `Fixint_binary_format` for classes implementing `D`.
Then, a class that satisfies *both* `C` and `D` could not be formatted/parsed as
`Fixint_binary_format`, because any invocation of `encode_impl` or
`decode_impl` would be ambiguous: both the `Binary_format,C` and the
`Fixint_binary_format,D` function would be "viable" and the compiler could not
rank any one of them as "better". By making the format types unrelated, such
ambiguities cannot arise.)

### Default formats

Suppose we define the Text format conversion for a user-defined type, say
`My_type`, and suppose that the format has parameters such as configurable
delimiter characters. Those parameters are then conveniently stored in the
format object, but then the format cannot be Text_format, but rather something
like `My_text_format`, which contains the specific formatting parameters as
members.

Now suppose that a generic algorithm takes a type as template argument and
formats an object of that type as text. When the algorithm is given a `My_type`
object, it needs to use a `My_text_format` object. In other words, `Text_format`
must be replaced by `My_text_format` when formatting `My_type` objects. The
generic algorithm, since it works on any type, cannot know about
`My_text_format`. Instead, the framework provides a mechanism called Default
Format. The author of `My_text_format` can declare that the default format for
`Text_format` and `My_type` is `Text_format`. That allows the generic algorithm
to pass `Text_format` when formatting `My_type` object, and the framework then
replaces `Text_format` by `My_text_format`.

The default format is declared by overloading `get_default_format`, as follows:

```
namespace mysql::strconv {
template <class My_type>
auto get_default_format(const Text_format &) { return My_text_format{}; }
}
```

### Format resolution algorithm

The algorithm to resolve the format, among possibly defined formats, default
formats, and parent formats, is as follows:

Suppose the user invokes `encode(format, object)`, where `format` and
`object` are of types `Format_type` and `Object_type`. Then the framework runs
the following algorithm (at compile time):
1. If `encode_impl(Format_type, ... Object_type)` is defined, invoke it and
   exit.
2. Otherwise, if `Default_format<Format_type, Object_type>` is defined and
   `encode_impl(Default_Format<Format_type,
   Object_type>::get_default_format(format), ..., object)` is defined, invoke it
   and exit.
3. Otherwise, if `format.parent()` is defined, restart at step 1 using
   `format.parent()`.
4. Otherwise, generate a compilation error.

The analogous algorithm is used to find a suitable `decode_impl` when the
user invokes `decode`.
