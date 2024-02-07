/*
  Copyright (c) 2015, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef HARNESS_ARG_HANDLER_INCLUDED
#define HARNESS_ARG_HANDLER_INCLUDED

/** @file
 * @brief Defining the commandline argument handler class CmdArgHandler
 *
 * This file defines the commandline argument handler class CmdArgHandler.
 */

#include "harness_export.h"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

enum class CmdOptionValueReq {
  none = 0x01,
  required = 0x02,
  optional = 0x03,
};

/** @brief CmdOption stores information about command line options
 *
 * The CmdOption structure stores information about command line options.
 *
 */
struct CmdOption {
  using ActionFunc = std::function<void(const std::string &)>;
  using AtEndActionFunc = std::function<void(const std::string &)>;
  using OptionNames = std::vector<std::string>;

  OptionNames names;
  std::string description;
  CmdOptionValueReq value_req;
  std::string value;
  std::string metavar;
  ActionFunc action;
  AtEndActionFunc at_end_action;
  bool required{false};

  CmdOption(
      OptionNames names_, std::string description_,
      CmdOptionValueReq value_req_, const std::string metavar_,
      ActionFunc action_,
      AtEndActionFunc at_end_action_ = [](const std::string &) {})
      : names(names_),
        description(description_),
        value_req(value_req_),
        metavar(metavar_),
        action(action_),
        at_end_action(at_end_action_) {}
};

/** @brief Definition of a vector holding unique pointers to CmdOption
 * objects **/
using OptionContainer = std::vector<CmdOption>;

/** @class CmdArgHandler
 *  @brief Handles command line arguments
 *
 * The CmdArgHandler class handles command line arguments. It is a
 * replacement and supports most of the POSIX GNU getopt library.
 *
 * Command line options can have multiple aliases. For example, the
 * typical `--help` can be also called `-h`, or even `--help-me`. Long
 * names starting with one dash are not supported.
 *
 * Command line options are added through the `add_option()` method
 * and can be given 1 or more names and a description. It is also
 * possible to require the option to have a value, or make the value
 * optional.
 *
 * During processing of the command line arguments, actions will be
 * executed when the option (and it's potential value) was found.
 *
 * Usage example:
 *
 *     #include "arg_handler.h"
 *
 *     void MyApp::prepare_command_line_options() {
 *       // CmdArgHandler handler_;
 *       handler_.add_option(OptionNames({"-h", "--help", "--sos"}), "Show help
 * screen", CmdOptionValueReq::none, "", [this](const string &) {
 * this->show_help(); } handler_.add_option(OptionNames({"--config"}),
 * "Configuration file", CmdOptionValueReq::none, "", [this](const string
 * &value) { this->set_config_file(value); },
 *                          []{});
 *
 *     void MyApp::init(const vector<string> arguments) {
 *       prepare_command_line_options();
 *       handler_.process(arguments);
 *     }
 *
 *  All arguments which are not valid option names are are not values of options
 * are considered rest arguments. By default, rest arguments are not allowed and
 * will raise a std::invalid_argument exception. It is possible to allow them
 * through the constructor.
 *
 *  The CmdArgHandler class also provides functionality to help creating help
 *  screen or show how to use the the command line application. The method
 * `usage_lines()` produces a usage line showing all the option names, their
 * required or optional value with a meta variable. Similar, method
 * `option_descriptions()` will get all options and their descriptions. All this
 * is text wrapped at a configurable margin as well as, if needed, indented.
 *
 *  @internal
 *  The command line argument handling in CmdArgHandler is the bare minimum
 * needed for MySQL Router. It was needed to make sure that the application
 * would compile on system where the getopt library is not available.
 *  @endinternal
 *
 */
class HARNESS_EXPORT CmdArgHandler {
 public:
  /** @brief Constructor
   *
   * @param allow_rest_arguments_ whether we allow rest arguments or not
   * @param ignore_unknown_arguments_ whether we ignore unknown arguments or
   * give an error
   */
  explicit CmdArgHandler(bool allow_rest_arguments_,
                         bool ignore_unknown_arguments_ = false)
      : allow_rest_arguments(allow_rest_arguments_),
        ignore_unknown_arguments(ignore_unknown_arguments_) {}

