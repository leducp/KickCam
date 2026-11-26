# Install rules, pkg-config, and CMake find_package support.
# Included from the top-level CMakeLists.txt.

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# --- Headers ---
# version.h is generated (cmake/version.cmake), not committed -- install it
# alongside the committed headers.
install(FILES ${KICKCAM_VERSION_H}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/kickcam
)

# --- Library target + export ---
set_target_properties(kickcam PROPERTIES EXPORT_NAME kickcam)
install(TARGETS kickcam EXPORT kickcamTargets)

# --- pkg-config ---
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/kickcam.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/kickcam.pc
    @ONLY
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/kickcam.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

# --- CMake find_package support ---
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/kickcamConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/kickcamConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/kickcam
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/kickcamConfigVersion.cmake
    VERSION ${KICKCAM_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(EXPORT kickcamTargets
    FILE kickcamTargets.cmake
    NAMESPACE kickcam::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/kickcam
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/kickcamConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/kickcamConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/kickcam
)
