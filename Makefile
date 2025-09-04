# Central Makefile for CHERI PoCs (BSD make friendly; outputs in _build)

# Toolchain and flags
CC            = clang
TARGET        = --target=aarch64-unknown-freebsd
CFLAGS_COMMON = -g -O0 -Wall -fPIC $(TARGET) -mabi=purecap
LDFLAGS       = $(CFLAGS_COMMON)

# Build dirs
BUILD_DIR = _build
BIN_DIR   = $(BUILD_DIR)

# Utils
UTILS_DIR  = utils
UTILS_SRC  = $(UTILS_DIR)/utils.c
UTILS_INC  = -I$(UTILS_DIR)

# PoC directories and targets
POC_DIR = pocs

# compressed-subobject-bounds
CSB_SO_DIR     = $(POC_DIR)/compressed-subobject-bounds/simple-overflow
CSB_SO_BIN     = simple_overflow
CSB_SO_SRC     = $(CSB_SO_DIR)/simple_overflow.c
CSB_SO_CFLAGS  = -DCHERI_SUBOBJECT_SAFE -cheri-bounds=subobject-safe

CSB_OO_DIR     = $(POC_DIR)/compressed-subobject-bounds/overread-overwrite
CSB_OO_BIN     = overread_overwrite
CSB_OO_SRC     = $(CSB_OO_DIR)/main.c $(CSB_OO_DIR)/badlib.c $(UTILS_SRC)
CSB_OO_CFLAGS  = -DCHERI_SUBOBJECT_SAFE -cheri-bounds=subobject-safe -I$(CSB_OO_DIR) $(UTILS_INC)

# jemalloc-free-trimmed
JEM_SHADOW_DIR    = $(POC_DIR)/jemalloc-free-trimmed/shadow_alloc
JEM_SHADOW_BIN    = shadow_alloc
JEM_SHADOW_SRC    = $(JEM_SHADOW_DIR)/shadow_alloc.c $(UTILS_SRC)
JEM_SHADOW_CFLAGS = $(UTILS_INC)

JEM_UNPAINT_DIR     = $(POC_DIR)/jemalloc-free-trimmed/unpaint_shadow_memory
JEM_UNPAINT_BIN     = looped_threaded_10_step
JEM_UNPAINT_SRC     = $(JEM_UNPAINT_DIR)/looped_threaded_10_step.c $(UTILS_SRC)
JEM_UNPAINT_CFLAGS  = $(UTILS_INC) -pthread
JEM_UNPAINT_LDFLAGS = -pthread

# dlmalloc-header-bypass
DLM_DIR     = $(POC_DIR)/dlmalloc-header-bypass
DLM_BIN     = bypass_header_check
DLM_SRC     = $(DLM_DIR)/bypass_header_check.c $(UTILS_SRC)
DLM_CFLAGS  = $(UTILS_INC)

# uninit-capabilities
UIC_DIR     = $(POC_DIR)/uninit-capabilities
UIC_BIN     = call_execv_via_uninit_mmr
UIC_SRC     = $(UIC_DIR)/call_execv_via_uninit_mmr.c $(UTILS_SRC)
UIC_CFLAGS  = $(UTILS_INC)

# Aggregate binaries (full paths under _build/)
ALL_BINS = \
  $(BIN_DIR)/$(CSB_SO_BIN) \
  $(BIN_DIR)/$(CSB_OO_BIN) \
  $(BIN_DIR)/$(JEM_SHADOW_BIN) \
  $(BIN_DIR)/$(JEM_UNPAINT_BIN) \
  $(BIN_DIR)/$(DLM_BIN) \
  $(BIN_DIR)/$(UIC_BIN)

# Short names (for alias & run targets)
BIN_NAMES = \
  $(CSB_SO_BIN) \
  $(CSB_OO_BIN) \
  $(JEM_SHADOW_BIN) \
  $(JEM_UNPAINT_BIN) \
  $(DLM_BIN) \
  $(UIC_BIN)

.PHONY: all list clean run
all: $(ALL_BINS)

list:
	@echo "Available targets:"
	@for f in $(BIN_NAMES); do echo "  $$f"; done

# Generic runner: make run BIN=<name>
run:
.if empty(BIN)
	@echo "usage: make run BIN=<$(BIN_NAMES)>"; exit 2
.else
	@$(MAKE) $(BIN_DIR)/$(BIN)
	@echo "Running $(BIN)"
	@cd $(BIN_DIR) && ./$(BIN)
.endif

# --- Build rules (ensure _build exists in each recipe) ---
$(BIN_DIR)/$(CSB_SO_BIN): $(CSB_SO_SRC)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS_COMMON) $(CSB_SO_CFLAGS) -o $@ $(CSB_SO_SRC)

$(BIN_DIR)/$(CSB_OO_BIN): $(CSB_OO_SRC)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS_COMMON) $(CSB_OO_CFLAGS) -o $@ $(CSB_OO_SRC)

$(BIN_DIR)/$(JEM_SHADOW_BIN): $(JEM_SHADOW_SRC)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS_COMMON) $(JEM_SHADOW_CFLAGS) -o $@ $(JEM_SHADOW_SRC)

$(BIN_DIR)/$(JEM_UNPAINT_BIN): $(JEM_UNPAINT_SRC)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS_COMMON) $(JEM_UNPAINT_CFLAGS) -o $@ $(JEM_UNPAINT_SRC) $(JEM_UNPAINT_LDFLAGS)

$(BIN_DIR)/$(DLM_BIN): $(DLM_SRC)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS_COMMON) $(DLM_CFLAGS) -o $@ $(DLM_SRC)

$(BIN_DIR)/$(UIC_BIN): $(UIC_SRC)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS_COMMON) $(UIC_CFLAGS) -o $@ $(UIC_SRC)

# --- Convenience aliases: build by short name; run by short name ---
.for b in $(BIN_NAMES)
${b}: $(BIN_DIR)/${b}
	@:

run-${b}: $(BIN_DIR)/${b}
	@echo "Running ${b}"
	@cd $(BIN_DIR) && ./${b}
.endfor

clean:
	rm -rf $(BUILD_DIR)
