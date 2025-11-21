# Thread API Configuration File
# Format: FunctionName ThreadPrimitive
# Supported Primitives:
# TD_FORK, TD_JOIN, TD_DETACH, TD_EXIT, TD_CANCEL
# TD_ACQUIRE, TD_TRY_ACQUIRE, TD_RELEASE
# TD_COND_WAIT, TD_COND_SIGNAL, TD_COND_BROADCAST
# TD_MUTEX_INI, TD_MUTEX_DESTROY
# TD_CONDVAR_INI, TD_CONDVAR_DESTROY
# TD_BAR_INIT, TD_BAR_WAIT

# Linux Kernel Primitives Example
mutex_lock TD_ACQUIRE
mutex_unlock TD_RELEASE
spin_lock TD_ACQUIRE
spin_unlock TD_RELEASE
kthread_run TD_FORK
kthread_stop TD_JOIN
schedule TD_DUMMY

