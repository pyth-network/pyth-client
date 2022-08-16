OUT_DIR := ./target
SOLANA := ../../../solana
include $(SOLANA)/sdk/bpf/c/bpf.mk

cpyth-bpf:
#   Bundle C code compiled to bpf for use by rust
	bash -c "ar rcs target/libcpyth-bpf.a target/**/*.o"
cpyth-native:
#   Compile C code to system architecture for use by rust's cargo test
	gcc -c ./src/oracle/for_cargo_test/cpyth_test.c -o ./target/cpyth_test.o
#   Bundle C code compiled to system architecture for use by rust's cargo test
	ar rcs target/libcpyth-native.a ./target/cpyth_test.o
