#include <iostream>
#include <cassert>
#include <sys/mman.h>
#include <unistd.h>

using namespace std;

class memory_allocator {
	static const size_t SIZE = 64;
	static void **memory_ptr;	

public:
    static void* get() {
        if (memory_ptr == nullptr) {
            memory_ptr = (void **)mmap(nullptr, 4096, PROT_EXEC | PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        }

        void* ptr = (void *)memory_ptr;
        memory_ptr = (void **) *memory_ptr;
        return ptr;
    }

    static void free(void *ptr) {
        *(void **)ptr = memory_ptr;
        memory_ptr = (char **)ptr;
    }
};

void **memory_allocator::memory_ptr = nullptr;
static memory_allocator own_allocator;

template <typename T>
struct trampoline;

template <typename R, typename... Args>
struct trampoline<R(Args...)> {
	template <typename F>
	trampoline(F const &f) : func(new F(f)), deleter(delete_func <F>)/*, caller(&call_func <F>) */{
		code = own_allocator.get();
		unsigned char *pcode((unsigned char *) code);
		
		//49BC    mov r12, func
		*(pcode++) = 0x49;
		*(pcode++) = 0xBC;
		*(void **) pcode = func;
		pcode += 8;

		//48B8    mov rax, &call_func
		*(pcode++) = 0x48;
		*(pcode++) = 0xB8;
		*(void **) pcode = (void *) &call_func<F>;
		pcode += 8;

		//FFE0    jmp rax
		*(pcode++) = 0xFF;
		*(pcode++) = 0xE0;

	}

	template <typename F>
	static void delete_func(void *obj) {
		delete ((F *) obj);
	}

	template <typename F>
	static R call_func(Args... args) {
		void *obj;

		__asm__ volatile (
			"movq %%r12, %0\n"
			:"=r"(obj)
			:"r"(obj)
			: "memory"
		);

		return (*(F *) obj)(args...);
	}

	~trampoline() {
		if (func != nullptr) {
			deleter(func);
		}
		own_allocator.free(code);	
	}

	R (*get() const )(Args... args) {
		return (R (*)(Args... args)) code;
	}

private:
	void *func;
	void *code;

	R (*caller)(void *obj, Args... args);

	void (*deleter)(void *);
};

int main() {
    std::cout << sizeof(trampoline<void()>) << std::endl;
    int d = 124;
    trampoline<int(int, int, int)> tr([&](int a, int b, int c) { return printf("%d %d %d %d\n", a, b, c, d); });
    auto p = tr.get();

    assert(10 == p(6, 5, 3));

    trampoline<double(char, short, int, long, long long, double, float)> t2(
            [&](char x0, short x1, int x2, long x3, long long x4, double x5, float x6) {
                return x0 + x1 + x2 + x3 + x4 + x5 + x6;
            });
    assert(1 + 2 + 3 + 4 + 5 + 2.5 + 3.5
           == t2.get()(1, 2, 3, 4, 5, 2.5, 3.5));
    trampoline<double(char, short, int, long, long long, long long, double, float, float, float, float, float, float)> t3(
            [&](char x0, short x1, int x2, long x3, long long x4, long long x5,
                double x6, float x7, float x8, float x9, float x10, float x11, float x12) {
                return x0 + x1 + x2 + x3 + x4 + x5 + x6 + x7 + x8 + x9 + x10 + x11 + x12;
            });

    assert(1 + 2 + 3 + 4 + 5 + 6 + 2.5 + 3.5 + 3.5 + 3.5 + 3.5 + 3.5 + 3.5
           == t3.get()(1, 2, 3, 4, 5, 6, 2.5, 3.5, 3.5, 3.5, 3.5, 3.5, 3.5));
}