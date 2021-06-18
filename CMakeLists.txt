cmake_minimum_required( VERSION 3.16 )

# project pyth-client
project( pyth-client )

# default build convention
if( NOT CMAKE_BUILD_TYPE )
  set( CMAKE_BUILD_TYPE Release )
endif()

# use root directory as include
set( CMAKE_INCLUDE_CURRENT_DIR ON )

# find oracle header files
include_directories( program/src/ )

# gcc compiler/linker flags
add_compile_options( -ggdb -Wall -Wextra -Werror -std=c++11 -m64 )
set( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread")

#
# pyth client API library
#
set( PC_SRC
  pc/attr_id.cpp;
  pc/capture.cpp;
  pc/key_pair.cpp;
  pc/key_store.cpp;
  pc/jtree.cpp;
  pc/log.cpp;
  pc/manager.cpp;
  pc/mem_map.cpp;
  pc/misc.cpp;
  pc/net_socket.cpp;
  pc/pub_stats.cpp;
  pc/replay.cpp;
  pc/request.cpp;
  pc/rpc_client.cpp;
  pc/user.cpp;
  )

set( PC_HDR
  pc/attr_id.hpp;
  pc/capture.hpp;
  pc/dbl_list.hpp;
  pc/error.hpp;
  pc/jtree.hpp;
  pc/key_pair.hpp;
  pc/key_store.hpp;
  pc/hash_map.hpp;
  pc/log.hpp;
  pc/manager.hpp;
  pc/mem_map.hpp;
  pc/misc.hpp;
  pc/net_socket.hpp;
  pc/replay.hpp;
  pc/request.hpp;
  pc/rpc_client.hpp
  pc/user.hpp )

add_library( pc STATIC ${PC_SRC} )

# dependencies
set( PC_DEP pc ssl crypto z zstd )

#
# applications
#

add_executable( pythd pcapps/pythd.cpp )
target_link_libraries( pythd ${PC_DEP} )
add_executable( pyth pcapps/pyth.cpp )
target_link_libraries( pyth ${PC_DEP} )
add_executable( pyth_csv pcapps/pyth_csv.cpp )
target_link_libraries( pyth_csv ${PC_DEP} )
add_executable( pyth_tx pcapps/tx_svr.cpp pcapps/pyth_tx.cpp )
target_link_libraries( pyth_tx ${PC_DEP} )

#
# install
#

install( TARGETS pc DESTINATION lib )
install( TARGETS pyth pythd pyth_csv DESTINATION bin )
install( FILES ${PC_HDR} DESTINATION include/pc )
install( FILES program/src/oracle/oracle.h DESTINATION include/oracle )

#
# test programs
#

enable_testing()
add_executable( test_unit pctest/test_unit.cpp )
target_link_libraries( test_unit ${PC_DEP} )
add_executable( test_net pctest/test_net.cpp )
target_link_libraries( test_net ${PC_DEP} )
add_executable( test_publish pctest/test_publish.cpp )
target_link_libraries( test_publish ${PC_DEP} )
add_executable( test_qset pctest/test_qset.cpp )
target_link_libraries( test_qset ${PC_DEP} )
add_executable( slots_info pctest/slots_info.cpp )
target_link_libraries( slots_info ${PC_DEP} )
add_executable( leader_stats pctest/leader_stats.cpp )
target_link_libraries( leader_stats ${PC_DEP} )

add_test( test_unit test_unit )
add_test( test_net test_net )
