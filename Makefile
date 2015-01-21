# todo
CC := clang
CXX := clang++
ARCH := -arch x86_64
XCFLAGS := -O3 -Wall -Wextra -Werror -Ilib $(ARCH)
override CC := $(CC) $(XCFLAGS) $(CFLAGS)
override CXX := $(CXX) $(XCFLAGS) $(CFLAGS) -fno-exceptions -fno-asynchronous-unwind-tables
LIB_LDFLAGS := -lobjc -dynamiclib -fvisibility=hidden
IMAON2 := /Users/comex/c/imaon2
GEN_JS := node --harmony --harmony_arrow_functions $(IMAON2)/tables/gen.js

all: \
	generateds \
	out/libsubstitute.dylib

$(shell mkdir -p out generated)

HEADERS := lib/*.h
GENERATED := generated/generic-dis-arm.inc.h generated/generic-dis-thumb.inc.h generated/generic-dis-thumb2.inc.h generated/generic-dis-arm64.inc.h
define do_prefix
generated/generic-dis-$(1).inc.h:
	$(GEN_JS) --gen-hook-disassembler $(2) --dis-pattern='P(XXX)' $(IMAON2)/out/out-$(3).json > $$@ || rm -f $$@
generateds: generated/generic-dis-$(1).inc.h
endef
$(eval $(call do_prefix,thumb2,-n _thumb2,ARM))
$(eval $(call do_prefix,thumb,-n _thumb,ARM))
$(eval $(call do_prefix,arm,-n _arm,ARM))
$(eval $(call do_prefix,arm64,,AArch64))

out/%.o: lib/%.c Makefile $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) -fvisibility=hidden -std=c11 -c -o $@ $<
out/%.o: lib/%.S Makefile $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) -fvisibility=hidden -c -o $@ $<
out/jump-dis.o: $(GENERATED)
out/transform-dis.o: $(GENERATED)

LIB_OBJS := \
	out/darwin/find-syms.o \
	out/darwin/inject.o \
	out/darwin/inject-asm.o \
	out/darwin/interpose.o \
	out/darwin/objc-asm.o \
	out/darwin/objc.o \
	out/darwin/read.o \
	out/darwin/substrate-compat.o \
	out/jump-dis.o \
	out/transform-dis.o

out/libsubstitute.dylib: $(LIB_OBJS)
	$(CC) -o $@ $(LIB_OBJS) $(LIB_LDFLAGS)

# this doesn't need to be done on the building machine, just in case someone is
# trying to build with some Linux compiler that doesn't support all the
# architectures or something - meh
ASCLANG := clang -dynamiclib -nostartfiles -nodefaultlibs
out/inject-asm-raw-x86_64.o: lib/darwin/inject-asm-raw.S Makefile
	$(ASCLANG) -arch x86_64 -o $@ $<
out/inject-asm-raw-i386.o: lib/darwin/inject-asm-raw.S Makefile
	$(ASCLANG) -arch i386 -o $@ $<
out/inject-asm-raw-arm.o: lib/darwin/inject-asm-raw.S Makefile
	$(ASCLANG) -arch armv7 -o $@ $<
out/inject-asm-raw-arm64.o: lib/darwin/inject-asm-raw.S Makefile
	$(ASCLANG) -arch arm64 -o $@ $<
IAR_BINS := out/inject-asm-raw-x86_64.bin out/inject-asm-raw-i386.bin out/inject-asm-raw-arm.bin out/inject-asm-raw-arm64.bin
out/inject-asm.S: $(IAR_BINS) Makefile
	for i in x86_64 i386 arm arm64; do \
		echo ".globl inject_start_$$i"; \
		echo "inject_start_$$i:"; \
		echo ".align 2"; \
		printf  ".byte "; \
		xxd -i < out/inject-asm-raw-$$i.bin | xargs echo; \
	done > $@

define define_test
out/test-$(1): test/test-$(2).[cm]* $(HEADERS) $(GENERATED) Makefile out/libsubstitute.dylib
	$(3) -g -o $$@ $$< -Ilib -Isubstrate -Lout -lsubstitute
all: out/test-$(1)
endef
$(eval $(call define_test,tdarm-simple,td-simple,$(CC) -std=c11 -DHDR='"arm/dis-arm.inc.h"' -Dxdis=dis_arm -DFORCE_TARGET_arm))
$(eval $(call define_test,tdthumb-simple,td-simple,$(CC) -std=c11 -DHDR='"arm/dis-thumb.inc.h"' -Dxdis=dis_thumb -DFORCE_TARGET_arm))
$(eval $(call define_test,tdthumb2-simple,td-simple,$(CC) -std=c11 -DHDR='"arm/dis-thumb2.inc.h"' -Dxdis=dis_thumb2 -DFORCE_TARGET_arm))
$(eval $(call define_test,tdarm64-simple,td-simple,$(CC) -std=c11 -DHDR='"arm64/dis-arm64.inc.h"' -Dxdis=dis -DFORCE_TARGET_arm64))
$(eval $(call define_test,dis-arm,dis,$(CC) -std=c11 -DFORCE_TARGET_arm))
$(eval $(call define_test,dis-arm64,dis,$(CC) -std=c11 -DFORCE_TARGET_arm64))
$(eval $(call define_test,jump-dis-arm,jump-dis,$(CC) -std=c11 -DFORCE_TARGET_arm -O0))
$(eval $(call define_test,jump-dis-arm64,jump-dis,$(CC) -std=c11 -DFORCE_TARGET_arm64 -O0))
$(eval $(call define_test,transform-dis-arm,transform-dis,$(CC) -std=c11 -DFORCE_TARGET_arm -O0))
$(eval $(call define_test,transform-dis-arm64,transform-dis,$(CC) -std=c11 -DFORCE_TARGET_arm64 -O0))
$(eval $(call define_test,find-syms,find-syms,$(CC) -std=c89))
$(eval $(call define_test,find-syms-cpp,find-syms,$(CXX) -x c++ -std=c++98))
$(eval $(call define_test,substrate,substrate,$(CXX) -std=c++98))
$(eval $(call define_test,imp-forwarding,imp-forwarding,$(CC) -std=c11 -framework Foundation -lobjc))
$(eval $(call define_test,objc-hook,objc-hook,$(CC) -std=c11 -framework Foundation -lsubstitute))
$(eval $(call define_test,interpose,interpose,$(CC) -std=c11 -lsubstitute))
$(eval $(call define_test,inject,inject,$(CC) -std=c11 -lsubstitute))

out/insns-arm.o: test/insns-arm.S Makefile
	clang -arch armv7 -c -o $@ $<
out/insns-thumb2.o: test/insns-arm.S Makefile
	clang -arch armv7 -DTHUMB2 -c -o $@ $<

out/insns-libz-arm.o: test/insns-libz-arm.S Makefile
	clang -arch armv7 -c -o $@ $<
out/insns-libz-thumb2.o: test/insns-libz-arm.S Makefile
	clang -arch armv7 -c -o $@ $< -DTHUMB2

out/%.bin: out/%.o Makefile
	segedit -extract __TEXT __text $@ $<

clean:
	rm -rf out
distclean:
	make clean
	rm -rf generated
