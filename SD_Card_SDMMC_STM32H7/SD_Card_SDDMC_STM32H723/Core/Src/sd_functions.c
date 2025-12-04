#include "fatfs.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "bsp_driver_sd.h"

extern char SDPath[4];
FATFS fs;
BSP_SD_CardInfo myCardInfo;

/***************************************************************
 * Get the total and free space of the SD card in KB
 * Uses FatFs f_getfree to calculate available clusters
 * Prints the information to the console
 ***************************************************************/

int sd_get_space_kb(void) {
	FATFS *pfs;
	DWORD fre_clust, tot_sect, fre_sect, total_kb, free_kb;
	FRESULT res = f_getfree(SDPath, &fre_clust, &pfs);
	if (res != FR_OK) return res;

	 // Calculate total sectors and free sectors
	tot_sect = (pfs->n_fatent - 2) * pfs->csize;
	fre_sect = fre_clust * pfs->csize;

	// Convert to KB
	total_kb = tot_sect / 2;
	free_kb = fre_sect / 2;
	printf("💾 Total: %lu KB, Free: %lu KB\r\n", total_kb, free_kb);
	return FR_OK;
}

/***************************************************************
 * Mount the SD card filesystem
 * Uses f_mount to mount the SD card
 * Prints capacity, free space, card type, version, and class
 ***************************************************************/

int sd_mount(void) {
	FRESULT res;

	printf("Attempting mount at %s...\r\n", SDPath);
	res = f_mount(&fs, SDPath, 1);
	if (res == FR_OK)
	{
		printf("SD card mounted successfully at %s\r\n", SDPath);

		// Capacity and free space reporting
		sd_get_space_kb();

		// Get Card Info
		BSP_SD_GetCardInfo(&myCardInfo);
		printf("Card Type: %s\r\n", myCardInfo.CardType ? "SDSC" : "SDHC/SDXC");
		printf("Card Version: %s\r\n", myCardInfo.CardVersion ? "CARD_V1_X" : "CARD_V2_X");
		printf("Card Class: %lu\r\n", myCardInfo.Class);
		return FR_OK;
	}

	// Any other mount error
	printf("Mount failed with code: %d\r\n", res);
	return res;
}

/***************************************************************
 * Unmount the SD card
 * Calls f_mount with NULL to unmount
 * Prints success/failure status
 ***************************************************************/

int sd_unmount(void) {
	FRESULT res = f_mount(NULL, SDPath, 1);
	printf("SD card unmounted: %s\r\n\r\n\r\n", (res == FR_OK) ? "OK" : "Failed");
	return res;
}

/***************************************************************
 * Write text to a file (overwrite if exists)
 * Opens the file with FA_CREATE_ALWAYS | FA_WRITE
 * Writes the text and closes the file
 * Prints the number of bytes written
 ***************************************************************/

int sd_write_file(const char *filename, const char *text) {
	FIL file;
	UINT bw;

	// Open file for writing
	FRESULT res = f_open(&file, filename, FA_CREATE_ALWAYS | FA_WRITE);
	if (res != FR_OK) return res;

	// Write data using f_write
	res = f_write(&file, text, strlen(text), &bw);
	f_close(&file);
	printf("Write %u bytes to %s\r\n", bw, filename);
	return (res == FR_OK && bw == strlen(text)) ? FR_OK : FR_DISK_ERR;
}

/***************************************************************
 * Append text to an existing file
 * Opens file with FA_OPEN_ALWAYS | FA_WRITE
 * Moves the file pointer to the end
 * Writes new text and closes the file
 * Prints number of bytes appended
 ***************************************************************/

int sd_append_file(const char *filename, const char *text) {
	FIL file;
	UINT bw;

	// Open file for append
	FRESULT res = f_open(&file, filename, FA_OPEN_ALWAYS | FA_WRITE);
	if (res != FR_OK) return res;

	// Move pointer to end using f_lseek
	res = f_lseek(&file, f_size(&file));
	if (res != FR_OK) {
		f_close(&file);
		return res;
	}

	// Write new data
	res = f_write(&file, text, strlen(text), &bw);
	f_close(&file);
	printf("Appended %u bytes to %s\r\n", bw, filename);
	return (res == FR_OK && bw == strlen(text)) ? FR_OK : FR_DISK_ERR;
}

/***************************************************************
 * Read data from a file into a buffer
 * Opens file for reading
 * Reads up to bufsize-1 bytes
 * Null-terminates the buffer
 * Prints number of bytes read
 ***************************************************************/

int sd_read_file(const char *filename, char *buffer, UINT bufsize, UINT *bytes_read) {
	FIL file;
	*bytes_read = 0;

	// Open file for reading
	FRESULT res = f_open(&file, filename, FA_READ);
	if (res != FR_OK) {
		printf("f_open failed with code: %d\r\n", res);
		return res;
	}

	// Read file content using f_read
	res = f_read(&file, buffer, bufsize - 1, bytes_read);
	if (res != FR_OK) {
		printf("f_read failed with code: %d\r\n", res);
		f_close(&file);
		return res;
	}

	// Null-terminate buffer
	buffer[*bytes_read] = '\0';

	res = f_close(&file);
	if (res != FR_OK) {
		printf("f_close failed with code: %d\r\n", res);
		return res;
	}

	printf("Read %u bytes from %s\r\n", *bytes_read, filename);
	return FR_OK;
}

