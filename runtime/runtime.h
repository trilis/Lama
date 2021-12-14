# ifndef __LAMA_RUNTIME__
# define __LAMA_RUNTIME__

# include <stdio.h>
# include <stdio.h>
# include <string.h>
# include <stdarg.h>
# include <stdlib.h>
# include <sys/mman.h>
# include <assert.h>
# include <errno.h>
# include <regex.h>
# include <time.h>
# include <limits.h>
# include <ctype.h>

# define UNBOXED(x)  (((int) (x)) &  0x0001)
# define UNBOX(x)    (((int) (x)) >> 1)
# define BOX(x)      ((((int) (x)) << 1) | 0x0001)
# define LEN(x) ((x & 0xFFFFFFF8) >> 3)
# define TO_DATA(x) ((data*)((char*)(x)-sizeof(int)))

# define WORD_SIZE (CHAR_BIT * sizeof(int))

typedef struct {
  int tag; 
  char contents[0];
} data; 

void failure (char *s, ...);

# endif
