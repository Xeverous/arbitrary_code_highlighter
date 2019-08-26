add_library(ach_core STATIC
	color_tokenizer.cpp
	text_extractor.cpp
	html_builder.cpp
	core.cpp
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

# nothing to link against
# target_link_libraries(ach_core PRIVATE)
