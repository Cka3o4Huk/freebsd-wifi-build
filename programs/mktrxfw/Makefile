RM?=	rm
LDFLAGS+=	-lz -lcrypto
PREFIX?=	/usr/local
MIPSCC=mips-portbld-freebsd10.2-gcc
GZIP=gzip -nc9
OBJCOPY=mips-freebsd-objcopy

all: mktrxfw loader.gz

install:
	install -m 0755 mktrxfw ${PREFIX}/bin

mktrxfw: 
	cc -g mktrxfw.c crc32.c -o mktrxfw

loader.elf: trxloader.o
	$(MIPSCC) -g -EL -nostdlib trxloader.o -Xlinker -T -Xlinker loader.lds -o loader.elf
	#clang37 --target=mips -fintegrated-as head.S -c -o head.o

trxloader.o: trxloader.c
	$(MIPSCC) -EL -O2 -g -nostdlib -I/usr/include -c trxloader.c -o trxloader.o
	
loader: loader.elf
	$(OBJCOPY) -S -O binary loader.elf loader

loader.gz: loader
	$(GZIP) loader > loader.gz.unaligned
	@(head -c $$((4-$$(stat -f "%z" loader.gz.unaligned)%4)) /dev/zero > loader.gz.zero)
	cat loader.gz.unaligned loader.gz.zero > loader.gz
	rm loader.gz.unaligned loader.gz.zero

clean:
	$(RM) -f loader.elf loader loader.gz* mktrxfw *.o
