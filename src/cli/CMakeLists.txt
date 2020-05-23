find_package(Boost 1.65 REQUIRED
	COMPONENTS
		program_options
)

add_executable(ach_cli
	parse_args.cpp
	main.cpp
)

target_include_directories(ach_cli
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(ach_cli
	PRIVATE
		cxx_std_17
)

target_compile_options(ach_cli
	PRIVATE
		$<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic -ffast-math>
		$<$<CXX_COMPILER_ID:Clang>:-Wall -Wpedantic -ffast-math>
		$<$<CXX_COMPILER_ID:MSVC>:/W4>
)

target_link_libraries(ach_cli
	PRIVATE
		ach_core
		Boost::program_options
)

if(ACH_ENABLE_LTO)
	set_target_properties(ach_cli PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()
