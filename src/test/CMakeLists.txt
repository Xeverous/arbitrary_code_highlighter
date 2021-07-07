# declare dependencies

find_package(Boost 1.65 REQUIRED
	COMPONENTS
		unit_test_framework
)

##############################################################################
# create target and set its properties

add_executable(ach_test
	main.cpp
	text_extractor_tests.cpp
	cc_highlighter_tests.cpp
)

target_include_directories(ach_test
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
)

##############################################################################
# setup compiler flags

target_compile_features(ach_test
	PRIVATE
		cxx_std_17
)

# add warnings if supported
target_compile_options(ach_test
	PRIVATE
		$<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic -ffast-math>
		$<$<CXX_COMPILER_ID:Clang>:-Wall -Wpedantic -ffast-math>
		$<$<CXX_COMPILER_ID:MSVC>:/W4>
)

##############################################################################
# add libs that require linking and/or include paths

target_link_libraries(ach_test
	PRIVATE
		ach_core
		Boost::boost
)

##############################################################################
# register test executable for CTest

add_test(NAME unit_tests COMMAND ach_test)
