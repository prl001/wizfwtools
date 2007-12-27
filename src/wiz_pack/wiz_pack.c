// Simple Wiz wrp firmware repacker. - Eric Fry 2007
// arg1 = romfs image
// arg2 = output wrp file
// WARNING!!! Custom firmwares could brick your unit!
// I haven't tested this on little endian machines!
// Remember to construct your romfs images on a case sensitive filesystem!

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "md5.c"
#include "hdr.h"

#define HDR_LENGTH 0x200
#define MAX_VERSION_STRING_LENGTH 0x40

#define OFFSET_MACHINE_MAGIC 0xc
#define OFFSET_VERSION_STRING 0x14
#define OFFSET_MD5_FILE 0x54
#define OFFSET_IMAGE_LENGTH 0x78
#define OFFSET_MD5_IMAGE 0x7c

const unsigned char S1_magic[] = { 0x3e, 0xbe, 0x20, 0x0e, 0x00, 0x00, 0x08, 0x08 };
const unsigned char P1_magic[] = { 0x3c, 0xbe, 0x22, 0x0a, 0x00, 0x00, 0x08, 0x08 };
const unsigned char H1_magic[] = { 0x3c, 0x7e, 0x22, 0x00, 0x00, 0x00, 0x08, 0x04 };

typedef struct {
unsigned char *wrpbuf;
long image_length;
char machine_type;
char version_string[0x40];
} wrp;

int build_wrp(char *imagefile, wrp *w)
{
 unsigned char md5[0x10];
 struct cvs_MD5Context ctx; 
 FILE *fd = fopen(imagefile, "rb");
 if(fd == NULL)
  return 0;

 fseek(fd, 0L, SEEK_END);
 w->image_length = ftell(fd);
 
 w->wrpbuf = (unsigned char *)malloc(HDR_LENGTH + w->image_length + HDR_LENGTH);
 
 fseek(fd, 0L, SEEK_SET);

 memcpy(w->wrpbuf, hdr, HDR_LENGTH);

 //write version string (max 0x40 chars)
 memset(&w->wrpbuf[OFFSET_VERSION_STRING], 0, MAX_VERSION_STRING_LENGTH);
 strncpy(&((char *)w->wrpbuf)[OFFSET_VERSION_STRING], w->version_string, MAX_VERSION_STRING_LENGTH);
 
 //write machine magic
 switch(w->machine_type)
 {
	case 's' : memcpy(&w->wrpbuf[OFFSET_MACHINE_MAGIC], S1_magic, 0x8); break;
	case 'p' : memcpy(&w->wrpbuf[OFFSET_MACHINE_MAGIC], P1_magic, 0x8); break;
	case 'h' : memcpy(&w->wrpbuf[OFFSET_MACHINE_MAGIC], H1_magic, 0x8); break;
 }
 
 memset(&w->wrpbuf[OFFSET_MD5_FILE], 0, 0x10);
 memset(&w->wrpbuf[OFFSET_IMAGE_LENGTH], 0, 0x4);
 memset(&w->wrpbuf[OFFSET_MD5_IMAGE], 0, 0x10);

 putu32(w->image_length, &w->wrpbuf[OFFSET_IMAGE_LENGTH]);

 // read in image
 fread(&w->wrpbuf[HDR_LENGTH], 1, w->image_length, fd); 

 fclose(fd);

 //blank padding at end of wrp file.
 memset(&w->wrpbuf[HDR_LENGTH + w->image_length], 0, HDR_LENGTH);
 
 // calc and output image md5 sum
 cvs_MD5Init(&ctx);
 cvs_MD5Update(&ctx, &w->wrpbuf[HDR_LENGTH], w->image_length);
 cvs_MD5Final(md5, &ctx); 
 
 memcpy(&w->wrpbuf[OFFSET_MD5_IMAGE], md5, 0x10);

 // calc and output wrp md5 sum
 cvs_MD5Init(&ctx);
 cvs_MD5Update(&ctx, w->wrpbuf, HDR_LENGTH + w->image_length + HDR_LENGTH);
 cvs_MD5Final(md5, &ctx); 
 
 memcpy(&w->wrpbuf[OFFSET_MD5_FILE], md5, 0x10);

 return 1; 
}
void usage(char *filename)
{
 printf("\nUsage: %s [-t machine_type] [-V version_string] [-h] -i infile -o outfile\n", filename);
 printf("-t    machine_type.   Either s, p or h. Default: s, [DP-S1]\n");
 printf("-V    version_string. Max 64 characters. Default: \"wiz_pack\"\n");
 printf("-i    infile.         Input romfs filename\n");
 printf("-o    outfile.        Output wrp firmware filename\n");
 printf("-h    Print this help.\n\n");
 
}

int main(int argc, char *argv[])
{
 wrp w; 
 FILE *fd;
 char *wrpfile = NULL;
 char *romfsfile = NULL;
 int c;
 
 w.machine_type = 's';
 strcpy(w.version_string, "wiz_pack");
 
 while((c = getopt(argc, argv, "t:i:o:V:h")) != EOF)
 {
	switch(c)
	{
		case 'i' : romfsfile = optarg;
			break;
		case 'o' : wrpfile = optarg;
			break;
		case 't' :
			if(strlen(optarg) != 1)
			{
				printf("\nError: Invalid machine type!\n");
				usage(argv[0]);
				exit(1);
				break;
			}
			switch(optarg[0])
			{
				case 's' :
				case 'h' :
				case 'p' : w.machine_type = optarg[0];
					break;
				default : printf("\nError: Unknown machine type!\n");
					usage(argv[0]);
					exit(1);
					break;
			}
			break;
		case 'V' : strncpy(w.version_string, optarg, MAX_VERSION_STRING_LENGTH);
			break;
		case 'h' : usage(argv[0]);
			exit(0);
			break;
	}
 }

 if(wrpfile == NULL || romfsfile == NULL)
 {
	printf("\nError: Not enough arguments!\n");
	usage(argv[0]);
	exit(1);
 }

 if(build_wrp(romfsfile, &w) != 0)
 {
  fd = fopen(wrpfile, "wb");
  if(fd == NULL)
   return 1;

  fwrite(w.wrpbuf, 1, HDR_LENGTH + w.image_length + HDR_LENGTH, fd);

  fclose(fd); 
 }

 return 0;
}

