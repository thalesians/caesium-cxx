cmake_minimum_required( VERSION 3.5 )

set( CMAKE_INSTALL_PREFIX /usr/local )

project(sodium)
include(CheckCXXCompilerFlag)

check_cxx_compiler_flag("-std=c++11" SUPPORTS_CXX11)
if( SUPPORTS_CXX11 )
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
else()
    check_cxx_compiler_flag("-std=c++0x" SUPPORTS_CXX0X)
    if( SUPPORTS_CXX0X )
        set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
    else()
        message("C++11 is not supported")
        add_definitions(-DSODIUM_NO_CXX11)
    endif()
endif()


set( SODIUM_BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/sodium )

set( CMAKE_INCLUDE_CURRENT_DIR ON )

find_package( Boost REQUIRED )

file( GLOB_RECURSE 
    SODIUM_INCLUDE_FILES 
    ${SODIUM_BASE_DIR}/*.h
    )

file( GLOB_RECURSE 
    SODIUM_SOURCE_FILES
    ${SODIUM_BASE_DIR}/*.cpp
    )

file(
    COPY ${SODIUM_INCLUDE_FILES} 
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/include/sodium
    )

include_directories( ${Boost_INCLUDE_DIRS} )

set( ALL_SOURCES ${SODIUM_INCLUDE_FILES} ${SODIUM_SOURCE_FILES} )

add_library( sodium SHARED ${ALL_SOURCES} )
add_library( libsodium STATIC ${ALL_SOURCES} )

add_executable(bench_cpu benchmarks/bench.cpp)
find_package(Threads REQUIRED)

target_link_libraries(bench_cpu
    PRIVATE libsodium Threads::Threads m     # m = libm for sin()
)

target_include_directories(bench_cpu PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/include      # sodium headers copied earlier
)



set_target_properties( libsodium PROPERTIES OUTPUT_NAME sodium )

# INSTALL
# -------

install (
    TARGETS sodium libsodium
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    )

# Install header files
install (
    FILES ${SODIUM_INCLUDE_FILES}
    DESTINATION include/sodium
    )


#demo: inter-partition concurrency
add_executable(demo_inter demos/inter.cpp)
target_link_libraries(demo_inter PRIVATE libsodium Threads::Threads)
target_include_directories(demo_inter PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/include) 