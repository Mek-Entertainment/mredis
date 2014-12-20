SET(LIBMREDISSRC mredis.c )

ADD_LIBRARY(mredis STATIC ${LIBMREDISSRC})
target_include_directories(mredis PUBLIC ../SDL2_net)
TARGET_LINK_LIBRARIES(mredis SDL2-static SDL2_net ${CMAKE_REQUIRED_LIBRARIES}) 
target_compile_options(mredis PRIVATE -std=c99)
