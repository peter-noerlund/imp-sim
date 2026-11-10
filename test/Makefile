all: test.bin

test.elf: test.c init.S
	riscv64-unknown-elf-gcc -march=rv32e -mabi=ilp32e -mstrict-align -mcmodel=medany -mtune=size -mno-relax -Wl,-no-relax -fno-builtin -O3 -Wall -o $@ --specs=/usr/lib/picolibc/riscv64-unknown-elf/picolibc.specs -Wl,-e_init -Ttest.ld -DNDEBUG $^

test.bin: test.elf
	riscv64-unknown-elf-objcopy -O binary $^ $@

clean:
	rm -f test.elf test.bin

.PHONY: all clean
