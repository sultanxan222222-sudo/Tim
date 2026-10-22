TITLE       := TIM PS4 File Manager
VERSION     := 0.1.0
TITLE_ID    := TIMF00001
CONTENT_ID  := IV0000-TIMF00001_00-TIMFILEMANAGER001
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

.PHONY: all clean
all: $(CONTENT_ID).pkg

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

eboot.bin: $(OBJS)
	$(LD) $(OBJS) -o $(INTDIR)/file-manager.elf $(LDFLAGS)
	./tools/create-fself -in=$(INTDIR)/file-manager.elf -out=$(INTDIR)/file-manager.oelf --eboot "eboot.bin" --paid 0x3800000000000011
	test -s $(INTDIR)/file-manager.oelf
	test -s eboot.bin

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

pkg.gp4: eboot.bin sce_sys/param.sfo sce_sys/icon0.png
	test -s eboot.bin
	./tools/create-gp4 -out $@ --content-id=$(CONTENT_ID) --files "$^"

$(CONTENT_ID).pkg: pkg.gp4
	$(PKGTOOLS)/PkgTool.Core pkg_build $< .
	test -s $@

clean:
	rm -rf $(INTDIR) sce_sys/param.sfo eboot.bin pkg.gp4 $(CONTENT_ID).pkg
