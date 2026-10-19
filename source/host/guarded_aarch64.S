/* guarded_aarch64.S - guarded.S for an aarch64 host: every call into the guest
 * goes through here, so that a guest that dies can hand control back instead
 * of taking the process with it. Read guarded.S first; this is the same
 * routine, register for register where aarch64 allows, and the comments there
 * are the ones that matter.
 *
 * Differences from x86-64:
 *   - the entry thunk passes the context in x9 and the guest function in x10
 *     (r10/r11 there), the arguments untouched in x0-x7;
 *   - the escape record saves x19-x28, the frame pointer and the link
 *     register, and d8-d15: what AAPCS64 says a callee keeps;
 *   - "%fs" is the word at MB_TP_SLOT, where an aarch64 guest finds its
 *     thread pointer (minibox_internal.h). Every aarch64 guest owns it
 *     (fs_swap is always on), so what it held is parked at entry and put back
 *     on the way out exactly as guarded.S does for a Rust guest;
 *   - the interop blob keeps no per-host-thread block (no gs), so an escape
 *     has only the context's four stack fields to put back.
 *
 * Layout: CTX_* must match mb_context (context.c asserts every one).
 */

#include "asm_aarch64.h"

#define CTX_THREAD_AREA   0x000
#define CTX_HOST_RSP      0x008
#define CTX_GUEST_RSP     0x010
#define CTX_HOST_RSP_ALT  0x018
#define CTX_GUEST_RSP_ALT 0x020
#define CTX_HOST_FS       0x238
#define CTX_FS_SWAP       0x240
#define CTX_DEAD          0x241
#define CTX_ESC_RSP       0x248
#define CTX_CALLS         0x250

/* the escape record, at the stack pointer the blob is called with */
#define FRAME_CTX           0
#define FRAME_PREV_ESC      8
#define FRAME_HOST_RSP      16
#define FRAME_GUEST_RSP     24
#define FRAME_HOST_RSP_ALT  32
#define FRAME_GUEST_RSP_ALT 40
#define FRAME_X19           48   /* x19..x28, then x29 and x30 */
#define FRAME_X29           128
#define FRAME_D8            144  /* d8..d15 */
#define FRAME_SIZE          208

	.text
	.globl MB_SYM(mb_guarded_call)
	.globl MB_SYM(mb_guarded_escape)
	.globl MB_SYM(mb_guarded_escape_now)
	MB_HIDDEN(mb_guarded_call)
	MB_HIDDEN(mb_guarded_escape)
	MB_HIDDEN(mb_guarded_escape_now)
	MB_TYPE_FUNCTION(MB_SYM(mb_guarded_call))
	MB_TYPE_FUNCTION(MB_SYM(mb_guarded_escape))
	MB_TYPE_FUNCTION(MB_SYM(mb_guarded_escape_now))

/* Entered by a guest-entry thunk (a branch, not a call: x30 is still the
 * caller's return address) with x9 = the context and x10 = the guest
 * function, the guest's arguments untouched in x0-x7. */
	.p2align 4
