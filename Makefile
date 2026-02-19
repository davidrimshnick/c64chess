# C64 Chess Engine - Dual-target Makefile
# cc65 for C64, gcc for PC

# --- Tool paths (override on command line if needed) ---
# w64devkit provides a self-contained gcc at C:\w64devkit\bin
CC65    ?= cl65
CC      ?= gcc
W64DEV  = C:/w64devkit/bin
export PATH := $(W64DEV):$(PATH)

# --- Directories ---
SRCDIR  = src
BUILDDIR= build
C64DIR  = $(SRCDIR)/c64
UCIDIR  = $(SRCDIR)/uci
TESTDIR = tests

# --- Source files ---
COMMON_SRC = $(SRCDIR)/board.c $(SRCDIR)/movegen.c $(SRCDIR)/search.c \
             $(SRCDIR)/eval.c $(SRCDIR)/movesort.c $(SRCDIR)/tt.c \
             $(SRCDIR)/tables.c

C64_SRC    = $(COMMON_SRC) $(SRCDIR)/main.c $(C64DIR)/ui.c $(C64DIR)/platform.c
PC_SRC     = $(COMMON_SRC) $(SRCDIR)/main.c $(UCIDIR)/uci.c
MCTS_SRC   = $(SRCDIR)/board.c $(SRCDIR)/movegen.c $(SRCDIR)/search.c \
             $(SRCDIR)/tables.c $(SRCDIR)/tt.c $(SRCDIR)/eval.c \
             $(SRCDIR)/movesort.c $(SRCDIR)/mcts.c $(SRCDIR)/mcts_main.c \
             $(UCIDIR)/uci.c
TEST_SRC   = $(COMMON_SRC) $(UCIDIR)/uci.c \
             $(TESTDIR)/test_main.c $(TESTDIR)/test_board.c \
             $(TESTDIR)/test_movegen.c $(TESTDIR)/test_search.c

# --- cc65 flags ---
C64_CFLAGS  = -t c64 -O -Cl -DTARGET_C64
C64_LDFLAGS = -t c64 -C c64chess.cfg -m $(BUILDDIR)/c64chess.map

# --- gcc flags ---
PC_CFLAGS   = -O2 -Wall -Wextra -DTARGET_PC -I$(SRCDIR)
TEST_CFLAGS = -O0 -g -Wall -Wextra -DTARGET_PC -DTARGET_TEST -I$(SRCDIR) -I$(TESTDIR)

# --- Targets ---
.PHONY: all pc c64 mcts test clean

all: pc

mcts: $(BUILDDIR)/mctslite.exe

pc: $(BUILDDIR)/c64chess.exe

c64: $(BUILDDIR)/c64chess.prg

test: $(BUILDDIR)/test_chess.exe
	$(BUILDDIR)/test_chess.exe

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# PC build
$(BUILDDIR)/c64chess.exe: $(PC_SRC) | $(BUILDDIR)
	$(CC) $(PC_CFLAGS) -o $@ $(PC_SRC)

# MCTS engine build
$(BUILDDIR)/mctslite.exe: $(MCTS_SRC) | $(BUILDDIR)
	$(CC) $(PC_CFLAGS) -o $@ $(MCTS_SRC) -lm

# C64 build
$(BUILDDIR)/c64chess.prg: $(C64_SRC) c64chess.cfg | $(BUILDDIR)
	$(CC65) $(C64_CFLAGS) $(C64_LDFLAGS) -o $@ $(C64_SRC)

# Test build
$(BUILDDIR)/test_chess.exe: $(TEST_SRC) | $(BUILDDIR)
	$(CC) $(TEST_CFLAGS) -o $@ $(TEST_SRC)

clean:
	rm -rf $(BUILDDIR)

# Run in VICE emulator
vice: c64
	x64sc -autostart $(BUILDDIR)/c64chess.prg
