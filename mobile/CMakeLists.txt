CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(bt-syspopup C)

SET(SRCS ${CMAKE_SOURCE_DIR}/src/bt-syspopup.c)

SET(PREFIX ${CMAKE_INSTALL_PREFIX})

IF("${CMAKE_BUILD_TYPE}" STREQUAL "")
	SET(CMAKE_BUILD_TYPE "Release")
ENDIF("${CMAKE_BUILD_TYPE}" STREQUAL "")
MESSAGE("Build type: ${CMAKE_BUILD_TYPE}")

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR})

INCLUDE(FindPkgConfig)
pkg_check_modules(pkgs REQUIRED
	appcore-efl
	bundle
	devman
	devman_haptic
	dlog
	dbus-glib-1
	ecore
	ecore-input
	ecore-imf
	edbus
	edje
	elementary
	evas
	gobject-2.0
	pmapi
	syspopup
	utilX
	bluetooth-api
	feedback
	aul)

FOREACH(flag ${pkgs_CFLAGS})
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag} -Wall -g")
ENDFOREACH(flag)
#SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} -finstrument-functions")

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS} -fPIC")
SET(CMAKE_C_FLAGS_DEBUG "-O0 -g ")
SET(CMAKE_C_FLAGS_RELEASE "-O2")
#This is flag for Gprof
#SET(CMAKE_C_FLAGS_RELEASE "-pg")

FIND_PROGRAM(UNAME NAMES uname)
EXEC_PROGRAM("${UNAME}" ARGS "-m" OUTPUT_VARIABLE "ARCH")
IF("${ARCH}" STREQUAL "arm")
	ADD_DEFINITIONS("-DTARGET")
	MESSAGE("add -DTARGET")
ENDIF("${ARCH}" STREQUAL "arm")

ADD_DEFINITIONS("-DPREFIX=\"${CMAKE_INSTALL_PREFIX}\"")
ADD_DEFINITIONS("-DSLP_DEBUG")

SET(CMAKE_EXE_LINKER_FLAGS "-Wl,--as-needed -pie")

ADD_EXECUTABLE(${PROJECT_NAME} ${SRCS})
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${pkgs_LDFLAGS})

ADD_CUSTOM_TARGET(custom_popup.edj
COMMAND edje_cc -id ${CMAKE_CURRENT_SOURCE_DIR}/data
${CMAKE_CURRENT_SOURCE_DIR}/data/custom_popup.edc
${CMAKE_CURRENT_SOURCE_DIR}/data/custom_popup.edj DEPENDS
${CMAKE_CURRENT_SOURCE_DIR}/data/custom_popup.edc
)

ADD_DEPENDENCIES(${PROJECT_NAME} custom_popup.edj)


INSTALL(TARGETS ${PROJECT_NAME} DESTINATION /opt/apps/org.tizen.bt-syspopup/bin)

# install desktop file & icon
SET(PREFIX ${CMAKE_INSTALL_PREFIX})
INSTALL(FILES ${CMAKE_BINARY_DIR}/data/org.tizen.bt-syspopup.xml DESTINATION /usr/share/packages/)
INSTALL(FILES ${CMAKE_BINARY_DIR}/data/org.tizen.bt-syspopup.rule DESTINATION /opt/etc/smack/accesses.d)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/data/org.tizen.bt-syspopup.png DESTINATION /opt/share/icons/default/small)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/data/custom_popup.edj DESTINATION /opt/apps/org.tizen.bt-syspopup/res/edje)
