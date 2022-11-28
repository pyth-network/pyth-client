OUT_DIR := ./target
SOLANA := $(shell dirname $(shell which cargo-build-bpf))
include $(SOLANA)/sdk/bpf/c/bpf.mk

cpyth-bpf:
#   Bundle C code compiled to bpf for use by rust
	bash -c "ar rcs target/libcpyth-bpf.a target/**/*.o"
cpyth-native:
#   Compile C code to system architecture for use by rust's cargo test
	gcc -c ./src/oracle/native/upd_aggregate.c -o ./target/cpyth-native.o -fPIC
#   Bundle C code compiled to system architecture for use by rust's cargo test
	ar rcs target/libcpyth-native.a ./target/cpyth-native.o
