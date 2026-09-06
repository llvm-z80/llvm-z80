//! A process-wide cap on how much heavy work runs at once.
//!
//! Suites decide their own shape of parallelism: `run_all` gives each suite a
//! thread, `torture` pulls tasks from a pool. Run one at a time that is fine,
//! but `full` runs them together, and the two counts simply added would put
//! far more work on the machine than it has cores. Rather than have each suite
//! guess a job count, every expensive step takes a permit from here, so total
//! concurrency is bounded no matter how the callers are arranged.
//!
//! "Expensive" means compiling and emulating. Assembling, linking and objcopy
//! run unmetered: they are milliseconds next to a clang invocation or a run of
//! several hundred million emulated cycles, and metering them would only add
//! contention.
//!
//! Permits are taken one at a time and never held across another acquisition,
//! so there is nothing here that can deadlock.

use std::sync::{Condvar, Mutex, OnceLock};

struct Pool {
    available: Mutex<usize>,
    freed: Condvar,
}

fn pool() -> &'static Pool {
    static POOL: OnceLock<Pool> = OnceLock::new();
    POOL.get_or_init(|| Pool {
        available: Mutex::new(capacity()),
        freed: Condvar::new(),
    })
}

/// How many heavy steps may run at once. One per core: these steps are whole
/// subprocesses that keep a core busy, so more would only add scheduling
/// overhead, and fewer would leave the machine idle.
pub fn capacity() -> usize {
    std::thread::available_parallelism().map(|n| n.get()).unwrap_or(4)
}

/// A permit, returned to the pool when dropped.
pub struct Permit {
    _private: (),
}

impl Drop for Permit {
    fn drop(&mut self) {
        let pool = pool();
        *pool.available.lock().unwrap() += 1;
        pool.freed.notify_one();
    }
}

/// Wait for room to run one expensive step.
pub fn acquire() -> Permit {
    let pool = pool();
    let mut available = pool.available.lock().unwrap();
    while *available == 0 {
        available = pool.freed.wait(available).unwrap();
    }
    *available -= 1;
    Permit { _private: () }
}
