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
#include <string.h>

#include <sys/queue.h>
#include <sys/stat.h>

#include "utils.h"

static void	usage();
static int	create_image(struct fw_methods* api, int nfiles,
		    char** filenames, char* output);

#define	FILEMAX	16

char* filenames[FILEMAX];

int main(int argc, char** argv){
	if(argc > 1){
		if ((strcmp("-v",argv[1]) == 0) && (argc == 3))
			return utils_read(argv[2], &fw);
		if ((strcmp("-c",argv[1]) == 0) && (argc > 3)) {
			int size = argc - 3;
			for (int i = 0; i < size; i++)
				filenames[i] = argv[i + 2];

			return create_image(&fw, size, filenames, argv[argc - 1]);
		}
	}
	usage(argv[0]);
	return 0;
}

void usage(char* progname){
	printf("usage: %s [-v] filename\n",progname);
	printf("       %s [-c] %s output\n", progname, fw_create_args);
	return;
}

int
create_image(struct fw_methods* api, int nfiles, char** filenames, char* output)
{
	struct fw_ctx		*ctx;
	struct fw_fileentry	*tmp, *ttmp;
	struct stat		 filestat;
	int			 err;

	err = 0;
	if ((ctx = malloc(sizeof(struct fw_ctx))) == NULL) {
		perror("Out of memory during context allocation");
		goto error;
	}

	ctx->output = output;
	ctx->nfiles = 0;
	TAILQ_INIT(&ctx->files);
	for (int i = 0; i < nfiles; i++) {
		if (stat(filenames[i], &filestat) != 0) {
			perror(filenames[i]);
			goto error;
		}

		tmp = malloc(sizeof(struct fw_fileentry));
		if (tmp == NULL) {
			perror("Can't allocate memory for file entry");
			goto error;
		}
		TAILQ_INSERT_TAIL(&ctx->files, tmp, entries);
		ctx->nfiles++;
		tmp->name = filenames[i];
		tmp->size = filestat.st_size;
		tmp->indx = i;
	}

	if ((ctx->header = malloc(api->get_headersize())) == NULL) {
		perror("Out of memory during header allocation");
		goto error;
	}

	if (api->init_header(ctx) != 0) {
		perror("Can't initialize header");
		goto error;
	}

	if (utils_write(ctx, api, NOCRC) != 0) {
		perror("Can't write header without checksum(s)");
		goto error;
	}

	if (api->calculate_crc(ctx) != 0) {
		perror("Can't calculate checksums");
		goto error;
	}

	if (utils_write(ctx, api, WITHCRC) != 0) {
		perror("Can't write header with checksum(s)");
		goto error;
	}

	/* append rest sections */
	TAILQ_FOREACH(tmp, &ctx->files, entries)
		if(utils_fappend(tmp->name, ctx->output) < 0)
			goto error;

	utils_read(ctx->output, api);

	goto success;
error:
	err = -1;

success:
	if (ctx != NULL && ctx->header != NULL)
		free(ctx->header);
	if (ctx != NULL) {
		TAILQ_FOREACH_SAFE(tmp, &ctx->files, entries, ttmp) {
			TAILQ_REMOVE(&ctx->files, tmp, entries);
			free(tmp);
		}
	}
	if (ctx != NULL)
		free(ctx);
	return err;
}
