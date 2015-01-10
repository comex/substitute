# todo
CC := clang
CXX := clang++
CFLAGS := -O3 -Wall -Wextra -Werror -arch x86_64
override CC := $(CC) $(CFLAGS)
override CXX := $(CXX) $(CFLAGS)

IMAON2 := /Users/comex/c/imaon2
GEN_JS := node $(IMAON2)/tables/gen.js

all: \
	generated/transform-dis-thumb2.inc \
	generated/transform-dis-thumb.inc \
	generated/transform-dis-arm.inc \
	generated/transform-dis-arm64.inc \
	out/libsubstitute.dylib \
	test/test-find-syms \
	test/test-find-syms-cpp \
	test/test-substrate

out:
	mkdir out

out/%.o: lib/%.c Makefile out
	$(CC) -fvisibility=hidden -std=c11 -c -o $@ $<

LIB_OBJS := out/find-syms.o out/substrate-compat.o
out/libsubstitute.dylib: $(LIB_OBJS) lib/*.h out
	$(CC) -dynamiclib -fvisibility=hidden -o $@ $(LIB_OBJS)

test/test-%: test/test-%.c Makefile out/libsubstitute.dylib
	$(CC) -std=c89 -o $@ $< -Ilib -Lout -lsubstitute
test/test-%-cpp: test/test-%.c Makefile out/libsubstitute.dylib
	$(CXX) -x c++ -std=c++98 -o $@ $< -Ilib -Lout -lsubstitute
test/test-%: test/test-%.cpp Makefile out/libsubstitute.dylib
	$(CXX) -std=c++11 -o $@ $< -Ilib -Isubstrate -Lout -lsubstitute

generated: Makefile
	rm -rf generated
	mkdir generated
generated/transform-dis-thumb2.inc: generated
	$(GEN_JS) --gen-hook-disassembler -n '_thumb2' -p transform_dis_thumb2 $(IMAON2)/out/out-ARM.json > $@ || rm -f $@
generated/transform-dis-thumb.inc: generated
	$(GEN_JS) --gen-hook-disassembler -n '_thumb' -p transform_dis_thumb $(IMAON2)/out/out-ARM.json > $@ || rm -f $@
generated/transform-dis-arm.inc: generated
	$(GEN_JS) --gen-hook-disassembler -n '_arm' -p transform_dis_arm $(IMAON2)/out/out-ARM.json > $@ || rm -f $@
generated/transform-dis-arm64.inc: generated
	$(GEN_JS) --gen-hook-disassembler -p transform_dis_arm64 $(IMAON2)/out/out-AArch64.json > $@ || rm -f $@
clean:
	rm -rf out
distclean:
	make clean
	rm -rf generated
