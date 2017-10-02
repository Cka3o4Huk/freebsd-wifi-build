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

#ifndef _MKTRXFW_UTILS_H_
#define _MKTRXFW_UTILS_H_

#include <sys/queue.h>

struct fw_fileentry {
	TAILQ_ENTRY(fw_fileentry) entries;
	char			*name;
	int			 size;
	int			 indx;
};

struct fw_ctx {
	TAILQ_HEAD(lf,fw_fileentry)	 files;
	int				 nfiles;
	void				*header;
	char				*output;
};

struct fw_methods {
	int	(*get_headersize) (void);
	int	(*init_header) (struct fw_ctx* ctx);
	int	(*calculate_crc) (struct fw_ctx* ctx);
	int	(*write_header) (void* header, int fd, int mode);
	int	(*print_header) (int fd);
};

extern struct fw_methods fw;
extern char *fw_create_args;

#define	NOCRC	0
#define	WITHCRC	1

#define	READ_HEADER(a)						\
	do{							\
		if(read(fd, &(a), sizeof(a)) < 0){ 		\
			perror("can't read file"); 		\
			return -1; 				\
		}						\
	} while(0);

#define READ_HEADER_BE(a)					\
	do{							\
		READ_HEADER(a);					\
		a = be32toh(a);					\
	} while (0);

#define	WRITE_HEADER(a)						\
	do{							\
		if(write(fd, &(a), sizeof(a)) < 0){ 		\
			perror("can't write file"); 		\
			return -1; 				\
		}						\
	} while(0);

#define	WRITE_HEADER_BE(a)					\
	do {							\
		a = htobe32(a);					\
		WRITE_HEADER(a);				\
		a = be32toh(a);					\
	} while (0);

int	utils_fpad_zero(char* output, int nzeros);
int	utils_fappend(char* src, char* dst);
int	utils_read(const char* filename, struct fw_methods *api);
int	utils_write(struct fw_ctx *ctx, struct fw_methods *m, int mode);

#endif /* PROGRAMS_MKTRXFW_UTILS_H_ */
