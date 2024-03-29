##############################################################################
# core library target

add_library(ach_core STATIC
	ach/clangd/code_tokenizer.cpp
	ach/clangd/core.cpp
	ach/clangd/spliced_text_parser.cpp
	ach/text/extractor.cpp
	ach/mirror/color_tokenizer.cpp
	ach/mirror/core.cpp
	ach/web/html_builder.cpp
)

target_include_directories(ach_core
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(ach_core
	PRIVATE
		cxx_std_17
)

target_compile_options(ach_core
	PRIVATE
		$<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic -ffast-math>
		$<$<CXX_COMPILER_ID:Clang>:-Wall -Wpedantic -ffast-math>
		$<$<CXX_COMPILER_ID:MSVC>:/W4>
)

if(ACH_ENABLE_LTO)
	set_target_properties(ach_core PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

target_link_libraries(ach_core PUBLIC enum.hpp)

##############################################################################

if(ACH_BUILD_EXECUTABLE_CLI)
	add_subdirectory(cli)
endif()

if(ACH_BUILD_PYTHON_MODULE)
	add_subdirectory(python)
endif()

include(CTest) # adds option BUILD_TESTING (default ON)
if(BUILD_TESTING AND ACH_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
