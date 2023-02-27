# Copyright (c) 2020, 2022, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms, as
# designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
# This program is distributed in the hope that it will be useful,  but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
# the GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
"""Plugin Manager used for simplified plugin registration"""

import inspect
import re
from functools import wraps, partial

# Callbacks for additional handling on registered plugin
# functions should be added here, they should be in the form
# def callback(definition)
#
# Where definition is an instance of FunctionData
_registration_callbacks = []


def add_registration_callback(callback):
    _registration_callbacks.append(callback)


def validate_shell_version(min=None, max=None):
    """
    Validates the plugin Shell version requirements for plugin.
    """
    import mysqlsh

    raw_version = mysqlsh.globals.shell.version
    shell_version = tuple([int(v) for v in raw_version.split()[1].split('-')[0].split('.')])

    # Ensures the plugin can be installed on the current version of the Shell
    min_version_ok = True
    if not min is None:
        min_version = tuple([int(v) for v in min.split('.')])
        min_version_ok = shell_version >= min_version

    max_version_ok = True
    if not max is None:
        max_version = tuple([int(v) for v in max.split('.')])
        max_version_ok = shell_version <= max_version

    error = ""
    if not min_version_ok or not max_version_ok:
        if min is not None and max is not None:
            error = f"This plugin requires Shell between versions {min} and {max}."
        elif min is not None:
            error = f"This plugin requires at least Shell version {min}."
        elif max is not None:
            error = f"This plugin does not work on Shell versions newer than {max}."

    if len(error) != 0:
        raise Exception(error)
