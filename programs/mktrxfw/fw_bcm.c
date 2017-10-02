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

#include <sys/types.h>
#include <sys/stat.h>

#include "extern.h"
#include "utils.h"

/*
 * Aligned on 0x010000 (found - 0x020000, 0x400000).
 * Flash may contains several firmware images (upgrade redundancy?)
 * Header (92 bytes):
 *    base + 0x00 = 0xa81c0005 (firmware header magic + control)
 *    base + 0x04 = 0x010001ff (version?)
 *    base + 0x08 = 0x5317fda2 (timestamp) 1394081186 / 06 Mar 2014 04:46:26 GMT
 *    base + 0x0c = 0x00363851 (firmware size except header, e.g.
 *    				LZMA compressed size)
 *    base + 0x10 = 0x80004000 (load address)
 *    base + 0x14 = firmware name char[64]
 *    base + 0x54 = 0x6af80000 (header CRC16-CCITT with initial 0xFFFF,
 *    				padded by 0000)
 *    base + 0x58 = 0x11757d30 (CRC???)
 *
 * Body starts with +0x5c = assumed LZMA start
 */

#define MAGIC				0xA81C0005
#define BCM_VERSION			0x010001FF
#define LOAD_ADDRESS			0x80001100
#define NUM_OFFSETS			3

struct bcm_header {
	uint32_t	magic;
	uint32_t	version;
	uint32_t	timestamp;
	uint32_t	file_length;
	uint32_t	loadaddress;
	char		name[64];
	uint16_t	crc16_header;
	uint16_t	PAD1;
	uint32_t	crc32;
};

static int	fw_bcm_crc(struct fw_ctx* ctx);
static int	fw_bcm_get_headersize(void);
static int	fw_bcm_init(struct fw_ctx* ctx);
static int	fw_bcm_print(int fd);
static int	fw_bcm_write(void* header, int fd, int mode);

struct fw_methods fw_chk = {
	.get_headersize	= fw_bcm_get_headersize,
	.print_header	= fw_bcm_print,
	.calculate_crc	= fw_bcm_crc,
	.init_header	= fw_bcm_init,
	.write_header 	= fw_bcm_write
};

static int
fw_bcm_print(int fd)
{
	int			i;
	struct bcm_header	header;

	READ_HEADER(header.magic);
	READ_HEADER(header.version);
	READ_HEADER(header.timestamp);
	READ_HEADER(header.file_length);
	READ_HEADER(header.loadaddress);
	READ_HEADER(header.name);
	READ_HEADER(header.crc16_header);
	READ_HEADER(header.PAD1);
	READ_HEADER(header.crc32);

	printf("magic\t= 0x%04x\n", header.magic);
	printf("version\t= 0x%04x\n", header.version);
	printf("timestamp\t= %d\n", header.timestamp);
	printf("length\t= %d\n", header.file_length);
	printf("loadaddr\t= 0x%04x\n", header.loadaddress);
	printf("name\t= %s\n", header.name);
	printf("crc16\t = 0x%02x\n", header.crc16_header);
	printf("padding\t= 0x%02x\n", header.PAD1);
	printf("crc32\t= 0x%04x\n", header.crc32);
	printf("~crc32\t= 0x%04x\n", ~header.crc32);

	return 0;
}

static int
fw_bcm_get_headersize(void)
{
	return 0x5c;
}

static int
fw_bcm_init(struct fw_ctx* ctx)
{
	struct fw_fileentry	*file;
	struct bcm_header	*header;
	time_t			 cts;

	if (ctx->nfiles != 1) {
		perror("Number of input files should be 1");
		return -1;
	}
	header = ctx->header;
	header->magic = MAGIC;
	header->version = BCM_VERSION;
	/* year 2038 problem 8) */
	header->timestamp = (uint32_t)(((uint64_t)time()) & 0xFFFFFFFF);

	file = TAILQ_FIRST(&ctx->files);
	header->file_length = file->size;
	header->loadaddress = LOAD_ADDRESS;
	header->name = "FreeBSD for BCM";
	header->PAD1 = 0;
	return 0;
}

static int
fw_bcm_crc(struct fw_ctx* ctx)
{
	struct fw_fileentry	*tmp;
	struct trx_header	*header;
	uint32_t		 crc;
	off_t			 size;
	int			 fd;

	/* Initial is 0 */
	crc = 0;

	if((fd  = open(ctx->output, O_RDONLY)) < 0){
		perror("can't open temp TRX file");
		return -1;
	}

	printf(" * %s\n", ctx->output);
	crc32(fd, &crc, &size);
	close(fd);

	TAILQ_FOREACH(tmp, &ctx->files, entries) {
		if ((fd = open(tmp->name, O_RDONLY)) < 0){
			perror("can't open file");
			return -1;
		}

		printf(" * %s\n", tmp->name);
		crc32(fd, &crc, &size);
		close(fd);
	}

	header = (struct trx_header*) ctx->header;
	/* Inverse */
	header->crc32 = ~crc;
	return 0;
}

static int
fw_bcm_write(void* h, int fd, int mode)
{
	struct bcm_header	*header;
	int 			 i;

	header = (struct bcm_header*)h;

	WRITE_HEADER(header->magic);
	WRITE_HEADER(header->version);
	WRITE_HEADER(header->timestamp);
	WRITE_HEADER(header->file_length);
	WRITE_HEADER(header->loadaddress);
	WRITE_HEADER(header->name);

	if (mode == WITHCRC) {
		WRITE_HEADER(header->crc16_header);
		WRITE_HEADER(header->PAD1);
		WRITE_HEADER(header->crc32);
	}

	return 0;
}
