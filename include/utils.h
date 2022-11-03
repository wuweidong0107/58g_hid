#ifndef __UTILS_H__
#define __UTILS_H__

#include <unistd.h>
#include <fcntl.h>

#define sizearray(a)  (sizeof(a) / sizeof((a)[0]))

#ifndef container_of
#define container_of(ptr, type, member)					\
	({								\
		const __typeof__(((type *) NULL)->member) *__mptr = (ptr);	\
		(type *) ((char *) __mptr - offsetof(type, member));	\
	})
#endif

/* Test for GCC < 2.96 */
#if __GNUC__ < 2 || (__GNUC__ == 2 && (__GNUC_MINOR__ < 96))
#define __builtin_expect(x) (x)
#endif

#ifndef likely
#define likely(x)	__builtin_expect(!!(x), 1)
#endif

#ifndef unlikely
#define unlikely(x)	__builtin_expect(!!(x), 0)
#endif

static inline bool fd_is_nonblock(int fd)
{
    return (fcntl(fd, F_GETFL) & O_NONBLOCK) == O_NONBLOCK;
}

#endif