CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(bt-syspopup C)

IF("$ENV{CFLAGS}" MATCHES "-DTIZEN_WEARABLE")
SET(SRCS ${CMAKE_SOURCE_DIR}/src/bt-syspopup-w.c)
ENDIF()
IF("$ENV{CFLAGS}" MATCHES "-DTIZEN_MOBILE")
SET(SRCS ${CMAKE_SOURCE_DIR}/src/bt-syspopup-m.c)
ENDIF()

SET(PREFIX ${CMAKE_INSTALL_PREFIX})

IF("$ENV{CFLAGS}" MATCHES "-DTIZEN_WEARABLE")
SET(TABLEDIR "/usr/apps/org.tizen.bt-syspopup/shared/res/tables")
ENDIF()

IF("${CMAKE_BUILD_TYPE}" STREQUAL "")
	SET(CMAKE_BUILD_TYPE "Release")
ENDIF("${CMAKE_BUILD_TYPE}" STREQUAL "")
MESSAGE("Build type: ${CMAKE_BUILD_TYPE}")

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR})

INCLUDE(FindPkgConfig)
IF("$ENV{CFLAGS}" MATCHES "-DTIZEN_WEARABLE")
pkg_check_modules(pkgs REQUIRED
	capi-appfw-application
	bundle
	deviced
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
	syspopup
	syspopup-caller
	utilX
	capi-network-bluetooth
	feedback
	aul
	efl-assist
	vconf)
ELSEIF("$ENV{CFLAGS}" MATCHES "-DTIZEN_MOBILE")
pkg_check_modules(pkgs REQUIRED
	capi-appfw-application
	bundle
	deviced
	dlog
	dbus-glib-1
	ecore
	ecore-input
	ecore-imf
	edbus
	edje
	elementary
	evas
	efl-assist
	gobject-2.0
	syspopup
	utilX
	capi-network-bluetooth
	feedback
	notification
	aul
	vconf)
ENDIF()

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

IF("$ENV{CFLAGS}" MATCHES "-DTIZEN_WEARABLE")
ADD_CUSTOM_TARGET(custom_popup.edj
COMMAND edje_cc -id ${CMAKE_CURRENT_SOURCE_DIR}/data/images
${CMAKE_CURRENT_SOURCE_DIR}/data/custom_popup_w.edc
${CMAKE_CURRENT_SOURCE_DIR}/data/custom_popup.edj DEPENDS
${CMAKE_CURRENT_SOURCE_DIR}/data/custom_popup_w.edc
)
ENDIF()
IF("$ENV{CFLAGS}" MATCHES "-DTIZEN_MOBILE")
ADD_CUSTOM_TARGET(custom_popup.edj
COMMAND edje_cc -id ${CMAKE_CURRENT_SOURCE_DIR}/data
${CMAKE_CURRENT_SOURCE_DIR}/data/custom_popup_m.edc
${CMAKE_CURRENT_SOURCE_DIR}/data/custom_popup.edj DEPENDS
${CMAKE_CURRENT_SOURCE_DIR}/data/custom_popup_m.edc
)
ENDIF()

ADD_DEPENDENCIES(${PROJECT_NAME} custom_popup.edj)

INSTALL(TARGETS ${PROJECT_NAME} DESTINATION /usr/apps/org.tizen.bt-syspopup/bin)

# install desktop file & icon
SET(PREFIX ${CMAKE_INSTALL_PREFIX})
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/data/org.tizen.bt-syspopup.png DESTINATION /usr/share/icons/default/small)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/data/custom_popup.edj DESTINATION /usr/apps/org.tizen.bt-syspopup/res/edje)
IF("$ENV{CFLAGS}" MATCHES "-DTIZEN_WEARABLE")
INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tables/ DESTINATION ${TABLEDIR} FILES_MATCHING PATTERN "*.xml")
ENDIF()
