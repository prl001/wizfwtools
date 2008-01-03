/* romfs_extract - check and optionally extract a ROMFS file system
 *
 * Copyright (C) 2004 Harald Welte <laforge@gnumonks.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * Version 2 as published by the Free Software Foundation.
 *
 */

/*
 * Warning!  Not fully implemented yet, and spaghetti code. Patches welcome.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <fnmatch.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>

#include <netinet/in.h>

struct romfh {
	int32_t nextfh;
	int32_t spec;
	int32_t size;
	int32_t checksum;
};

#define ROMFS_MAXFN 128
#define ROMFH_HRD 0
#define ROMFH_DIR 1
#define ROMFH_REG 2
#define ROMFH_LNK 3
#define ROMFH_BLK 4
#define ROMFH_CHR 5
#define ROMFH_SCK 6
#define ROMFH_FIF 7
#define ROMFH_EXEC 8

struct node_info {
	u_int32_t nextfh;
	u_int32_t spec;
	u_int32_t size;
	u_int32_t checksum;
	char name[1024];
	void *file;
	u_int32_t mode;
	unsigned int namelen;
};

static int case_insensitive_mode = 0;

static int parse_node(void *start, unsigned int offset, struct node_info *ni)
{
	struct stat s;
    struct romfh *rfh = start + offset;
	char *name;
	int namelen;
	int i;
	char filetype = 'r';


    ni->size = ntohl(rfh->size);
    ni->mode = (ntohl(rfh->nextfh) & 0x0f);
    ni->nextfh = ntohl(rfh->nextfh) & ~0x0f;
    ni->checksum = ntohl(rfh->checksum);
    ni->spec = ntohl(rfh->spec);
    name = (char *) (rfh+1);

    namelen = strlen(name) + 1; // The original wasn't including the trailing zero.

    if(namelen & 15)
        namelen += 16 - (namelen&15);

    ni->file = (char *)name + namelen;
	
	ni->namelen = namelen;

	if(offset != 0 && case_insensitive_mode == 1) //offset 0 is the header record.
	{

		if (ni->mode & ROMFH_EXEC)
			filetype = 'x';

		ni->namelen += 5;

		if(ni->namelen > 1024)
		{
			fprintf(stderr, "filename > 1024!\n");
			exit(1);
		}
	
		//find a free filename. This is used for case in-sensitive file systems.
		for(i=1;i<1000;i++)
		{
			sprintf(ni->name, "%03d%c_", i, filetype);
			strncpy(&ni->name[5], name, namelen);
			if(stat(ni->name, &s) != 0)
				break;
		}
	}
	else
		strncpy(ni->name, name, namelen);

	return 1;
}

static int recurse(void *mem, unsigned int offset, const char *dir)
{
	int ret;
	FILE *outfd;
	unsigned int mode;
	struct node_info ni;

	while (1) {
		printf("==========================\n");
		printf("parse_node(%u/0x%x)\n", offset, offset);
		parse_node(mem, offset, &ni);
		printf("%s: ", ni.name);

		switch (ni.mode & ~ROMFH_EXEC) {
		case ROMFH_HRD:
			printf("hard link\n");
			if (ni.size)
				fprintf(stderr, "nonstandard size `%u'\n",
					ni.size);
			break;
		case ROMFH_DIR:
			printf("directory\n");
			if (ni.size)
				fprintf(stderr, "nonstandard size `%u'\n",
					ni.size);
			if (case_insensitive_mode && (!strcmp(&ni.name[5], "..") || !strcmp(&ni.name[5], ".")) ||
				(!strcmp(ni.name, "..") || !strcmp(ni.name, ".")))
			 {
				fprintf(stderr, "name invalid!!\n");
				break;
			}
			if (mkdir(ni.name, 0750) < 0) {
				fprintf(stderr, "can't create `%s': %s\n",
					ni.name, strerror(errno));
				return -1;
			}
			chdir(ni.name);

			if (ni.spec != offset) {
				ret = recurse(mem, ni.spec, dir);
				chdir("..");
				if (ret < 0)
					return ret;
			}
			break;
		case ROMFH_REG:
			printf("regular file\n");
			mode = 0640;
			if (ni.mode & ROMFH_EXEC)
				mode |= 0110;

			outfd = fopen(ni.name, "wb+"); //creat(ni.name, mode);
			if (outfd == NULL) { //< 0) {
				fprintf(stderr, "can't create `%s': %s\n",
					ni.name, strerror(errno));
				return -1;
			}

			if(ni.size > 0) {
				if (fwrite(ni.file, ni.size, 1, outfd) != 1) {
					fprintf(stderr, "short write `%s': %s\n",
						ni.name, strerror(errno));
					fclose(outfd);
					return -1;
				}
			}
			fclose(outfd);
			chmod(ni.name, mode);
			break;
		case ROMFH_LNK:
			printf("symbolic link: %s\n", (char*)ni.file);
			if(symlink((char*)ni.file, ni.name) < 0) {
				fprintf(stderr, "can't link `%s' to `%s': %s\n",
					ni.name, (char*)ni.file,
					strerror(errno));
				return -1;
			}
			break;
		case ROMFH_BLK:
			printf("block device\n");
			if (ni.size)
				fprintf(stderr, "nonstandard size `%u'\n",
					ni.size);
			break;
		case ROMFH_CHR:
			printf("character device\n");
			if (ni.size)
				fprintf(stderr, "nonstandard size `%u'\n",
					ni.size);
			break;
		case ROMFH_SCK:
			printf("socket\n");
			if (ni.size)
				fprintf(stderr, "nonstandard size `%u'\n",
					ni.size);
			break;
		case ROMFH_FIF:
			printf("fifo\n");
			if (ni.size)
				fprintf(stderr, "nonstandard size `%u'\n",
					ni.size);
			break;
		}

		if (!ni.nextfh)
			return 1;

		printf("nextfh=%u\n", ni.nextfh);

		offset = ni.nextfh;
		//curmem += ni.nextfh;
	}
	/* not reached */
	return -10;
}


