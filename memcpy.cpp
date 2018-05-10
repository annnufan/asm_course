#include <iostream>
#include <emmintrin.h>

void memcpy_asm_1(void* to, void const* from, size_t size) {
	for (size_t i = 0; i < size; i++) {
		*((char *) to + i) = *((char *) from + i);
	}
}

void memcpy_asm_8(void* to, void const* from, size_t size) {
	size_t tmp;
	for (size_t i = 0; i < size; i++) {
		__asm__ volatile(
			"mov (%1), %0\n"
			"mov %0, (%2)"
			: "=r"(tmp)
			: "r"((const char *) from + i), "r"((char *) to + i)
			: "memory"
		);
	}
}

void memcpy_asm_16(void* to, void const* from, size_t size) {
	__m128i tmp;
	for (size_t i = 0; i < size; i += 16) {
		__asm__ volatile(
			"movdqu  (%1), %0\n"
		    "movdqu %0, (%2)\n"
			: "=x"(tmp)
			: "r"((char *) from + i), "r"((char *) to + i)
			: "memory"
		);
	}
}

void memcpy_asm(void* to, void const* from, size_t size) {
	size_t offset = (size_t) to % 16;
	if (offset != 0) {
		offset = 16 - offset;
		memcpy_asm_1(to, from, offset);
		size -= offset;
	}

	void* to_off = (char *) to + offset;
	void* from_off = (char *) from + offset;
	size_t tail = size % 16;
	size -= tail;

	if (size >= 16) {
		__m128i tmp;
		__asm__ volatile(
		"1:"
			"movdqu  (%0), %3\n"
			"movntdq %3, (%1)\n"
			"add     $16, %0\n"
			"add     $16, %1\n"
			"sub     $16, %2\n"
			"jnz     1b\n"
			"sfence\n"
			: "=r"(to_off), "=r"(from_off), "=r"(size), "=x"(tmp)
			: "0"(to_off), "1"(from_off), "2"(size)
			: "memory", "cc"
		);
	}

	memcpy_asm_1(to_off, from_off, tail);
} 

const int N = 1 << 30;

int main() {
    char *src = new char[N];
    char *dst = new char[N];
    memcpy_asm(dst + 3, src + 3, N - 10);
    return 0;
}