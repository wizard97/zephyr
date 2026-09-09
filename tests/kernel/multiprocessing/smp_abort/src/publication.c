/*
 * SPDX-FileCopyrightText: Copyright The Zephyr Project Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/irq_offload.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/ztest.h>

#define ITERATIONS 100
#define PAYLOAD_WORDS 16
#define STACK_SIZE (1024 + CONFIG_TEST_EXTRA_STACK_SIZE)

static struct k_thread target_thread;
static struct k_thread requester_thread;
static K_THREAD_STACK_DEFINE(target_stack, STACK_SIZE);
static K_THREAD_STACK_DEFINE(requester_stack, STACK_SIZE);
static atomic_t target_running;
static uint32_t cleanup[PAYLOAD_WORDS];

void thread_abort_hook(struct k_thread *thread)
{
	if (thread == &target_thread) {
		for (size_t i = 0; i < ARRAY_SIZE(cleanup); i++) {
			cleanup[i] = (uint32_t)i + 1U;
		}
	}
}

static void abort_from_isr(const void *arg)
{
	k_thread_abort(&target_thread);
	/* Check before join or any other operation can acquire the cleanup. */
	for (size_t i = 0; i < ARRAY_SIZE(cleanup); i++) {
		zassert_equal(cleanup[i], (uint32_t)i + 1U);
	}
}

static void target(void *p1, void *p2, void *p3)
{
	atomic_set(&target_running, 1);
	for (;;) {
		arch_nop();
	}
}

static void requester(void *p1, void *p2, void *p3)
{
	for (size_t i = 0; i < ITERATIONS; i++) {
		memset(cleanup, 0, sizeof(cleanup));
		atomic_clear(&target_running);
		k_thread_create(&target_thread, target_stack, K_THREAD_STACK_SIZEOF(target_stack),
				target, NULL, NULL, NULL, 0, 0, K_FOREVER);
		zassert_ok(k_thread_cpu_pin(&target_thread, 1));
		k_thread_start(&target_thread);
		while (atomic_get(&target_running) == 0) {
			arch_nop();
		}
		irq_offload(abort_from_isr, NULL);
	}
}

ZTEST(smp_abort, test_smp_abort_cleanup_publication)
{
	k_thread_create(&requester_thread, requester_stack, K_THREAD_STACK_SIZEOF(requester_stack),
			requester, NULL, NULL, NULL, 0, 0, K_FOREVER);
	zassert_ok(k_thread_cpu_pin(&requester_thread, 0));
	k_thread_start(&requester_thread);
	zassert_ok(k_thread_join(&requester_thread, K_FOREVER));
}
