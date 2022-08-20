#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#define NO_CPP_DEMANGLE
#define SIGSEGV_NO_AUTO_INIT

#include <stdbool.h>
#include "log.h"

# define siginfo_log(fmt, args...) a2j_error(fmt "\n", ##args);

#include "siginfo/siginfo.c"
