#include "npk.h"
#include "win_port.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include <magic.h>
#include <zlib.h>

#define _WITH_MAGIC_LIB

#define NXPK_MAGIC "NXPK"
#define NXPK_MAGIC_LENGTH 4

#define DATA_UNIT_BYTES 4
#define FILE_INFO_BLOCK_LENGTH 7

/* Size type */
#define BB_T 0
#define KB_T 1
#define MB_T 2
#define GB_T 3

typedef struct _file_info_block
{
	uint32_t index; // 0
	uint32_t offset; // 1 相对于NPK文件的偏移字节数
	uint32_t length; // 2 数据压缩后字节长度
	uint32_t uncompress_length; // 3 数据原字节长度
	uint32_t unknow_4;
	uint32_t unknow_5;
	uint32_t is_compressed; // 6 是否是压缩的
} file_info_block;

typedef struct _file_size_info
{
	float size;
	enum_t unit;
	enum_t type;
} file_size_info;

/* Size unit */
#define SIZE_UNIT_1000 1000
#define SIZE_UNIT_1024 1024

static enum_t npk_error = NPK_NO_ERROR;
static _Bool npk_dbg = false;

static void print_file_info_block(const file_info_block *block);
static void print_error(enum_t e); 
static file_size_info get_format_size(uint32_t len, enum_t unit);
static const char * get_file_extension(const char *file_out_type, const char *file_out_buf, uint32_t len);
static void set_error(enum_t e);
static _Bool is_npk_file(FILE *file);

void enable(enum_t e)
{
	switch (e)
	{
	case NPK_DEBUG:
		npk_dbg = true;
		break;
	default:
		set_error(INVALID_ENUM);
		break;
	}
}

void disable(enum_t e)
{
	switch (e)
	{
	case NPK_DEBUG:
		npk_dbg = false;
		break;
	default:
		set_error(INVALID_ENUM);
		break;
	}
}

_Bool is_enabled(enum_t e)
{
	switch (e)
	{
	case NPK_DEBUG:
		return npk_dbg;
	default:
		set_error(INVALID_ENUM);
		return false;
	}
}

