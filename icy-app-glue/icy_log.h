#pragma once

#if !defined(log_host)
#   define log_host(fmt, ...)       (printf(fmt "\n", __VA_ARGS__), fflush(nullptr))
#endif

