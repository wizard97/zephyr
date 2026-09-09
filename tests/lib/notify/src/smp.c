/*
 * SPDX-FileCopyrightText: Copyright The Zephyr Project Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/notify.h>
#include <zephyr/ztest.h>

#if defined(CONFIG_SMP) && defined(CONFIG_SCHED_CPU_MASK)

#define ITERATIONS 2000
#define PAYLOAD_WORDS 32
#define STACK_SIZE (1024 + CONFIG_TEST_EXTRA_STACK_SIZE)

static struct sys_notify notification;
static uint32_t payload[PAYLOAD_WORDS];
static struct k_thread producer_thread;
static struct k_thread consumer_thread;
static K_THREAD_STACK_DEFINE(producer_stack, STACK_SIZE);
static K_THREAD_STACK_DEFINE(consumer_stack, STACK_SIZE);
static K_SEM_DEFINE(request, 0, 1);

static void producer(void *p1, void *p2, void *p3)
{
	for (int iteration = 1; iteration <= ITERATIONS; iteration++) {
		zassert_ok(k_sem_take(&request, K_FOREVER));
		for (size_t i = 0; i < ARRAY_SIZE(payload); i++) {
			payload[i] = (uint32_t)iteration ^ (uint32_t)i;
		}
		sys_notify_finalize(&notification, iteration);
	}
}

static void consumer(void *p1, void *p2, void *p3)
{
	for (int iteration = 1; iteration <= ITERATIONS; iteration++) {
		int result;

		sys_notify_init_spinwait(&notification);
		k_sem_give(&request);
		/* Only the notification publishes the producer's result and payload. */
		while (sys_notify_fetch_result(&notification, &result) == -EAGAIN) {
			arch_nop();
		}
		zassert_equal(result, iteration);
		for (size_t i = 0; i < ARRAY_SIZE(payload); i++) {
			zassert_equal(payload[i], (uint32_t)iteration ^ (uint32_t)i);
		}
	}
}

ZTEST(sys_notify_api, test_smp_publication)
{
	k_thread_create(&producer_thread, producer_stack, K_THREAD_STACK_SIZEOF(producer_stack),
			producer, NULL, NULL, NULL, 0, 0, K_FOREVER);
	k_thread_create(&consumer_thread, consumer_stack, K_THREAD_STACK_SIZEOF(consumer_stack),
			consumer, NULL, NULL, NULL, 0, 0, K_FOREVER);
	zassert_ok(k_thread_cpu_pin(&producer_thread, 0));
	zassert_ok(k_thread_cpu_pin(&consumer_thread, 1));
	k_thread_start(&producer_thread);
	k_thread_start(&consumer_thread);
	zassert_ok(k_thread_join(&producer_thread, K_FOREVER));
	zassert_ok(k_thread_join(&consumer_thread, K_FOREVER));
}

#endif
