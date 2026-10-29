# libfabric Hook Trace

A minimal working example of libfabric's `ofi_hook_trace` provider, with a step-by-step guide for writing your own custom hook tracer.

## What's in this repo

```
libfabric_tracer/
├── examples/
│   ├── pingpong.c          # loopback send/recv over tcp
│   └── Makefile
├── traces/
│   └── trace_output.txt    # captured ofi_hook_trace output
└── hook_trace_explainer.html  # architecture reference (open in browser)
```

The `src/` (libfabric source tree) and `install/` (build prefix) directories are excluded from git — they are fully reproducible from the build steps below.

---

## Prerequisites

- GCC, autoconf, automake, libtool
- `wget` or `curl`

---

## Build libfabric v2.7.0

```bash
# download and extract
cd src/
wget https://github.com/ofiwg/libfabric/releases/download/v2.7.0/libfabric-2.7.0.tar.bz2
tar -xjf libfabric-2.7.0.tar.bz2

# configure with the hook-debug provider enabled
cd libfabric-2.7.0/
./configure --prefix=$(pwd)/../../install --enable-hook-debug

# build and install (~2–5 min depending on CPU count)
make -j$(nproc) install
```

The `configure` output should confirm these built-in providers:

```
*** Built-in providers: ... hook_debug monitor trace perf ...
```

---

## Build and run the example

```bash
cd examples/
make          # compiles pingpong against the local install prefix

make run      # runs with FI_HOOK=trace FI_LOG_LEVEL=trace
              # saves output to traces/trace_output.txt
```

`make run` expands to:

```bash
LD_LIBRARY_PATH=../install/lib:$LD_LIBRARY_PATH \
FI_PROVIDER=tcp \
FI_HOOK=trace \
FI_LOG_LEVEL=trace \
./pingpong
```

Expected output:

```
[app] provider: tcp
[app] sent:     "hello from hook_trace"
[app] received: "hello from hook_trace"
[app] done.
```

The hook trace lines go to stderr and are saved to `traces/trace_output.txt`.

---

## How `ofi_hook_trace` works

Hook providers are transparent layers inserted between your application and the transport. When `FI_HOOK=trace` is set, libfabric wraps every object your application opens (fabric, domain, AV, CQ, endpoint) and replaces its vtable with tracing wrappers. Each wrapper calls the real function, then emits a log line on success.

The trace line format is:

```
libfabric:<PID>:<TIMESTAMP>::<PROVIDER>:<SUBSYSTEM>:<FUNCTION>():<LINE><LEVEL> MESSAGE
```

Example lines from a real run:

```
tcp:fabric:hook_trace_fabric():1606<trace> Installing trace hook
tcp:domain:trace_av_open():1352<trace>     av 0xd87d600, context (nil)
tcp:ep_data:trace_recv():623<trace>        buf 0x…7280 len 64 addr 0 flags 0x0
tcp:ep_data:trace_send():665<trace>        buf 0x…72c0 len 22 addr 0 flags 0x0
tcp:cq:trace_cq_read():1187<trace>         ctx 0x…230 flags 0x802        ← TX done
tcp:cq:trace_cq_read():1187<trace>         ctx 0x…250 flags 0x402 len 22 ← RX done
```

For a full breakdown of the source-level architecture and a guide to writing your own hook provider, open **`hook_trace_explainer.html`** in a browser.

---

## Writing your own hook provider

The hook system is designed to be extended. The minimum shape of a custom hook:

```c
// my_hook.c
#include "ofi_hook.h"

static ssize_t
my_send(struct fid_ep *ep, const void *buf, size_t len,
        void *desc, fi_addr_t dest, void *ctx)
{
    struct hook_ep *myep = container_of(ep, struct hook_ep, ep);

    uint64_t t0 = my_clock();                              // pre-call
    ssize_t ret = fi_send(myep->hep, buf, len, desc, dest, ctx);
    my_record("fi_send", t0, len, ret);                    // post-call

    return ret;
}

// ... define fi_ops_msg, fi_ops_domain, hook_prov_ctx, INI function
```

Build it as a shared library, point `FI_PROVIDER_PATH` at its directory, and activate it with `FI_HOOK=myhook`. Multiple hooks can be stacked: `FI_HOOK=trace;myhook`.

The key internal headers are `ofi_hook.h` and `ofi.h` from the libfabric source tree. See `hook_trace_explainer.html` for the complete step-by-step.

---

## Key environment variables

| Variable | Example | Effect |
|---|---|---|
| `FI_HOOK` | `trace` | Activates one or more hook providers (semicolon-separated) |
| `FI_LOG_LEVEL` | `trace` | Must be `trace` for `FI_TRACE()` macros to emit output |
| `FI_PROVIDER` | `tcp` | Restricts which transport provider is used |
| `FI_LOG_PROV` | `tcp` | Filters log output to a single provider name |

---

## References

- [libfabric v2.7.0 release](https://github.com/ofiwg/libfabric/releases/tag/v2.7.0)
- [`prov/hook/trace/src/hook_trace.c`](https://github.com/ofiwg/libfabric/blob/v2.7.0/prov/hook/trace/src/hook_trace.c) — the hook_trace implementation
- [`prov/hook/include/`](https://github.com/ofiwg/libfabric/tree/v2.7.0/prov/hook/include) — hook infrastructure headers
- `man fi_hook` (installed with libfabric) — hook provider documentation
