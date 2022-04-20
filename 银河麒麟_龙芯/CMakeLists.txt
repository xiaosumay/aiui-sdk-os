CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

project(aiui_demo)

set(CMAKE_BUILD_TYPE "Debug")

#set(CMAKE_C_COMPILER_INIT g++)
add_compile_options(-O0 -g -ggdb -Wall)
#add_compile_options(-Wno-unused-result -Wunused-function)
#set(CFLAGS -Wunused-result)
#set(CMAKE_C_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -Wunused-result -g -ggdb")
#set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -Wunused-result -g -ggdb")

#set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O0 -Wunused-result -Wall")

include_directories(
"./aiui/inc"
"./depend"
"./depend/alsa/inc"
"./depend/ringbuf/inc"
"./depend/vad/inc"
"./depend/wav/inc"
"./depend/mad/inc"
"./depend/wakeup/inc"
)

aux_source_directory(. SRC_LIST)
aux_source_directory(depend/alsa/src SRC_LIST)
aux_source_directory(depend/ringbuf/src SRC_LIST)
aux_source_directory(aiui/src SRC_LIST)
aux_source_directory(depend/vad/src SRC_LIST)
aux_source_directory(depend/wav/src SRC_LIST)

link_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib)

set_source_files_properties(${SRC_LIST} PROPERTIES LANGUAGE CXX)
add_executable(aiui_demo ${SRC_LIST})
target_link_libraries(aiui_demo asound)
target_link_libraries(aiui_demo pthread)
target_link_libraries(aiui_demo mad)
target_link_libraries(aiui_demo mbedtls)
target_link_libraries(aiui_demo mbedx509)
target_link_libraries(aiui_demo mbedcrypto)
target_link_libraries(aiui_demo vtn_mic1)