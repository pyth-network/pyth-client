OUT_DIR := ./target
SOLANA := ../../../solana
include $(SOLANA)/sdk/bpf/c/bpf.mk
cpyth:
	bash -c "ar rcs target/libcpyth.a target/**/*.o"
cpythtest:
	gcc -c ./src/oracle/for_cargo_test/cpyth_test.c -o ./target/cpyth_test.o
	ar rcs target/libcpythtest.a ./target/cpyth_test.o
