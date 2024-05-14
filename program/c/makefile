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

FEATURES_H_BODY:="\#pragma once"


.PHONY: features.h # Putting this in .PHONY makes sure the header is always regenerated
features.h:
	echo $(FEATURES_H_BODY) > src/oracle/features.h


# Bundle C code compiled to bpf for use by rust
# The all target is defined by the solana makefile included above and generates the needed .o file.
.PHONY: cpyth-bpf
cpyth-bpf: features.h all
	bash -c "ar rc $(OUT_DIR)/libcpyth-bpf.a $(OUT_DIR)/oracle/*.o"


# 2-Stage Contract Build
#
# 1) Compile C code to system architecture for use by rust's cargo test
# 2) Bundle C code compiled to system architecture for use by rust's cargo test
.PHONY: cpyth-native
cpyth-native: features.h
	gcc -c ./src/oracle/native/upd_aggregate.c -o $(OUT_DIR)/cpyth-native.o -fPIC
	ar rcs $(OUT_DIR)/libcpyth-native.a $(OUT_DIR)/cpyth-native.o


# Note: there's probably a smart way to do this with wildcards but I (jayant) can't figure it out
.PHONY: test
test: features.h
	mkdir -p $(OUT_DIR)/test/
	gcc -c ./src/oracle/model/test_price_model.c -o $(OUT_DIR)/test/test_price_model.o -fPIC
	gcc -c ./src/oracle/util/test_align.c -o $(OUT_DIR)/test/test_align.o -fPIC
	gcc -c ./src/oracle/util/test_avg.c -o $(OUT_DIR)/test/test_avg.o -fPIC
	gcc -c ./src/oracle/util/test_hash.c -o $(OUT_DIR)/test/test_hash.o -fPIC
	gcc -c ./src/oracle/util/test_prng.c -o $(OUT_DIR)/test/test_prng.o -fPIC
	gcc -c ./src/oracle/util/test_round.c -o $(OUT_DIR)/test/test_round.o -fPIC
	gcc -c ./src/oracle/util/test_sar.c -o $(OUT_DIR)/test/test_sar.o -fPIC
	ar rcs $(OUT_DIR)/libcpyth-test.a $(OUT_DIR)/test/*.o
