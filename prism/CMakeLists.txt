add_executable(prism src/main.cpp)

target_link_libraries(prism PRIVATE dl)

install(TARGETS prism RUNTIME DESTINATION bin)
