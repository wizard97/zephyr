.. _async_notification:

Asynchronous Notifications
##########################

Zephyr APIs often include :ref:`api_term_async` functions where an
operation is initiated and the application needs to be informed when it
completes, and whether it succeeded.  Using :c:func:`k_poll` is
often a good method, but some application architectures may be more
suited to a callback notification, and operations like enabling clocks
and power rails may need to be invoked before kernel functions are
available so a busy-wait for completion may be needed.

This API is intended to be embedded within specific subsystems such as
:ref:`resource_mgmt_onoff` and other APIs that support async
transactions.  The subsystem wrappers are responsible for extracting
operation-specific data from requests that include a notification
element, and for invoking callbacks with the parameters required by the
API.

A successful :c:func:`sys_notify_fetch_result` acquires the result and the
service's writes preceding :c:func:`sys_notify_finalize`. A client may therefore
use spin-wait notification to consume a result produced on another CPU without
an additional lock. This does not extend the lifetime of a containing object
past the ownership rules documented by the service.

A limitation is that this API is not suitable for :ref:`syscalls`
because:

* :c:struct:`sys_notify` is not a kernel object;
* copying the notification content from userspace will break use of
  :c:macro:`CONTAINER_OF` in the implementing function;
* neither the spin-wait nor callback notification methods can be
  accepted from userspace callers.

Where a notification is required for an asynchronous operation invoked
from a user mode thread the subsystem or driver should provide a syscall
API that uses :c:struct:`k_poll_signal` for notification.

API Reference
*************

.. doxygengroup:: sys_notify_apis
