/**
 * @file dl.h
 * @brief reimplementations of libdl functions to allow static linking
 *
 * @date 2 mai 2016
 * @author ncarrier
 * @copyright Copyright (C) 2016 Parrot S.A.
 */

#ifndef LEDD_SRC_LEDD_DL_H_
#define LEDD_SRC_LEDD_DL_H_

void *dlopen(__const char *__file, int __mode);
char *dlerror(void);
int dlclose(void *__handle);

#endif /* LEDD_SRC_LEDD_DL_H_ */
