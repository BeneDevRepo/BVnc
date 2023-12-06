cmake_minimum_required(VERSION 3.0.0)
project(Software_Rasterizer VERSION 0.1.0)

include(CTest)
enable_testing()

add_compile_options(-O2 -Wall -Wno-c++98-compat -Wno-c++98-compat-pedantic
	-Wno-missing-prototypes -Wno-unused-parameter -Wno-shadow-field-in-constructor -Wno-shadow
	-Wno-global-constructors -Wno-exit-time-destructors -Wno-implicit-int-float-conversion -Wno-float-equal -Wsuggest-override -Werror=suggest-override)

set(CMAKE_CXX_STANDARD 20)

file(GLOB_RECURSE SRC "src/*.cpp")
file(GLOB_RECURSE SRC "modules/*.cpp")

add_executable(prog main.cpp ${SRC})
target_include_directories(prog PUBLIC src)
target_include_directories(prog PUBLIC modules)
target_link_libraries(prog "Ws2_32.lib")

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)