TITLE       := Tim PS4 Download Manager
VERSION     := 0.1.0
TITLE_ID    := TIMA00001
CONTENT_ID  := IV0000-TIMA00001_00-TIMCLIENT0000001

LIBS        := -lc -lkernel -lc++ -lSceVideoOut -lScePad -lSceUserService -lSceNet -lSceNetCtl -lSceSsl -lSceHttp -lSceImeDialog -lSceCommonDialog
TOOLCHAIN   := $(OO_PS4_TOOLCHAIN)
PROJDIR     := tim
COMMONDIR   := platform
INTDIR      := build
CDIR        := linux
CC          := clang
CCX         := clang++
LD          := ld.lld

CFILES      := ../src/torrent_queue.c ../src/torrent_engine.c ../src/http_url.c ../src/http_download.c
CPPFILES    := $(wildcard *.cpp)
COMMONFILES := $(wildcard $(COMMONDIR)/*.cpp)
OBJS        := $(patsubst ../src/%.c,$(INTDIR)/%.o,$(CFILES)) $(patsubst %.cpp,$(INTDIR)/%.o,$(CPPFILES)) $(patsubst $(COMMONDIR)/%.cpp,$(INTDIR)/%.o,$(COMMONFILES))
CFLAGS      := --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -c -isysroot $(TOOLCHAIN) -isystem $(TOOLCHAIN)/include
CXXFLAGS    := $(CFLAGS) -isystem $(TOOLCHAIN)/include/c++/v1
LDFLAGS     := -m elf_x86_64 -pie --script $(TOOLCHAIN)/link.x --eh-frame-hdr -L$(TOOLCHAIN)/lib $(LIBS) $(TOOLCHAIN)/lib/crt1.o

.PHONY: all clean
all: $(CONTENT_ID).pkg

$(INTDIR):
	mkdir -p $@

$(INTDIR)/%.o: ../src/%.c | $(INTDIR)
	$(CC) $(CFLAGS) -I../src -o $@ $<

$(INTDIR)/%.o: %.cpp | $(INTDIR)
	$(CCX) $(CXXFLAGS) -I$(COMMONDIR) -o $@ $<

$(INTDIR)/%.o: $(COMMONDIR)/%.cpp | $(INTDIR)
	$(CCX) $(CXXFLAGS) -I$(COMMONDIR) -o $@ $<

eboot.bin: $(OBJS)
	$(LD) $(OBJS) -o $(INTDIR)/$(PROJDIR).elf $(LDFLAGS)
	./tools/create-fself -in=$(INTDIR)/$(PROJDIR).elf -out=$(INTDIR)/$(PROJDIR).oelf --eboot "eboot.bin" --paid 0x3800000000000011
	test -s $(INTDIR)/$(PROJDIR).oelf
	test -s eboot.bin

sce_sys/icon0.png: scripts/make_icon.py
	mkdir -p sce_sys
	python3 scripts/make_icon.py $@

# OpenOrbis homebrew packages should include the system rights screen resource.
# Resolve it from the installed SDK rather than shipping an empty placeholder.
sce_sys/about/right.sprx:
	mkdir -p sce_sys/about
	RIGHT_SPRX="$$(find "$(TOOLCHAIN)" -type f -path '*/sce_sys/about/right.sprx' -print -quit)"; \
	if [ -z "$$RIGHT_SPRX" ]; then \
	  RIGHT_SPRX="$$(find "$(TOOLCHAIN)" -type f -name right.sprx -print -quit)"; \
	fi; \
	test -n "$$RIGHT_SPRX" || { echo "OpenOrbis right.sprx not found in toolchain"; exit 1; }; \
	cp "$$RIGHT_SPRX" $@

# These runtime modules are provided by OpenOrbis and are needed by some
# toolchain/runtime combinations. Fail the build instead of silently omitting them.
sce_module/libc.prx:
	mkdir -p sce_module
	MODULE="$$(find "$(TOOLCHAIN)" -type f -path '*/modules/libc.prx' -print -quit)"; \
	test -n "$$MODULE" || { echo "OpenOrbis libc.prx not found in toolchain"; exit 1; }; \
	cp "$$MODULE" $@

sce_module/libSceFios2.prx:
	mkdir -p sce_module
	MODULE="$$(find "$(TOOLCHAIN)" -type f -path '*/modules/libSceFios2.prx' -print -quit)"; \
	test -n "$$MODULE" || { echo "OpenOrbis libSceFios2.prx not found in toolchain"; exit 1; }; \
	cp "$$MODULE" $@

sce_sys/param.sfo: Makefile
	mkdir -p sce_sys
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_new $@
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ APP_TYPE --type Integer --maxsize 4 --value 1
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ APP_VER --type Utf8 --maxsize 8 --value '$(VERSION)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ ATTRIBUTE --type Integer --maxsize 4 --value 0
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ CATEGORY --type Utf8 --maxsize 4 --value 'gd'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ CONTENT_ID --type Utf8 --maxsize 48 --value '$(CONTENT_ID)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ SYSTEM_VER --type Integer --maxsize 4 --value 0
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ TITLE --type Utf8 --maxsize 128 --value '$(TITLE)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ TITLE_ID --type Utf8 --maxsize 12 --value '$(TITLE_ID)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ VERSION --type Utf8 --maxsize 8 --value '$(VERSION)'

pkg.gp4: eboot.bin sce_sys/param.sfo sce_sys/icon0.png sce_sys/about/right.sprx sce_module/libc.prx sce_module/libSceFios2.prx
	@set -eu; \
	for f in $^; do test -s "$$f" || { echo "Missing or empty package file: $$f"; exit 1; }; done
	./tools/create-gp4 -out $@ --content-id=$(CONTENT_ID) --files "$^"

$(CONTENT_ID).pkg: pkg.gp4
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core pkg_build $< .

# Keep sce_sys/icon0.png: CI creates it on the host before running make inside the
# OpenOrbis container, where python3 may not be installed.
clean:
	rm -rf $(INTDIR) sce_sys/param.sfo sce_sys/about/right.sprx sce_module eboot.bin pkg.gp4 $(CONTENT_ID).pkg
