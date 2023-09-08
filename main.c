#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <filesystem.h>
#include <array.h>
#include <types.h>
#include <strvec.h>
#include <extra.h>

#define die(msg, ...)					\
	do {						\
		fprintf(stderr, msg, ##__VA_ARGS__);	\
		exit(1);				\
	} while(0);					\

struct section {
	usize len_path;
	usize len_data;
};

struct file {
	void *data;
	usize size;
};

usize num_lines(char *buf)
{
	usize num = 0;
	while (*buf != '\0') {
		if (*buf == '\n')
			num++;
		buf++;
	}

	return num;
}

#define MAX_PATH_SIZE 4096
char tmp[MAX_PATH_SIZE];
char *encode_file_path = "encoded.buf";

char* read_line_to_buffer(char *src, char *buf, usize buf_size)
{
	char *beginning = src;
	while (--buf_size && *src != '\n')
		src++;

	memcpy(buf, beginning, (usize)ABS((src - beginning)));
	return ++src;
}

void encode(char *file_path)
{
	usize num_files;
	char *ptr;
	char *env;
	struct fs_file file = fs_file_read(file_path, FS_READ_TEXT);
	struct strvec files;
	struct strvec files_absolute;

	if (file.data == NULL || file.size == 0)
		die("Unable to read file\n");

	num_files= num_lines(file.data);
	ptr = file.data;

	if (!strvec_init(&files))
		die("Unable to initialize files");

	/* Add files to vector */
	for (int i = 0; i < num_files; i++) {
		memset(tmp, 0, MAX_PATH_SIZE);
		ptr = read_line_to_buffer(ptr, tmp, MAX_PATH_SIZE);
		strvec_push(&files, tmp);
	}

	env = getenv("HOME");
	if (!strvec_init(&files_absolute))
		die("Unable to init strvec");

	/* Get absolute paths to the file, replacing $HOMEDIR to read files */
	for (int i = 0; i < num_files; i++) {
		char *path;
		path = strvec_get(&files, i);

		/* Replace homedir with enviroment variable */
		if ((strlen(path) > strlen("$HOMEDIR")) && (memcmp(path, "$HOMEDIR", 8) == 0)) {
			path += strlen("$HOMEDIR");
			char *absolute_path = xasprintf("%s%s", env, path);
			strvec_push(&files_absolute, absolute_path);
		} else {
			strvec_push(&files_absolute, path);
		}
	}

	FILE *file_encoded = fopen(encode_file_path, "a+");
	usize file_size;
	fseek(file_encoded, 0, SEEK_END);
	file_size = ftell(file_encoded);
	fseek(file_encoded, 0, SEEK_SET);

	if (file_size != 0) {
		printf("encoded.buf is not empty, do you want to erease all data? [y/N] ");
		char line[2];
		char c;

		if (!fgets(line, sizeof(line), stdin))
			die("Unable to get input\n");

		if (sscanf(line, "%c", &c) != 1)
			die("Invalid input\n");

		if (c == 'y' || c == 'Y') {
			FILE *tmpf = fopen(encode_file_path, "w");
			fclose(tmpf);
		} else {
			exit(0);
		}
	}


	if (file_encoded == NULL)
		die("Unable to open file: %s\n", encode_file_path);

	for (int i = 0; i < num_files; i++) {
		char *path;
		struct fs_file file;
		struct section header;

		path = strvec_get(&files_absolute, i);
		file = fs_file_read(path, FS_READ_BINARY);

		if (file.data == NULL || file.size == 0)
			die("Unable to read file: %s\n", path);

		header.len_path = strlen(path);
		header.len_data = file.size;

		fwrite(&header, 1, sizeof(header), file_encoded);
		fwrite(path, 1, strlen(path), file_encoded);
		fwrite(file.data, 1, file.size, file_encoded);
	}

	/*
	 * for (int i = 0; i < num_files; i++)
	 * 	printf("%s", strvec_get(&files, i));
	 */

	fclose(file_encoded);
}

void decode(char *file_path)
{
	struct fs_file file;
	char *env;

	file = fs_file_read(file_path, FS_READ_BINARY);
	if (file.data == NULL || file.size < sizeof(usize) * 2)
		die("Unable to read file: %s\n", file_path);

	env = getenv("HOME");
	/*
	 * if (env == NULL) {
	 * 	printf("HOME variable is not set, you will be writing the data to the root directory, are you sure? [y/N] ");
	 * }
	 */

	for (;;) {
		/* Check if we have 2 more size_t to read for header */
		if (file.size <= (sizeof(usize) * 2))
			break;

		usize len_path = *(usize*)file.data;
		usize len_data = *(usize*)((char*)(file.data + sizeof(usize)));
		char *path = file.data + (2 * sizeof(usize));
		void *data = path + len_path;
		usize num_bytes_to_next_header;
		FILE *destfs;
		usize destfs_size;


		memset(tmp, 0, MAX_PATH_SIZE);
		memcpy(tmp, path, len_path);

		/*
		 * printf("len_path: %zu, len_data: %zu\n", len_path, len_data);
		 * printf("path: %s\n", tmp);
		 * printf("data: %.*s\n", (int)len_data, (char*)data);
		 */

		destfs = fopen(tmp, "w+");
		if (destfs == NULL)
			die("Unable to read file: %s: %s\n", tmp, strerror(errno));

		fwrite(data, 1, len_data, destfs);

		fclose(destfs);

		num_bytes_to_next_header = len_data + len_path + (2 * sizeof(usize));
		file.size -= num_bytes_to_next_header;
		file.data += num_bytes_to_next_header;
	}
}

int main(int argc, char **argv)
{
	if (argc < 2)
		die("Not enough arguments\n");

	if (strcmp(argv[1], "encode") == 0)
		encode(argv[2]);
	else if (strcmp(argv[1], "decode") == 0)
		decode(argv[2]);
	else
		die("Bad argument\n");


	return 0;
}
