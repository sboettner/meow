find_program(GLIB_COMPILE_RESOURCES NAMES glib-compile-resources REQUIRED)

set(RESOURCES_XML ${CMAKE_CURRENT_SOURCE_DIR}/resources.xml)
set(RESOURCES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ui)
set(RESOURCES_CC  ${CMAKE_CURRENT_BINARY_DIR}/resources.cc)
set(RESOURCES_H   ${CMAKE_CURRENT_BINARY_DIR}/resources.h)

add_custom_command(
    OUTPUT ${RESOURCES_CC} ${RESOURCES_H}
    COMMAND ${GLIB_COMPILE_RESOURCES} --generate-source --target ${RESOURCES_CC} --sourcedir=${RESOURCES_DIR} ${RESOURCES_XML}
    COMMAND ${GLIB_COMPILE_RESOURCES} --generate-header --target ${RESOURCES_H}  --sourcedir=${RESOURCES_DIR} ${RESOURCES_XML}
    DEPENDS ${RESOURCE_XML}
    COMMENT "Compiling resources"
)

add_custom_target(compile_resources ALL DEPENDS ${RESOURCES_CC} ${RESOURCES_H})
add_dependencies(meow compile_resources)

target_sources(meow PUBLIC main.cc canvas.cc intonationeditor.cc ${RESOURCES_CC})
