project(pyach)

add_subdirectory(pybind11)
pybind11_add_module(pyach binding.cpp)
target_link_libraries(pyach PRIVATE ach_core)

if(ACH_ENABLE_LTO)
	set_target_properties(pyach PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()
