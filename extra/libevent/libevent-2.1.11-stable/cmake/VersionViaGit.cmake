# This module defines the following variables utilizing
# git to determine the parent tag. And if found the macro
# will attempt to parse them in the github tag fomat
#
# Useful for auto-versioning in our CMakeLists
#
#  EVENT_GIT___VERSION_MAJOR - Major version.
#  EVENT_GIT___VERSION_MINOR - Minor version
#  EVENT_GIT___VERSION_STAGE - Stage version
#
# Example usage:
#
# event_fuzzy_version_from_git()
#    message("Libvent major=${EVENT_GIT___VERSION_MAJOR}")
#    message("        minor=${EVENT_GIT___VERSION_MINOR}")
#    message("        patch=${EVENT_GIT___VERSION_PATCH}")
#    message("        stage=${EVENT_GIT___VERSION_STAGE}")
# endif()

include(FindGit)

macro(event_fuzzy_version_from_git)
	# set our defaults.
	set(EVENT_GIT___VERSION_MAJOR 2)
	set(EVENT_GIT___VERSION_MINOR 1)
	set(EVENT_GIT___VERSION_PATCH 11)
	set(EVENT_GIT___VERSION_STAGE "stable")

	find_package(Git)

	if (GIT_FOUND)
		execute_process(
			COMMAND
				${GIT_EXECUTABLE} describe --abbrev=0
			WORKING_DIRECTORY
				${PROJECT_SOURCE_DIR}
			RESULT_VARIABLE
				GITRET
			OUTPUT_VARIABLE
				GITVERSION
			OUTPUT_STRIP_TRAILING_WHITESPACE
		)

		string(REGEX REPLACE "[\\._-]" ";" VERSION_LIST "${GITVERSION}")
		list(LENGTH VERSION_LIST VERSION_LIST_LENGTH)

		if ((GITRET EQUAL 0) AND (VERSION_LIST_LENGTH EQUAL 5))
			list(GET VERSION_LIST 1 _MAJOR)
			list(GET VERSION_LIST 2 _MINOR)
			list(GET VERSION_LIST 3 _PATCH)
			list(GET VERSION_LIST 4 _STAGE)

			set(_DEFAULT_VERSION "${EVENT_GIT___VERSION_MAJOR}.${EVENT_GIT___VERSION_MINOR}.${EVENT_GIT___VERSION_PATCH}-${EVENT_GIT___VERSION_STAGE}")
			set(_GIT_VERSION     "${_MAJOR}.${_MINOR}.${_PATCH}-${_STAGE}")

			if (${_DEFAULT_VERSION} VERSION_LESS ${_GIT_VERSION})
				set(EVENT_GIT___VERSION_MAJOR ${_MAJOR})
				set(EVENT_GIT___VERSION_MINOR ${_MINOR})
				set(EVENT_GIT___VERSION_PATCH ${_PATCH})
				set(EVENT_GIT___VERSION_STAGE ${_STAGE})
			endif()
		endif()
	endif()
endmacro()
