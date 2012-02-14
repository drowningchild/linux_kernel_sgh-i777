#include <stdio.h>
#include "../include/generated/autoconf.h"
#include "crc32defs.h"
#include <inttypes.h>

#define ENTRIES_PER_LINE 4

<<<<<<< HEAD
#if CRC_LE_BITS <= 8
#define LE_TABLE_SIZE (1 << CRC_LE_BITS)
#else
#define LE_TABLE_SIZE 256
#endif

#if CRC_BE_BITS <= 8
#define BE_TABLE_SIZE (1 << CRC_BE_BITS)
#else
#define BE_TABLE_SIZE 256
#endif

static uint32_t crc32table_le[8][256];
static uint32_t crc32table_be[8][256];
=======
#if CRC_LE_BITS > 8
# define LE_TABLE_ROWS (CRC_LE_BITS/8)
# define LE_TABLE_SIZE 256
#else
# define LE_TABLE_ROWS 1
# define LE_TABLE_SIZE (1 << CRC_LE_BITS)
#endif

#if CRC_BE_BITS > 8
# define BE_TABLE_ROWS (CRC_BE_BITS/8)
# define BE_TABLE_SIZE 256
#else
# define BE_TABLE_ROWS 1
# define BE_TABLE_SIZE (1 << CRC_BE_BITS)
#endif

static uint32_t crc32table_le[LE_TABLE_ROWS][256];
static uint32_t crc32table_be[BE_TABLE_ROWS][256];
static uint32_t crc32ctable_le[LE_TABLE_ROWS][256];
>>>>>>> 3dc87d1... CRC32 library patched up

/**
 * crc32init_le() - allocate and initialize LE table data
 *
 * crc is the crc of the byte i; other entries are filled in based on the
 * fact that crctable[i^j] = crctable[i] ^ crctable[j].
 *
 */
