#include <sys/types.h>

#define FLASHADDR 			0xbc000000
#define TARGETADDR			0x80800000 // Trampoline
#define FAIL				0x00000004 // CFE exception
#define FAIL2				0x00000008 // CFE exception
#define FAIL3				0x00000010 // CFE exception
#define MAGIC				0x30524448 // "HDR0"
#define NUM_OFFSETS			3

struct trx_header {
	u_int32_t magic;
	u_int32_t file_length;
	u_int32_t crc32;
	u_int16_t flags;
	u_int16_t version;
	u_int32_t offsets[NUM_OFFSETS];
};

void _startC(register_t a0, register_t a1, register_t a2, register_t a3){
	__asm__ __volatile__(
			/* Make sure KSEG0 is uncached */
			"li		$t0, 0x2\n"
			"mtc0	$t0, $16\n"
			"ehb");

	uint32_t* ptr = (unsigned int*)FLASHADDR;
	void (*entry_point)(register_t, register_t, register_t, register_t) = (void*)FAIL;

	for(int i = 0; i < 0x200; i++) {//32MB
		if(*ptr != MAGIC){
			ptr += 0x1000; // 0xbc800000 0x1000 / sizeof(uint32_t); //+4 - 0x40000
			continue;
		}
		//found TRX - copy as-is to 0x80001000
		struct trx_header* h = (struct trx_header*)ptr;
		uint32_t* kstart = (uint32_t*)(((char*)ptr) + h->offsets[1]);
		int size = h->offsets[2] - h->offsets[1];
		uint32_t* dst = (uint32_t*)TARGETADDR;

		entry_point = (void*)TARGETADDR;
		while(size > 0){
			*dst = *kstart;
			size -= 4;
			kstart++;
			dst++;
		}
		entry_point(a0,a1,a2,a3);
	}
	entry_point(a0,a1,a2,a3);
}

