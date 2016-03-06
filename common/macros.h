#ifndef MACROS_H
#define MACROS_H

#define ARRAY_SIZE(x)     (sizeof(x) / sizeof(*(x)))

#define MIN(x, y)         (((x) < (y)) ? (x) : (y))
#define MAX(x, y)         (((x) > (y)) ? (x) : (y))

#define IS_DIGIT(x)       (((x) >= '0') && ((x) <= '9'))
#define IS_WHITE_SPACE(x) (((x) == ' ') || ((x) == '\t'))

#endif /* MACROS_H */