static void crc32init_le_generic(const uint32_t polynomial,
								 uint32_t (*tab)[256])
{
	unsigned i, j;
	uint32_t crc = 1;
<<<<<<< HEAD

	crc32table_le[0][0] = 0;

	for (i = LE_TABLE_SIZE >> 1; i; i >>= 1) {
		crc = (crc >> 1) ^ ((crc & 1) ? CRCPOLY_LE : 0);
=======
	
	tab[0][0] = 0;
	
	for (i = LE_TABLE_SIZE >> 1; i; i >>= 1) {
		crc = (crc >> 1) ^ ((crc & 1) ? polynomial : 0);
>>>>>>> 3dc87d1... CRC32 library patched up
		for (j = 0; j < LE_TABLE_SIZE; j += 2 * i)
			tab[0][i + j] = crc ^ tab[0][j];
	}
	for (i = 0; i < LE_TABLE_SIZE; i++) {
<<<<<<< HEAD
		crc = crc32table_le[0][i];
		for (j = 1; j < 8; j++) {
			crc = crc32table_le[0][crc & 0xff] ^ (crc >> 8);
			crc32table_le[j][i] = crc;
=======
		crc = tab[0][i];
		for (j = 1; j < LE_TABLE_ROWS; j++) {
			crc = tab[0][crc & 0xff] ^ (crc >> 8);
			tab[j][i] = crc;
>>>>>>> 3dc87d1... CRC32 library patched up
		}
	}
}

static void crc32init_le(void)
{
	crc32init_le_generic(CRCPOLY_LE, crc32table_le);
}

static void crc32cinit_le(void)
{
	crc32init_le_generic(CRC32C_POLY_LE, crc32ctable_le);
}

/**
 * crc32init_be() - allocate and initialize BE table data
 */
static void crc32init_be(void)
{
	unsigned i, j;
	uint32_t crc = 0x80000000;
	
	crc32table_be[0][0] = 0;
	
	for (i = 1; i < BE_TABLE_SIZE; i <<= 1) {
		crc = (crc << 1) ^ ((crc & 0x80000000) ? CRCPOLY_BE : 0);
		for (j = 0; j < i; j++)
			crc32table_be[0][i + j] = crc ^ crc32table_be[0][j];
	}
	for (i = 0; i < BE_TABLE_SIZE; i++) {
		crc = crc32table_be[0][i];
<<<<<<< HEAD
		for (j = 1; j < 8; j++) {
=======
		for (j = 1; j < BE_TABLE_ROWS; j++) {
>>>>>>> 3dc87d1... CRC32 library patched up
			crc = crc32table_be[0][(crc >> 24) & 0xff] ^ (crc << 8);
			crc32table_be[j][i] = crc;
		}
	}
}

<<<<<<< HEAD
static void output_table(uint32_t table[8][256], int len, char trans)
{
	int i, j;

	for (j = 0 ; j < 8; j++) {
		printf("static const u32 t%d_%ce[] = {", j, trans);
=======
static void output_table(uint32_t (*table)[256], int rows, int len, char *trans)
{
	int i, j;
	
	for (j = 0 ; j < rows; j++) {
		printf("{");
>>>>>>> 3dc87d1... CRC32 library patched up
		for (i = 0; i < len - 1; i++) {
			if ((i % ENTRIES_PER_LINE) == 0)
				printf("\n");
			printf("to%ce(0x%8.8xL),", trans, table[j][i]);
			if ((i % ENTRIES_PER_LINE) != (ENTRIES_PER_LINE - 1))
				printf(" ");
		}
		printf("to%ce(0x%8.8xL)};\n\n", trans, table[j][len - 1]);

		if (trans == 'l') {
			if ((j+1)*8 >= CRC_LE_BITS)
				break;
		} else {
			if ((j+1)*8 >= CRC_BE_BITS)
				break;
		}
	}
}

int main(int argc, char** argv)
{
<<<<<<< HEAD
	printf("/*\n");
	printf(" * crc32table.h - CRC32 tables\n");
	printf(" *    this file is generated - do not edit\n");
	printf(" *	# gen_crc32table > crc32table.h\n");
	printf(" *    with\n");
	printf(" *	CRC_LE_BITS = %d\n", CRC_LE_BITS);
	printf(" *	CRC_BE_BITS = %d\n", CRC_BE_BITS);
	printf(" */\n");
	printf("\n");

	if (CRC_LE_BITS > 1) {
		crc32init_le();
		output_table(crc32table_le, LE_TABLE_SIZE, 'l');
=======
	printf("/* this file is generated - do not edit */\n\n");
	
	if (CRC_LE_BITS > 1) {
		crc32init_le();
		printf("static const u32 __cacheline_aligned "
		       "crc32table_le[%d][%d] = {",
		       LE_TABLE_ROWS, LE_TABLE_SIZE);
		output_table(crc32table_le, LE_TABLE_ROWS,
					 LE_TABLE_SIZE, "tole");
		printf("};\n");
>>>>>>> 3dc87d1... CRC32 library patched up
	}
	
	if (CRC_BE_BITS > 1) {
		crc32init_be();
<<<<<<< HEAD
		output_table(crc32table_be, BE_TABLE_SIZE, 'b');
=======
		printf("static const u32 __cacheline_aligned "
		       "crc32table_be[%d][%d] = {",
		       BE_TABLE_ROWS, BE_TABLE_SIZE);
		output_table(crc32table_be, LE_TABLE_ROWS,
					 BE_TABLE_SIZE, "tobe");
		printf("};\n");
>>>>>>> 3dc87d1... CRC32 library patched up
	}
	if (CRC_LE_BITS > 1) {
		crc32cinit_le();
		printf("static const u32 __cacheline_aligned "
		       "crc32ctable_le[%d][%d] = {",
		       LE_TABLE_ROWS, LE_TABLE_SIZE);
		output_table(crc32ctable_le, LE_TABLE_ROWS,
					 LE_TABLE_SIZE, "tole");
		printf("};\n");
	}
	
	return 0;
}
