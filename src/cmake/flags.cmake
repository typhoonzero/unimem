set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

include(CheckCXXCompilerFlag)

function(safe_set_flag is_c src_list flag_name)
    string(REPLACE "-" "_" safe_name ${flag_name})
    string(REPLACE "=" "_" safe_name ${safe_name})
    if(is_c)
        CHECK_C_COMPILER_FLAG(${flag_name} C_COMPILER_SUPPORT_FLAG_${safe_name})
        set(safe_name C_COMPILER_SUPPORT_FLAG_${safe_name})
    else()
        CHECK_CXX_COMPILER_FLAG(${flag_name} CXX_COMPILER_SUPPORT_FLAG_${safe_name})
        set(safe_name CXX_COMPILER_SUPPORT_FLAG_${safe_name})
    endif()
    if(${safe_name})
        set(${src_list} "${${src_list}} ${flag_name}" PARENT_SCOPE)
    endif()
endfunction()

# helper macro to set cflag
macro(safe_set_cflag src_list flag_name)
    safe_set_flag(ON ${src_list} ${flag_name})
endmacro()

# helper macro to set cxxflag
macro(safe_set_cxxflag src_list flag_name)
    safe_set_flag(OFF ${src_list} ${flag_name})
endmacro()

set(COMMON_FLAGS
    -fPIC
    -fno-omit-frame-pointer
    -Werror
    -Wall
    -Wextra
    -Wnon-virtual-dtor
    -Wdelete-non-virtual-dtor
    -Wno-unused-parameter
    -Wno-unused-function
    -Wno-error=literal-suffix
    -Wno-error=sign-compare
    -Wno-error=unused-local-typedefs
)

foreach(flag ${COMMON_FLAGS})
    safe_set_cxxflag(CMAKE_CXX_FLAGS ${flag})
endforeach()

IF(APPLE)
    SET(HOST_SYSTEM "macosx")
    EXEC_PROGRAM(sw_vers ARGS -productVersion OUTPUT_VARIABLE HOST_SYSTEM_VERSION)
    STRING(REGEX MATCH "[0-9]+.[0-9]+" MACOS_VERSION "${HOST_SYSTEM_VERSION}")
    IF(NOT DEFINED $ENV{MACOSX_DEPLOYMENT_TARGET})
        # Set cache variable - end user may change this during ccmake or cmake-gui configure.
        SET(CMAKE_OSX_DEPLOYMENT_TARGET ${MACOS_VERSION} CACHE STRING
            "Minimum OS X version to target for deployment (at runtime); newer APIs weak linked. Set to empty string for default value.")
    ENDIF()
    set(CMAKE_EXE_LINKER_FLAGS "-framework CoreFoundation -framework Security")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=pessimizing-move")
ENDIF(APPLE)