_Bool unnpk(const char *npk_path, const char *to)
{
#if 0
	char *npk_path = "./ui.npk";
	char *out_path = "./";
#else
	if (!npk_path || !to)
	{
		set_error(MISS_ARGUMENTS_MASK);
		return false;
	}
#endif

	// open npk
	FILE *npk = fopen(npk_path, "rb"); // binary flag for windows
	if (npk == NULL)
	{
		set_error(NPK_FILE_CANNOT_OPEN);
		return false;
	}

	// mkdir for outut
	char *out_path = _strdup(to);
	if (out_path[strlen(out_path) - 1] == '/')
		out_path[strlen(out_path) - 1] = 0;
	if (mkdir_m(out_path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH))
	{
		set_error(MKDIR_FOR_DEST_FAIL);
	}

	if (!is_npk_file(npk))
		set_error(MAY_BE_NOT_NPK_FILE);
#if 0
	else
		printf("this is a npk file.\n");
#endif

	// get npk file size
	fseek(npk, 0L, SEEK_END);
	uint32_t npk_size = ftell(npk);
	printf("npk file size -> %d\n", npk_size);

#if 0
	typedef struct _npk_file
	{
		uint32_t map_offset;
	} npk_file;
#endif

	// get npk map offset
	fseek(npk, 0x14, SEEK_SET);
	uint32_t map_offset;
	fread(&map_offset, DATA_UNIT_BYTES, 1, npk);

	// start
	file_info_block file_info;
	FILE *file_out = NULL;
	char *file_out_name = NULL;
	if (!(file_out_name = malloc(strlen(out_path) + 1 /* / */ + 30 /* file_type */ + 1 /* / */ + 8 /* file_name */ + 20 /* .extension_name */ + 1 /* 0 */)))
	{
		set_error(OUT_OF_MEMORY);
		return false;
	}
	uint8_t *file_read_buf = NULL;
	uint8_t *file_out_buf = NULL;
	uLongf file_destLen = 0;
	char *file_out_type = NULL;      // file type by MIME
	char *file_out_type_p = NULL;    // file directory by MIME
	const char *file_out_extension = NULL; // file extension

	printf("| Index\t\t | Offset\t | Size\t\t | Unzip size\t | zip\t | MIME Type\t | Extension\t |\n| -\t\t | -\t\t | -\t\t | -\t\t | -\t | -\t\t | -\t\t |\n");
	uint32_t block_len = FILE_INFO_BLOCK_LENGTH * DATA_UNIT_BYTES;
	uint32_t file_offset;
	for (file_offset = map_offset; file_offset < npk_size; file_offset += block_len)
	{
		memset(&file_info, 0, sizeof(file_info_block));
		// read map block
		fseek(npk, file_offset, SEEK_SET);
		fread(&file_info, DATA_UNIT_BYTES, FILE_INFO_BLOCK_LENGTH, npk);

		// read data
		if (!(file_read_buf = malloc(file_info.length))) // 2
		{
			set_error(OUT_OF_MEMORY_FOR_READ);
			return false;
		}
		fseek(npk, file_info.offset, SEEK_SET); // 1
		fread(file_read_buf, 1, file_info.length, npk); // 2

		if (file_info.is_compressed)
		{
			// uncompress
			if (!(file_out_buf = malloc(file_info.uncompress_length))) // 3
			{
				set_error(OUT_OF_MEMORY_FOR_WRITE);
				return false;
			}
			file_destLen = file_info.uncompress_length; // 3
			switch (uncompress(file_out_buf, &file_destLen, file_read_buf, file_info.length)) // 2
			{
			case Z_OK:
				//clear
				free(file_read_buf);
				file_read_buf = NULL;
				break;
			case Z_MEM_ERROR:
				set_error(UNCOMPRESS_ERROR_FOR_OUT_OF_MEMORY);
				return false;
				break;
			case Z_BUF_ERROR:
				free(file_out_buf);
				file_out_buf = NULL;
				file_out_buf = file_read_buf;
				file_read_buf = NULL;
				file_info.uncompress_length = file_info.length; // 3 2
				set_error(UNCOMPRESS_ERROR_FOR_MAP_IS_INVALID);
				fprintf(stderr, "Z_BUF_ERROR: The raw data will be output\n");
				break;
			case Z_DATA_ERROR:
				free(file_out_buf);
				file_out_buf = NULL;
				file_out_buf = file_read_buf;
				file_read_buf = NULL;
				file_info.uncompress_length = file_info.length; // 3 2
				set_error(UNCOMPRESS_ERROR_FOR_MAP_IS_INVALID);
				fprintf(stderr, "Z_DATA_ERROR: The raw data will be output\n");
				break;
			}
		}
		else
		{
			if (file_info.length != file_info.uncompress_length) // 2 3
			{
				set_error(NOT_BE_COMPRESSED_DATA_SIZE_IS_NOT_EQUALS_MAP);
			}
			file_out_buf = file_read_buf;
			file_read_buf = NULL;
		}

		//print_file_info_block(&file_info);

		// get file type by MIME
#ifdef _WITH_MAGIC_LIB
		magic_t cookie;
		cookie = magic_open(MAGIC_MIME_TYPE);
		magic_load(cookie, "./share/misc/magic.mgc"); // local magic file
		file_out_type = (char *)magic_buffer(cookie, file_out_buf, file_info.uncompress_length);
#else
		file_out_type = "image/png";
#endif

		// get file extension
		file_out_extension = get_file_extension(file_out_type, file_out_buf, file_info.uncompress_length);
		// make file path
		sprintf(file_out_name, "%s/%s/%08X%s", out_path, file_out_type, file_info.index, file_out_extension);

		// make MIME type directory
		file_out_type_p = file_out_type;
		while ((file_out_type_p = strchr(file_out_type_p, '/') + 1) - 1)
		{
			file_out_name[strlen(out_path) + 1 + (file_out_type_p - 1 - file_out_type)] = 0;
			mkdir_m(file_out_name, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
			file_out_name[strlen(out_path) + 1 + (file_out_type_p - 1 - file_out_type)] = '/';
		}
		file_out_name[strlen(out_path) + 1 + strlen(file_out_type)] = 0;
		mkdir_m(file_out_name, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
		file_out_name[strlen(out_path) + 1 + strlen(file_out_type)] = '/';

		// write data
		file_out = fopen(file_out_name, "wb+"); // wb+ for win
		fwrite(file_out_buf, 1, file_info.uncompress_length, file_out);
		fclose(file_out);

		// print
		file_size_info size_info = get_format_size(file_info.length, SIZE_UNIT_1024);
		file_size_info size_info_u = get_format_size(file_info.uncompress_length, SIZE_UNIT_1024);

#if 1
		printf("| %08X\t | 0x%08X\t | %.3f %s\t | %.3f %s\t | %s\t | %s\t | %s\t |\n",
			file_info.index,
			file_info.offset,
			size_info.size, // 2
			size_info.type == GB_T ? "G" : (size_info.type == MB_T ? "M" : (size_info.type == KB_T ? "K" : "B")), // 2
			size_info_u.size, // 3
			size_info_u.type == GB_T ? "G" : (size_info_u.type == MB_T ? "M" : (size_info_u.type == KB_T ? "K" : "B")), // 3
			file_info.is_compressed ? "Yes" : "No",
			file_out_type,
			strlen(file_out_extension) ? file_out_extension : "None");
#endif

		// clear
		free(file_out_buf);
		file_out_buf = NULL;
#ifdef _WITH_MAGIC_LIB
		magic_close(cookie);
#endif
	}

	// close
	fclose(npk);
	free(file_out_name);
	file_out_name = NULL;

	free(out_path);
	return true;
}

enum_t get_error(void)
{
	enum_t e = npk_error;
	npk_error = NPK_NO_ERROR;
	return e;
}

file_size_info get_format_size(uint32_t len, enum_t unit)
{
	file_size_info info;
	uint32_t u = unit == SIZE_UNIT_1000 ? 1000 : 1024;
	if (len > u * u * u)
	{
		info.type = GB_T;
		info.size = (float)len / (float)(u * u * u);
	}
	else if (len > u * u)
	{
		info.type = MB_T;
		info.size = (float)len / (float)(u * u);
	}
	else if (len > u)
	{
		info.type = KB_T;
		info.size = (float)len / (float)u;
	}
	else
	{
		info.type = BB_T;
		info.size = (float)len;
	}
	return info;
}

const char * get_file_extension(const char *file_out_type, const char *file_out_buf, uint32_t len)
{
	if (!file_out_type || !file_out_buf)
		return ".null";

	char *file_out_extension = NULL;

	if (strstr(file_out_type, "image/png"))
	{
		file_out_extension = ".png";
	}
	else if (strstr(file_out_type, "image/jpeg"))
	{
		file_out_extension = ".jpg";
	}
	else if (strstr(file_out_type, "image/vnd.adobe.photoshop"))
	{
		file_out_extension = ".psd";
	}
	else if (strstr(file_out_type, "video/mp4"))
	{
		file_out_extension = ".mp4";
	}
	else if (strstr(file_out_type, "application/zip"))
	{
		file_out_extension = ".zip";
	}
	else if (strstr(file_out_type, "application/font-sfnt"))
	{
		file_out_extension = ".ttf";
	}
	else if (strstr(file_out_type, "application/vnd.ms-excel"))
	{
		file_out_extension = ".xls";
	}
	else if (strstr(file_out_type, "xml"))
	{
		file_out_extension = ".xml";
	}
	else if (memcmp(file_out_buf + 1, "KTX", 3) == 0)
	{
		file_out_extension = ".ktx";
	}
	else if (memcmp(file_out_buf, "RGIS", 4) == 0)
	{
		file_out_extension = ".RGIS";
	}
	else if (memcmp(file_out_buf, "PKM", 3) == 0)
	{
		file_out_extension = ".PKM";
	}
	else if (strstr(file_out_type, "text"))
	{
		if (memcmp(file_out_buf, "<NeoX", 5) == 0 || memcmp(file_out_buf, "<Neox", 5) == 0)
		{
			file_out_extension = ".NeoX.xml";
		}
		else if (memcmp(file_out_buf, "<FxGroup", 8) == 0)
		{
			file_out_extension = ".FxGroup.xml";
		}
		else if (memcmp(file_out_buf, "<SceneMusic", 11) == 0)
		{
			file_out_extension = ".SceneMusic.xml";
		}
		else if (memcmp(file_out_buf, "<MusicTriggers", 14) == 0)
		{
			file_out_extension = ".MusicTriggers.xml";
		}
		else if (memcmp(file_out_buf, "<cinematic", 10) == 0)
		{
			file_out_extension = ".cinematic.xml";
		}
		else if (memcmp(file_out_buf, "<EquipList", 10) == 0)
		{
			file_out_extension = ".EquipList.xml";
		}
		else if (memcmp(file_out_buf, "<SceneConfig", 12) == 0)
		{
			file_out_extension = ".SceneConfig.xml";
		}
		else if (memcmp(file_out_buf, "<SceneRoad", 12) == 0)
		{
			file_out_extension = ".SceneRoad.xml";
		}
		else if (file_out_buf[0] == '{' && file_out_buf[len - 1] == '}')
		{
			file_out_extension = ".json";
		}
		else if (memmem(file_out_buf, len, "vec4", 4) || memmem(file_out_buf, len, "vec2", 4) || memmem(file_out_buf, len, "tex2D", 5) || memmem(file_out_buf, len, "tex3D", 5) || memmem(file_out_buf, len, "float", 5) || memmem(file_out_buf, len, "define", 5) || memmem(file_out_buf, len, "incloud", 5))
		{
			file_out_extension = ".glsl"; // maybe not a OpenGL shader language source, it also is other shader language source.
		}
		else if (memmem(file_out_buf, len, "v ", 2) && memmem(file_out_buf, len, "vt ", 3) && memmem(file_out_buf, len, "f ", 2))
		{
			file_out_extension = ".obj"; // obj model file
		}
		else
		{
			file_out_extension = ".txt";
		}
	}
	else
	{
		file_out_extension = "";
	}

	return file_out_extension;
}

void print_error(enum_t e)
{
	switch (e)
	{
	case NPK_NO_ERROR:
		fprintf(stdout, "INFO -> no error.\n");
		break;
	case NPK_OTHER_ERROR:
		fprintf(stderr, "ERROR -> other error.\n");
		break;
	case MISS_ARGUMENTS_MASK:
		fprintf(stderr, "ERROR -> missing arguments.\n");
		break;
	case NPK_FILE_CANNOT_OPEN:
		fprintf(stderr, "ERROR -> npk file can not open.\n");
		break;
	case MKDIR_FOR_DEST_FAIL:
		fprintf(stderr, "ERROR -> make dest directory fail.\n");
		break;
	case MKDIR_FOR_DEST_SUB_FAIL:
		fprintf(stderr, "ERROR -> make dest sub directory fail.\n");
		break;
	case CANNOT_WRITE_DEST_FILE:
		fprintf(stderr, "ERROR -> can not write dset file.\n");
		break;
	case OUT_OF_MEMORY:
		fprintf(stderr, "ERROR -> out of memory.\n");
		break;
	case OUT_OF_MEMORY_FOR_READ:
		fprintf(stderr, "ERROR -> out of memory when read file.\n");
		break;
	case OUT_OF_MEMORY_FOR_WRITE:
		fprintf(stderr, "ERROR -> out of memory when write file.\n");
		break;
	case UNCOMPRESS_ERROR_FOR_OUT_OF_MEMORY:
		fprintf(stderr, "ERROR -> out of memory uncompress block.\n");
		break;
	case UNCOMPRESS_ERROR_FOR_MAP_IS_INVALID:
		fprintf(stderr, "ERROR -> map is invalid when uncompress block.\n");
		break;
	case UNCOMPRESS_ERROR_FOR_DATA_IS_NOT_Z:
		fprintf(stderr, "ERROR -> data is not z when uncompress block.\n");
		break;
	case NOT_BE_COMPRESSED_DATA_SIZE_IS_NOT_EQUALS_MAP:
		fprintf(stderr, "ERROR -> data is not be compressed, but size is not equals map size.\n");
		break;
	case MAY_BE_NOT_NPK_FILE:
		fprintf(stderr, "ERROR -> this file may be not a npk.\n");
		break;

	case INVALID_ENUM:
		fprintf(stderr, "ERROR -> invalid enum.\n");
		break;
	default:
		break;
	}
}

void print_file_info_block(const file_info_block *block) 
{
	if (!block)
		return;
	printf("BLOCK(%d) - OFFSET(%d) - LENGTH(%d) - UNCOMPRESS_LENGTH(%d) - UNKNOW_4(%d) - UNKNOW_5(%d) - IS_COMPRESSED(%s)\n",
		block->index, block->offset, block->length, block->uncompress_length, block->unknow_4, block->unknow_5, block->is_compressed ? "true" : "false");
}

void set_error(enum_t e)
{
	npk_error = e;
	if (npk_dbg)
		print_error(npk_error);
}

_Bool is_npk_file(FILE *file)
{
	if (!file)
		return false;
	char npk_magic[NXPK_MAGIC_LENGTH];
	memset(npk_magic, 0, NXPK_MAGIC_LENGTH);
	long pos = ftell(file);
	fseek(file, 0L, SEEK_SET);
	fread(npk_magic, 1, NXPK_MAGIC_LENGTH, file);
	fseek(file, pos, SEEK_SET);
	return(memcmp(npk_magic, NXPK_MAGIC, NXPK_MAGIC_LENGTH) == 0);
}