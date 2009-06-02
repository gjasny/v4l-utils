/*-
 * Copyright (c) 2009 Hans Petter Selasky. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * The following file allows for having the complete V4L stack and
 * hardware drivers in userland.
 */

#ifndef _LIBV4LSYSCALL_PRIV_H_
#define _LIBV4LSYSCALL_PRIV_H_

/* Some of these headers are not needed by us, but by linux/videodev2.h,
   which is broken on some systems and doesn't include them itself :( */

#ifdef linux
#include <sys/time.h>
#include <syscall.h>
#include <linux/types.h>
#include <linux/ioctl.h>
/* On 32 bits archs we always use mmap2, on 64 bits archs there is no mmap2 */
#ifdef __NR_mmap2
#define	SYS_mmap2 __NR_mmap2
#define	MMAP2_PAGE_SHIFT 12
#else
#define	SYS_mmap2 SYS_mmap
#define	MMAP2_PAGE_SHIFT 0
#endif
#endif

#ifdef __FreeBSD__
#include <sys/time.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#define	_IOC_NR(cmd) ((cmd) & 0xFF)
#define	_IOC_TYPE(cmd) IOCGROUP(cmd)
#define	_IOC_SIZE(cmd) IOCPARM_LEN(cmd)
#define	MAP_ANONYMOUS MAP_ANON
#define	SYS_mmap2 SYS_mmap
#define	MMAP2_PAGE_SHIFT 0
typedef off_t __off_t;
#endif

#undef SYS_OPEN
#undef SYS_CLOSE
#undef SYS_IOCTL
#undef SYS_READ
#undef SYS_WRITE
#undef SYS_MMAP
#undef SYS_MUNMAP

#ifndef CONFIG_SYS_WRAPPER

#define SYS_OPEN(file, oflag, mode) \
    syscall(SYS_open, (const char *)(file), (int)(oflag), (mode_t)(mode))
#define SYS_CLOSE(fd) \
    syscall(SYS_close, (int)(fd))
#define SYS_IOCTL(fd, cmd, arg) \
    syscall(SYS_ioctl, (int)(fd), (unsigned long)(cmd), (void *)(arg))
#define SYS_READ(fd, buf, len) \
    syscall(SYS_read, (int)(fd), (void *)(buf), (size_t)(len));
#define SYS_WRITE(fd, buf, len) \
    syscall(SYS_write, (int)(fd), (void *)(buf), (size_t)(len));
#define SYS_MMAP(addr, len, prot, flags, fd, off) \
    syscall(SYS_mmap2, (void *)(addr), (size_t)(len), \
	(int)(prot), (int)(flags), (int)(fd), (__off_t)((off) >> MMAP2_PAGE_SHIFT))
#define SYS_MUNMAP(addr, len) \
    syscall(SYS_munmap, (void *)(addr), (size_t)(len))

#else

int v4lx_open_wrapper(const char *, int, int);
int v4lx_close_wrapper(int);
int v4lx_ioctl_wrapper(int, unsigned long, void *);
int v4lx_read_wrapper(int, void *, size_t);
int v4lx_write_wrapper(int, void *, size_t);
void *v4lx_mmap_wrapper(void *, size_t, int, int, int, off_t);
int v4lx_munmap_wrapper(void *, size_t);

#define SYS_OPEN(...) v4lx_open_wrapper(__VA_ARGS__)
#define SYS_CLOSE(...) v4lx_close_wrapper(__VA_ARGS__)
#define SYS_IOCTL(...) v4lx_ioctl_wrapper(__VA_ARGS__)
#define SYS_READ(...) v4lx_read_wrapper(__VA_ARGS__)
#define SYS_WRITE(...) v4lx_write_wrapper(__VA_ARGS__)
#define SYS_MMAP(...) v4lx_mmap_wrapper(__VA_ARGS__)
#define SYS_MUNMAP(...) v4lx_munmap_wrapper(__VA_ARGS__)

#endif

#endif /* _LIBV4LSYSCALL_PRIV_H_ */
