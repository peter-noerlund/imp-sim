all: test.bin font_test.bin doom.bin

%.elf: %.c
	riscv64-unknown-elf-gcc -march=rv32ezicsr -mabi=ilp32e -mstrict-align -mcmodel=medany -mtune=size -mno-relax -Wl,-no-relax -fno-builtin -O3 -Wall -o $@ --specs=/usr/lib/picolibc/riscv64-unknown-elf/picolibc.specs -Wl,-e_init -Ttest.ld -DNDEBUG $^ init.S

%.bin: %.elf
	riscv64-unknown-elf-objcopy -O binary $^ $@

clean:
	rm -f test.elf test.bin

.PHONY: all clean
