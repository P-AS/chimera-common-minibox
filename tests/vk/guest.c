/* The Vulkan bridge's test, as a guest: vktest.h run through the bridge. */
#include <emulibc.h>
#include <stdint.h>
#include <string.h>
#include "vk-bridge.h"
#include "vktest.h"

static struct vkt_result g_result;

ECL_EXPORT int Install(uint64_t bridge)
{
	return chimera_vk_install((chimera_vk_bridge_fn)bridge) ? 1 : 0;
}

ECL_EXPORT int Run(void)
{
	vkt_run(chimera_vk_lookup, &g_result, 1);
	return g_result.failed_at;
}

/* 0 the step that failed, 1 its VkResult, 2 the pixel, 3 the image's hash,
 * 4 the copied buffer's hash, 5 the bytes a second mapping gave back, 6 a
 * pixel the triangle covers, 7 one it does not, 8 the drawn image's hash,
 * 9 how many descriptors may be pushed, 10 a pixel of the triangle drawn with
 * a pushed uniform, 11 that image's hash */
ECL_EXPORT uint64_t Value(int which)
{
	switch (which) {
	case 0: return (uint64_t)g_result.failed_at;
	case 1: return (uint64_t)(int64_t)g_result.vk_result;
	case 2: return g_result.pixel;
	case 3: return g_result.image_hash;
	case 4: return g_result.copy_hash;
	case 5: return g_result.round_trip;
	case 6: return g_result.drawn_inside;
	case 7: return g_result.drawn_outside;
	case 8: return g_result.drawn_hash;
	case 9: return g_result.push_limit;
	case 10: return g_result.pushed_inside;
	case 11: return g_result.pushed_hash;
	default: return 0;
	}
}

ECL_EXPORT uint64_t ContextId(void) { return chimera_vk_context_id(); }

/* For the test of a stale handle: make an instance and keep it, and later
 * ask for its devices - after the host has been told a state was loaded. */
static VkInstance g_stale;

ECL_EXPORT int MakeInstance(void)
{
	VkInstanceCreateInfo ici = { .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	return (int)((PFN_vkCreateInstance)chimera_vk_lookup("vkCreateInstance"))(&ici, NULL, &g_stale);
}

ECL_EXPORT int AskKeptInstance(void)
{
	uint32_t count = 0;
	return (int)((PFN_vkEnumeratePhysicalDevices)chimera_vk_lookup("vkEnumeratePhysicalDevices"))(g_stale, &count, NULL);
}

ECL_EXPORT uint64_t KeptInstance(void) { return (uint64_t)(uintptr_t)g_stale; }

/* A handle handed over BY VALUE to a command that returns nothing: the kept
 * instance's first card, kept too, and asked for its properties after the
 * load. The host has to refuse the call. Handing the driver a null card
 * instead ends the host. What comes back is the version the card claimed, or
 * zero when nothing was written. */
static VkPhysicalDevice g_stale_card;

ECL_EXPORT int KeepCard(void)
{
	uint32_t count = 1;
	const VkResult r = ((PFN_vkEnumeratePhysicalDevices)chimera_vk_lookup("vkEnumeratePhysicalDevices"))(g_stale, &count, &g_stale_card);
	return (r == VK_SUCCESS || r == VK_INCOMPLETE) && count == 1 && g_stale_card ? 1 : 0;
}

ECL_EXPORT uint64_t AskKeptCard(void)
{
	VkPhysicalDeviceProperties props;
	memset(&props, 0, sizeof props);
	((PFN_vkGetPhysicalDeviceProperties)chimera_vk_lookup("vkGetPhysicalDeviceProperties"))(g_stale_card, &props);
	return props.apiVersion;
}

/* The new context's own instance: the same place in the host's table as the
 * kept one had in the old context's, so only the context tells them apart. */
static VkInstance g_fresh;

ECL_EXPORT int MakeFreshInstance(void)
{
	VkInstanceCreateInfo ici = { .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	return (int)((PFN_vkCreateInstance)chimera_vk_lookup("vkCreateInstance"))(&ici, NULL, &g_fresh);
}

ECL_EXPORT int AskFreshInstance(void)
{
	uint32_t count = 0;
	return (int)((PFN_vkEnumeratePhysicalDevices)chimera_vk_lookup("vkEnumeratePhysicalDevices"))(g_fresh, &count, NULL);
}

ECL_EXPORT uint64_t FreshInstance(void) { return (uint64_t)(uintptr_t)g_fresh; }

ECL_EXPORT void DestroyFreshInstance(void)
{
	((PFN_vkDestroyInstance)chimera_vk_lookup("vkDestroyInstance"))(g_fresh, NULL);
}
