// Internal log-level convention for sumi_log_fn's `int level`
// (matches sokol's: 0 = panic, 1 = error, 2 = warning, 3 = info).
#pragma once

#define SUMI_LOG_PANIC 0
#define SUMI_LOG_ERROR 1
#define SUMI_LOG_WARN  2
#define SUMI_LOG_INFO  3
