if (NOT EKA2L1_MEEGO)
    return()
endif()

# The Harmattan Qt 4.7 SDK does not ship qmake/moc/uic in the sysroot; the
# Madde target provides host tools of the same version. The toolchain file
# points QT_QMAKE_EXECUTABLE at it.
find_package(Qt4 4.7 REQUIRED COMPONENTS QtCore QtGui QtNetwork)
include(${QT_USE_FILE})
find_package(X11 REQUIRED)

set(CMAKE_AUTOMOC ON)

add_executable(eka2l1_harmattan
    src/main.cpp
    src/emulator.cpp
    src/display_widget.cpp
    src/main_window.cpp
    src/install_dialog.cpp
    src/frontend_bridge.cpp
    include/harmattan/dialog_host.h
    include/harmattan/display_widget.h
    include/harmattan/emulator.h
    include/harmattan/install_dialog.h
    include/harmattan/main_window.h
    include/harmattan/frontend_bridge.h)

target_include_directories(eka2l1_harmattan PRIVATE include)

target_link_libraries(eka2l1_harmattan PRIVATE
    ${QT_LIBRARIES}
    ${X11_LIBRARIES}
    common
    config
    drivers
    epoc
    epocservs
    epockern
    epocpkg
    gdbstub
    uvw
    j2me
    stb
    yaml-cpp)

set_target_properties(eka2l1_harmattan PROPERTIES
    OUTPUT_NAME eka2l1
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin")

add_custom_command(
    TARGET eka2l1_harmattan
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:eka2l1_harmattan>/resources"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:eka2l1_harmattan>/resources/upscale"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/src/emu/drivers/resources/gles2/"
        "$<TARGET_FILE_DIR:eka2l1_harmattan>/resources/"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/src/emu/drivers/resources/upscale/"
        "$<TARGET_FILE_DIR:eka2l1_harmattan>/resources/upscale/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/src/emu/drivers/resources/defaultbank.hsb"
        "$<TARGET_FILE_DIR:eka2l1_harmattan>/resources/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/src/emu/drivers/resources/defaultbank.sf2"
        "$<TARGET_FILE_DIR:eka2l1_harmattan>/resources/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/miscs/panic/panic.json"
        "$<TARGET_FILE_DIR:eka2l1_harmattan>/")
