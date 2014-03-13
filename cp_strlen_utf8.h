/* 
 * File:   cp_strlen_utf8.h
 * Author: zh99998
 *
 * Created on 2014年3月9日, 上午11:40
 */

#ifndef CP_STRLEN_UTF8_H
#define	CP_STRLEN_UTF8_H

#ifdef	__cplusplus
extern "C" {
#endif

#define ONEMASK ((size_t)(-1) / 0xFF)
#include <stdlib.h>
#include <stdint.h>
static size_t
cp_strlen_utf8(const char * _s);


#ifdef	__cplusplus
}
#endif

#endif	/* CP_STRLEN_UTF8_H */

