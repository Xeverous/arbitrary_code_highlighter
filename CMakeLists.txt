# https://pabloariasal.github.io/2018/02/19/its-time-to-do-cmake-right/
# https://cliutils.gitlab.io/modern-cmake/

# minimal version is 3.9 because of IPO policy changes in this version
# https://cmake.org/cmake/help/latest/policy/CMP0069.html#policy:CMP0069
cmake_minimum_required(VERSION 3.9...3.14)

if(${CMAKE_VERSION} VERSION_LESS 3.12)
    cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

project(arbitrary_code_highlighter
	VERSION 1.3.7
	DESCRIPTION "Embed arbitrary code in HTML <span> tags with CSS classes of your choice for rich syntax highlightning."
	LANGUAGES CXX)

set(default_build_type "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
	set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
		STRING "Choose the type of build." FORCE)
	# Set the possible values of build type for cmake-gui
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
		"Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

##############################################################################
# options for customizing the build

option(ACH_BUILD_EXECUTABLE_CLI
	"build ACH command line interface program" ON)
option(ACH_BUILD_PYTHON_MODULE
	"build ACH Python extension module" ON)
option(ACH_BUILD_TESTS
	"build ACH tests" ON)
option(ACH_ENABLE_LTO
	"enable link-time optimization for ACH targets" ON)

##############################################################################
# specify explicitly where to output all binary objects

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(ACH_ENABLE_LTO)
	include(CheckIPOSupported)
	check_ipo_supported(RESULT IPO_SUPPORTED OUTPUT IPO_ERROR)

	if(IPO_SUPPORTED)
		message(STATUS "IPO / LTO supported, enabling for ACH")
	else()
		message(FATAL_ERROR "IPO / LTO not supported: <${IPO_ERROR}>, will not enable")
	endif()
endif()

##############################################################################

if(ACH_BUILD_PYTHON_MODULE)
	add_subdirectory(external/pybind11)
endif()

add_subdirectory(external/enum.hpp)
add_subdirectory(src)
