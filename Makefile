# I really want to rewrite this with some configure script written in a real
# language, that supports cross compilation properly, etc.  In fact, making a
# good generic configure framework is on my todo list; but since that's a lot
# of work, have fun with this hacky Makefile.
CC := clang
CXX := clang++
ARCH := -arch x86_64
XCFLAGS := -g -O3 -Wall -Wextra -Werror -Ilib $(ARCH)
LIB_LDFLAGS := -lobjc -dynamiclib -fvisibility=hidden -install_name /usr/lib/libsubstitute.0.dylib -dead_strip
IOS_APP_LDFLAGS := -framework UIKit -framework Foundation -dead_strip
IS_IOS := $(findstring -arch arm,$(CC) $(CFLAGS) $(XCFLAGS))
ifneq (,$(IS_IOS))
# I don't know anything in particular that would break this on older versions,
# but I don't have any good way to test it and don't really care.  So ensure it
# doesn't get run on them.
XCFLAGS := $(XCFLAGS) -miphoneos-version-min=7.0
endif
override CC := $(CC) $(XCFLAGS) $(CFLAGS)
override CXX := $(CXX) $(XCFLAGS) $(CFLAGS) -fno-exceptions -fno-asynchronous-unwind-tables

# These are only required to rebuild the generated disassemblers.
IMAON2 := /Users/comex/c/imaon2
GEN_JS := node --harmony --harmony_arrow_functions $(IMAON2)/tables/gen.js

all: \
	out/libsubstitute.dylib

$(shell mkdir -p out generated)

GENERATED_DIS_HEADERS := generated/generic-dis-arm.inc.h generated/generic-dis-thumb.inc.h generated/generic-dis-thumb2.inc.h generated/generic-dis-arm64.inc.h
define do_prefix
generated/generic-dis-$(1).inc.h:
	$(GEN_JS) --gen-hook-disassembler $(2) --dis-pattern='P(XXX)' $(IMAON2)/out/out-$(3).json > $$@ || rm -f $$@
generateds: generated/generic-dis-$(1).inc.h
endef
$(eval $(call do_prefix,thumb2,-n _thumb2,ARM))
$(eval $(call do_prefix,thumb,-n _thumb,ARM))
$(eval $(call do_prefix,arm,-n _arm,ARM))
$(eval $(call do_prefix,arm64,,AArch64))

HEADERS := lib/*.h lib/*/*.h generated/manual-mach.inc.h

out/%.o: lib/%.c Makefile $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) -fvisibility=hidden -std=c11 -c -o $@ $<
out/%.o: generated/%.S Makefile $(HEADERS)
	$(CC) -fvisibility=hidden -c -o $@ $<
out/%.o: lib/%.S Makefile $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) -fvisibility=hidden -c -o $@ $<
out/jump-dis.o: $(GENERATED_DIS_HEADERS)
out/transform-dis.o: $(GENERATED_DIS_HEADERS)

# Note: the order of darwin-inject-asm.o is significant.  Per man page, ld is
# guaranteed to link objects in order, which is necessary because
# darwin-inject-asm.S does not itself ensure there is at least 0x4000 bytes of
# executable stuff after inject_page_start (so that arm can remap into arm64).
# By putting it at the beginning, we can just reuse the space for the rest of
# the library rather than having to pad with zeroes.
# (This only matters on 32-bit ARM, and the text segment is currently 0xa000
# bytes there, more than enough.)

LIB_OBJS := \
	out/darwin-inject-asm.o \
	out/darwin/find-syms.o \
	out/darwin/inject.o \
	out/darwin/interpose.o \
	out/darwin/objc-asm.o \
	out/darwin/objc.o \
	out/darwin/read.o \
	out/darwin/substrate-compat.o \
	out/darwin/execmem.o \
	out/cbit/vec.o \
	out/jump-dis.o \
	out/transform-dis.o \
	out/hook-functions.o \
	out/strerror.o

out/libsubstitute.dylib: $(LIB_OBJS)
	$(CC) -o $@ $(LIB_OBJS) $(LIB_LDFLAGS)
	dsymutil $@

# The result of this is also checked into generated, just in case someone is
# trying to build with some Linux compiler that doesn't support all the
# architectures or something - meh.
# Did you know?  With -Oz + -marm, Apple clang-600.0.56 actually generated
# wrong code for the ARM version.  It works with -Os and with newer clang.
IACLANG := clang -Os -fno-stack-protector -dynamiclib -nostartfiles -nodefaultlibs -isysroot /dev/null -Ilib -fPIC
define define_iar
out/inject-asm-raw-$(1).o: lib/darwin/inject-asm-raw.c Makefile lib/darwin/manual-syscall.h lib/darwin/inject-asm-raw.order
	$(IACLANG) -arch $(2) -Wl,-order_file,lib/darwin/inject-asm-raw.order -o $$@ $$<
