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
#include <ctype.h>

#include "md5.c"
#include "hdr.h"

#define HDR_LENGTH 0x200
#define MAX_VERSION_STRING_LENGTH 0x40
#define MACHINE_MAGIC_LENGTH 0x8

#define OFFSET_MACHINE_MAGIC 0xc
#define OFFSET_VERSION_STRING 0x14
#define OFFSET_MD5_FILE 0x54
#define OFFSET_IMAGE_LENGTH 0x78
#define OFFSET_MD5_IMAGE 0x7c

/* Australian terrestrial models */

const char DP_S1_magic[]   = "0808 0000 0E20 BE3E";
const char DP_P1_magic[]   = "0808 0000 0A22 BE3C";
const char DP_P2_magic[]   = "0908 0000 0A22 9E3C";
const char DP_Lite_magic[] = "0808 0000 0002 9E3C";
const char DP_H1_magic[]   = "0408 0000 0022 7E3C";

/* Australian Freeview terrestrial models */

const char FV_L1_magic[]   = "0808 0002 0A22 9E3C";

/* Finnish terrestrial models */

const char FT_P1_magic[]   = "0808 0000 0E20 BE3E"; /* not confirmed */
const char FT_H1_magic[]   = "0808 0000 0A22 BE3C"; /* not confirmed */ 

/* Finnish cable models */

const char FC_P1_magic[]   = "0808 0000 0A22 BE3C"; /* not confirmed */
const char FC_H1_magic[]   = "0408 0000 0022 7E3C"; /* not confirmed */ 

enum magic_flags { none = 0, deprecated = (1<<0) };

typedef struct {
    const char const* name;
    const char const* magic;
    const enum magic_flags flags;
} magic_ent;

const magic_ent magic_ents[] = {
    { "DP-S1",   DP_S1_magic,   none       }, // Entry [0] is the default!
    { "DPS1",    DP_S1_magic,   none       },
    { "DP-P1",   DP_P1_magic,   none       },
    { "DPP1",    DP_P1_magic,   none       },
    { "DP-P2",   DP_P2_magic,   none       },
    { "DPP2",    DP_P2_magic,   none       },
    { "DP-H1",   DP_H1_magic,   none       },
    { "DPH1",    DP_H1_magic,   none       },
    { "FV-L1",   FV_L1_magic,   none       },
    { "FVL1",    FV_L1_magic,   none       },
    { "FC-P1",   FC_P1_magic,   none       },
    { "FCP1",    FC_P1_magic,   none       },
    { "FC-H1",   FC_H1_magic,   none       },
    { "FCH1",    FC_H1_magic,   none       },
    { "FT-P1",   FT_P1_magic,   none       },
    { "FTP1",    FT_P1_magic,   none       },
    { "FT-H1",   FT_H1_magic,   none       },
    { "FTH1",    FT_H1_magic,   none       },
    { "DP-Lite", DP_Lite_magic, none       },
    { "DPLite",  DP_Lite_magic, none       },
    { "H",       DP_H1_magic,   deprecated },
    { "P",       DP_P1_magic,   deprecated },
    { "S",       DP_S1_magic,   deprecated },
    { NULL,      NULL,          none       },
};

typedef struct {
    unsigned char *wrpbuf;
    long image_length;
    unsigned char machine_type[MACHINE_MAGIC_LENGTH];
    char version_string[0x40];
} wrp;

magic_ent const* get_magic_ent(char *name)
{
    magic_ent const* m_ent;
    for(m_ent = magic_ents; m_ent->name; m_ent++) {
        if(strcasecmp(name, m_ent->name) == 0) {
	    return m_ent;
	}
    }
    return NULL;
}

