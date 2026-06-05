add_library(example_mod MODULE src/main.cpp)

target_include_directories(example_mod PRIVATE ${CMAKE_SOURCE_DIR}/sdk/include)
add_dependencies(example_mod prismloader)

install(TARGETS example_mod LIBRARY DESTINATION share/prismloader/mods/example)