endef
$(eval $(call define_iar,x86_64,x86_64))
$(eval $(call define_iar,i386,i386))
$(eval $(call define_iar,arm,armv7 -marm))
$(eval $(call define_iar,arm64,arm64))

IAR_BINS := out/inject-asm-raw-x86_64.bin out/inject-asm-raw-i386.bin out/inject-asm-raw-arm.bin out/inject-asm-raw-arm64.bin
out/darwin-inject-asm.S: $(IAR_BINS) Makefile script/gen-inject-asm.sh
	./script/gen-inject-asm.sh > $@ || rm -f $@
generateds: out/darwin-inject-asm.S
out/darwin-inject-asm.S: out/darwin-inject-asm.S
	cp $< generated/

generateds: generated/manual-mach.inc.h
generated/manual-mach.inc.h: ./script/gen-manual-mach.sh
	./script/gen-manual-mach.sh

out/%.bin: out/%.o Makefile
	segedit -extract __TEXT __text $@ $<

define define_test
out/test-$(1): test/test-$(2).[cm]* $(HEADERS) $(GENERATED) Makefile out/libsubstitute.dylib
	$(3) -o $$@ $$< -Ilib -Isubstrate -Lout -dead_strip -lsubstitute
	install_name_tool -change /usr/lib/libsubstitute.0.dylib '@executable_path/libsubstitute.dylib' $$@
ifneq (,$(IS_IOS))
	ldid -Sent.plist $$@
endif
tests: out/test-$(1)
endef
$(eval $(call define_test,td-simple-arm,td-simple,$(CC) -std=c11 -DHDR='"arm/dis-arm.inc.h"' -Dxdis=dis_arm -DFORCE_TARGET_arm))
$(eval $(call define_test,td-simple-thumb,td-simple,$(CC) -std=c11 -DHDR='"arm/dis-thumb.inc.h"' -Dxdis=dis_thumb -DFORCE_TARGET_arm))
$(eval $(call define_test,td-simple-thumb2,td-simple,$(CC) -std=c11 -DHDR='"arm/dis-thumb2.inc.h"' -Dxdis=dis_thumb2 -DFORCE_TARGET_arm))
$(eval $(call define_test,td-simple-arm64,td-simple,$(CC) -std=c11 -DHDR='"arm64/dis-main.inc.h"' -Dxdis=dis -DFORCE_TARGET_arm64))
$(eval $(call define_test,td-simple-i386,td-simple,$(CC) -std=c11 -DHDR='"x86/dis-main.inc.h"' -Dxdis=dis -DFORCE_TARGET_i386))
$(eval $(call define_test,td-simple-x86-64,td-simple,$(CC) -std=c11 -DHDR='"x86/dis-main.inc.h"' -Dxdis=dis -DFORCE_TARGET_x86_64))
$(eval $(call define_test,dis-arm,dis,$(CC) -std=c11 -DFORCE_TARGET_arm))
$(eval $(call define_test,dis-arm64,dis,$(CC) -std=c11 -DFORCE_TARGET_arm64))
$(eval $(call define_test,jump-dis-arm,jump-dis,$(CC) -std=c11 -DFORCE_TARGET_arm -O0 out/cbit/vec.o))
$(eval $(call define_test,jump-dis-arm64,jump-dis,$(CC) -std=c11 -DFORCE_TARGET_arm64 -O0 out/cbit/vec.o))
$(eval $(call define_test,transform-dis-arm,transform-dis,$(CC) -std=c11 -DFORCE_TARGET_arm -O0))
$(eval $(call define_test,transform-dis-arm64,transform-dis,$(CC) -std=c11 -DFORCE_TARGET_arm64 -O0))
$(eval $(call define_test,transform-dis-i386,transform-dis,$(CC) -std=c11 -DFORCE_TARGET_i386 -O0))
$(eval $(call define_test,transform-dis-x86_64,transform-dis,$(CC) -std=c11 -DFORCE_TARGET_x86_64 -O0))
$(eval $(call define_test,find-syms,find-syms,$(CC) -std=c89))
$(eval $(call define_test,find-syms-cpp,find-syms,$(CXX) -x c++ -std=c++98))
$(eval $(call define_test,substrate,substrate,$(CXX) -std=c++98))
$(eval $(call define_test,imp-forwarding,imp-forwarding,$(CC) -std=c11 -framework Foundation -lobjc))
$(eval $(call define_test,objc-hook,objc-hook,$(CC) -std=c11 -framework Foundation -lsubstitute))
$(eval $(call define_test,interpose,interpose,$(CC) -std=c11 -lsubstitute))
$(eval $(call define_test,inject,inject,$(CC) -std=c11 -lsubstitute out/darwin/inject.o out/darwin/read.o))
$(eval $(call define_test,pc-patch,pc-patch,$(CC) -std=c11 out/darwin/execmem.o))
$(eval $(call define_test,execmem,execmem,$(CC) -std=c11 out/darwin/execmem.o -segprot __TEST rwx rx))
$(eval $(call define_test,hook-functions,hook-functions,$(CC) -std=c11 -lsubstitute -segprot __TEST rwx rx))
$(eval $(call define_test,posixspawn-hook,posixspawn-hook,$(CC) -std=c11))
$(eval $(call define_test,htab,htab,$(CC) -std=c11))
$(eval $(call define_test,vec,vec,$(CXX) -std=c++98 out/cbit/vec.o))

