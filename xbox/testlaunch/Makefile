# xemu test aid for the network loader (tools/xemu_loader_test.sh): starts the XBE named in E:\launch.txt from the
# hard disk, as the dashboard would. Built through the symlink (F = the repo). NEVER `make clean`.
XBE_TITLE = testlaunch
F ?= $(CURDIR)/../..
SRCS = $(F)/xbox/testlaunch/main.c
OUTPUT_DIR = $(F)/build/xbox/testlaunch
include $(NXDK_DIR)/Makefile
