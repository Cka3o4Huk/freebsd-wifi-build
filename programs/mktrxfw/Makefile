RM?=		rm
LDFLAGS+=	-lz -lcrypto
PREFIX?=	/usr/local
TRXSRC=		main.c utils.c fw_trx.c crc32.c
CHKSRC=		main.c utils.c fw_chk.c crc32.c
CC=		cc
MIPSCC!=	(pkg info -l mips-gcc | grep '\-gcc$$')
MIPSOBJECTS=	trxloader.o tinfl.o mem.o
MIPSCC_CLANG=	cc -target mips
MIPSOBJ_CLANG=	objjj/trxloader.o objjj/tinfl.o objjj/mem.o
MIPSCFLAGS=	-EL -O1 -g -fno-pic -mno-abicalls -nostdlib -I/usr/include
GZIP=		gzip -nc9
LZMA=		/usr/local/bin/lzma e 
OBJCOPY=	mips-freebsd-objcopy

all: mktrxfw mkchkfw loader.gz loader.lzma

install:
	install -m 0755 mktrxfw ${PREFIX}/bin

mktrxfw: 
	$(CC) -g -O0 $(TRXSRC) -o mktrxfw

mkchkfw:
	$(CC) -g -O0 $(CHKSRC) -o mkchkfw

loader.elf: $(MIPSOBJECTS)
	$(MIPSCC) -g -EL -nostdlib $(MIPSOBJECTS) -Xlinker -T -Xlinker loader.lds -o loader.elf
	ld.lld $(MIPSOBJ_CLANG) -T loader.lds -o loader.elf.clang
	#clang37 --target=mips -fintegrated-as head.S -c -o head.o

$(MIPSOBJECTS): $(.PREFIX).c
	$(MIPSCC) $(MIPSCFLAGS) -c $(.PREFIX).c -o $(.PREFIX).o
	
loader: loader.elf
	$(OBJCOPY) -S -O binary loader.elf loader
	$(OBJCOPY) -S -O binary loader.elf.clang loader.clang	

$(MIPSOBJ_CLANG): $(.PREFIX).c
	$(MIPSCC) $(MIPSCFLAGS) -c $(.PREFIX).c -o $(.PREFIX).o
	mkdir -p objjj
	$(MIPSCC_CLANG) $(MIPSCFLAGS) -c $(.PREFIX).c -o objjj/$(.PREFIX).o

loader.gz: loader
	$(GZIP) loader > loader.gz.unaligned
	@(head -c $$((4-$$(stat -f "%z" loader.gz.unaligned)%4)) /dev/zero > loader.gz.zero)
	cat loader.gz.unaligned loader.gz.zero > loader.gz
	$(RM) loader.gz.unaligned loader.gz.zero

loader.lzma: loader
	$(LZMA) loader loader.lzma.unaligned
	@(head -c $$((4-$$(stat -f "%z" loader.lzma.unaligned)%4)) /dev/zero > loader.lzma.zero)
	cat loader.lzma.unaligned loader.lzma.zero > loader.lzma
	$(RM) loader.lzma.unaligned loader.lzma.zero

clean:
	$(RM) -f loader.elf loader loader.gz* mktrxfw mkchkfw *.o objjj/*
