

# Copy tattle into the given directory.
function(target_stage_tattle target)
    if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        # Copy tattle bundle into app bundle
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
                "$<PATH:CMAKE_PATH,NORMALIZE,$<TARGET_FILE_DIR:tattle::tattle>/../../..>tattle.app"
                "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/tattle.app"
            COMMAND_EXPAND_LISTS)
    else()
        # Copy tattle executable into binary directory
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:tattle::tattle>"
                "$<TARGET_FILE_DIR:${target}>"
            COMMAND_EXPAND_LISTS)
    endif()
endfunction()
