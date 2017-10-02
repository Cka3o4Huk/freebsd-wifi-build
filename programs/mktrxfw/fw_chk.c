/*-
 * Copyright (c) 2016 Michael Zhilin <mizhka@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include <sys/endian.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "extern.h"
#include "utils.h"

/*
 * Header (58 bytes):
 *
 * $ hexdump -C  -n 62 WNR3500L-V1.2.2.48_35.0.55.chk
 * 00000000  2a 23 24 5e 00 00 00 3a  01 01 02 02 30 23 00 37  |*#$^...:....0#.7|
 * 00000010  38 35 b2 24 00 00 00 00  00 51 c0 00 00 00 00 00  |85.$.....Q......|
 * 00000020  38 35 b2 24 f1 99 09 f5  55 31 32 48 31 33 36 54  |85.$....U12H136T|
 * 00000030  39 39 5f 4e 45 54 47 45  41 52 48 44 52 30        |99_NETGEARHDR0|
 *
 * 2a 23 24 5e = magic
 * 00 00 00 3a = header size
 * 01 01 02 02 = area id / version (row 1.2.2)
 * 30 23 00 37 =
 * 38 35 b2 24 = check sum of first partition (cksum -o 2)
 * 00 00 00 00 = padding / check sum of second partition ?
 * 00 51 c0 00 = TRX size (8Kb aligned by itself)
 * 00 00 00 00 = padding / size of second partition ?
 * 38 35 b2 24 = whole checksum (same as 1st partition)
 * f1 99 09 f5 = header checksum?
 * 55 31 32 48 31 33 36 54 39 39 5f 4e 45 54 47 45 41 52 = model name (U12H136T99_NETGEAR)
 */

#define MAGIC				0x2a23245e
#define	HEADER_SIZE			0x0000003a
#define VERSION				0x01010202

struct chk_header {
	uint32_t	magic;
	uint32_t	headersize; 		/* may be something more? */
	uint32_t	version;		/* static as of now */
	uint32_t	PAD0;
	uint32_t	cksum_part1;
	uint32_t	cksum_part2;
	uint32_t	size_part1;
	uint32_t	size_part2;
	uint32_t	cksum_total;
	uint32_t	cksum_header;
	char		model_name[18];
	char		model_name_eos;
};

static int	fw_chk_crc(struct fw_ctx* ctx);
static int	fw_chk_get_headersize(void);
static int	fw_chk_init(struct fw_ctx* ctx);
static int	fw_chk_print(int fd);
static int	fw_chk_write(void* header, int fd, int mode);
/* extra */
static int	fw_chk_precrc(struct fw_ctx* ctx);

struct fw_methods fw = {
	.get_headersize	= fw_chk_get_headersize,
	.print_header	= fw_chk_print,
	.calculate_crc	= fw_chk_crc,
	.init_header	= fw_chk_init,
	.write_header 	= fw_chk_write
};

char * fw_create_args = "trxfile";

static int
fw_chk_print(int fd)
{
	int			i;
	struct chk_header	header;

	READ_HEADER_BE(header.magic);
	READ_HEADER_BE(header.headersize);
	READ_HEADER_BE(header.version);
	READ_HEADER_BE(header.PAD0);
	READ_HEADER_BE(header.cksum_part1);
	READ_HEADER_BE(header.cksum_part2);
	READ_HEADER_BE(header.size_part1);
	READ_HEADER_BE(header.size_part2);
	READ_HEADER_BE(header.cksum_total);
	READ_HEADER_BE(header.cksum_header);
	READ_HEADER(header.model_name);
	header.model_name_eos = '\0';

	printf("magic\t\t= 0x%08x\n", header.magic);
	printf("headersize\t= 0x%08x\n", header.headersize);
	printf("version\t\t= 0x%08x\n", header.version);
	printf("unknown\t\t= 0x%08x\n", header.PAD0);
	printf("cksum_part1\t= 0x%08x\n", header.cksum_part1);
	printf("cksum_part2\t= 0x%08x\n", header.cksum_part2);
	printf("size_part1\t= 0x%08x\n", header.size_part1);
	printf("size_part2\t= 0x%08x\n", header.size_part2);
	printf("cksum_total\t= 0x%08x\n", header.cksum_total);
	printf("cksum_header\t= 0x%08x\n", header.cksum_header);
	printf("model_name\t= %s\n", header.model_name);

	return 0;
}

