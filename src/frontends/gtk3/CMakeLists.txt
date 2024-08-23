find_program(GLIB_COMPILE_RESOURCES NAMES glib-compile-resources REQUIRED)

set(RESOURCES_XML_IN ${CMAKE_CURRENT_SOURCE_DIR}/resources.xml.in)
set(RESOURCES_XML    ${CMAKE_CURRENT_BINARY_DIR}/resources.xml)
set(RESOURCES_DIR    ${CMAKE_CURRENT_SOURCE_DIR}/ui)
set(RESOURCES_CC     ${CMAKE_CURRENT_BINARY_DIR}/resources.cc)
set(RESOURCES_H      ${CMAKE_CURRENT_BINARY_DIR}/resources.h)

if(NOT EXISTS ${RESOURCES_XML} OR (${RESOURCES_XML_IN} IS_NEWER_THAN ${RESOURCES_XML}))
    configure_file(${RESOURCES_XML_IN} ${RESOURCES_XML})

    execute_process(
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND ${GLIB_COMPILE_RESOURCES} --generate-dependencies ${RESOURCES_XML}
        OUTPUT_VARIABLE RESOURCES_DEPENDENCIES
    )

    string(REPLACE "\n" ";" RESOURCES_DEPENDENCIES ${RESOURCES_DEPENDENCIES})
    list(POP_BACK RESOURCES_DEPENDENCIES)
    list(TRANSFORM RESOURCES_DEPENDENCIES PREPEND ui/)
endif()

add_custom_command(
    OUTPUT ${RESOURCES_CC} ${RESOURCES_H}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND ${GLIB_COMPILE_RESOURCES} --generate-source --target ${RESOURCES_CC} --sourcedir=${RESOURCES_DIR} ${RESOURCES_XML}
    COMMAND ${GLIB_COMPILE_RESOURCES} --generate-header --target ${RESOURCES_H}  --sourcedir=${RESOURCES_DIR} ${RESOURCES_XML}
    MAIN_DEPENDENCY ${RESOURCE_XML}
    DEPENDS ${RESOURCES_DEPENDENCIES}
    COMMENT "Compiling resources"
)

set_source_files_properties(${RESOURCES_CC} ${RESOURCES_H} TARGET_DIRECTORY meow PROPERTIES GENERATED TRUE)

add_custom_target(compile_resources ALL DEPENDS ${RESOURCES_CC} ${RESOURCES_H})
add_dependencies(meow compile_resources)

target_sources(meow PUBLIC main.cc asyncoperationwindow.cc mainwindow.cc canvas.cc intonationeditor.cc ${RESOURCES_CC})
