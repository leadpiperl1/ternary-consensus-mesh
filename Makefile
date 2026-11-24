CC ?= gcc
NM ?= nm
PYTHON ?= python3
BUILD_DIR ?= $(HOME)/.cache/ternary_consensus_mesh_v1_1_0
CFLAGS_C99 := -std=c99 -O2 -ffp-contract=off -Wall -Wextra -Werror -pedantic
SEED ?= 42
EXIT_UNLATCHED ?= 255

.PHONY: all verify clean

all: $(BUILD_DIR)/verify_hotstate_64b_v1_1_0

$(BUILD_DIR)/verify_hotstate_64b_v1_1_0: src/verify_hotstate_64b_v1_1_0.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS_C99) $< -o $@
	@if $(NM) -u $@ | grep -Eq '\b(malloc|calloc|realloc|free)\b'; then \
		echo "[FAIL] Dynamic heap symbol found in $@" >&2; \
		exit 1; \
	fi
	@echo "[OK] C99 HotStateEntry64B verifier compiled (sizeof == 64 B, malloc = 0)."

verify: all
	@rc=0; $(BUILD_DIR)/verify_hotstate_64b_v1_1_0 --seed $(SEED) || rc=$$?; \
	if [ "$$rc" -ne "$(EXIT_UNLATCHED)" ]; then \
		echo "[FAIL] C99 verifier exited with $$rc (expected $(EXIT_UNLATCHED))" >&2; \
		exit 1; \
	fi
	$(PYTHON) benchmark_harness.py --seed $(SEED) --trials 100000 --json-out "$(BUILD_DIR)/benchmark_receipt.json"
	$(PYTHON) -c "import ast; ast.parse(open('triton_lut_kernel.py', encoding='utf-8').read()); print('[OK] triton_lut_kernel.py AST verified.')"
	env -u BASH_ENV bash "$(CURDIR)/scripts/audit_wfx_loot.sh" "$(CURDIR)"
	@echo "[ === TERNARY CONSENSUS MESH v1.1.0 VERIFIED (sizeof=64B, malloc=0, --seed $(SEED), CODE_SHOKUNIN_UNLATCHED=255) === ]"

clean:
	rm -rf "$(BUILD_DIR)"
