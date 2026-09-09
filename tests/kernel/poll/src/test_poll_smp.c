/*
 * SPDX-FileCopyrightText: Copyright The Zephyr Project Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#if defined(CONFIG_SMP) && defined(CONFIG_SCHED_CPU_MASK)

#define ITERATIONS 2000
#define PAYLOAD_WORDS 32
#define STACK_SIZE (1024 + CONFIG_TEST_EXTRA_STACK_SIZE)

static struct k_poll_signal signal;
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
		zassert_ok(k_poll_signal_raise(&signal, iteration));
	}
}

static void consumer(void *p1, void *p2, void *p3)
{
	for (int iteration = 1; iteration <= ITERATIONS; iteration++) {
		unsigned int signaled;
		int result;

		k_poll_signal_reset(&signal);
		k_sem_give(&request);
		/* Exercise direct checks without acquiring completion through k_poll(). */
		do {
			k_poll_signal_check(&signal, &signaled, &result);
		} while (signaled == 0U);
		zassert_equal(result, iteration);
		for (size_t i = 0; i < ARRAY_SIZE(payload); i++) {
			zassert_equal(payload[i], (uint32_t)iteration ^ (uint32_t)i);
		}
	}
}

ZTEST(poll_api, test_smp_signal_publication)
{
	k_poll_signal_init(&signal);
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
