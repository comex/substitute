# todo
CC := clang
CXX := clang++
CFLAGS := -O3 -Wall -Wextra -Werror -arch x86_64
override CC := $(CC) $(CFLAGS)
override CXX := $(CXX) $(CFLAGS) -fno-exceptions -fno-asynchronous-unwind-tables

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
	$(CC) -fvisibility=hidden -std=c11 -c -o $@ $<
out/%.o: lib/%.S Makefile $(HEADERS)
	$(CC) -fvisibility=hidden -c -o $@ $<
out/jump-dis-arm-multi.o: generated/generic-dis-arm.inc.h generated/generic-dis-thumb.inc.h generated/generic-dis-thumb2.inc.h

LIB_OBJS := \
	out/find-syms.o \
	out/interpose.o \
	out/objc.o \
	out/objc-asm.o \
	out/substrate-compat.o \
	out/jump-dis-arm-multi.o
out/libsubstitute.dylib: $(LIB_OBJS)
	$(CC) -dynamiclib -fvisibility=hidden -o $@ $(LIB_OBJS) -lobjc

define define_test
out/test-$(1): test/test-$(2).[cm]* $(HEADERS) $(GENERATED) Makefile out/libsubstitute.dylib
	$(3) -o $$@ $$< -Ilib -Isubstrate -Lout -lsubstitute
all: out/test-$(1)
endef
$(eval $(call define_test,tdarm-simple,td-simple,$(CC) -std=c11 -DHDR='"dis-arm.inc.h"' -Dxdis=dis_arm))
$(eval $(call define_test,tdthumb-simple,td-simple,$(CC) -std=c11 -DHDR='"dis-thumb.inc.h"' -Dxdis=dis_thumb))
$(eval $(call define_test,tdthumb2-simple,td-simple,$(CC) -std=c11 -DHDR='"dis-thumb2.inc.h"' -Dxdis=dis_thumb2))
$(eval $(call define_test,tdarm64-simple,td-simple,$(CC) -std=c11 -DHDR='"dis-arm64.inc.h"' -Dxdis=dis))
$(eval $(call define_test,dis,dis,$(CC) -std=c11))
$(eval $(call define_test,find-syms,find-syms,$(CC) -std=c89))
$(eval $(call define_test,find-syms-cpp,find-syms,$(CXX) -x c++ -std=c++98))
$(eval $(call define_test,substrate,substrate,$(CXX) -std=c++98))
$(eval $(call define_test,jump-dis,jump-dis,$(CC) -std=c11))
$(eval $(call define_test,imp-forwarding,imp-forwarding,$(CC) -std=c11 -framework Foundation -lobjc))

out/insns-arm.o: test/insns-arm.S Makefile
	clang -arch armv7 -c -o $@ $<
out/insns-thumb2.o: test/insns-arm.S Makefile
	clang -arch armv7 -DTHUMB2 -c -o $@ $<

out/insns-libz-arm.o: test/insns-libz-arm.S Makefile
	clang -arch armv7 -c -o $@ $<
out/insns-libz-thumb2.o: test/insns-libz-arm.S Makefile
	clang -arch armv7 -c -o $@ $< -DTHUMB2

out/insns-%.bin: out/insns-%.o Makefile
	segedit -extract __TEXT __text $@ $<

clean:
	rm -rf out
distclean:
	make clean
	rm -rf generated
