add_library(BundledTLSF STATIC
    tlsf.h
    tlsf.cpp
)

target_include_directories(BundledTLSF PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

target_link_libraries(BundledTLSF PUBLIC Qt6::Core)
