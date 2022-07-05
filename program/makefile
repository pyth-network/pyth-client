OUT_DIR := ./target
SOLANA := ../../solana
include $(SOLANA)/sdk/bpf/c/bpf.mk
cpyth:
	bash -c "ar rcs target/libcpyth.a target/**/*.o"
