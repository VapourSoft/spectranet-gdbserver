#include "state.h"

// Global state object for the GDB server
// Zero-initialized by default; specific fields will be set at runtime
struct gdbserver_state_t gdbserver_state;
