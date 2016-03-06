#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "common/file.h"

static int allocate(files_t* files);

void files_init(files_t* files)
{
  files->files = NULL;
  files->size = 0;
  files->used = 0;
}

void files_free(files_t* files)
{
  size_t i;

  if (files->files) {
    for (i = 0; i < files->used; i++) {
      free(files->files[i].data);
    }

    free(files->files);
    files->files = NULL;
  }

  files->size = 0;
  files->used = 0;
}

int files_add(files_t* files, const char* filename)
{
  struct stat buf;
  uint8_t* data;
  uint8_t* p;
  int fd;

  if ((stat(filename, &buf) < 0) || (!S_ISREG(buf.st_mode))) {
    return -1;
  }

  if (allocate(files) < 0) {
    return -1;
  }

  if ((fd = open(filename, O_RDONLY)) < 0) {
    return -1;
  }

  if ((p = mmap(NULL,
                buf.st_size,
                PROT_READ,
                MAP_SHARED,
                fd,
                0)) == MAP_FAILED) {
    close(fd);
    return -1;
  }

  if ((data = (uint8_t*) malloc(buf.st_size)) == NULL) {
    munmap(p, buf.st_size);
    close(fd);

    return -1;
  }

  memcpy(data, p, buf.st_size);

  munmap(p, buf.st_size);
  close(fd);

  files->files[files->used].data = data;
  files->files[files->used++].len = buf.st_size;

  return 0;
}

int files_add_dummy(files_t* files)
{
  uint8_t* data;

  if (allocate(files) < 0) {
    return -1;
  }

  if ((data = (uint8_t*) malloc(DUMMY_FILESIZE)) == NULL) {
    return -1;
  }

  memset(data, 'A', DUMMY_FILESIZE);

  files->files[files->used].data = data;
  files->files[files->used++].len = DUMMY_FILESIZE;

  return 0;
}

int allocate(files_t* files)
{
  file_t* tmpfiles;
  size_t size;

  if (files->used == files->size) {
    size = (files->size == 0) ? 4 : files->size * 2;

    if ((tmpfiles = (file_t*) realloc(files->files,
                                      size * sizeof(file_t))) == NULL) {
      return -1;
    }

    files->files = tmpfiles;
    files->size = size;
  }

  return 0;
}
