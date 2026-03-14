# 如有需要自定义编译的文件，可以使用executable(aaa, bbb) 排除编译
function (executable)
    #    message("xxxxx ${ARGV} ${ARGC} ------")
    file(GLOB SOURCE_SRC "*.c" "*.cpp")

    foreach (FILE ${SOURCE_SRC})
        string(REGEX REPLACE ".*/(.*)\\.(.*)" "\\1" TARGET_NAME ${FILE})
        string(REGEX REPLACE ".*/(.*)" "\\1" FILE_NAME ${FILE})

        message("${FILE_NAME} ------")
        if (NOT (ARGC AND TARGET_NAME IN_LIST ARGV))
            add_executable(${TARGET_NAME} ${FILE_NAME})
        endif ()
    endforeach ()
endfunction ()

function (auxLibrary targetName link include)
    aux_source_directory(. SOURCE_SRC)
    add_library(${targetName} SHARED ${SOURCE_SRC})
    add_library(${targetName}_static STATIC $<TARGET_OBJECTS:${targetName}>)

    set_target_properties(${targetName}_static PROPERTIES OUTPUT_NAME ${targetName})

    if (link)
        target_link_libraries(${targetName} PUBLIC ${link})
        target_link_libraries(${targetName}_static PUBLIC ${link})
    endif ()

    if (include)
        target_include_directories(${targetName} PUBLIC ${include})
        target_include_directories(${targetName}_static PUBLIC ${include})
    endif ()
endfunction ()

function (DebugGeneratorExpression content)
    string(TIMESTAMP CURRENT_Date "%Y-%m-%d")
    string(TIMESTAMP CURRENT_TIME "%Y-%m-%d %H:%M:%S")
    if (NOT EXISTS ${CMAKE_BINARY_DIR}/log)
        file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/log)
    endif ()
    file(GENERATE
         OUTPUT
         "${CMAKE_BINARY_DIR}/log/generator.${CURRENT_Date}.log"
         CONTENT
         "${CURRENT_TIME}\t${content}\n")
endfunction ()

function (ListToString input output)
    set(outVar "")
    foreach (item ${input})
        if (outVar)
            set(outVar "${outVar},${item}")
        else ()
            set(outVar "${item}")
        endif ()
    endforeach ()
    set(${output} ${outVar} PARENT_SCOPE)
endfunction ()