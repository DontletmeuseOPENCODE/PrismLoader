if(WIN32)
    add_library(prismloader SHARED
        src/dllmain.cpp
        src/hookengine.cpp
        src/patternscan.cpp
        src/modloader.cpp
        src/gdinit.cpp
        src/loader.cpp
        src/ui/modmenu.cpp
        src/log.cpp
    )
    target_link_libraries(prismloader PRIVATE psapi)
    target_link_options(prismloader PRIVATE -static -static-libgcc -static-libstdc++)
else()
    # Dummy / Linux native support if needed
    add_library(prismloader SHARED
        src/entry.cpp
        src/loader.cpp
        src/log.cpp
        src/hookengine.cpp
        src/patternscan.cpp
        src/modloader.cpp
        src/gdinit.cpp
        src/ui/modmenu.cpp
    )
    target_link_libraries(prismloader PRIVATE dl)
endif()

target_include_directories(prismloader
    PUBLIC  include
    PRIVATE src
    PRIVATE ${CMAKE_SOURCE_DIR}/sdk/include
)

target_compile_definitions(prismloader PRIVATE PRISM_BUILD_LOADER)

install(TARGETS prismloader LIBRARY DESTINATION lib/prismloader)
