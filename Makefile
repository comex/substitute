IMAON2 := /Users/comex/c/imaon2
GEN_JS := node $(IMAON2)/tables/gen.js
all: \
	generated/transform-dis-thumb2.inc \
	generated/transform-dis-thumb.inc \
	generated/transform-dis-arm.inc \
	generated/transform-dis-arm64.inc
generated:
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