static int extract_from_mem(void *mem, unsigned int len, const char *dir)
{
	unsigned int offset = 0;
	struct node_info ni;

	chdir(dir);

	//ri.checksum = htonl(0x55555555);
	//
	parse_node(mem, offset, &ni);
	printf("+= %lu + %u\n", sizeof(struct romfh), ni.namelen);
	offset += sizeof(struct romfh) + ni.namelen;

	printf("romfs image name: `%s'\n", ni.name);
	printf("romfs image size: %u\n", ni.size);

	return recurse(mem, offset, dir);
}


static int extract(const char *pathname, const char *outpath)
{
	int fd, ret = -1;
	struct stat st;
	void *mem;
	void *mem_ptr;

	fd = open(pathname, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "Error opening `%s': %s\n", pathname,
			strerror(errno));
		goto err_out;
	}

	if (fstat(fd, &st) < 0) {
		fprintf(stderr, "Error in fstat: %s\n", strerror(errno));
		goto err_close;
	}

	mem = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (!mem) {
		fprintf(stderr, "Error during MMAP: %s\n", strerror(errno));
		goto err_close;
	}
	mem_ptr = mem;
	
	if (!strncmp((char *)mem_ptr, "WizFwPkg", 8) && st.st_size > 0x200) //This is a wrp file. Skip the header.
	{
		mem_ptr += 0x200;
	}
		
	if (strncmp((char *)mem_ptr, "-rom1fs-", 8)) {
		fprintf(stderr,"File is not a romfs image\n");
		goto err_unmap;
	}

	
	ret = extract_from_mem(mem_ptr, st.st_size, outpath);

err_unmap:
	munmap(mem, st.st_size);
err_close:
	close(fd);
err_out:
	return ret;
}

static char *cmdname;


static void version(void)
{
	fprintf(stderr, "%s, Version 0.01\n", cmdname);
	fprintf(stderr, "Romfsck (Modified to unpack Beyonwiz firmware files.)\n");
	fprintf(stderr, "(C) 2007 by Eric Fry <efry78@gmail.com>\n\n");
	fprintf(stderr, "Original Romfsck (C) 2004 by Harald Welte <laforge@gnumonks.org>\n\n");
	
	fprintf(stderr, "This program is Free Software covered by GNU GPLv2\n");
}

static void usage(void)
{
	version();
	fprintf(stderr, "%s [options] firmware\n",cmdname);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "-i           Case in-sensitive mode. (prepend nnn_ to all files/directories)\n");	
	fprintf(stderr, "-x outdir    Expand firmware romfs image into directory/\n");
	fprintf(stderr, "-V           Print version and exit\n");
	fprintf(stderr, "-h           Print this usage and exit\n\n");
}

int main(int argc, char **argv)
{
	int c;
	char *outdir = NULL;
	struct stat s;
	
	cmdname = argv[0];

#ifdef WIN32
	case_insensitive_mode = 1; // We hard code this for the Windows platform.
#endif

	while ((c = getopt(argc, argv, "x:hiV")) != EOF) {
		switch (c) {
		case 'i':
			case_insensitive_mode = 1;
			break;
		case 'x':
			outdir = optarg;
			break;
		case 'h':
			usage();
			exit(0);
			break;
		case 'V':
			version();
			exit(0);
			break;
		}
	}

	if (argc <= optind) {
		usage();
		exit(2);
	}

	if (outdir && stat(outdir,&s) != 0) {
		if (mkdir(outdir, 0750) < 0) { //try and create directory
			fprintf(stderr, "unable to create `%s': %s\n",
				outdir, strerror(errno));
			exit(1);
		}
	}

	if (extract(argv[optind], outdir) < 0)
		exit(1);

	exit(0);
}