  /** @brief Default constructor
   *
   * By default, rest arguments are not allowed and unknown arguments are not
   * ignored.
   */
  CmdArgHandler() : CmdArgHandler(false, false) {}

  /** @brief Adds a command line option
   *
   * Adds a command line option given names, description, value
   * requirement and optional action.
   *
   * `names` is a vector of strings which contains names starting
   * with either a single or double dash, `-` or `--`. It is possible
   * to add more than one name for an option.
   *
   * The description text will be used in the help output. Note that
   * this can be a very long as text will be wrapped (and optionally
   * indented). New lines in the description will be respected.
   *
   * The `metavar` argument is used in the usage text as a placeholder
   * for the (optional) value of the option, for example, when `metavar`
   * is set to `path`, the usage would show:
   *
   *     --config=<path>
   *
   * The value_req argument should be either:
   *
   * * `CmdOptionValueReq::none` : option has no value
   * * `CmdOptionValueReq::required` : option requires value
   * * `CmdOptionValueReq::optional` : option has optional value
   *
   * The `action` argument should be a `std::function` and is called
   * with the (optional) value of the option. The function should
   * accept only a `const std::string`.
   *
   * The `at_end_action` argument should be a `std::function`. This is optional
   * argument, if not provided then []{} is used as at_end_action. The
   * `at_end_action` is meant to be used for additional validation, if
   * particular set of options has to be used together, or if particular set of
   * options cannot be used together.
   *
   * Example usage:
   *
   *       handler_.add_option(OptionNames({"--config"}), "Configuration file",
   *                          CmdOptionValueReq::none, "",
   *                          [this](const string &value) {
   * this->set_config_file(value); },
   *                          []{});
   *
   * @internal
   * The `add_option` method will assert when `names` is empty,
   * one of the names is not valid or when a name was already used with
   * another option.
   * @endinternal
   *
   * @param names vector of string with option names, each starting with - or --
   * @param description descriptive text explaining the option
   * @param value_req value requirement of the option
   * @param metavar for formatting help text when option accepts a value
   * @param action action to perform when the option was found
   * @param at_end_action task to perform after all actions have been done
   */
  void add_option(
      const CmdOption::OptionNames &names, const std::string &description,
      const CmdOptionValueReq &value_req, const std::string &metavar,
      CmdOption::ActionFunc action,
      CmdOption::AtEndActionFunc at_end_action = [](const std::string &) {
      }) noexcept;

  void add_option(const CmdOption &other) noexcept;

  /** @brief Processes given command line arguments
   *
   * Processes given command line argument provided as a vector
   * of strings. It uses the stored option information added through the
   * `add_option()` method.
   *
   * Typically, the vector passed to process() are the argc and argv
   * arguments of the main() function:
   *
   *     process({argv + 1, argv + argc})
   *
   * When an option is found which requires an argument, optional or
   * not, process() will exit the application with an error message.
   *
   * If the option has an action defined, the function will be
   * executed with the (optional) value as argument.
   *
   * @param arguments vector of strings
   */
  void process(const std::vector<std::string> &arguments);

#ifndef NDEBUG
  bool debug_check_option_names(const CmdOption::OptionNames &names) const;
#endif

  /** @brief Checks whether given name is a valid option name
   *
   * Checks whether the given name is a valid option name.
   *
   * A valid option name should:
   *
   * * have at least consist of 2 characters
   * * start with a dash '-'
   * * match the reqular expression ^--[A-Za-z]{2}[A-Za-z_-]+$
   *
   * It is allowed to use the equal sign when giving value. Following options
   * are equal:
   *     --config /path/to/mysqlrouter.conf
   *     --config=/path/to/mysqlrouter.conf
   *
   * Throws std::invalid_argument when option name is not valid or
   * option was not registered.
   *
   * Examples of valid option names:
   *
   *     -h
   *     --with-ham
   *     --with_spam
   *
   * Example of invalid option names:
   *
   *     -help
   *     ---spam
   *     --x-ham
   *
   * @param name option name to check
   * @return true if name is valid; false otherwise
   */
  bool is_valid_option_name(const std::string &name) const noexcept;

  /** @brief Finds the option by name
   *
   * Finds the option by one of its name. The name should include the the dash
   * prefix.
   *
   * Example usage:
   *     // check if option name is already present
   *     assert(end() == find_option(name))
   *
   * @param name name of the option as string
   * @returns iterator object
   */
  OptionContainer::const_iterator find_option(
      const std::string &name) const noexcept;

