# c64-hw-sim - pin level simulator of the C64 ASSY 250407 motherboard.
#
#   make            build ./8BitHWSimulator (8BitHWSimulator.exe on Windows)
#   make clean

CC      ?= cc
OPT     ?= -O3 -flto
CFLAGS  := $(OPT) -g -Wall -Wextra -Wno-unused-parameter -Isrc -Ichips -Ithird_party -DSOKOL_GLCORE

SIM_SRC = src/sim.c src/parts.c src/board.c src/c64.c src/cart.c src/harness.c src/c64_board_data.c src/c64c_board_data.c src/c64b_board_data.c src/zx48.c src/pet_parts.c src/pet_chips.c src/pet.c src/vic20_chips.c src/vic20.c src/c16_chips.c src/c128_chips.c src/c128_drive.c src/c16.c src/c128.c src/c128_board_data.c src/c16_board_data.c src/plus4.c src/plus4_board_data.c src/vic20b_board_data.c src/vic20a_board_data.c src/scld.c src/ts2068.c src/ula81.c src/ts1000.c src/ts1500.c src/zx80.c src/zx80_parts.c src/ula.c src/zx48_board_data.c src/zx48i2_board_data.c src/zx48i1_board_data.c src/zx48i4b_board_data.c src/zx48i6a_board_data.c src/pet4032_board_data.c src/pet8032_board_data.c src/ts2068_board_data.c src/ts1000_board_data.c src/ts1000i1_board_data.c src/ts1500_board_data.c src/zx80_board_data.c src/png.c src/chips_impl.c src/apple2_chips.c src/apple2e.c src/apple2e_chips.c src/apple2e_board_data.c src/m6809.c src/coco_chips.c src/coco2.c src/coco2_board_data.c src/dragon64.c src/dragon64_board_data.c src/m6803.c src/mc10_chips.c src/mc10.c src/mc10_board_data.c src/atari800.c src/atari800_chips.c src/atari800_board_data.c src/cpc464.c src/cpc464_board_data.c
APP_SRC = src/main.c src/ui.c src/inspect.c src/browser.c src/diagnose.c src/repairs.c src/icon_data.c src/atlas_browse.c src/thumbtex.c src/probe.c src/probe_db.c src/probe_file.c src/filedialog.c src/atlas.c src/atlas_match.c src/stb_image_impl.c src/healthy.c src/lacap.c src/lacap_file.c src/lacap_record.c src/lacap_compare.c src/lacap_capture.c src/lacap_rules.c src/lacap_verdict.c src/lacap_diagnose.c src/lacap_zxtest.c src/lapico.c src/la_window.c src/keymap_view.c

EXE =
ifeq ($(OS),Windows_NT)
  # MinGW-w64 gcc; run make from a POSIX shell (Git Bash, MSYS2) for mkdir/rm.
  ifeq ($(origin CC),default)
    CC = gcc
  endif
  EXE = .exe
  SOKOL_FLAGS = -Wno-unused-but-set-variable -Wno-sign-compare -Wno-unused-variable \
                -Wno-cast-function-type -Wno-unknown-pragmas
  LIBS = -mwindows -static -lpthread -lkernel32 -luser32 -lgdi32 -lshell32 -lcomdlg32 -lopengl32 -ladvapi32 -lsetupapi -lcfgmgr32 -lwinmm -lm
else ifeq ($(shell uname -s),Darwin)
  SOKOL_FLAGS = -x objective-c -Wno-deprecated-declarations -Wno-unused-but-set-variable \
                -Wno-sign-compare -Wno-unused-variable
  LIBS = -framework Cocoa -framework QuartzCore -framework OpenGL
else
  SOKOL_FLAGS = -Wno-unused-but-set-variable -Wno-sign-compare -Wno-unused-variable
  LIBS = -lX11 -lXi -lXcursor -lGL -ldl -lpthread -lm
endif

all: 8BitHWSimulator$(EXE)

build/sokol_impl.o: src/sokol_impl.c third_party/sokol/*.h third_party/nuklear/*.h
	@mkdir -p build
	$(CC) $(CFLAGS) $(SOKOL_FLAGS) -c $< -o $@

build/%.o: src/%.c src/*.h chips/*.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

OBJS = $(patsubst src/%.c,build/%.o,$(SIM_SRC) $(APP_SRC)) build/sokol_impl.o

# The .exe's icon (Explorer, the taskbar, shortcuts): src/icon.rc names src/c64-hw-sim.ico.
ifeq ($(OS),Windows_NT)
  WINDRES ?= windres
  OBJS += build/icon.o
build/icon.o: src/icon.rc src/c64-hw-sim.ico
	@mkdir -p build
	$(WINDRES) -I src -i src/icon.rc -O coff -o $@
endif

8BitHWSimulator$(EXE): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LIBS)

clean:
	rm -rf build 8BitHWSimulator$(EXE)

.PHONY: all clean
