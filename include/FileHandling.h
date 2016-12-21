#ifndef FILEHANDLING_H
#define FILEHANDLING_H

#include <string.h>
#include <stdio.h>
#include "Controller.h"

fmat ImportAsciiMatrix (const char* fname, int *nrows, int *ncols, int padRows);

#endif
