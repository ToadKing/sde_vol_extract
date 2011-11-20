/**
 * sde_vol_extract.c - a simple extractor of .vol files found in Sierra's
 * Driver's Education '98/'99. While some file extractors work for .vol files
 * found in other games (like Tribes), there do not seem to be any for the
 * different format found in those two games.
 *
 * Copyright (c) 2011 Michael Lelli
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#define VOL_TYPE_FILE 0x00000080
#define VOL_TYPE_DIR 0x00000010

#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>
#include <direct.h>

typedef unsigned __int8 u8;
typedef unsigned __int16 u16;
typedef unsigned __int32 u32;

#pragma pack(push,1)

// all values are little-endian

// 26 + string length entry items, comments document my observations of the values
struct _entry
{
	u32 type; // 0x80 for file, 0x10 for directory
	u32 w1; // usually 1 for files, 0 for directories
	u32 length; // length of file, 0 for directories
	u32 offset; // offset of file, need to add 4 bytes to compensate for offset at start, 0 for directories
	u32 ff1; // usually 0xFFFFFFFF for files, current item count for directories
	u32 ff2; // usually current item count for files, 0xFFFFFFFF for directories, sometimes 0xFFFFFFFF if last file in subdirectory
	u16 nameLength; // name of file, including relative path
	char *name; // not null-terminated
};

// there is a relisting of all the file names after this, but they are not needed for extraction purposes

#pragma pack(pop)

typedef struct _entry entry;

int main(int argc, char *argv[])
{
	FILE *f = NULL;
	FILE *out = NULL;
	u32 i;
	u32 offset;
	u32 count;
	entry *entries;
	int r = 0;

	if (argc > 2)
	{
		f = fopen(argv[1], "rb");

		if (f == NULL)
		{
			printf("file can't be opened\n");
			goto error;
		}

		out = fopen(argv[2], "w");

		if (out == NULL)
		{
			printf("log can't be opened for writin\n");
			goto error;
		}
	}
	else if (argc > 1)
	{
		f = fopen(argv[1], "rb");

		if (f == NULL)
		{
			printf("file can't be opened\n");
			goto error;
		}

		out = stdout;
	}
	else
	{
		printf("usage: %s file.vol [log.txt]\n(files are not dumped if log is enabled)\n", argv[0]);
		goto error;
	}

	fread(&offset, 4, 1, f);

	if (offset == 0x4C4F5650 /* "PVOL" */)
	{
		printf("this looks like a PVOL file, try a program that supports other .vol files\n");
		goto error;
	}

	fprintf(out, "offset:     0x%08X\n", offset);
	fseek(f, offset, SEEK_SET);

	if (fread(&count, 4, 1, f) != 1)
	{
		printf("file incorrect format\n");
		goto error;
	}

	fprintf(out, "count:      0x%08X\n", count);

	entries = malloc(sizeof(entry) * count);;

	for(i = 0; i < count; i++)
	{
		char *newname;

		entry e = entries[i];

		if (fread(&e, 0x1a, 1, f) != 1)
		{
			break;
		}

		if (e.nameLength > 4096)
		{
			break;
		}

		newname = malloc(sizeof(char) * (e.nameLength + 1));

		if (fread(newname, e.nameLength, 1, f) != 1)
		{
			break;
		}

		e.name = newname;
		newname[e.nameLength] = 0;

		fprintf(out, "\ntype:       0x%08X (%s)\n", e.type, (e.type == VOL_TYPE_FILE ? "file" : (e.type == VOL_TYPE_DIR ? "dir" : "???")));
		fprintf(out, "w1:         0x%08X\n", e.w1);
		fprintf(out, "length:     0x%08X\n", e.length);
		fprintf(out, "offset:     0x%08X\n", e.offset);
		fprintf(out, "ff1:        0x%08X\n", e.ff1);
		fprintf(out, "ff2:        0x%08X\n", e.ff2);
		fprintf(out, "nameLength: 0x%04X\n", e.nameLength);
		fprintf(out, "name:       \"%s\"\n", e.name);

		if (e.type == VOL_TYPE_FILE && argc == 2)
		{
			int currPos = ftell(f);
			u8 *buffer = malloc(sizeof(u8) * e.length);
			FILE *sav;
			fseek(f, e.offset + 0x04, SEEK_SET);
			fread(buffer, e.length, 1, f);
			fseek(f, currPos, SEEK_SET);

			sav = fopen(e.name, "wb");
			fwrite(buffer, e.length, 1, sav);
			fclose(sav);
			free(buffer);
		}
		else if (argc == 2 && !(e.nameLength == 1 && e.name[0] == '.')) // the first item is usually the current directory
		{
			_mkdir(e.name);
		}
	}

end:
	if (f != NULL)
	{
		fclose(f);
	}

	if (out != NULL && out != stdout)
	{
		fclose(out);
	}

	return r;

error:
	r = 1;
	goto end;
}
