MySQL Router
================

This is a release of MySQL Router.

For the avoidance of doubt, this particular copy of the software
is released under the version 2 of the GNU General Public License.
MySQL Router is brought to you by Oracle.

Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

Documentation
-------------

For further information about MySQL or additional documentation, see:

* http://www.mysql.com
* http://dev.mysql.com/doc/mysql-router/en/

You can browse the MySQL Router Reference Manual online or download it
in any of several formats at the URL given earlier in this file.
Source distributions include a local copy of the manual in the
Docs directory.

Developer documentation can be build when Doxygen 1.8.9 or greater
has been installed:

    shell> cd build
    shell> cmake ..
    shell> make doc

You can then open the folder `doc/html/index.html` in your browser.


Coverage information
--------------------

To build so that coverage information is generated:

    cmake <path-to-source> -DENABLE_COVERAGE=1

To get coverage information, just run the program or the unit tests
(do not forget to enable the unit tests if you want to run them). Once
you have collected coverage information, you can generate an HTML
report in `<build-dir>/coverage/html` using:

    make coverage-html

There are three variables to control where the information is
collected and where the reports are written:

- `GCOV_BASE_DIR` is a cache variable with the full path to a base
  directory for the coverage information.

  It defaults to `${CMAKE_BUILD_DIR}/coverage`.

- `GCOV_INFO_FILE` is a cache varible with the full path to the info
  file for the collected coverage information.

  It defaults to `${GCOV_BASE_DIR}/coverage.info`.

- `GCOV_XML_FILE` is a cache varible with the full path to the XML
  file for the collected coverage information.

  It defaults to `${GCOV_BASE_DIR}/coverage.xml`.

- `GCOV_HTML_DIR` is a cache variable with the full path to the
  directory where the HTML coverage report will be generated.

  It defaults to `${GCOV_BASE_DIR}/html`.


License
-------

License information can be found in the License.txt file.

MySQL FOSS License Exception
We want free and open source software applications under certain
licenses to be able to use specified GPL-licensed MySQL client
libraries despite the fact that not all such FOSS licenses are
compatible with version 2 of the GNU General Public License.
Therefore there are special exceptions to the terms and conditions
of the GPLv2 as applied to these client libraries, which are
identified and described in more detail in the FOSS License
Exception at
<http://www.mysql.com/about/legal/licensing/foss-exception.html>.

This distribution may include materials developed by third
parties. For license and attribution notices for these
materials, please refer to the documentation that accompanies
this distribution (see the "Licenses for Third-Party Components"
appendix) or view the online documentation at
<http://dev.mysql.com/doc/>.

GPLv2 Disclaimer
For the avoidance of doubt, except that if any license choice
other than GPL or LGPL is available it will apply instead,
Oracle elects to use only the General Public License version 2
(GPLv2) at this time for any software where a choice of GPL
license versions is made available with the language indicating
that GPLv2 or any later version may be used, or where a choice
of which version of the GPL is applied is otherwise unspecified.

Licenses for Third-Party Components
-----------------------------------

### GMock and GTest

Copyright 2008, Google Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

    * Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
copyright notice, this list of conditions and the following disclaimer
in the documentation and/or other materials provided with the
distribution.
    * Neither the name of Google Inc. nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
