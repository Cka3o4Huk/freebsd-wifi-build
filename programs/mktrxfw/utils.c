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
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include "extern.h"
#include <stdlib.h>
#include "utils.h"

#define	BUFSIZE		4096

int utils_fpad_zero(char* output, int nzeros){
	int		fd, err;
	const uint8_t	zero =	0;

	err = 0;
	fd = open(output, O_WRONLY | O_CREAT);
	if(fd < 0){
		perror("can't create padding file");
		goto error;
	}

	if(ftruncate(fd,0) != 0){
		perror("can't erase padding file");
		goto error;
	}

	if(chmod(output, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) != 0){
		perror("can't set permissions 0644 of TRX file");
		goto error;
	}

	for(int i = 0; i < nzeros; i++)
		WRITE_HEADER(zero);

	goto success;
error:
	err = -1;
success:
	close(fd);
	return err;
}

int
utils_fappend(char* src, char* dst)
{
	int	 fs, fd, n, w, err;
	uint8_t	 buf[BUFSIZE];
	uint8_t	*wbuf;

	err = 0;
	if((fs = open(src, O_RDONLY)) < 0){
		perror("can't open file");
		goto error;
	}

	if((fd = open(dst, O_WRONLY | O_APPEND)) < 0){
		perror("can't open file");
		goto error;
	}

	do{
		n = read(fs, buf, BUFSIZE);
		wbuf = buf;
		if(n > 0){
			while(n > 0) {
				if((w = write(fd, wbuf, n)) < 0){
					perror("can't append to file");
					goto error;
				}
				n -= w;
				wbuf += w;
			}
		}else if(n == 0){
			break;
		}else{
			perror("can't read from file");
			return -1;
		}
	}while(1);

	goto success;
error:
	err = -1;
success:
	if (fs > 0)
		close(fs);
	if (fd > 0)
		close(fd);
	return err;
}

int
utils_write(struct fw_ctx *ctx, struct fw_methods *m, int mode)
{
	int	fd, err;

	err = 0;
	fd = open(ctx->output, O_WRONLY | O_CREAT);
	if(fd < 0){
		perror("can't create file");
		err = -1;
		goto exit;
	}

	if(ftruncate(fd,0) < 0){
		perror("can't erase TRX file");
		err = -2;
		goto exit;
	}

	if(chmod(ctx->output, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) < 0){
		perror("can't set permissions 0644 of TRX file");
		err = -3;
		goto exit;
	}

	err = m->write_header(ctx->header, fd, mode);

exit:
	if (fd > 0)
		close(fd);
	return err;
}

int
utils_read(const char* filename, struct fw_methods *api)
{
	int	fd, err, i;

	fd = open(filename, O_RDONLY);
	if(fd < 0){
		perror("can't open file:");
		return -1;
	}

	printf("======= %s =========\n", filename);
	err = api->print_header(fd);
	printf("====================\n");

	close(fd);
	return err;
}
