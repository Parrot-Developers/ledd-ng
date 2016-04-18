/**
 * @file dl.c
 * @brief reimplementations of libdl functions to allow static linking
 *
 * @date 2 mai 2016
 * @author ncarrier
 * @copyright Copyright (C) 2016 Parrot S.A.
 */
#include <stdlib.h>

#include "dl.h"

void *dlopen(__const char *__file, int __mode)
{
	return (void *)-1;
}

char *dlerror(void)
{
	return "";
}

int dlclose(void *__handle)
{
	return 0;
}
