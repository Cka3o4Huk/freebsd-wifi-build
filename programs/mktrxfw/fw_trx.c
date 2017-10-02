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

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/queue.h>

#include "extern.h"
#include "utils.h"

/* https://wiki.openwrt.org/doc/techref/header */
#define	MAGIC				0x30524448 // "HDR0"
#define	TRX_VERSION			1
#define	TRX_HEADER_SIZE			28
#define	NUM_OFFSETS			3
#define TRX_ALIGNMENT			0x2000

static int	fw_trx_crc(struct fw_ctx* ctx);
static int	fw_trx_get_headersize(void);
static int	fw_trx_init(struct fw_ctx* ctx);
static int	fw_trx_print(int fd);
static int	fw_trx_write(void* header, int fd, int mode);

struct trx_header {
	uint32_t magic;
	uint32_t file_length;
	uint32_t crc32;
	uint16_t flags;
	uint16_t version;
	uint32_t offsets[NUM_OFFSETS];
};

struct fw_methods fw = {
	.get_headersize	= fw_trx_get_headersize,
	.print_header	= fw_trx_print,
	.calculate_crc	= fw_trx_crc,
	.init_header	= fw_trx_init,
	.write_header 	= fw_trx_write
};

char 	*fw_create_args = "lzmaloader lzmakernel fsimage";

static int
fw_trx_print(int fd)
{
	int			i;
	struct trx_header	header;

	READ_HEADER(header.magic);
	READ_HEADER(header.file_length);
	READ_HEADER(header.crc32);
	READ_HEADER(header.flags);
	READ_HEADER(header.version);
	for (i = 0; i < NUM_OFFSETS; i++)
		READ_HEADER(header.offsets[i]);

	printf("magic\t= 0x%04x\n", header.magic);
	printf("length\t= %d\n", header.file_length);
	printf("crc32\t= 0x%04x\t", header.crc32);
	printf("~crc32\t= 0x%04x\n", ~header.crc32);
	printf("flags\t= 0x%04x\n", header.flags);
	printf("version\t= %d\n", header.version);

	for (i = 0; i < NUM_OFFSETS; i++)
		printf("offset[%d] = %d\n", i, header.offsets[i]);

	return 0;
}

static int
fw_trx_get_headersize(void)
{
	return TRX_HEADER_SIZE;
}

static int
fw_trx_init(struct fw_ctx* ctx)
{
	struct fw_fileentry	*file, *tmp, *safe;
	struct trx_header	*header;
	int 			 offset;

	offset = TRX_HEADER_SIZE;
	header = ctx->header;
	header->magic = MAGIC;
	header->flags = 0;
	header->version = 1;

	if (ctx->nfiles != NUM_OFFSETS) {
		perror("Number of input files should be 3");
		return -1;
	}

	/* calculate TRX header offsets */
	TAILQ_FOREACH_SAFE(file, &ctx->files, entries, safe) {
		/* align last offset to 8Kb (16 sectors) */
		if (!(TAILQ_NEXT(file, entries)) && (offset % TRX_ALIGNMENT)) {
			/* file for zeros */
			tmp = malloc(sizeof(struct fw_fileentry));
			if (tmp == NULL) {
				perror("ENOMEM during alloc fileentry");
				return -1;
			}
			tmp->name = ".zeros";
			tmp->size = TRX_ALIGNMENT - offset % TRX_ALIGNMENT;
			tmp->indx = -1; /* fake file */
			TAILQ_INSERT_BEFORE(file, tmp, entries);

			if (utils_fpad_zero(tmp->name, tmp->size) < 0)
				return -1;

			offset = (offset / TRX_ALIGNMENT + 1) * TRX_ALIGNMENT;

		}
		header->offsets[file->indx] = offset;
		offset += file->size;
	}
	header->file_length = offset;
	return 0;
}

static int
fw_trx_crc(struct fw_ctx* ctx)
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
	header->crc32 = ~crc32_total;
	return 0;
}

static int
fw_trx_write(void* h, int fd, int mode)
{
	struct trx_header	*header;
	int 			 i;

	header = (struct trx_header*)h;

	if (mode == WITHCRC) {
		WRITE_HEADER(header->magic);
		WRITE_HEADER(header->file_length);
		WRITE_HEADER(header->crc32);
	}

	WRITE_HEADER(header->flags);
	WRITE_HEADER(header->version);
	for (i = 0; i < NUM_OFFSETS; i++)
		WRITE_HEADER(header->offsets[i]);

	return 0;
}
