set(EXTERNAL_SOURCE_DIR ${PROJECT_SOURCE_DIR}/external)
set(EXTERNAL_BUILD_DIR ${PROJECT_BINARY_DIR}/external/build)

find_library(PUGIXML_LIBRARY pugixml
 HINTS "/usr/local" "/usr/local/lib" "/usr/lib"
)
find_path(PUGIXML_INCLUDE_DIR pugixml.hpp
 HINTS "/usr/local/include" "/usr/include"
)

if((NOT PUGIXML_LIBRARY) OR (NOT PUGIXML_INCLUDE_DIR))
  message("Unable to find pugixml, cloning...")

  ExternalProject_Add(pugixml_external
    PREFIX ${EXTERNAL_BUILD_DIR}/pugixml
    GIT_REPOSITORY https://github.com/zeux/pugixml
    UPDATE_COMMAND ""
    INSTALL_DIR ${EXTERNAL_BUILD_DIR}/pugixml/install
    CMAKE_ARGS "-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
  )

  install(DIRECTORY ${EXTERNAL_BUILD_DIR}/pugixml/install/ DESTINATION "${CMAKE_INSTALL_PREFIX}/")

  set(PUGIXML_LIBRARY ${EXTERNAL_BUILD_DIR}/pugixml/src/pugixml_external-build/libpugixml${CMAKE_STATIC_LIBRARY_SUFFIX})
  set(PUGIXML_INCLUDE_DIR ${EXTERNAL_BUILD_DIR}/pugixml/src/pugixml_external/src)

  add_library(pugixml::pugixml STATIC IMPORTED)
  add_dependencies(pugixml::pugixml pugixml_external)
  set_target_properties(pugixml::pugixml PROPERTIES IMPORTED_LOCATION ${PUGIXML_LIBRARY})

else()
  message("Found pugixml")
  add_library(pugixml::pugixml INTERFACE)
  target_include_directories(pugixml::pugixml INTERFACE ${PUGIXML_INCLUDE_DIR})
  target_link_libraries(pugixml::pugixml INTERFACE ${PUGIXML_LIBRARY})
endif()