class PluginRegistrar:
    """Helper class to register a shell plugin.

    It should be used by calling: register_object which requires:
    - The object path as it should be seen in the shell.
    - The python function that will be added as members of the object.
    - The documentation for the object (brief and details).

    Examples for object name:
    - 'cloud': Would register the 'cloud' as a shell global object.
    - 'cloud.os': Would register the 'os' object as a child of the 'cloud'
    global object.

    The name can have any number of parents in the chain, the conditions is
    that such parent should be already registered.

    For this reason, the caller may have to do something like:

    register_object('myGlobal', [], {'brief':'...', 'details':[]})
    register_object('myGlobal.myChild', [], {'brief':'...', 'details':[]})

    Before calling:
    register_object('myGlobal.myChild.myGrandChild',
    [function1, function2], {'brief':'...', 'details':[]})

    Any object that is already defined will NOT be re-defined.
    """

    @staticmethod
    def sphinx_2shell_type(type):
        """Helper function to translate the sphinx types into the types required by
        the shell"""
        if type == "str":
            return "string"
        elif type == "int":
            return "integer"
        elif type == "dict":
            return "dictionary"
        elif type == "list":
            return "array"
        else:
            return type

    class ItemDoc:
        """ Simple container for brief and details of the different items"""

        def __init__(self):
            self.brief = ""
            self.details = None

    class OptionData:
        """Holds the documentation for a specific option.

        This object is completely created from parsed docs for an option.

        TODO: Does not support 'details' for option as sphinx doesn't seem to
        have a way to specify that, so the details MUST be placed at the
        function details section (which is OK considering that's what the
        shell will do when rendering the help data)
        """

        def __init__(self, option_def):
            self.docs = PluginRegistrar.ItemDoc()
            self.name = option_def["name"]
            self.type = option_def.get("type")
            self.docs.brief = option_def["brief"]
            self.options = []
            self.required = option_def.get("required", False)

            if "details" in option_def:
                self.docs.details = option_def["details"]

            if "options" in option_def:
                for option in option_def["options"]:
                    self.options.append(PluginRegistrar.OptionData(option))

            self.default = None

        def format_info(self):
            """Translates the parameter definition to shell required format"""
            info = {
                "name": self.name,
                "brief": self.docs.brief,
            }

            if self.type:
                info["type"] = PluginRegistrar.sphinx_2shell_type(self.type)

            if self.docs.details is not None:
                info["details"] = self.docs.details

            if self.required:
                info["required"] = True

            if self.default is not None:
                info["default"] = self.default

            if "type" in info.keys() and info["type"] == "dictionary":
                options = []
                for o in self.options:
                    options.append(o.format_info())
                info["options"] = options

            return info

    class ParameterData:
        """Holds the documentation for a specific argument.

        This object is created part from document parsing and part from
        inspection of the function definition.

        The function definition provides parameter name, whether it is
        optional or not and the default value.

        The documentation provides type, brief and options (when applicable)

        TODO: Does not support 'details' for option as sphinx doesn't seem to
        have a way to specify that, so the details MUST be placed at the
        function details section (which is OK considering that's what the
        shell will do when rendering the help data)
        """

        def __init__(self, parameter):
            self.definition = parameter
            self.docs = PluginRegistrar.ItemDoc()
            self.type = ""
            self.options = []

        def set_info(self, info):
            """Fills parameter details that come from documentation"""
            if "type" in info:
                self.type = info["type"]
            self.docs.brief = info["brief"]

            # Note: this is here even it's not really supported.
            try:
                self.docs.details = info["details"]
            except KeyError:
                pass

            try:
                options = info["options"]
                for option in options:
                    self.options.append(PluginRegistrar.OptionData(option))
            except KeyError:
                pass

        def format_info(self):
            """Translates the parameter definition to shell required format"""
            info = {
                "name": self.definition.name,
                "brief": self.docs.brief,
            }

            if self.type:
                info["type"] = PluginRegistrar.sphinx_2shell_type(self.type)

            if self.docs.details is not None:
                info["details"] = self.docs.details

            if self.definition.default != inspect.Parameter.empty:
                info["default"] = self.definition.default
                info["required"] = False

            if "type" in info.keys() and info["type"] == "dictionary" and len(self.options):
                options = []
                for o in self.options:
                    options.append(o.format_info())
                info["options"] = options

            if self.definition.kind == inspect.Parameter.VAR_KEYWORD:
                info["required"] = False

            return info

    class FunctionData:
        """Holds the documentation for the function.

        This object is created part from document parsing and part from
        inspection of the function definition.

        The function definition provides parameter information.

        The documentation provides  brief and details.
        """

        def __init__(self, function,
                     fully_qualified_name=None,
                     shell=True,
                     cli=False,
                     web=False):
            # Get the plugin name by stripping the function name
            name = None
            if not fully_qualified_name is None:
                name_path = fully_qualified_name.split(".")
                name = name_path[-1]

            self.function = function
            self.fully_qualified_name = fully_qualified_name
            self.name = function.__name__ if name is None else name
            self.docs = PluginRegistrar.ItemDoc()

            # These flags indicate where the function should be available
            self.shell = shell
            self.cli = cli
            self.web = web

            signature = inspect.signature(function)
            self.parameters = [
                PluginRegistrar.ParameterData(p)
                for p in signature.parameters.values()
            ]

            docs = inspect.getdoc(function)

            if docs:
                self._parse_docs(docs)

        def format_info(self):
            """Translates the parameter definition to shell required format"""
            info = {"brief": self.docs.brief, "details": self.docs.details}

            if len(self.parameters):
                params = []
                for param in self.parameters:
                    params.append(param.format_info())

                info["parameters"] = params

            if self.cli:
                info["cli"] = self.cli

            return info

        def _get_doc_section(self, name):
            """Ensures the indicated section exists and returns it"""
            try:
                return self._doc_sections[name]
            except KeyError:
                self._doc_sections[name] = []
                self._doc_section_list.append(name)
                return self._doc_sections[name]

        def _parse_docs(self, docstring):
            """Main parser function.

            Retrieves the documentation and splits it in sections, then
            process the different sections such as: brief, parameters and
            details.

            There are different sections that are considered on the parsing:
            - 'Args:': Contains the parameter definitions.
            - 'Keyword Args:': Contains parameter definitions for keyword
               arguments.
            - 'Allowed options for <dictionary>:': Contains option definitions
               for a dictionary parameter or option.

            These options will be consumed during the parsing process.
            """
            self._doc_index = 0
            self._doc_lines = [line for line in docstring.split("\n")]
            self._doc_sections = {}
            self._doc_section_list = []

            # Document splitting logic
            section_name = "global"
            section = self._get_doc_section(section_name)
            global_count = 0
            for line in self._doc_lines:
                if line.endswith(":"):
                    section_name = line
                    section = self._get_doc_section(section_name)
                elif section_name.startswith("global") or len(line) == 0 or line[0] == " " or line.startswith("* "):
                    section.append(line)
                else:
                    global_count = global_count + 1
                    section_name = "global{}".format(global_count)
                    section = self._get_doc_section(section_name)
                    section.append(line)

            # Removes leading and trailing blank lines from all the sections
            for name, section in self._doc_sections.items():
                name = name  # name is not used

                while section and len(section[0]) == 0:
                    section.pop(0)

                while section and len(section[-1]) == 0:
                    section.pop(-1)

                if len(section) == 0:
                    raise Exception(f"Invalid format: section without content: {name}")

            # Parses the function brief description
            self._parse_function_brief()

            # Triggers the parameter parsing and updates the parameter
            # definitions
            arg_docs = self._parse_args()
            if arg_docs:
                self._set_parameter_docs(arg_docs)

            # Constructs the function details
            self._parse_details()

        def _parse_function_brief(self):
            """Constructs the function brief from the beginning of the docs and until
            the first blank line is found"""
            brief_lines = []
            lines = self._doc_sections["global"]
            for line in lines:
                if len(line):
                    brief_lines.append(line)
                else:
                    break

            count = len(brief_lines)
            if count:
                self.docs.brief = " ".join(brief_lines)

            # Removes any subsequent blank line, rest will be part of the
            # function details
            while (
                count < len(self._doc_sections["global"])
                and len(self._doc_sections["global"][count]) == 0
            ):
                count = count + 1

            self._doc_sections["global"] = self._doc_sections["global"][count:]

        def _parse_args(self):
            """Parses the function arguments.

            This function will ensure:
            - All the parameters are documented
            - No nonexisting parameters are documented
            """
            args_section = self._doc_sections.pop("Args:", None)
            arg_docs = []

            if len(self.parameters) and args_section is None:
                raise Exception("Missing arguments documentation")
            elif args_section is None and len(self.parameters):
                raise Exception("Unexpected argument documentation")
            elif args_section:
                while len(args_section):
                    arg_docs.append(self._parse_parameter_doc(args_section))

            return arg_docs

        def _parse_return_value(self):
            """Some day it will parse the return section
            TODO: The shell extension objects do not do any handling of the
            return value so it's documentation is embedded into the function
            details (in an ugly way).
            """
            pass

        def _parse_details(self):
            """ Creates the details section of the function.
            When this function is called, the remaining sections will be glued
            together to create the function details.

            This means, it is possible to define totally unrelated sections of
            documentation and they will also be included on the shell docs.
            """
            details = []
            paragraph = []
            for section_name in self._doc_section_list:
                section = self._doc_sections.pop(section_name, None)
                if section:
                    # Sections other than globals need to include the header
                    if not section_name.startswith("global"):
                        details.append("<b>" + section_name + "</b>")

                    # Backup the section index, in case it was not really a
                    # section, i.e. it is the description of a list of items
                    # the bold will be removed
                    section_index = len(details) - 1

                    for line in section:
                        stripped_line = line.strip()
                        if len(stripped_line) == 0:
                            details.append(" ".join(paragraph))
                            paragraph.clear()
                        elif line.startswith("* "):
                            # If this is the first thing added to the section,
                            # then it was not a real section but the
                            # description of a list of items. Remove the <b>
                            if section_index == len(details) - 2:
                                details[section_index] = section_name
                            details.append(line.replace("* ", "@li "))
                        else:
                            paragraph.append(line)

                    if len(paragraph):
                        details.append(" ".join(paragraph))
                        paragraph.clear()

            if details:
                self.docs.details = details

        def _set_parameter_docs(self, docs):
            """Fills all the parameter definitions with the information coming
            from document parsing"""
            for doc in docs:
                target_param = None
                for param in self.parameters:
                    if doc["name"] == param.definition.name:
                        target_param = param

                if target_param is None:
                    raise Exception(
                        "Parameter does not exist but is documented: {}."
                        "".format(doc)
                    )
                else:
                    target_param.set_info(doc)

            missing = []
            for p in self.parameters:
                if len(p.docs.brief) == 0:
                    missing.append(p.definition.name)

            if len(missing):
                raise Exception(
                    "Missing documentation for the following parameters: "
                    "{}".format(", ".join(missing))
                )

        def _parse_parameter_doc(self, section):
            """Parses the first parameter/option definition coming on the
            section.

            Once it is parsed it will be removed from the section, next call
            will process the next definition.
            """
            # Parameters are documented as <name> [(<type>)]: <brief>
            # kwards is documented as **<name>: <brief>
            info = {}
            param_line = section.pop(0)
            param_doc = param_line.strip()
            brief = []

            match = re.match(
                "^(\*\*)?([a-z|A-Z|_][a-z|A-Z|0-9|_]*)(\\s(\\(([a-z|,|\s]+)\\)))?:\\s(.*)$", param_doc)

            if match:
                options_section = None
                info["name"] = match.group(2)

                # kwargs definition
                if match.group(1):
                    info["type"] = "dictionary"
                    options_section = self._doc_sections.pop(
                        "Keyword Args:", None)

                # data type definition included
                elif match.group(5):
                    param_options = match.group(5).split(",")
                    info["type"] = PluginRegistrar.sphinx_2shell_type(
                        param_options[0])

                    if len(param_options) > 1:
                        if param_options[1] == "required":
                            info["required"] = True

                    if info["type"] == "dictionary":
                        options_section = self._doc_sections.pop(
                            "Allowed options for {}:".format(info["name"]),
                            None,
                        )

                if options_section:
                    info["options"] = []
                    while len(options_section):
                        info["options"].append(
                            self._parse_parameter_doc(options_section)
                        )

                brief.append(match.group(6))

                ident_size = param_line.find(param_doc)

                while (
                    len(section)
                    and len(section[0]) > ident_size
                    and section[0][ident_size + 1] == " "
                ):
                    brief.append(section.pop(0).strip())

                info["brief"] = " ".join(brief)
            else:
                raise Exception("Invalid parameter documentation: {}"
                                "".format(param_line))

            return info

    def __init__(self):
        pass

    def _get_python_name(self, name):
        return "_".join([x.lower() for x in re.split("([A-Z][a-z0-9_]*)", name) if x])

    def register_object(self, name, docs, members=None):
        try:
            plugin_obj = self.get_plugin_object(name, docs)
            if plugin_obj is None:
                raise ValueError(f"Plugin object {name} was not found.")

            if members is not None:
                for member in members:
                    if inspect.isfunction(member):
                        self.register_function(plugin_obj, member)
        except Exception as e:
            raise Exception(
                f"Could not register object '{name}'.\nERROR: {str(e)}"
            )

    def get_plugin_object(self, name, docs):
        """Get the leaf object in the object hierarchy defined in name.

        If the leaf object does not exist, it will be created with the provided
        documentation.

        If any object in the middle of the chain does not exist, an error will
        be raised.
        """
        import mysqlsh

        shell_obj = mysqlsh.globals.shell

        hierarchy = name.split(".")

        try:
            plugin_obj = None
            if len(hierarchy) > 1:
                # Get the name of the last plugin object in the
                # fully qualified name
                plugin_name = hierarchy[-1]

                # Loop over all plugins in the hierarchy and ensure they exist
                parent = mysqlsh.globals
                for h_name in hierarchy[0:-1]:
                    py_name = h_name
                    # Global objects have the same name in JS/PY, but child
                    # objects are registered as properties in which case naming
                    # convention is followed
                    if parent != mysqlsh.globals:
                        py_name = self._get_python_name(h_name)
                    if py_name in dir(parent):
                        parent = getattr(parent, py_name)
                    else:
                        raise Exception(
                            f"Object {py_name} not found in hierarchy: {name}"
                        )

                # Return the plugin object if it already exists
                py_plugin_name = self._get_python_name(plugin_name)
                plugin_obj = getattr(parent, py_plugin_name, None)
                if not plugin_obj:
                    if docs is None:
                        raise ValueError(
                            f"No docs specified for plugin object " f"{name}"
                        )
                    # If it does not exist yet, create it
                    plugin_obj = shell_obj.create_extension_object()
                    shell_obj.add_extension_object_member(
                        parent, plugin_name, plugin_obj, docs
                    )
            else:
                try:
                    plugin_obj = getattr(mysqlsh.globals, name, None)
                except KeyError:
                    plugin_obj = shell_obj.create_extension_object()
                    shell_obj.register_global(name, plugin_obj, docs)
            return plugin_obj
        except Exception as e:
            raise Exception(
                f"Could not get plugin object '{name}'.\nERROR: {str(e)}"
            )

    def register_function(
        self, plugin_obj, function,
        fully_qualified_name=None,
        shell=True,
        cli=False,
        web=False
    ):
        """Registers a new member into the provided shell extension object"""
        import mysqlsh

        shell_obj = mysqlsh.globals.shell

        if cli and not shell:
            raise Exception(
                "The CLI can only be enabled on registered functions.")

        definition = PluginRegistrar.FunctionData(
            function, shell=shell,
            fully_qualified_name=fully_qualified_name,
            cli=cli,
            web=web)

        try:
            if shell:
                shell_obj.add_extension_object_member(
                    plugin_obj,
                    definition.name,
                    function,
                    definition.format_info(),
                )

            # On success registration, the function is reported to the callbacks
            for callback in _registration_callbacks:
                callback(definition)
        except Exception as e:
            raise Exception(
                f"Could not add function '{definition.name}' "
                f"using {definition.format_info()}.\nERROR: {str(e)}"
            )

    def register_property(self, property):
        pass

