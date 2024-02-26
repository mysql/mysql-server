/*
  Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "mysql/harness/config_option.h"

#include <sstream>
#include <stdexcept>

namespace mysql_harness {

double option_as_double(const std::string &value,
                        const std::string &option_name, double min_value,
                        double max_value) {
  std::istringstream ss(value);
  // we want to make sure the decinal separator is always '.' regardless of the
  // user locale settings so we force classic locale
  ss.imbue(std::locale("C"));
  double result = 0.0;
  if (!(ss >> result) || !ss.eof() || (result < min_value - 0.0001) ||
      (result > max_value + 0.0001)) {
    std::stringstream os;
    os << option_name << " needs value between " << min_value << " and "
       << max_value << " inclusive";
    os << ", was '" << value << "'";

    throw std::invalid_argument(os.str());
  }

  return result;
}
}  // namespace mysql_harness
