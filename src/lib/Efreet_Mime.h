/* vim: set sw=4 ts=4 sts=4 et: */
#ifndef EFREET_MIME_H
#define EFREET_MIME_H

/**
 * @file Efreet_Mime.h 
 * @brief The file that must be included by any project wishing to use
 * Efreet_Mime. Efreet_Mime.h provides all of the necessary headers and 
 * includes to work with Efreet_Mime.
 */

/**
 * @mainpage The Efreet Mime Library
 *
 * @section intro Introduction
 *
 * Efreet Mime is a library designed to help apps work with the 
 * Freedesktop.org Shared Mime Info standard.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @{
 */

enum Efreet_Mime_Method
{
    EFREET_MIME_FLAG_GLOB  = 1,
    EFREET_MIME_FLAG_MAGIC = 2,
    EFREET_MIME_FLAG_TYPES = 4
};
typedef enum Efreet_Mime_Method Efreet_Mime_Method;

int         efreet_mime_init(void);
void        efreet_mime_shutdown(void);

const char *efreet_mime_get(const char *file, int flags);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
