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
  pc/jtree.cpp;
  pc/log.cpp;
  pc/mem_map.cpp;
  pc/misc.cpp;
  pc/net_socket.cpp;
  pc/key_pair.cpp;
  pc/pyth_client.cpp;
  pc/rpc_client.cpp;
  )

add_library( pc STATIC ${PC_SRC} )

# dependencies
set( PC_DEP pc ssl crypto )

#
# applications
#

add_executable( pythd pb/pythd.cpp )
target_link_libraries( pythd ${PC_DEP} )

#
# test programs
#

enable_testing()
add_executable( test_key pt/test_key.cpp )
target_link_libraries( test_key ${PC_DEP} )
add_executable( test_net pt/test_net.cpp )
target_link_libraries( test_net ${PC_DEP} )
add_executable( test_oracle pt/test_oracle.cpp )
target_link_libraries( test_oracle ${PC_DEP} )

add_executable( test_rpc pt/test_rpc.cpp )
target_link_libraries( test_rpc ${PC_DEP} )

add_executable( test_transfer pt/test_transfer.cpp )
target_link_libraries( test_transfer ${PC_DEP} )

add_executable( test_mapping pt/test_mapping.cpp )
target_link_libraries( test_mapping ${PC_DEP} )

add_test( test_key test_key )
add_test( test_net test_net )
add_test( test_oracle test_oracle )