def plugin(cls=None, shell_version_min=None, shell_version_max=None, parent=None):
    """Decorator to register a class as a Shell extension object

    This decorator can be used to register a class structure as a Shell
    extension object. After registering the class it self it will also scan for
    inner classes and register them as nested extension objects.

    Args:
        cls (class): The class structure to register

    Returns:

    """
    if cls is None:
        return partial(plugin, shell_version_min=shell_version_min, shell_version_max=shell_version_max, parent=parent)
    else:
        try:
            validate_shell_version(shell_version_min, shell_version_max)

            plugin_manager = PluginRegistrar()

            # Use the class name as the plugin name and the DocString as docs and
            # register the class as Shell plugin
            object_qualified_name = cls.__name__ if parent is None else parent + "." + cls.__name__
            plugin_manager.register_object(
                object_qualified_name, PluginRegistrar.FunctionData(cls).format_info()
            )

            def register_inner_classes(cls):
                """Register all inner classes as nested extension objects

                Args:
                    cls (class): The class containing the subclasses to register
                """
                # Get all inner classes
                inner_classes = [
                    inner_class
                    for inner_class in cls.__dict__.values()
                    if inspect.isclass(inner_class)
                ]

                # Register those as extension objects
                for inner_class in inner_classes:
                    plugin_manager.register_object(
                        inner_class.__qualname__,
                        PluginRegistrar.FunctionData(inner_class).format_info(),
                    )

                    # Recursively also register the inner classes of this class
                    register_inner_classes(inner_class)

                # Create an instance of the class to also register the plugin functions
                # initalized in the class's constructor __init__
                cls()

            # Register the inner classes as nested extension objects
            register_inner_classes(cls)

        except Exception as e:
            print(
                "Could not register plugin object '{0}'.\nERROR: {1}".format(
                    cls.__name__, str(e)
                )
            )
            raise

        @wraps(cls)
        def wrapper(*args, **kwargs):
            result = cls(*args, **kwargs)
            return result

        return wrapper


