project(pyach)

set(PYBIND11_CPP_STANDARD
	$<$<CXX_COMPILER_ID:GNU>:-std=c++1z>
	$<$<CXX_COMPILER_ID:Clang>:-std=c++1z>
	$<$<CXX_COMPILER_ID:MSVC>:/std:c++latest>)
add_subdirectory(pybind11)
pybind11_add_module(pyach binding.cpp)
target_link_libraries(pyach PRIVATE ach_core)

if(ACH_ENABLE_LTO)
	set_target_properties(pyach PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()