unsigned char parse_hex_char(unsigned char c)
{
    if(c >= '0' && c <= '9')
        return c - '0';

    if(islower(c))
        c = toupper(c);

    if(c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return 16; //invalid character. must be 0-9A-Fa-f
}

int parse_machine_type(char const *arg,
			unsigned char *machine_type_buf)
{
    int i;
    char c;

    for(i = MACHINE_MAGIC_LENGTH-1; i >= 0; i--) {
        if(i < MACHINE_MAGIC_LENGTH-1 && (*arg == ' ' || *arg == '-'))
            arg++;

        c = parse_hex_char(*arg++);
        if(c > 15)
            return 0;
        machine_type_buf[i] = c << 4;

        c = parse_hex_char(*arg++);
        if(c > 15)
            return 0;
        machine_type_buf[i] |= c;
    }

    return *arg == '\000';
}

int build_wrp(char *imagefile, wrp *w)
{
    unsigned char md5[0x10];
    struct cvs_MD5Context ctx; 
    FILE *fd = fopen(imagefile, "rb");
    if(fd == NULL)
        return 0;

    fseek(fd, 0L, SEEK_END);
    w->image_length = ftell(fd);
    
    w->wrpbuf = (unsigned char *)malloc(HDR_LENGTH + w->image_length
					+ HDR_LENGTH);
    
    fseek(fd, 0L, SEEK_SET);

    memcpy(w->wrpbuf, hdr, HDR_LENGTH);

    //write version string (max 0x40 chars)
    memset(&w->wrpbuf[OFFSET_VERSION_STRING], 0, MAX_VERSION_STRING_LENGTH);
    strncpy(&((char *)w->wrpbuf)[OFFSET_VERSION_STRING],
		w->version_string, MAX_VERSION_STRING_LENGTH);
    
    //write machine magic
    memcpy(&w->wrpbuf[OFFSET_MACHINE_MAGIC], w->machine_type, 0x8);

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

void print_valid_magics(char *indent)
{
    magic_ent const* m_ent;
    magic_ent const* m_last = NULL;
    for(m_ent = magic_ents; m_ent->name; m_ent++) {
	if(!(m_ent->flags & deprecated))
	    m_last = m_ent;
    }
    int pos = 0;
    int indent_len = strlen(indent) + 1;
    for(m_ent = magic_ents; m_ent->name; m_ent++) {
	if(!(m_ent->flags & deprecated)) {
	    if(pos == 0) {
		printf("%s", indent);
		pos = indent_len;
	    }
	    pos += strlen(m_ent->name) + (m_ent == m_last ? 0 : 2);
            printf("%s%s", m_ent->name, (m_ent == m_last ? "" : ", "));
	    if(pos >= 72) {
	        pos = 0;
		printf("\n");
	    }
	}
    }
    if(pos != 0)
	printf("\n");
}

void usage(char *filename)
{
    printf("\nUsage: %s [-t machine_type] [-T machine_type_hex_string] [-V version_string] [-h] -i infile -o outfile\n", filename);
    printf("-t    machine_type.         Default: DP-S1\n");
    print_valid_magics("                            ");
    printf("-T    custom machine_type.  Enter a 16 character machine code.\n");
    printf("                            eg. \"080800000e20be3e\" for the DP-S1 \n");
    printf("-V    version_string.       Max 64 characters. Default: \"wiz_pack\"\n");
    printf("-i    infile.               Input romfs filename\n");
    printf("-o    outfile.              Output wrp firmware filename\n");
    printf("-h    Print this help.\n\n");
}

int main(int argc, char *argv[])
{
    wrp w; 
    FILE *fd;
    char *wrpfile = NULL;
    char *romfsfile = NULL;
    int c;
    char const* machine_magic = magic_ents[0].magic;

    //clear machine_type
    memset(w.machine_type, 0, MACHINE_MAGIC_LENGTH);

    strcpy(w.version_string, "wiz_pack");

    while((c = getopt(argc, argv, "T:t:i:o:V:h")) != EOF) {
	switch(c) {
	    case 'i' :
		romfsfile = optarg;
		break;
	    case 'o' :
		wrpfile = optarg;
		break;
	    case 't' : {
		const magic_ent const* m_ent = get_magic_ent(optarg);
		if(!m_ent) {
		    printf("\nError: Invalid machine type %s!\n",
			optarg);
		    usage(argv[0]);
		    exit(1);
		    break;
		}
		if(m_ent->flags & deprecated) {
		    printf("Machine type code \"%s\" is deprecated\n", optarg);
		    printf("Codes:\n");
		    print_valid_magics("        ");
		    printf("are preferred\n");
		}
		machine_magic = m_ent->magic;
		break;
	    }
	    case 'T' :
		machine_magic = optarg;
		break;
	    case 'V' :
		strncpy(w.version_string, optarg, MAX_VERSION_STRING_LENGTH);
		break;
	    case 'h' : usage(argv[0]);
		exit(0);
		break;
	}
    }

    if(!parse_machine_type(machine_magic, w.machine_type)) {
	printf("\nError: Invalid machine type: %s!\n", machine_magic);
	usage(argv[0]);
	exit(1);
    }

    if(wrpfile == NULL || romfsfile == NULL) {
	printf("\nError: Not enough arguments!\n");
	usage(argv[0]);
	exit(1);
    }

    if(build_wrp(romfsfile, &w) != 0) {
	fd = fopen(wrpfile, "wb");
	if(fd == NULL)
	    return 1;

	fwrite(w.wrpbuf, 1, HDR_LENGTH + w.image_length + HDR_LENGTH, fd);

	fclose(fd); 
    }

    return 0;
}

