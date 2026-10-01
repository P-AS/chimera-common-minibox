/* Error numbers a guest sees are Linux's, whatever the host's libc spells
 * (mb_linux_errno in minibox_internal.h). On Linux the table is the identity;
 * on Windows it is a translation, and this is where the two meet: the same
 * names must come out as the same Linux numbers on both. */
#include "minibox_internal.h"
#include "test_util.h"

static void test_names_are_linux_numbers(void) {
	CHECK_EQ(mb_linux_errno(ENOENT), 2);
	CHECK_EQ(mb_linux_errno(EBADF), 9);
	CHECK_EQ(mb_linux_errno(EAGAIN), 11);
	CHECK_EQ(mb_linux_errno(ENOMEM), 12);
	CHECK_EQ(mb_linux_errno(EFAULT), 14);
	CHECK_EQ(mb_linux_errno(EEXIST), 17);
	CHECK_EQ(mb_linux_errno(EINVAL), 22);
	/* the ones MSVCRT numbers differently */
	CHECK_EQ(mb_linux_errno(EDEADLK), 35);
	CHECK_EQ(mb_linux_errno(ENAMETOOLONG), 36);
	CHECK_EQ(mb_linux_errno(ENOSYS), 38);
	CHECK_EQ(mb_linux_errno(ENOTEMPTY), 39);
	CHECK_EQ(mb_linux_errno(ELOOP), 40);
	CHECK_EQ(mb_linux_errno(EOVERFLOW), 75);
	CHECK_EQ(mb_linux_errno(EILSEQ), 84);
	CHECK_EQ(mb_linux_errno(EOPNOTSUPP), 95);
	CHECK_EQ(mb_linux_errno(ENOTSUP), 95);
	CHECK_EQ(mb_linux_errno(EWOULDBLOCK), 11);
	CHECK_EQ(mb_linux_errno(ETIMEDOUT), 110);
	CHECK_EQ(mb_linux_errno(ECANCELED), 125);
}

static void test_unknown_passes_through(void) {
	CHECK_EQ(mb_linux_errno(0), 0);
	CHECK_EQ(mb_linux_errno(9999), 9999);
}

static void run_all(void) {
	RUN(test_names_are_linux_numbers);
	RUN(test_unknown_passes_through);
}

TEST_MAIN()
