# todo
CC := clang
CXX := clang++
CFLAGS := -O3 -Wall -Wextra -Werror -arch x86_64
override CC := $(CC) $(CFLAGS)
override CXX := $(CXX) $(CFLAGS)

IMAON2 := /Users/comex/c/imaon2
GEN_JS := node $(IMAON2)/tables/gen.js

all: \
	out/libsubstitute.dylib \
	out/test-find-syms \
	out/test-find-syms-cpp \
	out/test-substrate \
	out/test-dis \
	out/test-tdarm-simple

out:
	mkdir out

out/%.o: lib/%.c Makefile out
	$(CC) -fvisibility=hidden -std=c11 -c -o $@ $<

LIB_OBJS := out/find-syms.o out/substrate-compat.o
HEADERS := lib/*.h generated/*.h
out/libsubstitute.dylib: $(LIB_OBJS) lib/*.h out
	$(CC) -dynamiclib -fvisibility=hidden -o $@ $(LIB_OBJS)

out/test-tdarm-simple: test/test-tdarm-simple.c $(HEADERS) Makefile
	$(CC) -std=c11 -o $@ $< -Ilib
out/test-dis: test/test-dis.c $(HEADERS) Makefile
	$(CC) -std=c11 -o $@ $< -Ilib
out/test-%: test/test-%.c Makefile $(HEADERS) out/libsubstitute.dylib
	$(CC) -std=c89 -o $@ $< -Ilib -Lout -lsubstitute
out/test-%-cpp: test/test-%.c Makefile $(HEADERS) out/libsubstitute.dylib
	$(CXX) -x c++ -std=c++98 -o $@ $< -Ilib -Lout -lsubstitute
out/test-%: test/test-%.cpp Makefile $(HEADERS) out/libsubstitute.dylib
	$(CXX) -std=c++11 -o $@ $< -Ilib -Isubstrate -Lout -lsubstitute

generated: Makefile
	rm -rf generated
	mkdir generated

define do_prefix
generated/transform-dis-$(1).inc.h: generated Makefile
	$(GEN_JS) --gen-hook-disassembler $(2) --dis-pattern='P(XXX)' $(IMAON2)/out/out-$(3).json > $$@ || rm -f $$@
all: generated/transform-dis-$(1).inc.h
#generated/jump-dis-$(1).inc.h: generated Makefile
#	$(GEN_JS) --gen-hook-jump-disassembler $(2) -p jump_dis_$(1)_ $(IMAON2)/out/out-$(3).json > $$@ || rm -f $$@
#all: generated/jump-dis-$(1).inc.h
endef
$(eval $(call do_prefix,thumb2,-n _thumb2,ARM))
$(eval $(call do_prefix,thumb,-n _thumb,ARM))
$(eval $(call do_prefix,arm,-n _arm,ARM))
$(eval $(call do_prefix,arm64,,AArch64))

clean:
	rm -rf out
distclean:
	make clean
	rm -rf generated
