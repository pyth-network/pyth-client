OUT_DIR := ./target
SOLANA := ../../../solana
include $(SOLANA)/sdk/bpf/c/bpf.mk
cpyth:
	bash -c "ar rcs target/libcpyth.a target/**/*.o"
cpythtest:
	gcc -c ../rust/upd_aggregate.c -o ./target/upd_aggregate.o
	ar rcs target/libcpythtest.a ./target/upd_aggregate.o
