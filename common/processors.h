#ifndef PROCESSORS_H
#define PROCESSORS_H

#define MAX_PROCESSORS 256

int parse_processors(const char* str,
                     unsigned* processors,
                     unsigned* nprocessors);

#endif /* PROCESSORS_H */
