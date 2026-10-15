/* The guest half's hand-written part: the call across, and the few commands
 * that are not a plain call. Compiled into a core beside the generated
 * vk-bridge-guest.c. */
#include "vk-bridge.h"
#include "vk-bridge-ops.h"

#include <stdlib.h>
#include <string.h>

static chimera_vk_bridge_fn s_bridge;

uint64_t chimera_vk_call(uint32_t op, void *args)
{
	return s_bridge ? s_bridge(op, (uint64_t)(uintptr_t)args, 0, 0, 0, 0) : 0;
}

bool chimera_vk_install(chimera_vk_bridge_fn bridge)
{
	s_bridge = NULL;
	if (!bridge)
		return false;
	if (bridge(VKB_OP_LIST_LENGTH, 0, 0, 0, 0, 0) < VKB_LIST_LENGTH)
		return false;
	s_bridge = bridge;
	return true;
}

uint64_t chimera_vk_context_id(void)
{
	return s_bridge ? s_bridge(VKB_OP_CONTEXT_ID, 0, 0, 0, 0, 0) : 0;
}

void chimera_vk_description(char *out, uint32_t size)
{
	struct vkb_description_args a = { (uint64_t)(uintptr_t)out, size };
	if (size)
		out[0] = 0;
	chimera_vk_call(VKB_OP_DESCRIPTION, &a);
}

/* ---- which function has this name ----------------------------------------
 *
 * First a question about this half's own table. For a device, the host is
 * then asked whether the driver really has the command: the table carries
 * every command the bridge knows, and vkGetDeviceProcAddr must answer NULL
 * for one the device does not have (a call to it was refused, and a
 * renderer probing for an optional feature took the wrapper as a yes). */

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkb_g_vkGetInstanceProcAddr(VkInstance instance, const char *pName)
{
	(void)instance;
	return (PFN_vkVoidFunction)chimera_vk_lookup(pName);
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkb_g_vkGetDeviceProcAddr(VkDevice device, const char *pName)
{
	(void)device;
	void *fn = chimera_vk_lookup(pName);
	if (!fn || !s_bridge)
		return (PFN_vkVoidFunction)fn;
	struct vkb_has_command_args a = { (uint64_t)(uintptr_t)pName, 0, 0 };
	s_bridge(VKB_OP_HAS_COMMAND, (uint64_t)(uintptr_t)&a, 0, 0, 0, 0);
	/* a host older than the question leaves it unanswered: as before */
	return (PFN_vkVoidFunction)(a.answered && !a.present ? NULL : fn);
}

/* ---- mapped memory --------------------------------------------------------
 *
 * A mapping is a buffer of the guest's own. The host says how many bytes the
 * mapping has; the buffer is made here, and the host is told where it is and
 * fills it. What is lent out is kept in a list, in guest memory, so that it
 * is in a savestate with the pointers a renderer holds into it. */

/* `block` is what was allocated, and what is freed; `shadow` (inside it) is
 * where the mapped bytes are, handed to the guest and to the host */
struct mapping { uint64_t memory; void *shadow; void *block; };
static struct mapping *s_maps;
static uint32_t s_map_count, s_map_room;

static void *take_mapping(uint64_t memory)
{
	for (uint32_t i = 0; i < s_map_count; i++) {
		if (s_maps[i].memory == memory) {
			void *block = s_maps[i].block;
			s_maps[i] = s_maps[--s_map_count];
			return block;
		}
	}
	return NULL;
}

void chimera_vk_forget_mappings(void)
{
	for (uint32_t i = 0; i < s_map_count; i++)
		free(s_maps[i].block);
	s_map_count = 0;
}

VKAPI_ATTR VkResult VKAPI_CALL vkb_g_vkMapMemory(VkDevice device, VkDeviceMemory memory, VkDeviceSize offset,
                                                VkDeviceSize size, VkMemoryMapFlags flags, void **ppData)
{
	/* The first half: the host maps, and answers with the mapping's length
	 * where a driver would have put its pointer. */
	uint64_t bytes = 0;
	struct vkb_vkMapMemory_args a;
	a.device = device;
	a.memory = memory;
	a.offset = offset;
	a.size = size;
	a.flags = flags;
	a.ppData = (void **)&bytes;
	const VkResult r = (VkResult)chimera_vk_call(VK_OP_vkMapMemory, &a);
	if (r != VK_SUCCESS)
		return r;

	/* The pointer minus the offset must be aligned as the device asks
	 * (minMemoryMapAlignment, at most a page): so the shadow starts as far
	 * into a page-aligned block as the offset is into its page. */
	const size_t lead = (size_t)(offset & 4095);
	void *block = NULL, *shadow = NULL;
	if (s_map_count == s_map_room) {
		const uint32_t room = s_map_room ? s_map_room * 2 : 16;
		struct mapping *grown = realloc(s_maps, room * sizeof *grown);
		if (grown) {
			s_maps = grown;
			s_map_room = room;
		}
	}
	if (s_map_count < s_map_room && posix_memalign(&block, 4096, lead + (bytes ? bytes : 1)) == 0)
		shadow = (uint8_t *)block + lead;
	if (!shadow) {
		struct vkb_vkUnmapMemory_args u;
		u.device = device;
		u.memory = memory;
		chimera_vk_call(VK_OP_vkUnmapMemory, &u);
		return VK_ERROR_MEMORY_MAP_FAILED;
	}
	s_maps[s_map_count].memory = (uint64_t)(uintptr_t)memory;
	s_maps[s_map_count].shadow = shadow;
	s_maps[s_map_count].block = block;
	s_map_count++;

	struct vkb_map_attach_args at = { (uint64_t)(uintptr_t)memory, (uint64_t)(uintptr_t)shadow };
	chimera_vk_call(VKB_OP_MAP_ATTACH, &at);
	*ppData = shadow;
	return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkb_g_vkUnmapMemory(VkDevice device, VkDeviceMemory memory)
{
	struct vkb_vkUnmapMemory_args a;
	a.device = device;
	a.memory = memory;
	chimera_vk_call(VK_OP_vkUnmapMemory, &a);
	free(take_mapping((uint64_t)(uintptr_t)memory));
}

VKAPI_ATTR void VKAPI_CALL vkb_g_vkFreeMemory(VkDevice device, VkDeviceMemory memory,
                                             const VkAllocationCallbacks *pAllocator)
{
	struct vkb_vkFreeMemory_args a;
	a.device = device;
	a.memory = memory;
	a.pAllocator = pAllocator;
	chimera_vk_call(VK_OP_vkFreeMemory, &a);      /* freeing unmaps */
	free(take_mapping((uint64_t)(uintptr_t)memory));
}
