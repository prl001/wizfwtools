//svcTool Copyright 2008 Eric Fry efry@users.sourceforge.net
//Licensed under the GPLv2
//

#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>

#define u8 unsigned char
#define u16 unsigned short int
#define u32 unsigned int

const char *svc_dat = "svc.dat";

typedef struct {
	u16 unk1;
	u16 unk2;
	u32 rf;
	u16 onid;
	u16 tsid;
	unsigned char *name;
} RFInfo;

u16 read_u16(FILE *fp)
{
	u16 i;

#ifdef BIG_ENDIAN
	unsigned char c;
	fread(&c, 1, 1, fp);
	i = c;
	fread(&c, 1, 1, fp);
	i += (c << 8);
#else
	fread(&i, sizeof(u16), 1, fp);
#endif

	return i;
}

u32 read_u32(FILE *fp)
{
	u32 i;

#ifdef BIG_ENDIAN
	unsigned char c;
	fread(&c, 1, 1, fp);
	i = c;
	fread(&c, 1, 1, fp);
	i += (c << 8);
	fread(&c, 1, 1, fp);
	i += (c << 16);
	fread(&c, 1, 1, fp);
	i += (c << 24);
#else
	fread(&i, sizeof(u32), 1, fp);
#endif

	return i;
}

int read_svn(const char *filename)
{
	FILE *fp;
	int n,i,svc_name_len, rf_name_len;
	unsigned char *names, *rf_names;
	u16 radio;
	u16 svcid;
	u16 name_offset;
	u16 rf_offset;
	u16 rfname_offset;
	u16 lcn;
	u32 svc_offset;
	RFInfo *rf, *rf_ptr;
	u16 rf_count;
	
	fp = fopen(filename, "r");
	if(fp == NULL)
		return 0;

//u32 start magic 90 BE 00 10
//u32 number of segments
//u32 segment magic 90 BE 03 01
//u16 segment number
//u32 unknown always 1?

// 0x1e bytes of unknown

	fseek(fp, 0x30, SEEK_SET);
	//read number of channels
	n = read_u16(fp);
	//u16 number of deleted channels
	//u16 total available channels
	fseek(fp, 0x4, SEEK_CUR);
	svc_name_len = read_u32(fp);
	rf_name_len = read_u32(fp);

	names = (unsigned char *)malloc(svc_name_len);
	
	fseek(fp, 0x3e, SEEK_SET);
	
	//read in name data
	fread(names,svc_name_len, 1, fp);

	rf_names = (unsigned char *)malloc(rf_name_len);

	//read in name data
	fread(rf_names,rf_name_len, 1, fp);

//u32 segment magic 90 BE 03 01
//u16 segment number 1
//u32 number of channels

// 0x4e bytes of unknown. Always looks the same.

//13 00 00 04 00 00 01 04 00 00 02 04 00 00 03 04
//00 00 04 08 00 00 05 08 00 00 06 08 00 00 07 08
//00 00 08 08 00 00 09 08 00 00 0A 04 00 00 0B 04
//00 00 0C 08 00 00 0D 10 00 00 0E 10 00 00 0F 08
//00 00 11 08 00 00 10 08 00 00 12 08 00 00 

	//move to start of svcid segment
	fseek(fp, 0x58, SEEK_CUR);
	
	//save the offset for later
	svc_offset = ftell(fp);
	
	//skip the svcid segment
	fseek(fp, 0x24 * n, SEEK_CUR);

//u32 segment magic 90 BE 03 01
//u16 segment number 2

	fseek(fp, 0x6, SEEK_CUR); //move to rf_count
	rf_count = read_u32(fp);
	
	rf = malloc(sizeof(RFInfo) * rf_count);
	
	rf_ptr = rf;

// 0x20 bytes of unknown. Always looks the same.

//00 00 07 00 00 04 00 00 01 04 00 00 02 04 00 00
//03 04 00 00 04 10 00 00 05 08 00 00 06 08 00 00
	
	//move to first rf entry
	fseek(fp, 0x1e, SEEK_CUR);
	
	for(i=0;i < rf_count && !feof(fp);i++)
	{
		rf_ptr->unk1 = read_u16(fp);
		rf_ptr->unk2 = read_u16(fp);
		rf_ptr->rf = read_u32(fp);
		rf_ptr->onid = read_u16(fp);
		rf_ptr->tsid = read_u16(fp);

		rf_ptr++;
	}

	fseek(fp, svc_offset, SEEK_SET);

	printf("lcn,unk1,unk2,mhz,onId,tsId,svcId,radio,rf_name,name\n");

	for(i=0;i<n;i++)
	{
		radio = read_u16(fp);
		fseek(fp, 0x2, SEEK_CUR);
		rf_offset = read_u16(fp); //rf offset
		fseek(fp, 0x2, SEEK_CUR); //skip unknown
		
		svcid = read_u16(fp); //svcid
		
		fseek(fp, 0xa, SEEK_CUR); //0xa bytes of unknown.

		name_offset = read_u16(fp);

		fseek(fp, 0x2, SEEK_CUR); //u16 unknown
		
		rfname_offset = read_u16(fp);

		fseek(fp, 0x6, SEEK_CUR); //u48 unknown

		lcn = read_u16(fp);
		fseek(fp, 0x2, SEEK_CUR); //u16 unknown. skip to end of this record.

		printf("%d,%d,%d,%d,%d,%d,%d,%d,\"%s\",\"%s\"\n", lcn, rf[rf_offset].unk1, rf[rf_offset].unk2, rf[rf_offset].rf, rf[rf_offset].onid, rf[rf_offset].tsid, svcid, radio, &rf_names[rfname_offset], &names[name_offset]);
	}

	free(rf);
	free(names);
	fclose(fp);

	return 1;
}

void print_usage(char *filename)
{
	printf("Usage: %s [filename]\n",filename);
	printf("If filename isn't specified then 'svc.dat' is used.\n");

	return;
}

int main(int argc, char *argv[])
{
	const char *filename = svc_dat;
 
	if(argc > 1)
		filename = argv[1];

	if(read_svn(filename) == 0)
	{
		printf("Error: Opening '%s'\n", filename);
		print_usage(argv[0]);
		return 1;
	}

	return 0;
}
