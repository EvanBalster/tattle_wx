

# Copy tattle into the given directory.
function(target_stage_tattle target)
    if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:tattle::tattle> $<TARGET_BUNDLE_CONTENT_DIR:${target}>
            COMMAND_EXPAND_LISTS)
    else()
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:tattle::tattle> $<TARGET_FILE_DIR:${target}>
            COMMAND_EXPAND_LISTS)
    endif()
endfunction()
