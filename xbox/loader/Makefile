# FOLD's network loader: built by tools/build_xbox.sh --loader (through the symlink, F = the repo) ->
# build/xbox/loader/default.xbe, titled FOLD. NEVER `make clean` here (nxdk's clean deletes its shared libraries).
XBE_TITLE = FOLD
F ?= $(CURDIR)/../..
SRCS = $(F)/xbox/loader/main.c $(F)/xbox/loader/xnet_http.c
CFLAGS += -O2
NXDK_NET = y
OUTPUT_DIR = $(F)/build/xbox/loader
include $(NXDK_DIR)/Makefile
