/* Where the strings glGetString and glGetStringi answer with are kept.
 *
 * OpenGL promises those strings are static: a caller may ask for the vendor,
 * the renderer and the version and read all three afterwards, and renderers
 * do - they pick their driver workarounds from them. The bridge cannot hand a
 * guest the driver's own pointer, so each answer is copied into the guest.
 * Copied into ONE buffer, as it first was, the second answer overwrote the
 * first under the pointer the caller still held: a core read "4.6.0 NVIDIA"
 * as its vendor.
 *
 * So every (name, index) has a place of its own here, and the same question
 * is answered with the same pointer for as long as the core runs. The place
 * is guest memory like any other and a state carries it; asked again after a
 * state made on another machine was loaded, the answer is written over the
 * old one where it fits, and gets a new place where it does not (the old
 * pointer then keeps what it held, which is what a context that went away
 * leaves behind anyway).
 *
 * This file is pasted into the generated guest file by gen-gl-bridge.py, and
 * included as it is by tests/unit/test_gl_strings.c. Plain C that is also C++.
 */
#include <stddef.h>
#include <string.h>

#define CHIMERA_GL_STRING_SLOTS 2048
#define CHIMERA_GL_STRING_POOL (256 * 1024)
/* glGetString has no index */
#define CHIMERA_GL_STRING_NO_INDEX 0xFFFFFFFFu

struct chimera_gl_string_slot
{
	unsigned name, index, offset, capacity;
};

static struct chimera_gl_string_slot chimera_gl_string_slots[CHIMERA_GL_STRING_SLOTS];
static unsigned chimera_gl_string_count;
static unsigned chimera_gl_string_used;
static char chimera_gl_string_pool[CHIMERA_GL_STRING_POOL];

/* The place kept for (name, index), holding `fresh`. Null when there is no
 * room left for a new one, and the caller falls back on what it has. */
static const char *chimera_gl_string_keep(unsigned name, unsigned index, const char *fresh)
{
	const size_t len = strlen(fresh) + 1;
	struct chimera_gl_string_slot *slot = NULL;
	char *at;
	unsigned i;

	for (i = 0; i < chimera_gl_string_count; i++)
	{
		if (chimera_gl_string_slots[i].name == name && chimera_gl_string_slots[i].index == index)
		{
			slot = &chimera_gl_string_slots[i];
			break;
		}
	}
	if (slot != NULL && len <= slot->capacity)
	{
		at = chimera_gl_string_pool + slot->offset;
		/* compared first: a write nobody needed is a page in the next state */
		if (memcmp(at, fresh, len) != 0) memcpy(at, fresh, len);
		return at;
	}
	if (len > (size_t)CHIMERA_GL_STRING_POOL - chimera_gl_string_used) return NULL;
	if (slot == NULL)
	{
		if (chimera_gl_string_count == CHIMERA_GL_STRING_SLOTS) return NULL;
		slot = &chimera_gl_string_slots[chimera_gl_string_count++];
		slot->name = name;
		slot->index = index;
	}
	slot->offset = chimera_gl_string_used;
	slot->capacity = (unsigned)len;
	chimera_gl_string_used += (unsigned)len;
	at = chimera_gl_string_pool + slot->offset;
	memcpy(at, fresh, len);
	return at;
}
