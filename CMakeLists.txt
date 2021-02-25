cmake_minimum_required( VERSION 3.16 )

# project pyth-client
project( pyth-client )

# default build convention
if( NOT CMAKE_BUILD_TYPE )
  set( CMAKE_BUILD_TYPE Release )
endif()

# use root directory as include
set( CMAKE_INCLUDE_CURRENT_DIR ON )

# gcc compiler/linker flags
add_compile_options( -ggdb -Wall -Wextra -Werror -mavx2 -std=c++17 -m64 )
set( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread")

#
# pyth client API library
#
set( PC_SRC
  pc/encode.cpp;
  pc/jtree.cpp;
  pc/jwriter.cpp;
  pc/mem_map.cpp;
  pc/net_socket.cpp;
  pc/key_pair.cpp;
  pc/rpc_client.cpp;
  pc/tx.cpp;
  )

add_library( pc STATIC ${PC_SRC} )

#
# test programs
#

enable_testing()
add_executable( test_key pt/test_key.cpp )
target_link_libraries( test_key pc crypto )
add_executable( test_net pt/test_net.cpp )
target_link_libraries( test_net pc )
add_executable( test_http pt/test_http.cpp )
target_link_libraries( test_http pc )

add_test( test_key test_key )
add_test( test_net test_net )
