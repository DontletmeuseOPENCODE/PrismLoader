add_library(prismloader SHARED
    src/entry.cpp
    src/hookengine.cpp
    src/patternscan.cpp
    src/modloader.cpp
    src/gdinit.cpp
    src/ui/modmenu.cpp
)

target_include_directories(prismloader
    PUBLIC  include
    PRIVATE src
    PRIVATE ${CMAKE_SOURCE_DIR}/sdk/include
)

target_compile_definitions(prismloader PRIVATE PRISM_BUILD_LOADER)

install(TARGETS prismloader LIBRARY DESTINATION lib/prismloader)
