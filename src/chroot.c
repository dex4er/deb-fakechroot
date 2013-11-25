/*
    libfakechroot -- fake chroot environment
    Copyright (c) 2010, 2013 Piotr Roszatycki <dexter@debian.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
*/


#include <config.h>

#define _GNU_SOURCE
#include <errno.h>
#include <stddef.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include "setenv.h"
#include "libfakechroot.h"

#include "strlcpy.h"
#include "dedotdot.h"

#ifdef HAVE___XSTAT64
# include "__xstat64.h"
#else
# include "stat.h"
#endif

#include "getcwd_real.h"

wrapper(chroot, int, (const char * path))
{
    char *ld_library_path, *separator;
    const char *fakechroot_base = getenv("FAKECHROOT_BASE");
    int status, len;
    char cwd[FAKECHROOT_PATH_MAX];
    char tmp[FAKECHROOT_PATH_MAX], *tmpptr = tmp;
#ifdef HAVE___XSTAT64
    struct stat64 sb;
#else
    struct stat sb;
#endif

    debug("chroot(\"%s\")", path);

    if (path == NULL) {
        __set_errno(EFAULT);
        return -1;
    }
    if (!*path) {
        __set_errno(ENOENT);
        return -1;
    }

    if (getcwd_real(cwd, FAKECHROOT_PATH_MAX) == NULL) {
        __set_errno(ENAMETOOLONG);
        return -1;
    }

    if (fakechroot_base != NULL && strstr(cwd, fakechroot_base) == fakechroot_base) {
        expand_chroot_path(path);
    }
    else {
        if (*path == '/') {
            expand_chroot_rel_path(path);
        }
        else {
            snprintf(tmp, FAKECHROOT_PATH_MAX, "%s/%s", cwd, path);
            dedotdot(tmpptr);
            path = tmpptr;
        }
    }

#ifdef HAVE___XSTAT64
    if ((status = nextcall(__xstat64)(_STAT_VER, path, &sb)) != 0) {
        return status;
    }
#else
    if ((status = nextcall(stat)(path, &sb)) != 0) {
        return status;
    }
#endif

    if ((sb.st_mode & S_IFMT) != S_IFDIR) {
        __set_errno(ENOTDIR);
        return -1;
    }

    if (setenv("FAKECHROOT_BASE", path, 1) == -1) {
        return -1;
    }

    ld_library_path = getenv("LD_LIBRARY_PATH");

    if (ld_library_path != NULL && strlen(ld_library_path) > 0) {
        separator = ":";
    }
    else {
        ld_library_path = "";
        separator = "";
    }

    if ((len = strlen(ld_library_path)+strlen(separator)+strlen(path)*2+sizeof("/usr/lib:/lib")) > FAKECHROOT_PATH_MAX) {
        __set_errno(ENAMETOOLONG);
        return -1;
    }

    snprintf(tmp, len, "%s%s%s/usr/lib:%s/lib", ld_library_path, separator, path, path);
    setenv("LD_LIBRARY_PATH", tmp, 1);
    return 0;
}