out/injected-test-dylib.dylib: test/injected-test-dylib.c Makefile
	$(CC) -std=c11 -dynamiclib -o $@ $<
tests: out/injected-test-dylib.dylib

# These are just random sequences of instructions which you can compile to .bin
# for testing.

out/insns-arm.o: test/insns-arm.S Makefile
	clang -arch armv7 -c -o $@ $<
out/insns-thumb2.o: test/insns-arm.S Makefile
	clang -arch armv7 -DTHUMB2 -c -o $@ $<

out/insns-libz-arm.o: test/insns-libz-arm.S Makefile
	clang -arch armv7 -c -o $@ $<
out/insns-libz-thumb2.o: test/insns-libz-arm.S Makefile
	clang -arch armv7 -c -o $@ $< -DTHUMB2

define transform-dis-cases
out/transform-dis-cases-$(arch).o: test/transform-dis-cases-$(arch).S Makefile
	clang -arch $(arch) -c -o $$@ $$<
endef
$(foreach arch,i386 x86_64 armv7 arm64,$(eval $(transform-dis-cases)))

# iOS bootstrap...
ifneq (,$(IS_IOS))
SD_OBJS := out/safety-dance/main.o out/safety-dance/AutoGrid.o
out/safety-dance/%.o: darwin-bootstrap/safety-dance/%.m darwin-bootstrap/safety-dance/*.h Makefile
	@mkdir -p $(dir $@)
	$(CC) -c -o $@ $< -fobjc-arc -Wno-unused-parameter
out/safety-dance/SafetyDance.app/SafetyDance: $(SD_OBJS) Makefile
	@mkdir -p $(dir $@)
	$(CC) -o $@ $(SD_OBJS) $(IOS_APP_LDFLAGS)
	ldid -S $@
out/safety-dance/SafetyDance.app/Info.plist: darwin-bootstrap/safety-dance/Info.plist Makefile
	@mkdir -p $(dir $@)
	plutil -convert binary1 -o $@ $<
	cp darwin-bootstrap/safety-dance/white.png out/safety-dance/SafetyDance.app/Default.png
	cp darwin-bootstrap/safety-dance/white.png out/safety-dance/SafetyDance.app/Default@2x.png
safety-dance: out/safety-dance/SafetyDance.app/SafetyDance out/safety-dance/SafetyDance.app/Info.plist
all: safety-dance

out/posixspawn-hook.dylib: darwin-bootstrap/posixspawn-hook.c out/libsubstitute.dylib
	$(CC) -dynamiclib -o $@ $< -Lout -lsubstitute
out/bundle-loader.dylib: darwin-bootstrap/bundle-loader.c darwin-bootstrap/substituted-messages.h out/libsubstitute.dylib
	$(CC) -dynamiclib -o $@ $< -Lout
out/unrestrict: darwin-bootstrap/unrestrict.c darwin-bootstrap/ib-log.h out/libsubstitute.dylib
	$(CC) -o $@ $< -Lout -lsubstitute
	ldid -Sent.plist $@
out/inject-into-launchd: darwin-bootstrap/inject-into-launchd.c darwin-bootstrap/ib-log.h out/libsubstitute.dylib
	$(CC) -o $@ $< -Lout -lsubstitute -framework IOKit -framework CoreFoundation
	ldid -Sent.plist $@
out/substituted: darwin-bootstrap/substituted*
	$(CC) -o $@ darwin-bootstrap/substituted{.c,-plist-loader.m} -framework Foundation -framework CoreFoundation -lbsm -fobjc-arc
all: out/posixspawn-hook.dylib out/bundle-loader.dylib out/unrestrict out/inject-into-launchd out/substituted
endif


clean:
	rm -rf out
