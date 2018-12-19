if(${Qt5Core_VERSION} VERSION_LESS 5.0.0)
	message(FATAL_ERROR "Qt5 or above is required")
endif()

list(APPEND SOURCES
    qite.cpp
    qiteaudio.cpp
    qiteaudiorecorder.cpp
    )

list(APPEND HEADERS
    qite.h
    qiteaudio.h
    qiteaudiorecorder.h
    )

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    )