static int
fw_chk_get_headersize(void)
{
	return HEADER_SIZE;
}

static int
fw_chk_init(struct fw_ctx* ctx)
{
	struct fw_fileentry	*file;
	struct chk_header	*header;
	time_t			 cts;

	if (ctx->nfiles != 1) {
		perror("Number of input files should be 1");
		return -1;
	}
	header = ctx->header;
	header->magic = MAGIC;
	header->headersize = HEADER_SIZE;
	header->version = VERSION;
	header->PAD0 = 0; /* 0x30230037 */

	file = TAILQ_FIRST(&ctx->files);
	header->size_part1 = 0;
	header->size_part2 = 0;
	header->cksum_part1 = 0;
	header->cksum_part2 = 0;
	header->cksum_total = 0;
	header->cksum_header = 0;

	fw_chk_precrc(ctx);
	header->cksum_total = header->cksum_part1;
	strncpy(header->model_name, "U12H136T99_NETGEAR", strlen("U12H136T99_NETGEAR"));
	return 0;
}

static int
fw_chk_precrc(struct fw_ctx* ctx)
{
	struct fw_fileentry	*tmp;
	struct chk_header	*header;
	uint32_t		 crc;
	off_t			 size;
	int			 fd;
	int			 fn;

	/* Initial is 0 */
	crc = 0;

	header = (struct chk_header*) ctx->header;
	fn = 1;
	TAILQ_FOREACH(tmp, &ctx->files, entries) {
		if ((fd = open(tmp->name, O_RDONLY)) < 0){
			perror("can't open file");
			return -1;
		}

		printf(" * %s\n", tmp->name);
		csum2(fd, &crc, &size);
		close(fd);
		switch(fn)
		{
		case 1:
			header->cksum_part1 = crc;
			header->size_part1 = size;
			break;
		case 2:
			header->cksum_part2 = crc;
			header->size_part2 = size;
			break;
		default:
			break;
		}
		if (++fn > 2)
			break;
	}
	return (0);
}

static int
fw_chk_crc(struct fw_ctx* ctx)
{
	struct fw_fileentry	*tmp;
	struct chk_header	*header;
	uint32_t		 crc;
	off_t			 size;
	int			 fd;
	int			 fn;

	/* Initial is 0 */
	crc = 0;

	if((fd  = open(ctx->output, O_RDONLY)) < 0){
		perror("can't open header file");
		return -1;
	}

	printf(" * %s\n", ctx->output);
	csum2(fd, &crc, &size);
	close(fd);

	header = (struct chk_header*) ctx->header;
	header->cksum_header = crc;

	fn = 1;
	TAILQ_FOREACH(tmp, &ctx->files, entries) {
		if ((fd = open(tmp->name, O_RDONLY)) < 0){
			perror("can't open file");
			return -1;
		}

		printf(" * %s\n", tmp->name);
		csum2(fd, &crc, &size);
		close(fd);
		switch(fn)
		{
		case 1:
			header->cksum_part1 = crc;
			header->size_part1 = size;
			break;
		case 2:
			header->cksum_part2 = crc;
			header->size_part2 = size;
			break;
		default:
			break;
		}
		if (++fn > 2)
			break;
	}

	return 0;
}

static int
fw_chk_write(void* h, int fd, int mode)
{
	struct chk_header	*header;
	int 			 i;

	header = (struct chk_header*)h;

	WRITE_HEADER_BE(header->magic);
	WRITE_HEADER_BE(header->headersize);
	WRITE_HEADER_BE(header->version);
	WRITE_HEADER_BE(header->PAD0);
	WRITE_HEADER_BE(header->cksum_part1);
	WRITE_HEADER_BE(header->cksum_part2);
	WRITE_HEADER_BE(header->size_part1);
	WRITE_HEADER_BE(header->size_part2);
	WRITE_HEADER_BE(header->cksum_total);
	WRITE_HEADER_BE(header->cksum_header);
	WRITE_HEADER(header->model_name);

	return 0;
}
