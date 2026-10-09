/* source/gl/gl-string-keep.inc: where the GPU bridge's guest half keeps the
 * strings glGetString and glGetStringi answer with. It is pasted into every
 * core's generated wrapper file; it is plain C, so it is tested here as it is.
 *
 * What it is for is the first test: two answers held at once are two strings.
 * With one buffer for all of them - how the wrapper first was - a renderer
 * that asked for its vendor and then its version read the version twice. */
#include "test_util.h"

#include "../../source/gl/gl-string-keep.inc"

enum { VENDOR = 0x1F00, RENDERER = 0x1F01, VERSION = 0x1F02, EXTENSIONS = 0x1F03 };

static void two_answers_held_at_once_are_two_strings(void)
{
	const char *vendor = chimera_gl_string_keep(VENDOR, CHIMERA_GL_STRING_NO_INDEX, "NVIDIA Corporation");
	const char *renderer = chimera_gl_string_keep(RENDERER, CHIMERA_GL_STRING_NO_INDEX, "GeForce GTX 1060");
	const char *version = chimera_gl_string_keep(VERSION, CHIMERA_GL_STRING_NO_INDEX, "4.6.0 NVIDIA 581.42");
	CHECK(vendor && renderer && version);
	CHECK(vendor != renderer && renderer != version && vendor != version);
	CHECK(strcmp(vendor, "NVIDIA Corporation") == 0);
	CHECK(strcmp(renderer, "GeForce GTX 1060") == 0);
	CHECK(strcmp(version, "4.6.0 NVIDIA 581.42") == 0);
}

static void the_same_question_gets_the_same_pointer(void)
{
	const char *first = chimera_gl_string_keep(VENDOR, CHIMERA_GL_STRING_NO_INDEX, "NVIDIA Corporation");
	const char *again = chimera_gl_string_keep(VENDOR, CHIMERA_GL_STRING_NO_INDEX, "NVIDIA Corporation");
	CHECK(first == again);
}

static void an_index_is_part_of_the_question(void)
{
	const char *whole = chimera_gl_string_keep(EXTENSIONS, CHIMERA_GL_STRING_NO_INDEX, "GL_A GL_B");
	const char *zero = chimera_gl_string_keep(EXTENSIONS, 0, "GL_A");
	const char *one = chimera_gl_string_keep(EXTENSIONS, 1, "GL_B");
	CHECK(whole != zero && zero != one && whole != one);
	CHECK(strcmp(whole, "GL_A GL_B") == 0);
	CHECK(strcmp(zero, "GL_A") == 0);
	CHECK(strcmp(one, "GL_B") == 0);
	CHECK(chimera_gl_string_keep(EXTENSIONS, 1, "GL_B") == one);
}

/* After a state made on another machine is loaded, the same question has
 * another answer. It goes where the old one was when it fits - so a pointer
 * the core kept reads the new driver's string - and elsewhere when it does
 * not, leaving every other string alone. */
static void another_drivers_answer_replaces_the_old_one(void)
{
	const char *version = chimera_gl_string_keep(VERSION, CHIMERA_GL_STRING_NO_INDEX, "4.6.0 NVIDIA 581.42");
	const char *shorter = chimera_gl_string_keep(VERSION, CHIMERA_GL_STRING_NO_INDEX, "4.5 Mesa 25.2");
	CHECK(shorter == version);
	CHECK(strcmp(version, "4.5 Mesa 25.2") == 0);

	const char *vendor = chimera_gl_string_keep(VENDOR, CHIMERA_GL_STRING_NO_INDEX, "NVIDIA Corporation");
	const char *longer = chimera_gl_string_keep(VERSION, CHIMERA_GL_STRING_NO_INDEX,
		"4.6 (Core Profile) Mesa 25.2.8-0ubuntu0.24.04.2 and then some");
	CHECK(longer != NULL && longer != version);
	CHECK(strcmp(longer, "4.6 (Core Profile) Mesa 25.2.8-0ubuntu0.24.04.2 and then some") == 0);
	CHECK(strcmp(vendor, "NVIDIA Corporation") == 0);
	/* and it is the place for that question from now on */
	CHECK(chimera_gl_string_keep(VERSION, CHIMERA_GL_STRING_NO_INDEX, "4.6 short") == longer);
}

/* No room is said, not papered over: the wrapper then hands back its own
 * buffer, which is what it always did. Last, because it fills the pool. */
static void no_room_is_null(void)
{
	static char big[CHIMERA_GL_STRING_POOL];
	memset(big, 'x', sizeof big - 1);
	big[sizeof big - 1] = 0;
	CHECK(chimera_gl_string_keep(0x9000, 0, big) == NULL);

	unsigned made = 0, i;
	for (i = 0; i < CHIMERA_GL_STRING_SLOTS + 8; i++)
	{
		if (chimera_gl_string_keep(0xA000, i, "e") != NULL) made++;
	}
	CHECK(made > 0 && made < CHIMERA_GL_STRING_SLOTS + 8);
	CHECK(chimera_gl_string_count == CHIMERA_GL_STRING_SLOTS);
	/* the strings kept before it filled are still theirs */
	CHECK(strcmp(chimera_gl_string_keep(VENDOR, CHIMERA_GL_STRING_NO_INDEX, "NVIDIA Corporation"), "NVIDIA Corporation") == 0);
}

static void run_all(void)
{
	RUN(two_answers_held_at_once_are_two_strings);
	RUN(the_same_question_gets_the_same_pointer);
	RUN(an_index_is_part_of_the_question);
	RUN(another_drivers_answer_replaces_the_old_one);
	RUN(no_room_is_null);
}

TEST_MAIN()
