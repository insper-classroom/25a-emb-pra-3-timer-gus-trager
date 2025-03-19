add_executable(pico_emb main.c)

target_link_libraries(pico_emb 
    pico_stdlib
    hardware_rtc     
    hardware_timer     
)

set_target_properties(pico_emb PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

pico_add_extra_outputs(pico_emb)
