cmake_minimum_required(VERSION 3.14.0)
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.13 CACHE STRING "")

project(TakeMeInEQ
    VERSION 1.0.0.0
    DESCRIPTION "TakeMeIn 16-Band Parametric Equalizer VST3 Plug-in"
)

add_subdirectory(external)

smtg_enable_vst3_sdk()

smtg_add_vst3plugin(TakeMeInEQ
    source/version.h
    source/eqcids.h
    source/biquad.h
    source/eqprocessor.h
    source/eqprocessor.cpp
    source/eqcontroller.h
    source/eqcontroller.cpp
    source/eqpersist.h
    source/eqpersist.cpp
    source/eqeditor.h
    source/eqeditor.cpp
    source/eqentry.cpp
    resource/eqeditor.uidesc
)

target_link_libraries(TakeMeInEQ
    PRIVATE
        sdk
        vstgui_support
)

smtg_target_add_plugin_resources(TakeMeInEQ
    RESOURCES
        resource/eqeditor.uidesc
)

target_include_directories(TakeMeInEQ
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/source
)

smtg_target_configure_version_file(TakeMeInEQ)

if(SMTG_MAC)
    smtg_target_set_bundle(TakeMeInEQ
        BUNDLE_IDENTIFIER com.takemein.eq16band
        COMPANY_NAME "TakeMeIn"
    )
elseif(SMTG_WIN)
    target_sources(TakeMeInEQ PRIVATE
        resource/win32resource.rc
    )
    if(MSVC)
        set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT TakeMeInEQ)
    endif()
endif(SMTG_MAC)
