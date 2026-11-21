add_library(phase_timer INTERFACE)

target_include_directories(phase_timer INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}> $<INSTALL_INTERFACE:include/phase_timer>)
target_compile_features(phase_timer INTERFACE cxx_std_17)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ DESTINATION include/phase_timer FILES_MATCHING PATTERN "*.hpp")
