set(QT_REQUIRED_LIBS Core Gui Widgets Network WebSockets)
set(REQUIRED_LIBS OpenSSL)
set(REQUIRED_LIBS_QUALIFIED Qt::Core Qt::Gui Qt::Widgets Qt::Network Qt::WebSockets OpenSSL::Crypto)
set(REQUIRED_WIN_LIBS gcc_s_seh-1 stdc++-6 winpthread-1)

aux_source_directory(. SOURCE_SRC)

file(GLOB HEAD_FILES ${CMAKE_SOURCE_DIR}/include/*.h)
add_executable(${PROJECT_NAME} ${SOURCE_SRC} ${HEAD_FILES} ${CMAKE_SOURCE_DIR}/resource.rc ${CMAKE_SOURCE_DIR}/resources.qrc)

find_package(Qt${QT_VERSION} COMPONENTS ${QT_REQUIRED_LIBS} REQUIRED)
find_package(${REQUIRED_LIBS} REQUIRED)
target_link_libraries(${PROJECT_NAME} PUBLIC ${REQUIRED_LIBS_QUALIFIED})

if (CMAKE_BUILD_TYPE MATCHES "Release")
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE ON MACOSX_BUNDLE ON)
endif ()

#add_custom_command(TARGET ${PROJECT_NAME}
#        POST_BUILD
#        # COMMAND_EXPAND_LISTS
#        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/assets"
#        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/assets" "${CMAKE_BINARY_DIR}/assets"
#        COMMENT "copy assets ...")

message("==========${CMAKE_BUILD_TYPE}")
if (CMAKE_BUILD_TYPE MATCHES "Debug")

else ()
    install(TARGETS ${PROJECT_NAME}
            RUNTIME DESTINATION "bin")
    qt_generate_deploy_app_script(TARGET ${PROJECT_NAME}
            OUTPUT_SCRIPT deploy_script
            NO_TRANSLATIONS
            NO_COMPILER_RUNTIME
            NO_UNSUPPORTED_PLATFORM_ERROR
            DEPLOY_TOOL_OPTIONS "--dir ${CMAKE_INSTALL_PREFIX}/bin
                                --release
                                --skip-plugin-types imageformats,iconengines,generic,styles
                                --no-opengl-sw
                                --no-system-d3d-compiler
                                --force-openssl
                                --plugindir ${CMAKE_INSTALL_PREFIX}/bin/plugins")
    install(SCRIPT ${deploy_script} DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
    install(CODE "file (REMOVE \"${CMAKE_INSTALL_PREFIX}/bin/qt.conf\")")
    install(DIRECTORY ${CMAKE_INSTALL_PREFIX}/bin/
            DESTINATION ${CMAKE_SOURCE_DIR}/package/packages/${PROJECT_NAME}/data
            # 可选：如果需要排除某些文件/目录，可以添加 EXCLUDE 模式
            # PATTERN "*.tmp" EXCLUDE
            # PATTERN "build" EXCLUDE
    )
endif ()