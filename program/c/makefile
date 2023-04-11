# Use the OUT_DIR env variable as the output directory if available, otherwise
# default to ./target
OUT_DIR := $(if $(OUT_DIR),$(OUT_DIR),./target)
SOLANA := $(shell dirname $(shell which cargo-build-bpf))

ifneq ("$(wildcard $(SOLANA)/sdk/bpf/c/bpf.mk)","")
  $(info using Solana BPF SDK)
  include $(SOLANA)/sdk/bpf/c/bpf.mk
else
  $(info using Solana SBF SDK)
  include $(SOLANA)/sdk/sbf/c/sbf.mk
endif


# Bundle C code compiled to bpf for use by rust
.PHONY: cpyth-bpf
cpyth-bpf:
	bash -c "ar rcs $(OUT_DIR)/libcpyth-bpf.a $(OUT_DIR)/**/*.o"


# 2-Stage Contract Build
#
# 1) Compile C code to system architecture for use by rust's cargo test
# 2) Bundle C code compiled to system architecture for use by rust's cargo test
.PHONY: cpyth-native
cpyth-native:
	gcc -c ./src/oracle/native/upd_aggregate.c -o $(OUT_DIR)/cpyth-native.o -fPIC
	ar rcs $(OUT_DIR)/libcpyth-native.a $(OUT_DIR)/cpyth-native.o
