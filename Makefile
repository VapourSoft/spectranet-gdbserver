
ROOT_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
GDBSERVER_C_SOURCES=$(filter-out src/pcw_dart.c,$(wildcard src/*.c))
GDBSERVER_C_OBJECTS=$(GDBSERVER_C_SOURCES:.c=_c.o)
GDBSERVER_ASM_SOURCES=$(wildcard src/*.asm)
GDBSERVER_ASM_OBJECTS=$(GDBSERVER_ASM_SOURCES:.asm=_asm.o)
INCLUDES=-I$(ROOT_DIR)/include/spectranet -I$(ROOT_DIR)/src
JUST_PRINT:=$(findstring n,$(MAKEFLAGS))
Z80ASM_CANDIDATES := $(wildcard /c/z88dk/bin/z80asm) $(wildcard /c/z88dk/bin/z80asm.exe) $(shell command -v z80asm 2>/dev/null)
Z80ASM := $(firstword $(Z80ASM_CANDIDATES))
ZCC_CANDIDATES := $(wildcard /c/z88dk/bin/zcc) $(wildcard /c/z88dk/bin/zcc.exe) $(shell command -v zcc 2>/dev/null)
ZCC := $(firstword $(ZCC_CANDIDATES))

ifneq (,$(JUST_PRINT))
Z80ASM_CANDIDATES := $(wildcard /c/z88dk/bin/z80asm) $(wildcard /c/z88dk/bin/z80asm.exe) $(shell command -v z80asm 2>/dev/null)
Z80ASM := $(firstword $(Z80ASM_CANDIDATES))
	PHONY_OBJS := yes
	CC = gcc
	LD = ar
	FAKE_DEFINES = -D__LIB__="" -D__CALLEE__="" -D__FASTCALL__=""
	FAKE_INCLUDES = -I/usr/local/share/z88dk/include
	CFLAGS = $(FAKE_DEFINES) -nostdinc $(INCLUDES) $(FAKE_INCLUDES)
	GDBSERVER_FLAGS = -o build/gdbserver
else
	CC = zcc
	LD = zcc
	CFLAGS = +zx $(DEBUG) $(INCLUDES)
	LINK_FLAGS = -L$(ROOT_DIR)/libs -llibs/libsocket_np.lib -llibs/libspectranet_np.lib
	BIN_FLAGS = -startup=31 --no-crt -subtype=bin
	PRG_OBJECTS = src/prg/prg.c
	LDFLAGS = +zx $(DEBUG) $(LINK_FLAGS)
	GDBSERVER_FLAGS = -create-app
endif

all: gdbserver

# Build testver.com (CP/M COM utility to test BDOS 12/RSX handler)
.PHONY: testver
testver:
	# zcc +cpm -subtype=pcw80 src/testver.c -o testver -create-app 
	zcc +cpm  -compiler=sdcc -clib=sdcc_ix -SO3 -vn -O2 src/testver.c -o testver -create-app 

# CP/M serial build (PCW DART) using sdcc_ix
.PHONY: cpm
cpm:
	zcc +cpm  -compiler=sdcc -clib=sdcc_ix -SO3 -vn -O2 \
	  -DTARGET_PCW_DART $(CPM_EXTRA) \
	  src/pcw_rst8.asm src/pcw_rst8.c src/pcw_dart.c \
	  src/state.c src/utils.c src/server.c src/cpm_stubs.c src/cpm_main.c \
	  -o ZDBG -create-app

# Build RSX PRL from ASM+C (test C linkage in RSX)
.PHONY: rsx-c-prl

rsx-c-prl: build
	@echo "[RSX-C-PRL] Using z80asm to build dual ORG images (ASM+C)"
	@if ! command -v zcc >/dev/null 2>&1; then echo "ERROR: zcc not in PATH"; exit 1; fi
	@if ! command -v z80asm >/dev/null 2>&1; then echo "ERROR: z80asm not in PATH"; exit 1; fi
	rm -f rsx_body_template.bin rsx_body_real.bin src/*.o

#	#build our lib as sdcc_ix lib is is built on the fly by zcc and we need vcertain functions for the z80asm link step!
#	@echo "Compiling sdcc ix lib"
#	( cd src && zcc +cpm -compiler=sdcc -clib=sdcc_ix -c strstr.asm strstr_callee.asm asm_strstr.asm -o sdcc_ix.o ) || exit 1
#	@if [ ! -f src/sdcc_ix.o ]; then echo "ERROR: sdcc_ix.o not produced"; exit 1; fi	

	@echo "Compiling C object files"
	( cd src && zcc +cpm -DTARGET_PCW_DART -compiler=sdcc -clib=sdcc_ix -c rsx_cfunc.c -o rsx_cfunc.o ) || exit 1
	@if [ ! -f src/rsx_cfunc.o ]; then echo "ERROR: rsx_cfunc.o not produced"; exit 1; fi	

	(cd src && zcc +cpm -DTARGET_PCW_DART -compiler=sdcc -clib=sdcc_ix -c server.c utils.c -o server.o ) || exit 1
	@if [ ! -f src/server.o ]; then echo "ERROR: server.o not produced"; exit 1; fi	

	( cd src && zcc +cpm -DTARGET_PCW_DART -compiler=sdcc -clib=sdcc_ix -c pcw_dart.c -o pcw_dart.o ) || exit 1
	@if [ ! -f src/pcw_dart.o ]; then echo "ERROR: pcw_dart.o not produced"; exit 1; fi	

	( cd src && zcc +cpm -DTARGET_PCW_DART -compiler=sdcc -clib=sdcc_ix -c pcw_rst8.c -o pcw_rst8_c.o ) || exit 1
	@if [ ! -f src/pcw_rst8_c.o ]; then echo "ERROR: pcw_rst8_c.o not produced"; exit 1; fi	

	( cd src && zcc +cpm -DTARGET_PCW_DART -compiler=sdcc -clib=sdcc_ix -c state.c -o state.o ) || exit 1
	@if [ ! -f src/state.o ]; then echo "ERROR: state.o not produced"; exit 1; fi	

	( cd src && zcc +cpm -DTARGET_PCW_DART -compiler=sdcc -clib=sdcc_ix -c z80_decode.c -o z80_decode.o ) || exit 1
	@if [ ! -f src/z80_decode.o ]; then echo "ERROR: z80_decode.o not produced"; exit 1; fi	

# need to fix this - order of objects and everthing MUST be the same - should be defined once!
	@echo "[RSX-C-PRL] Template (TEMPLATE macro -> ORG 0000h)"
	( cd src && z80asm -b rsx_body.asm runtime.asm pcw_rst8.asm pcw_rst8_c.o state.o rsx_cfunc.o pcw_dart.o server.o z80_decode.o -DTEMPLATE  ) || exit 1
	mv src/rsx_body.bin rsx_body_template.bin
	@if [ ! -f rsx_body_template.bin ]; then echo "ERROR: rsx_body_template.bin not produced (template)"; exit 1; fi

	@echo "[RSX-C-PRL] Real (default ORG 0100h)"
	( cd src && z80asm -b rsx_body.asm runtime.asm pcw_rst8.asm pcw_rst8_c.o state.o rsx_cfunc.o pcw_dart.o server.o z80_decode.o  ) || exit 1
	mv src/rsx_body.bin rsx_body_real.bin
	@if [ ! -f rsx_body_real.bin ]; then echo "ERROR: rsx_body_real.bin not produced (real)"; exit 1; fi

	@echo "[RSX-C-PRL] Running makeprl"
	@if [ -f tools/makeprl.exe ]; then \
  tools/makeprl.exe rsx_body_template.bin rsx_body_real.bin NUL ZRSXC.RSX; \
else \
  ./tools/makeprl rsx_body_template.bin rsx_body_real.bin NUL ZRSXC.RSX; \
fi
	@echo "[RSX-C-PRL] Done -> ZRSXC.RSX"
	
# Build RSX PRL using dual ORG images and makeprl utility
.PHONY: rsx-prl
rsx-prl: build
	@echo "[RSX-PRL] Using z80asm to build dual ORG images"
	@if ! command -v z80asm >/dev/null 2>&1; then echo "ERROR: z80asm not in PATH"; exit 1; fi
	@echo "[RSX-PRL] Template (TEMPLATE macro -> ORG 0201h)"
	rm -f rsx_body_template.bin rsx_body_real.bin
	( cd src && rm -f rsx_body.bin && z80asm -b -DTEMPLATE rsx_body.asm ) || exit 1
	@if [ ! -f src/rsx_body.bin ]; then echo "ERROR: src/rsx_body.bin not produced (template)"; exit 1; fi
	mv src/rsx_body.bin rsx_body_template.bin
	@echo "[RSX-PRL] Real (default ORG 0100h)"
	( cd src && rm -f rsx_body.bin && z80asm -b rsx_body.asm ) || exit 1
	@if [ ! -f src/rsx_body.bin ]; then echo "ERROR: src/rsx_body.bin not produced (real)"; exit 1; fi
	mv src/rsx_body.bin rsx_body_real.bin
	@echo "[RSX-PRL] Running makeprl"
	@if [ -f tools/makeprl.exe ]; then \
	  tools/makeprl.exe rsx_body_template.bin rsx_body_real.bin NUL ZDBG_RSX.PRL; \
	else \
	  ./tools/makeprl rsx_body_template.bin rsx_body_real.bin NUL ZDBG_RSX.PRL; \
	fi
	@echo "[RSX-PRL] Done -> ZDBG_RSX.PRL"

# (Optional) build Linux helper version if needed (not required when makeprl.exe exists)
tools/makeprl: tools/makeprl.c
	gcc -O2 -o $@ $< || true

# Build RSXCHK utility
.PHONY: rsxchk
rsxchk:
	zcc +cpm -compiler=sdcc -clib=sdcc_ix -SO3 -vn -O2 \
	  src/rsxchk.c \
	  -o RSXCHK -create-app

build/gdbserver: $(GDBSERVER_C_OBJECTS) $(GDBSERVER_ASM_OBJECTS)
	$(LD) $(LDFLAGS) $(BIN_FLAGS) -o build/gdbserver $(GDBSERVER_FLAGS) $(GDBSERVER_C_OBJECTS) $(GDBSERVER_ASM_OBJECTS)

build:
	mkdir -p $@

include/spectranet:
	@mkdir -p include/spectranet

libs:
	mkdir -p $@

libs/libsocket_np.lib: libs include/spectranet
	make DESTLIBS=$(ROOT_DIR)/libs DESTINCLUDE=$(ROOT_DIR)/include/spectranet SRCGEN=$(ROOT_DIR)/spectranet/scripts/makesources.pl -C spectranet/socklib nplib install

libs/libspectranet_np.lib: libs include/spectranet
	make DESTLIBS=$(ROOT_DIR)/libs DESTINCLUDE=$(ROOT_DIR)/include/spectranet SRCGEN=$(ROOT_DIR)/spectranet/scripts/makesources.pl -C spectranet/libspectranet nplib install

spectranet-libs: libs/libsocket_np.lib libs/libspectranet_np.lib

gdbserver: build spectranet-libs build/gdbserver

%_c.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

%_asm.o: %.asm
	$(CC) $(CFLAGS) -o $@ -c $<

get-size:
	@cat build/gdbserver.map | sed -n "s/^\\([a-zA-Z0-9_]*\\).*= .\([A-Z0-9]*\) ; \([^,]*\), .*/\2,\1,\3/p" | sort | python3 tools/symbol_sizes.py

deploy:
	ethup 127.0.0.1 build/gdbserver

clean:
	@rm -rf build/*
	@rm -f src/*.o
	@rm -f libs/*
	@rm -f spectranet/libspectranet/*.o
	@rm -f spectranet/socklib/*.o

.PHONY: clean get-size deploy

ifeq ($(PHONY_OBJS),yes)
.PHONY: $(GDBSERVER_SOURCES)
.PHONY: spectranet-libs libs/libsocket_np.lib libs/libspectranet_np.lib
endif