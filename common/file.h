#ifndef FILE_H
#define FILE_H

#include <stdint.h>

#define DUMMY_FILESIZE 1400

typedef struct {
  uint8_t* data;
  size_t len;
} file_t;

typedef struct {
  file_t* files;
  size_t size;
  size_t used;
} files_t;

void files_init(files_t* files);
void files_free(files_t* files);

int files_add(files_t* files, const char* filename);
int files_add_dummy(files_t* files);

#endif /* FILE_H */
