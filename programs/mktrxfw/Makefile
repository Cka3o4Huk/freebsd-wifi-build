RM?=	rm
LDFLAGS+=	-lz -lcrypto
PREFIX?=	/usr/local

all:	mktrxfw

install:
	install -m 0755 mktrxfw ${PREFIX}/bin

mktrxfw: 
	cc -g mktrxfw.c crc32.c -o mktrxfw

clean:
	$(RM) -f mktrxfw *.o