def plugin_function(fully_qualified_name,
                    plugin_docs=None,
                    shell=True,
                    cli=False,
                    web=False):
    """Decorator factory to register Shell plugins functions

    Args:
        fully_qualified_name (str): The fully qualified name of the function,
            e.g. cloud.create.mysqlDbSystem
        plugin_docs (dict): The documentation structure of the plugin. This
            is only required for the first function that will be registered
        cli (bool): Defines whether the function should be available in the CLI

    Returns:
        The decorator function

    """

    def decorator(function):
        try:
            plugin_manager = PluginRegistrar()

            # Get the plugin name by stripping the function name
            name_path = fully_qualified_name.split(".")
            plugin_name = ".".join(name_path[0:-1])
            function_name = name_path[-1]

            # Get the plugin object or create it if it is available yet
            plugin_obj = plugin_manager.get_plugin_object(
                plugin_name, plugin_docs
            )

            # register the function
            plugin_manager.register_function(
                plugin_obj, function,
                fully_qualified_name=fully_qualified_name,
                shell=shell,
                cli=cli,
                web=web
            )
        except Exception as e:
            print(
                f"Could not register function '{function_name}' as a member "
                f"of the {plugin_name} plugin object.\nERROR: {str(e)}"
            )
            raise

        @wraps(function)
        def wrapper(*args, **kwargs):
            # TODO investigate if automated handling of certain args is
            # beneficial
            result = function(*args, **kwargs)
            # TODO investigate if transformation of results should be done
            # when functions are called from the shell prompt
            return result

        return wrapper

    return decorator
