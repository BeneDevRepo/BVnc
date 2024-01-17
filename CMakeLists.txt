cmake_minimum_required(VERSION 3.0.0)
project(Software_Rasterizer VERSION 0.1.0)

include(CTest)
enable_testing()

add_compile_options(-O2 -Wall -Wno-c++98-compat -Wno-c++98-compat-pedantic
	-Wno-missing-prototypes -Wno-unused-parameter -Wno-shadow-field-in-constructor -Wno-shadow
	-Wno-global-constructors -Wno-exit-time-destructors -Wno-implicit-int-float-conversion -Wno-float-equal -Wsuggest-override -Werror=suggest-override)

set(CMAKE_CXX_STANDARD 17)

file(GLOB_RECURSE SRC "src/*.cpp")
file(GLOB_RECURSE SRC "modules/*.cpp")

add_executable(bvnc main.cpp ${SRC})
target_include_directories(bvnc PUBLIC src)
target_include_directories(bvnc PUBLIC modules)
target_link_libraries(bvnc "Ws2_32.lib") # Winsock 2

target_compile_definitions(bvnc PUBLIC WIN32_LEAN_AND_MEAN) # fuer winsock 2

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)