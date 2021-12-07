# Utils for adding fake BPF targets for static analysis (CLion, etc.)

function( target_bpf targ src )

  # bpf.mk uses -std=c17:
  # https://github.com/solana-labs/solana/blob/master/sdk/bpf/c/bpf.mk
  if( ${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.21" )
    set( BPF_C_STANDARD 17 )
  else()
    set( BPF_C_STANDARD 11 )
  endif()

  set_target_properties( ${targ} PROPERTIES
    C_STANDARD ${BPF_C_STANDARD}
    CXX_STANDARD 17

    # don't decay standard
    C_STANDARD_REQUIRED ON
    CXX_STANDARD_REQUIRED ON

    # don't use -std=gnu*
    C_EXTENSIONS OFF
    CXX_EXTENSIONS OFF
  )

  # program/src/oracle/oracle.c -> program/src
  cmake_path( GET src PARENT_PATH src )
  cmake_path( GET src PARENT_PATH src )
  target_include_directories( ${targ} PRIVATE ${src} )

  set( BPF ${SOLANA}/sdk/bpf )
  target_include_directories( ${targ} SYSTEM PRIVATE
    ${BPF}/c/inc
    ${BPF}/dependencies/criterion/include
    ${BPF}/dependencies/bpf-tools/llvm/lib/clang/12.0.1/include
  )

endfunction()

function( add_bpf_lib targ src )
  if( SOLANA )
    add_library( ${targ} STATIC ${src} ${ARGN} )
    target_bpf( ${targ} ${src} )
    target_compile_definitions( ${targ} PRIVATE __bpf__ )
  endif()
endfunction()

function( add_bpf_test targ src )
  if( SOLANA )
    add_executable( ${targ} ${src} ${ARGN} )
    add_test( ${targ} ${targ} )
    target_bpf( ${targ} ${src} )
    target_compile_definitions( ${targ} PRIVATE SOL_TEST )
  endif()
endfunction()
