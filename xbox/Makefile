# FOLD on the original Xbox (nxdk): tools/build_xbox.sh (make can't take the space in the repo's path, so it builds
# through a symlink: F = the repo by a path without spaces)
# -> build/xbox/default.xbe, and build/xbox/FOLD/ (the XBE with assets/ and levels/ next to it, ready for E:\Games\FOLD)
# plus build/xbox/fold.iso of that folder (for xemu: tools/run_xemu.sh). DEMO=file puts a pad script in as demo.txt.
# NEVER `make clean` here: nxdk's clean also deletes the shared nxdk libraries in ~/xboxdev/nxdk/lib.
XBE_TITLE = FOLD
F ?= $(CURDIR)/..
SRCS = $(F)/app/main_xbox.c $(F)/app/app.c $(F)/app/gfx.c $(F)/app/vmx.c $(F)/app/icons3d.c $(F)/app/padscript.c $(F)/core/fold.c
CFLAGS += -O2 -include $(F)/xbox/xbox_compat.h
NXDK_SDL = y
OUTPUT_DIR = $(F)/build/xbox
include $(NXDK_DIR)/Makefile

package: $(OUTPUT_DIR)/default.xbe
	rm -rf $(OUTPUT_DIR)/FOLD $(OUTPUT_DIR)/fold.iso
	mkdir -p $(OUTPUT_DIR)/FOLD/assets $(OUTPUT_DIR)/FOLD/levels
	cp $(OUTPUT_DIR)/default.xbe $(OUTPUT_DIR)/FOLD/
	cp $(F)/assets/metal.rgb $(F)/assets/vmx_small.rgba $(F)/assets/vmx_large.rgba $(F)/assets/duke.mesh $(OUTPUT_DIR)/FOLD/assets/
	cp $(F)/levels/*.fold $(OUTPUT_DIR)/FOLD/levels/
	if [ -n "$(DEMO)" ]; then cp $(DEMO) $(OUTPUT_DIR)/FOLD/demo.txt; fi
	$(EXTRACT_XISO) -c $(OUTPUT_DIR)/FOLD $(OUTPUT_DIR)/fold.iso >/dev/null
	@echo "built $(OUTPUT_DIR)/FOLD and $(OUTPUT_DIR)/fold.iso"