  using UsagePredicate =
      std::function<std::pair<bool, CmdOption>(const CmdOption &)>;

  /** @brief Produces lines of text suitable to show usage
   *
   * Produces lines of text suitable to show usage of the command line
   * appliation. Each option is shown with all its names and with optional
   * or required value.
   *
   * The `prefix` argument can be used to add text, usually the name
   * of the command, in front of the options. The lines are indented
   * using the size of the prefix.
   *
   * The `rest_metavar` can be used to name non-options arguments.
   *
   * The `width` argument is used to set the maximum length of the lines.
   *
   * Example output when all lines are printed:
   *
   *     usage: mysqlrouter [-v|--version] [-h|--help] [-c|--config=<path>]
   *                        [-a=[<foo>]] [rest..]
   *
   *
   * @param prefix text in front of options (usually command name)
   * @param rest_metavar name of rest arguments (empty if not needed)
   * @param width maximum length of each line
   * @return vector of strings
   */
  std::vector<std::string> usage_lines(const std::string &prefix,
                                       const std::string &rest_metavar,
                                       size_t width) const noexcept {
    return usage_lines_if(
        prefix, rest_metavar, width,
        [](const CmdOption &opt) -> std::pair<bool, CmdOption> {
          return {true, opt};
        });
  }

  std::vector<std::string> usage_lines_if(
      const std::string &prefix, const std::string &rest_metavar, size_t width,
      UsagePredicate predicate) const noexcept;

  /** @brief Produces description of all options
   *
   * Produces description of all options. The result is typically shown
   * when the help screen is requested, for example when the `--help`
   * option is given.
   *
   * The `width` argument is used to set the maximum length of the lines. Text
   * is wrapped accordingly.
   *
   * Each description can be indented using space. The amount is given
   * by `indent` option.
   *
   * Example output when lines are printed:
   *
   *     -v, --version
   *           Show version
   *     -h, --help
   *           Show help
   *     -c <path>, --config <path>
   *           Path to the configuration file
   *
   * @param width maximum length of each line
   * @param indent how much the description should be indented.
   * @return vector of strings
   */
  std::vector<std::string> option_descriptions(
      const size_t width, const size_t indent) const noexcept;

  /** @brief Returns an iterator to first option
   *
   * Returns an iterator to the first option.
   *
   * @returns iterator
   */
  OptionContainer::const_iterator begin() { return options_.begin(); }

  /** @brief Returns an iterator to end of the option container
   *
   * Returns an iterator to the end of the option container.
   *
   * @returns iterator
   */
  OptionContainer::const_iterator end() { return options_.end(); }

  /** @brief Clears registered options
   *
   * Clears the registered options.
   *
   */
  void clear_options() { options_.clear(); }

  /** @brief Gets all registered options
   *
   * Returns as a reference to a vector of CmdOption objects.
   *
   * @return std::vector<CmdOption>
   */
  const std::vector<CmdOption> &get_options() const noexcept {
    return options_;
  }

  /** @brief Returns the rest arguments
   *
   * Returns the rest arguments.
   *
   * If rest arguments are not allow or there were no rest arguments,
   * an empty vector is returned.
   *
   * @return vector of strings
   */
  const std::vector<std::string> &get_rest_arguments() const noexcept {
    return rest_arguments_;
  }

  /** @brief Whether to allow rest arguments or not **/
  bool allow_rest_arguments;

  /** @brief Whether to ignore unknown arguments **/
  bool ignore_unknown_arguments;

  /** @brief The key is a section identificator (section name and optional
   * section key), the value is a map of all the overrides for a given section
   * (option/value pairs) **/
  using ConfigOverwrites = std::map<std::pair<std::string, std::string>,
                                    std::map<std::string, std::string>>;
  const ConfigOverwrites &get_config_overwrites() const noexcept {
    return config_overwrites_;
  }

 private:
  /** @brief Vector with registered options **/
  std::vector<CmdOption> options_;
  /** @brief Vector with arguments as strings not processed as options **/
  std::vector<std::string> rest_arguments_;
  /** @brief Keeps configuration options overwrites **/
  ConfigOverwrites config_overwrites_;
};

#endif  // HARNESS_ARG_HANDLER_INCLUDED
