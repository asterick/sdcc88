/** @file support.h
    Support functions for the S1C88 port.
*/
#ifndef S1C88_SUPPORT_INCLUDE
#define S1C88_SUPPORT_INCLUDE

typedef unsigned short WORD;
typedef unsigned char BYTE;

typedef struct
  {
    WORD w[2];
    BYTE b[4];
  }
S1C88_FLOAT;

/** Convert a native float into the target float format */
int s1c88_convertFloat (S1C88_FLOAT * f, double native);

#endif
