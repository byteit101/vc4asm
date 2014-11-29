#include "Disassembler.h"
#include "Validator.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <getopt.h>
#include <vector>
#include <sys/param.h>

using namespace std;

int hexinput = 0;


/// Byte swap
static inline uint64_t swap_uint64(uint64_t x)
{	x = x << 32 | x >> 32;
	x = (x & 0x0000FFFF0000FFFFULL) << 16 | (x & 0xFFFF0000FFFF0000ULL) >> 16;
	return (x & 0x00FF00FF00FF00FFULL) << 8  | (x & 0xFF00FF00FF00FF00ULL) >> 8;
}

static void file_load_bin(const char *filename, vector<uint64_t>& memory)
{	FILE *f = fopen(filename, "rb");
	if (!f)
	{	fprintf(stderr, "Failed to read %s: %s\n", filename, strerror(errno));
		return;
	}
	if (fseek(f, 0, SEEK_END) == 0)
	{	long size = ftell(f);
		if (size % sizeof(uint64_t))
			fprintf(stderr, "File size %li of source %s is not a multiple of 64 bit.\n", size, filename);
		size /= sizeof(uint64_t);
		size_t oldsize = memory.size();
		if (size > 0)
			memory.resize(oldsize + size);
		fseek(f, 0, SEEK_SET);
		memory.resize(oldsize + fread(&memory[oldsize], size, sizeof(uint64_t), f));
	} else
	{	size_t count = 8192;
		for (;;)
		{	size_t oldsize = memory.size();
			memory.resize(oldsize + count);
			count = fread(&memory[oldsize], 8192, sizeof(uint64_t), f);
			if (count < 8192)
			{	memory.resize(oldsize + count);
				break;
			}
		}
	}
	fclose(f);
	#if (defined(__BIG_ENDIAN__) && __BIG_ENDIAN__) || (defined(__BYTE_ORDER) && __BYTE_ORDER == __BIG_ENDIAN)
	for (auto& i : memory)
		i = swap_uint64(i);
	#endif
}

static void file_load_x32(const char *filename, vector<uint64_t>& memory)
{	FILE *f = fopen(filename, "r");
	if (!f)
	{	fprintf(stderr, "Failed to read %s: %s\n", filename, strerror(errno));
		return;
	}
	uint32_t value1, value2;
	while (fscanf(f, "%x,", &value1) == 1)
	{	if (fscanf(f, "%x,", &value2) != 1)
		{	if (feof(f))
			{	fprintf(stderr, "File %s must contain an even number of 32 bit words.\n", filename);
				goto done;
			} else
				break;
		}
		memory.push_back(value1 | (uint64_t)value2 << 32);
	}
	if (!feof(f))
	{	char buf[10];
		*buf = 0;
		fscanf(f, "%9s", buf);
		buf[9] = 0;
		fprintf(stderr, "File %s contains not parsable input %s.\n", filename, buf);
	}
 done:
	fclose(f);
}

static void file_load_x64(const char *filename, vector<uint64_t>& memory)
{	FILE *f = fopen(filename, "r");
	if (!f)
	{	fprintf(stderr, "Failed to read %s: %s", filename, strerror(errno));
		return;
	}
	uint64_t value;
	while (fscanf(f, "%Lx,", &value) == 1)
		memory.push_back(value);
	if (!feof(f))
	{	char buf[10];
		*buf = 0;
		fscanf(f, "%9s", buf);
		buf[9] = 0;
		fprintf(stderr, "File %s contains not parsable input %s.\n", filename, buf);
	}
	fclose(f);
}

int main(int argc, char * argv[]) {
	if (argc < 2) {
    fprintf(stderr, "vc4dis: Pass in a file name to disassemble as the first argument\n");
    return 1;
  }

	Disassembler dis;
	dis.Out = stdout;
	bool check = false;

	int c;
	while ((c = getopt(argc, argv, "x::MFvb:o:C")) != -1)
	{	switch (c)
		{case 'x':
			if (!optarg || (hexinput = atoi(optarg + 2) == 0))
				hexinput = 32;
			break;
		 case 'M':
			dis.UseMOV = false; break;
		 case 'F':
			dis.UseFloat = false; break;
		 case 'v':
			dis.PrintFields = true; break;
		 case 'b':
			dis.BaseAddr = atol(optarg); break;
		 case 'o':
			dis.Out = fopen(optarg, "w");
			if (!dis.Out)
			{	fprintf(stderr, "Failed to write %s: %s\n", optarg, strerror(errno));
				return 1;
			}
			break;
		 case 'C':
			check = true; break;
		}
	}
	argv += optind;

	while (*argv)
	{	fprintf(stderr, "Disassembling %s...\n", *argv);
		switch (hexinput)
		{default:
			fprintf(stderr, "Invalid argument %i to option -x.", hexinput);
			return 2;
		 case 32:
			file_load_x32(*argv, dis.Instructions);
			break;
		 case 64:
			file_load_x64(*argv, dis.Instructions);
			break;
		 case 0:
			file_load_bin(*argv, dis.Instructions);
			break;
		}
		if (dis.Instructions.size() == 0)
		{	fprintf(stderr, "Couldn't read any data from file %s, aborting.\n", *argv);
			continue;
		}
		dis.ScanLabels();
		dis.Disassemble();

		if (check)
			Validator().Validate(dis.Instructions);

		++argv;
	}
}
