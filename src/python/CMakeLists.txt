set(PYBIND11_CPP_STANDARD
	$<$<CXX_COMPILER_ID:GNU>:-std=c++1z>
	$<$<CXX_COMPILER_ID:Clang>:-std=c++1z>
	$<$<CXX_COMPILER_ID:MSVC>:/std:c++latest)
find_package(pybind11 REQUIRED)

pybind11_add_module(arbitrary_code_highlighter binding.cpp)
target_link_libraries(arbitrary_code_highlighter PRIVATE ach_core)
