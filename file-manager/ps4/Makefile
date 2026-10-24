TITLE       := TIM PS4 File Manager
VERSION     := 0.1.0
TITLE_ID    := TIMF00001
CONTENT_ID  := IV0000-TIMF00001_00-TIMFMANAGER00001
TOOLCHAIN   := $(OO_PS4_TOOLCHAIN)
CC          := clang
CCX         := clang++
LD          := ld.lld
PLATFORM    := ../../native/ps4/platform
INTDIR      := build
CFILES      := ../file_ops.c ../file_browser.c
CPPFILES    := main.cpp $(PLATFORM)/graphics.cpp
OBJS        := $(INTDIR)/file_ops.o $(INTDIR)/file_browser.o $(INTDIR)/main.o $(INTDIR)/graphics.o
CFLAGS      := --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -c -isysroot $(TOOLCHAIN) -isystem $(TOOLCHAIN)/include -I..
CXXFLAGS    := $(CFLAGS) -isystem $(TOOLCHAIN)/include/c++/v1 -I$(PLATFORM)
LIBS        := -lc -lkernel -lc++ -lSceVideoOut -lScePad
LDFLAGS     := -m elf_x86_64 -pie --script $(TOOLCHAIN)/link.x --eh-frame-hdr -L$(TOOLCHAIN)/lib $(LIBS) $(TOOLCHAIN)/lib/crt1.o
PKGTOOLS    := $(TOOLCHAIN)/bin/linux

.PHONY: all clean verify-pkg
all: $(CONTENT_ID).pkg verify-pkg

$(INTDIR):
	mkdir -p $@

$(INTDIR)/file_ops.o: ../file_ops.c ../file_ops.h | $(INTDIR)
	$(CC) $(CFLAGS) -o $@ $<

$(INTDIR)/file_browser.o: ../file_browser.c ../file_browser.h ../file_ops.h | $(INTDIR)
	$(CC) $(CFLAGS) -o $@ $<

$(INTDIR)/main.o: main.cpp ../file_browser.h ../file_ops.h $(PLATFORM)/graphics.h | $(INTDIR)
	$(CCX) $(CXXFLAGS) -o $@ $<

$(INTDIR)/graphics.o: $(PLATFORM)/graphics.cpp $(PLATFORM)/graphics.h | $(INTDIR)
	$(CCX) $(CXXFLAGS) -o $@ $<

# PS4 must receive the FSELF eboot.bin, not the intermediate ELF/OELF.
eboot.bin: $(OBJS)
	mkdir -p $(INTDIR)
	$(LD) $(OBJS) -o $(INTDIR)/file-manager.elf $(LDFLAGS)
	test -s $(INTDIR)/file-manager.elf
	./tools/create-fself -in=$(INTDIR)/file-manager.elf -out=$(INTDIR)/file-manager.oelf --eboot "eboot.bin" --paid 0x3800000000000011
	test -s $(INTDIR)/file-manager.oelf
	test -s eboot.bin
	! cmp -s $(INTDIR)/file-manager.elf eboot.bin
	! cmp -s $(INTDIR)/file-manager.oelf eboot.bin

sce_sys/icon0.png: ../../native/ps4/scripts/make_icon.py
	mkdir -p sce_sys
	python3 $< $@

sce_sys/param.sfo: Makefile
	mkdir -p sce_sys
	$(PKGTOOLS)/PkgTool.Core sfo_new $@
	$(PKGTOOLS)/PkgTool.Core sfo_setentry $@ APP_TYPE --type Integer --maxsize 4 --value 1
	$(PKGTOOLS)/PkgTool.Core sfo_setentry $@ APP_VER --type Utf8 --maxsize 8 --value '$(VERSION)'
	$(PKGTOOLS)/PkgTool.Core sfo_setentry $@ ATTRIBUTE --type Integer --maxsize 4 --value 0
	$(PKGTOOLS)/PkgTool.Core sfo_setentry $@ CATEGORY --type Utf8 --maxsize 4 --value 'gd'
	$(PKGTOOLS)/PkgTool.Core sfo_setentry $@ CONTENT_ID --type Utf8 --maxsize 48 --value '$(CONTENT_ID)'
	$(PKGTOOLS)/PkgTool.Core sfo_setentry $@ DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
	$(PKGTOOLS)/PkgTool.Core sfo_setentry $@ SYSTEM_VER --type Integer --maxsize 4 --value 0
	$(PKGTOOLS)/PkgTool.Core sfo_setentry $@ TITLE --type Utf8 --maxsize 128 --value '$(TITLE)'
	$(PKGTOOLS)/PkgTool.Core sfo_setentry $@ TITLE_ID --type Utf8 --maxsize 12 --value '$(TITLE_ID)'
	$(PKGTOOLS)/PkgTool.Core sfo_setentry $@ VERSION --type Utf8 --maxsize 8 --value '$(VERSION)'

# Write the GP4 manifest explicitly. create-gp4 has known cases where it
# emits incomplete rootdir entries; PkgTool may still build a PKG that the
# console rejects. Keep the file list and package metadata explicit here.
pkg.gp4: eboot.bin sce_sys/param.sfo sce_sys/icon0.png
	test -s eboot.bin
	test -s sce_sys/param.sfo
	test -s sce_sys/icon0.png
	@printf '<?xml version="1.0"?>\n' > $@
	@printf '<psproject fmt="gp4" version="1000">\n' >> $@
	@printf '\t<volume>\n' >> $@
	@printf '\t\t<volume_type>pkg_ps4_app</volume_type>\n' >> $@
	@printf '\t\t<volume_id>PS4VOLUME</volume_id>\n' >> $@
	@printf '\t\t<volume_ts>2026-01-01 00:00:00</volume_ts>\n' >> $@
	@printf '\t\t<package content_id="$(CONTENT_ID)" passcode="00000000000000000000000000000000" storage_type="digital50" app_type="full" />\n' >> $@
	@printf '\t\t<chunk_info chunk_count="1" scenario_count="1"><chunks><chunk id="0" layer_no="0" label="Chunk #0" /></chunks><scenarios default_id="0"><scenario id="0" type="sp" initial_chunk_count="1" label="Scenario #0">0</scenario></scenarios></chunk_info>\n' >> $@
	@printf '\t</volume>\n' >> $@
	@printf '\t<files img_no="0">\n' >> $@
	@printf '\t\t<file targ_path="eboot.bin" orig_path="eboot.bin" />\n' >> $@
	@printf '\t\t<file targ_path="sce_sys/param.sfo" orig_path="sce_sys/param.sfo" />\n' >> $@
	@printf '\t\t<file targ_path="sce_sys/icon0.png" orig_path="sce_sys/icon0.png" />\n' >> $@
	@printf '\t</files>\n' >> $@
	@printf '\t<rootdir><dir targ_name="sce_sys"><dir targ_name="about" /></dir></rootdir>\n' >> $@
	@printf '</psproject>\n' >> $@
	test -s $@
	grep -q 'content_id="$(CONTENT_ID)"' $@
	grep -q 'targ_path="eboot.bin"' $@
	grep -q 'targ_path="sce_sys/param.sfo"' $@
	grep -q 'targ_path="sce_sys/icon0.png"' $@

$(CONTENT_ID).pkg: pkg.gp4
	$(PKGTOOLS)/PkgTool.Core pkg_build $< .
	test -s $@

verify-pkg: $(CONTENT_ID).pkg
	test -s "$<"
	test "$(shell wc -c < $<)" -gt 1000000

clean:
	rm -rf $(INTDIR) sce_sys/param.sfo eboot.bin pkg.gp4 $(CONTENT_ID).pkg
