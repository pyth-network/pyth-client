OUT_DIR := ./target
#SOLANA := ../../solana
#include $(SOLANA)/sdk/bpf/c/bpf.mk
SOLANA_TOOLS = $(shell dirname $(shell which cargo-build-bpf))
include $(SOLANA_TOOLS)/sdk/bpf/c/bpf.mk
liblegacyc:
	bash -c "ar rcs target/liblegacyc.a target/**/*.o"
