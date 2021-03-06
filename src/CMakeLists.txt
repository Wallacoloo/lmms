ADD_SUBDIRECTORY(3rdparty)

CONFIGURE_FILE("lmmsconfig.h.in"        "${CMAKE_BINARY_DIR}/lmmsconfig.h")
CONFIGURE_FILE("lmmsversion.h.in"       "${CMAKE_BINARY_DIR}/lmmsversion.h")

SET(LMMS_SRCS "")
SET(LMMS_UIS "")

SET(CMAKE_AUTOMOC ON)
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Enable C++11
SET(CMAKE_CXX_STANDARD 11)

IF(LMMS_BUILD_APPLE)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
ENDIF()

ADD_SUBDIRECTORY(core)
ADD_SUBDIRECTORY(gui)
ADD_SUBDIRECTORY(tracks)

QT5_WRAP_UI(LMMS_UI_OUT ${LMMS_UIS})
INCLUDE_DIRECTORIES(
	"${CMAKE_CURRENT_BINARY_DIR}"
	"${CMAKE_BINARY_DIR}"
	"${CMAKE_BINARY_DIR}/include"
	"${CMAKE_SOURCE_DIR}"
	"${CMAKE_SOURCE_DIR}/include"
)

IF(WIN32 AND MSVC)
	SET(WINRC "${CMAKE_BINARY_DIR}/lmms.rc")
ELSEIF(WIN32)
	SET(WINRC "${CMAKE_BINARY_DIR}/lmmsrc.obj")
	ADD_CUSTOM_COMMAND(OUTPUT "${WINRC}"
				COMMAND "${CMAKE_RC_COMPILER}"
					"-I\"${CMAKE_SOURCE_DIR}\""
					"-o\"${CMAKE_BINARY_DIR}/lmmsrc.obj\""
					"-i\"${CMAKE_BINARY_DIR}/lmms.rc\""
				DEPENDS "${CMAKE_BINARY_DIR}/lmms.rc")
ENDIF()

INCLUDE(GenQrc)
ADD_GEN_QRC(LMMS_RCC_OUT lmms.qrc
	"${CMAKE_SOURCE_DIR}/doc/AUTHORS"
	"${CMAKE_SOURCE_DIR}/LICENSE.txt"
	"${CMAKE_BINARY_DIR}/CONTRIBUTORS"
)

# Paths relative to lmms executable
FILE(RELATIVE_PATH LIB_DIR_RELATIVE "/${BIN_DIR}" "/${LIB_DIR}")
FILE(RELATIVE_PATH PLUGIN_DIR_RELATIVE "/${BIN_DIR}" "/${PLUGIN_DIR}")
ADD_DEFINITIONS(-DLIB_DIR="${LIB_DIR_RELATIVE}" -DPLUGIN_DIR="${PLUGIN_DIR_RELATIVE}" ${PULSEAUDIO_DEFINITIONS} ${PORTAUDIO_DEFINITIONS})
INCLUDE_DIRECTORIES(
	${JACK_INCLUDE_DIRS}
	${SAMPLERATE_INCLUDE_DIRS}
	${SNDFILE_INCLUDE_DIRS}
	${SNDIO_INCLUDE_DIRS}
	${FFTW3F_INCLUDE_DIRS}
)

IF(NOT ("${SDL2_INCLUDE_DIR}" STREQUAL ""))
	INCLUDE_DIRECTORIES("${SDL2_INCLUDE_DIR}")
ELSEIF(NOT ("${SDL_INCLUDE_DIR}" STREQUAL ""))
	INCLUDE_DIRECTORIES("${SDL_INCLUDE_DIR}")
ENDIF()

IF(LMMS_HAVE_WEAKJACK)
	LIST(APPEND LMMS_SRCS "${WEAKJACK_INCLUDE_DIRS}/weak_libjack.c")
	LIST(APPEND LMMS_INCLUDES "${WEAKJACK_INCLUDE_DIRS}/weak_libjack.h")
	INCLUDE_DIRECTORIES("${WEAKJACK_INCLUDE_DIRS}")
	ADD_DEFINITIONS(-DUSE_WEAK_JACK=1 -DNO_JACK_METADATA=1)
ENDIF()

IF(NOT ("${PORTAUDIO_INCLUDE_DIR}" STREQUAL ""))
	INCLUDE_DIRECTORIES("${PORTAUDIO_INCLUDE_DIR}")
ENDIF()

IF(NOT ("${PULSEAUDIO_INCLUDE_DIR}" STREQUAL ""))
	INCLUDE_DIRECTORIES("${PULSEAUDIO_INCLUDE_DIR}")
ENDIF()

IF(NOT ("${OGGVORBIS_INCLUDE_DIR}" STREQUAL ""))
	INCLUDE_DIRECTORIES("${OGGVORBIS_INCLUDE_DIR}")