// we can add new field
typedef struct CsvRecord {
	char field1[32];
	char field2[32];
	int value;
} CsvRecord;

/***************************************************************
 * Read CSV file into an array of CsvRecord structures
 * Parses each line into fields separated by commas
 * Stores values in CsvRecord array
 * Prints parsed CSV content
 ***************************************************************/

int sd_read_csv(const char *filename, CsvRecord *records, int max_records, int *record_count) {
	FIL file;
	char line[128];
	*record_count = 0;

	// Open CSV file
	FRESULT res = f_open(&file, filename, FA_READ);
	if (res != FR_OK) {
		printf("Failed to open CSV: %s (%d)", filename, res);
		return res;
	}
	printf("📄 Reading CSV: %s\r\n", filename);

	// Loop through lines with f_gets
	while (f_gets(line, sizeof(line), &file) && *record_count < max_records) {
		char *token = strtok(line, ",");

		// Tokenize line using strtok
		if (!token) continue;

		// Copy tokens into record fields
		strncpy(records[*record_count].field1, token, sizeof(records[*record_count].field1));

		token = strtok(NULL, ",");
		if (!token) continue;

		// Copy tokens into record fields
		strncpy(records[*record_count].field2, token, sizeof(records[*record_count].field2));

		token = strtok(NULL, ",");
		if (token)

			// Convert numeric field with atoi
			records[*record_count].value = atoi(token);
		else
			records[*record_count].value = 0;

		(*record_count)++;
	}

	f_close(&file);

	// Print parsed data
	for (int i = 0; i < *record_count; i++) {
		printf("[%d] %s | %s | %d", i,
				records[i].field1,
				records[i].field2,
				records[i].value);
	}

	return FR_OK;
}

/***************************************************************
 * Delete a file from the SD card
 * Uses f_unlink
 * Prints success/failure message
 ***************************************************************/

int sd_delete_file(const char *filename) {
	FRESULT res = f_unlink(filename);
	printf("Delete %s: %s\r\n", filename, (res == FR_OK ? "OK" : "Failed"));
	return res;
}

/***************************************************************
 * Rename a file on the SD card
 * Uses f_rename
 * Prints success/failure message
 ***************************************************************/

int sd_rename_file(const char *oldname, const char *newname) {
	FRESULT res = f_rename(oldname, newname);
	printf("Rename %s to %s: %s\r\n", oldname, newname, (res == FR_OK ? "OK" : "Failed"));
	return res;
}

/***************************************************************
 * Create a directory on the SD card
 * Uses f_mkdir
 * Prints success/failure message
 ***************************************************************/

FRESULT sd_create_directory(const char *path) {
	FRESULT res = f_mkdir(path);
	printf("Create directory %s: %s\r\n", path, (res == FR_OK ? "OK" : "Failed"));
	return res;
}

/***************************************************************
 * Recursively list files and folders starting from a path
 * Uses f_opendir and f_readdir
 * Prints directory tree with indentation
 ***************************************************************/

void sd_list_directory_recursive(const char *path, int depth) {
	DIR dir;
	FILINFO fno;
//	char lfn[256];
//	fno.fname = lfn;
//	fno.fsize = sizeof(lfn);

	// Open directory
	FRESULT res = f_opendir(&dir, path);
	if (res != FR_OK) {
		printf("%*s[ERR] Cannot open: %s\r\n", depth * 2, "", path);
		return;
	}

	while (1) {

		// Loop through entries with f_readdir
		res = f_readdir(&dir, &fno);
		if (res != FR_OK || fno.fname[0] == 0) break;

		const char *name = (*fno.fname) ? fno.fname : fno.fname;

		// If entry is directory, call recursively
		if (fno.fattrib & AM_DIR) {
			if (strcmp(name, ".") && strcmp(name, "..")) {
				printf("%*s📁 %s\r\n", depth * 2, "", name);
				char newpath[128];
				snprintf(newpath, sizeof(newpath), "%s/%s", path, name);

				// call recursively
				sd_list_directory_recursive(newpath, depth + 1);
			}
		} else {
			// If entry is file, print file info
			printf("%*s📄 %s (%lu bytes)\r\n", depth * 2, "", name, (unsigned long)fno.fsize);
		}
	}
	f_closedir(&dir);
}

/***************************************************************
 * List all files and folders on SD card
 * Calls recursive directory listing starting from root
 ***************************************************************/

void sd_list_files(void) {
	// Print header
	printf("📂 Files on SD Card:\r\n");

	sd_list_directory_recursive(SDPath, 0);
	printf("\r\n\r\n");
}