MB_SYM(mb_guarded_call):
	sub sp, sp, #FRAME_SIZE
	stp x19, x20, [sp, #FRAME_X19 + 0]
	stp x21, x22, [sp, #FRAME_X19 + 16]
	stp x23, x24, [sp, #FRAME_X19 + 32]
	stp x25, x26, [sp, #FRAME_X19 + 48]
	stp x27, x28, [sp, #FRAME_X19 + 64]
	stp x29, x30, [sp, #FRAME_X29]
	stp d8, d9, [sp, #FRAME_D8 + 0]
	stp d10, d11, [sp, #FRAME_D8 + 16]
	stp d12, d13, [sp, #FRAME_D8 + 32]
	stp d14, d15, [sp, #FRAME_D8 + 48]
	add x29, sp, #FRAME_X29

	/* the record: who, the record before this one, and the context's stack
	 * fields as the blob is about to find them */
	str x9, [sp, #FRAME_CTX]
	ldr x11, [x9, #CTX_ESC_RSP]
	str x11, [sp, #FRAME_PREV_ESC]
	ldr x11, [x9, #CTX_HOST_RSP]
	str x11, [sp, #FRAME_HOST_RSP]
	ldr x11, [x9, #CTX_GUEST_RSP]
	str x11, [sp, #FRAME_GUEST_RSP]
	ldr x11, [x9, #CTX_HOST_RSP_ALT]
	str x11, [sp, #FRAME_HOST_RSP_ALT]
	ldr x11, [x9, #CTX_GUEST_RSP_ALT]
	str x11, [sp, #FRAME_GUEST_RSP_ALT]
	mov x11, sp
	str x11, [x9, #CTX_ESC_RSP]
	ldr x11, [x9, #CTX_CALLS]
	add x11, x11, #1
	str x11, [x9, #CTX_CALLS]

	/* what the slot held, where the syscall dispatcher and the way out both
	 * read it */
	tp_slot_addr x12
	ldr x11, [x12]
	str x11, [x9, #CTX_HOST_FS]
	/* a dead machine runs nothing */
	ldrb w11, [x9, #CTX_DEAD]
	cbnz w11, .Lrefused

	/* the guest's thread pointer, once it has one */
	ldrb w11, [x9, #CTX_FS_SWAP]
	cbz w11, 2f
	ldr x11, [x9, #CTX_THREAD_AREA]
	cbz x11, 2f
	str x11, [x12]
2:
	/* call_guest_impl, at its fixed address: 0x35f00000200 */
	movz x11, #0x0200
	movk x11, #0x035f, lsl #32
	blr x11

	/* x0: what the guest returned, or 0 from a refusal or an escape.
	 * sp: the record, whichever way control arrived. */
.Llanding:
	ldr x9, [sp, #FRAME_CTX]
	ldr x10, [sp, #FRAME_PREV_ESC]
	str x10, [x9, #CTX_ESC_RSP]
	/* and what the slot held back */
	ldr x10, [x9, #CTX_HOST_FS]
	tp_slot_addr x11
	str x10, [x11]
	ldp d8, d9, [sp, #FRAME_D8 + 0]
	ldp d10, d11, [sp, #FRAME_D8 + 16]
	ldp d12, d13, [sp, #FRAME_D8 + 32]
	ldp d14, d15, [sp, #FRAME_D8 + 48]
	ldp x19, x20, [sp, #FRAME_X19 + 0]
	ldp x21, x22, [sp, #FRAME_X19 + 16]
	ldp x23, x24, [sp, #FRAME_X19 + 32]
	ldp x25, x26, [sp, #FRAME_X19 + 48]
	ldp x27, x28, [sp, #FRAME_X19 + 64]
	ldp x29, x30, [sp, #FRAME_X29]
	add sp, sp, #FRAME_SIZE
	ret

.Lrefused:
	mov x0, #0
	b .Llanding
	MB_SIZE(MB_SYM(mb_guarded_call))

/* From host C (x0 = the context): leave for the innermost record now.
 * Everything on the host stack below that record is abandoned. */
	.p2align 4
MB_SYM(mb_guarded_escape_now):
	ldr x1, [x0, #CTX_ESC_RSP]
	mov sp, x1

/* From a fault handler, which has pointed the interrupted context's sp at the
 * record and its pc here. Undoes what the blob's entry did to the context,
 * then leaves the way a return would, with 0. */
MB_SYM(mb_guarded_escape):
	ldr x9, [sp, #FRAME_CTX]
	ldr x10, [sp, #FRAME_HOST_RSP]
	str x10, [x9, #CTX_HOST_RSP]
	ldr x10, [sp, #FRAME_GUEST_RSP]
	str x10, [x9, #CTX_GUEST_RSP]
	ldr x10, [sp, #FRAME_HOST_RSP_ALT]
	str x10, [x9, #CTX_HOST_RSP_ALT]
	ldr x10, [sp, #FRAME_GUEST_RSP_ALT]
	str x10, [x9, #CTX_GUEST_RSP_ALT]
	mov x0, #0
	b .Llanding
	MB_SIZE(MB_SYM(mb_guarded_escape))

	MB_NOTE_GNU_STACK