ENDIF()

IF(NOT ("${LAME_INCLUDE_DIRS}" STREQUAL ""))
	INCLUDE_DIRECTORIES("${LAME_INCLUDE_DIRS}")
ENDIF()

# Use libraries in non-standard directories (e.g., another version of Qt)
IF(LMMS_BUILD_LINUX)
	LINK_LIBRARIES(-Wl,--enable-new-dtags)
	SET(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
ENDIF()
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

ADD_LIBRARY(lmmsobjs OBJECT
	${LMMS_SRCS}
	${LMMS_INCLUDES}
	${LMMS_UI_OUT}
	${LMMS_RCC_OUT}
)

GENERATE_EXPORT_HEADER(lmmsobjs
	BASE_NAME lmms
)

ADD_EXECUTABLE(lmms
	core/main.cpp
	$<TARGET_OBJECTS:lmmsobjs>
	"${WINRC}"
)
TARGET_INCLUDE_DIRECTORIES(lmms
	PUBLIC ${CMAKE_CURRENT_BINARY_DIR}
)

# GENERATE_EXPORT_HEADER doesn't do this automatically for OBJECT libraries
TARGET_COMPILE_DEFINITIONS(lmmsobjs
	PRIVATE -Dlmmsobjs_EXPORTS
)
TARGET_COMPILE_DEFINITIONS(lmms
	PRIVATE -Dlmmsobjs_EXPORTS
)

# Set Visual Studio startup project to lmms
# https://stackoverflow.com/a/37994396/8166701
IF(NOT CMAKE_VERSION VERSION_LESS 3.6)
	SET_PROPERTY(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT lmms)
ENDIF()

SET_DIRECTORY_PROPERTIES(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${LMMS_RCC_OUT} ${LMMS_UI_OUT} lmmsconfig.h lmms.1.gz")

IF(LMMS_BUILD_WIN32)
	SET(EXTRA_LIBRARIES "winmm")
ENDIF()

IF(LMMS_BUILD_APPLE)
	SET(EXTRA_LIBRARIES "-framework CoreMIDI -framework CoreFoundation")
ENDIF()

if(LMMS_HAVE_OSS AND LMMS_BUILD_OPENBSD)
    SET(EXTRA_LIBRARIES "-lossaudio")
endif()

IF(LMMS_BUILD_HAIKU)
	SET(EXTRA_LIBRARIES "-lnetwork")
ENDIF()

SET(LMMS_REQUIRED_LIBS ${LMMS_REQUIRED_LIBS}
	${CMAKE_THREAD_LIBS_INIT}
	${QT_LIBRARIES}
	${ASOUND_LIBRARY}
	${SDL_LIBRARY}
	${SDL2_LIBRARY}
	${PORTAUDIO_LIBRARIES}
	${SOUNDIO_LIBRARY}
	${SNDIO_LIBRARIES}
	${PULSEAUDIO_LIBRARIES}
	${JACK_LIBRARIES}
	${OGGVORBIS_LIBRARIES}
	${LAME_LIBRARIES}
	${SAMPLERATE_LIBRARIES}
	${SNDFILE_LIBRARIES}
	${EXTRA_LIBRARIES}
	rpmalloc
)

# Expose required libs for tests binary
SET(LMMS_REQUIRED_LIBS ${LMMS_REQUIRED_LIBS} PARENT_SCOPE)

TARGET_LINK_LIBRARIES(lmms
	${LMMS_REQUIRED_LIBS}
)

FOREACH(LIB ${LMMS_REQUIRED_LIBS})
	IF(TARGET ${LIB})
		GET_TARGET_PROPERTY(INCLUDE_DIRS ${LIB} INTERFACE_INCLUDE_DIRECTORIES)
		IF(INCLUDE_DIRS)
			TARGET_INCLUDE_DIRECTORIES(lmmsobjs PRIVATE ${INCLUDE_DIRS})
		ENDIF()
	ENDIF()
ENDFOREACH()


# Required libs for debug msys builds
IF(LMMS_BUILD_MSYS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
	TARGET_LINK_LIBRARIES(lmms QtCore4 QtGui4 QtXml4)
ENDIF()

# Install
IF(NOT MSVC)
	IF(LMMS_BUILD_WIN32)
		SET_TARGET_PROPERTIES(lmms PROPERTIES
			LINK_FLAGS "${LINK_FLAGS} -mwindows"
			ENABLE_EXPORTS ON
		)

		IF(LMMS_BUILD_MSYS)
			# ENABLE_EXPORTS property has no effect in some MSYS2 configurations.
			# Add the linker flag manually to create liblmms.dll.a import library
			SET_PROPERTY(TARGET lmms
				APPEND_STRING PROPERTY LINK_FLAGS -Wl,--out-implib,liblmms.dll.a
			)
		ENDIF()

		IF(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
			ADD_CUSTOM_COMMAND(TARGET lmms POST_BUILD COMMAND "${STRIP}" "$<TARGET_FILE:lmms>")
		ENDIF()

		INSTALL(TARGETS lmms RUNTIME DESTINATION "${BIN_DIR}")
		INSTALL(FILES
					"${MINGW_PREFIX}/bin/Qt5Core.dll"
					"${MINGW_PREFIX}/bin/Qt5Gui.dll"
					"${MINGW_PREFIX}/bin/Qt5Widgets.dll"
					"${MINGW_PREFIX}/bin/Qt5Xml.dll"
				DESTINATION .)
		INSTALL(FILES
					"${MINGW_PREFIX}/lib/qt5/plugins/platforms/qwindows.dll"
				DESTINATION ./platforms)
		INSTALL(FILES
						"${MINGW_PREFIX}/bin/libsamplerate-0.dll"
						"${MINGW_PREFIX}/bin/libsndfile-1.dll"
						"${MINGW_PREFIX}/bin/libvorbis-0.dll"
						"${MINGW_PREFIX}/bin/libvorbisenc-2.dll"
						"${MINGW_PREFIX}/bin/libvorbisfile-3.dll"
						"${MINGW_PREFIX}/bin/libjpeg-9.dll"
						"${MINGW_PREFIX}/bin/libogg-0.dll"
						"${MINGW_PREFIX}/bin/libmp3lame-0.dll"
						"${MINGW_PREFIX}/bin/libfftw3f-3.dll"
						"${MINGW_PREFIX}/bin/libFLAC-8.dll"
						"${MINGW_PREFIX}/bin/libpng16-16.dll"
						"${MINGW_PREFIX}/bin/SDL.dll"
						"${MINGW_PREFIX}/bin/libglib-2.0-0.dll"
						"${MINGW_PREFIX}/bin/libgthread-2.0-0.dll"
						"${MINGW_PREFIX}/bin/zlib1.dll"
						"${MINGW_PREFIX}/${CMAKE_SYSTEM_PROCESSOR}-w64-mingw32/bin/libwinpthread-1.dll"
					DESTINATION .)
		IF(LMMS_BUILD_MSYS)
			INSTALL(FILES
						"${MINGW_PREFIX}/bin/libwinpthread-1.dll"
						"${MINGW_PREFIX}/bin/libgcc_s_seh-1.dll"
						"${MINGW_PREFIX}/bin/libstdc++-6.dll"
					DESTINATION .)
		ELSE()
			INSTALL(FILES
						"${MINGW_PREFIX}/${CMAKE_SYSTEM_PROCESSOR}-w64-mingw32/bin/libwinpthread-1.dll"
					DESTINATION .)
		ENDIF()
		IF(FLTK_FOUND)
			INSTALL(FILES
						"${MINGW_PREFIX}/bin/libfltk.dll"
					DESTINATION .)
		ENDIF()
		IF(FLUIDSYNTH_FOUND)
			INSTALL(FILES
						"${MINGW_PREFIX}/bin/libfluidsynth.dll"
					DESTINATION .)
		ENDIF()
		IF(GIG_FOUND)
			# Handle libgig-*.dll
			FILE(GLOB GIG_LIBRARY "${MINGW_PREFIX}/bin/libgig-*.dll")
			INSTALL(FILES
						${GIG_LIBRARY}
					DESTINATION .)
		ENDIF()
		IF(PORTAUDIO_FOUND)
			INSTALL(FILES
						"${MINGW_PREFIX}/bin/libportaudio-2.dll"
					DESTINATION .)
		ENDIF()
		IF(SOUNDIO_FOUND)
			INSTALL(FILES
						"${MINGW_PREFIX}/lib/libsoundio.dll"
					DESTINATION .)
		ENDIF()

	ELSE(LMMS_BUILD_WIN32)
		IF(NOT LMMS_BUILD_APPLE)
			SET_TARGET_PROPERTIES(lmms PROPERTIES LINK_FLAGS "${LINK_FLAGS} -Wl,-E")
		ENDIF(NOT LMMS_BUILD_APPLE)

		INSTALL(TARGETS lmms RUNTIME DESTINATION "${BIN_DIR}")
		INSTALL(FILES "${CMAKE_BINARY_DIR}/lmms.1.gz" DESTINATION "${CMAKE_INSTALL_PREFIX}/share/man/man1/" PERMISSIONS OWNER_READ GROUP_READ WORLD_READ)

	ENDIF(LMMS_BUILD_WIN32)
ELSE(NOT MSVC)
		SET_TARGET_PROPERTIES(lmms PROPERTIES
			ENABLE_EXPORTS ON
		)
		INSTALL(TARGETS lmms RUNTIME DESTINATION "${BIN_DIR}")
		
				SET_TARGET_PROPERTIES(lmms PROPERTIES
			LINK_FLAGS "${LINK_FLAGS} -mwindows"
			ENABLE_EXPORTS ON
		)

		#CMAKE_FIND_ROOT_PATH
		SET(VCPKG_ROOT ${CMAKE_FIND_ROOT_PATH})
		
		INSTALL(TARGETS lmms RUNTIME DESTINATION "${BIN_DIR}")
		
		INSTALL(FILES
					"${VCPKG_ROOT}/bin/Qt5Core.dll"
					"${VCPKG_ROOT}/bin/Qt5Gui.dll"
					"${VCPKG_ROOT}/bin/Qt5Widgets.dll"
					"${VCPKG_ROOT}/bin/Qt5Xml.dll"

					"${VCPKG_ROOT}/bin/zlib1.dll"
					"${VCPKG_ROOT}/bin/jpeg62.dll"
					"${VCPKG_ROOT}/bin/libpng16.dll"
					"${VCPKG_ROOT}/bin/gthread-2.dll"
					"${VCPKG_ROOT}/bin/glib-2.dll"
					"${VCPKG_ROOT}/bin/harfbuzz.dll"
					"${VCPKG_ROOT}/bin/pcre2-16.dll"
					"${VCPKG_ROOT}/bin/double-conversion.dll"
					"${VCPKG_ROOT}/bin/freetype.dll"
					"${VCPKG_ROOT}/bin/libbz2.dll"
					"${VCPKG_ROOT}/bin/pcre.dll"
					"${VCPKG_ROOT}/bin/libiconv.dll"
					"${VCPKG_ROOT}/bin/libcharset.dll"
					"${VCPKG_ROOT}/bin/libintl.dll"
				DESTINATION .)
		
		INSTALL(FILES
					"${VCPKG_ROOT}/plugins/platforms/qwindows.dll"
				DESTINATION ./platforms)
		
		INSTALL(FILES
					"${VCPKG_ROOT}/bin/libsndfile-1.dll"
						"${VCPKG_ROOT}/bin/ogg.dll"
						"${VCPKG_ROOT}/bin/vorbis.dll"
						"${VCPKG_ROOT}/bin/vorbisenc.dll"
						"${VCPKG_ROOT}/bin/FLAC.dll"
						"${VCPKG_ROOT}/bin/vorbisfile.dll"

					"${VCPKG_ROOT}/bin/libsamplerate-0.dll"
					"${VCPKG_ROOT}/bin/SDL2.dll"
					"${VCPKG_ROOT}/bin/fftw3f.dll"
				DESTINATION .)
		
		#not yet in vcpkg
		#IF(LAME_FOUND)
		#	INSTALL(FILES
		#				"${VCPKG_ROOT}/bin/libmp3lame-0.dll"
		#			DESTINATION .)
		#ENDIF(LAME_FOUND)
		
		IF(FLTK_FOUND)
			INSTALL(FILES
					"${VCPKG_ROOT}/bin/libfltk_SHARED.dll"
						
					"${VCPKG_ROOT}/bin/zlib1.dll"
					"${VCPKG_ROOT}/bin/jpeg62.dll"
				DESTINATION .)
		ENDIF()
		
		IF(FLUIDSYNTH_FOUND)
			INSTALL(FILES
					"${VCPKG_ROOT}/bin/libfluidsynth-1.dll"
					"${VCPKG_ROOT}/bin/glib-2.dll"
					"${VCPKG_ROOT}/bin/pcre.dll"
					"${VCPKG_ROOT}/bin/libiconv.dll"
					"${VCPKG_ROOT}/bin/libcharset.dll"
					"${VCPKG_ROOT}/bin/libintl.dll"
				DESTINATION .)
		ENDIF()

		#not yet included in vcpkg
		#IF(GIG_FOUND)
		#	# Handle libgig-*.dll
		#	FILE(GLOB GIG_LIBRARY "${VCPKG_ROOT}/bin/libgig-*.dll")
		#	INSTALL(FILES
		#				${GIG_LIBRARY}
		#			DESTINATION .)
		#ENDIF()

		IF(PORTAUDIO_FOUND)
			IF(LMMS_BUILD_WIN64)
				INSTALL(FILES
						"${VCPKG_ROOT}/bin/portaudio-x64.dll"
					DESTINATION .)
			ELSE(LMMS_BUILD_WIN64)
				INSTALL(FILES
						"${VCPKG_ROOT}/bin/portaudio-x86.dll"
					DESTINATION .)
			ENDIF(LMMS_BUILD_WIN64)
		ENDIF()
		
		#not yet in vcpkg
		#IF(SOUNDIO_FOUND)
		#	INSTALL(FILES
		#				"${VCPKG_ROOT}/bin/libsoundio.dll"
		#			DESTINATION .)
		#ENDIF()
ENDIF(NOT MSVC)