
# ==================
#  Target: generate
# ==================

add_executable(generate generate.cpp
			lib/mathutil.cpp
			lib/angular.cpp)
set_property(TARGET generate PROPERTY CXX_STANDARD 11)
target_include_directories(generate
  PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/libecpint
    ${CMAKE_CURRENT_BINARY_DIR}/../include/libecpint
)

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/generated/ecpint_gen.part" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/generated/qgen.part" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/generated")
find_package (Python COMPONENTS Interpreter)
execute_process(COMMAND ${Python_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/makelist.py" "${LIBECPINT_MAX_L}" "${CMAKE_CURRENT_BINARY_DIR}")
file(READ "${CMAKE_CURRENT_BINARY_DIR}/qlist.txt" GENERATED_SOURCES)
STRING(REGEX REPLACE "\n" ";" GENERATED_SOURCES "${GENERATED_SOURCES}")
add_custom_command(
	OUTPUT ${GENERATED_SOURCES}
	COMMAND generate "${PROJECT_SOURCE_DIR}/include/libecpint/"
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/generated/ecpint_gen.part
	COMMENT "Generating and unrolling integral code."
)

# ================
#  Target: ecpint
# ================

file(GLOB_RECURSE LIB_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/lib/*.cpp")
add_library(ecpint ${LIB_SOURCES} ${GENERATED_SOURCES})
add_library(ECPINT::ecpint ALIAS ecpint)

if(NOT BUILD_SHARED_LIBS)
  set_property(TARGET ecpint PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()

set_property(TARGET ecpint PROPERTY CXX_STANDARD 11)
set_target_properties(ecpint PROPERTIES SOVERSION ${API_VERSION})
target_include_directories(ecpint
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/libecpint>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/../include/libecpint>
    $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include/libecpint>
    $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include>
  PRIVATE
    ${PUGIXML_INCLUDE_DIR}
)
target_link_libraries(ecpint
 PRIVATE
  Faddeeva
  pugixml::pugixml
)

# =========
#  Install
# =========

install(TARGETS ecpint EXPORT ecpintTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})
include(CMakePackageConfigHelpers)
# Version file
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/ecpint-config-version.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY AnyNewerVersion
)
# Config file
configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/ecpint-config.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ecpint
)
# Targets files
export(
  EXPORT ecpintTargets
  FILE ${CMAKE_CURRENT_BINARY_DIR}/ecpint-targets.cmake
)
install(
  EXPORT ecpintTargets
  FILE ecpint-targets.cmake
  NAMESPACE ECPINT::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ecpint
)
install(
  FILES
    ${CMAKE_CURRENT_BINARY_DIR}/ecpint-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/ecpint-config-version.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ecpint
)

install(TARGETS ecpint DESTINATION ${CMAKE_INSTALL_LIBDIR})
