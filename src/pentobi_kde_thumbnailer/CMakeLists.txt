find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECompilerSettings)
include(KDECMakeSettings)

find_package(KF5 REQUIRED COMPONENTS KIO)

add_library(pentobi-thumbnail MODULE
  PentobiThumbCreator.h
  PentobiThumbCreator.cpp
)

target_include_directories(pentobi-thumbnail PRIVATE "${CMAKE_SOURCE_DIR}/src")

target_link_libraries(pentobi-thumbnail
  pentobi_kde_thumbnailer
  KF5::KIOWidgets
)

install(TARGETS pentobi-thumbnail DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES pentobi-thumbnail.desktop DESTINATION ${SERVICES_INSTALL_DIR})
