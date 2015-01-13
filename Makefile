# todo
CC := clang
CXX := clang++
CFLAGS := -O3 -Wall -Wextra -Werror -arch x86_64
override CC := $(CC) $(CFLAGS)
override CXX := $(CXX) $(CFLAGS) -fno-exceptions -fno-asynchronous-unwind-tables

IMAON2 := /Users/comex/c/imaon2
GEN_JS := node $(IMAON2)/tables/gen.js

all: \
	out/libsubstitute.dylib

$(shell mkdir -p out)

HEADERS := lib/*.h \
	generated/transform-dis-arm.inc.h

define do_prefix
generated/transform-dis-$(1).inc.h: generated Makefile
	$(GEN_JS) --gen-hook-disassembler $(2) --dis-pattern='P(XXX)' $(IMAON2)/out/out-$(3).json > $$@ || rm -f $$@
all: generated/transform-dis-$(1).inc.h
#generated/jump-dis-$(1).inc.h: generated Makefile
#	$(GEN_JS) --gen-hook-jump-disassembler $(2) -p jump_dis_$(1)_ $(IMAON2)/out/out-$(3).json > $$@ || rm -f $$@
#all: generated/jump-dis-$(1).inc.h
HEADERS := $$(HEADERS) generated/transform-dis-$(1).inc.h
endef
$(eval $(call do_prefix,thumb2,-n _thumb2,ARM))
$(eval $(call do_prefix,thumb,-n _thumb,ARM))
$(eval $(call do_prefix,arm,-n _arm,ARM))
$(eval $(call do_prefix,arm64,,AArch64))

out/%.o: lib/%.c Makefile $(HEADERS)
	$(CC) -fvisibility=hidden -std=c11 -c -o $@ $<

LIB_OBJS := out/find-syms.o out/substrate-compat.o
out/libsubstitute.dylib: $(LIB_OBJS)
	$(CC) -dynamiclib -fvisibility=hidden -o $@ $(LIB_OBJS)

define define_test
out/test-$(1): test/test-$(2).c* $(HEADERS) Makefile out/libsubstitute.dylib
	$(3) -o $$@ $$< -Ilib -Isubstrate -Lout -lsubstitute
all: out/test-$(1)
endef
$(eval $(call define_test,tdarm-simple,td-simple,$(CC) -std=c11 -DHDR='"dis-arm.inc.h"'))
$(eval $(call define_test,tdthumb-simple,td-simple,$(CC) -std=c11 -DHDR='"dis-thumb.inc.h"'))
$(eval $(call define_test,tdthumb2-simple,td-simple,$(CC) -std=c11 -DHDR='"dis-thumb2.inc.h"'))
$(eval $(call define_test,tdarm64-simple,td-simple,$(CC) -std=c11 -DHDR='"dis-arm64.inc.h"'))
$(eval $(call define_test,dis,dis,$(CC) -std=c11))
$(eval $(call define_test,find-syms,find-syms,$(CC) -std=c89))
$(eval $(call define_test,find-syms-cpp,find-syms,$(CXX) -x c++ -std=c++98))
$(eval $(call define_test,substrate,substrate,$(CXX) -std=c++98))

out/arm-insns.o: test/arm-insns.S Makefile
	clang -arch armv7 -c -o $@ $<
out/thumb2-insns.o: test/arm-insns.S Makefile
	clang -arch armv7 -DTHUMB2 -c -o $@ $<
out/%-insns.bin: out/%-insns.o Makefile
	segedit -extract __TEXT __text $@ $<

generated: Makefile
	rm -rf generated
	mkdir generated

clean:
	rm -rf out
distclean:
	make clean
	rm -rf generated
