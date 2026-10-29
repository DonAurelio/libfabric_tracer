/*
 * my_tracer.c — minimal libfabric hook provider that traces fi_send.
 *
 * Prints ">>> fi_send entry" / "<<< fi_send exit" around the real call.
 * All other msg ops are pass-throughs defined inline here.
 *
 * Build:
 *   see Makefile
 *
 * Run:
 *   FI_PROVIDER_PATH=. FI_HOOK=mytracer FI_PROVIDER=tcp ./pingpong
 */

#define _GNU_SOURCE   /* needed for mremap in linux/osd.h pulled by ofi.h */

#include <stdio.h>
#include <stdlib.h>

#include "ofi_hook.h"   /* hook_fabric, hook_ep, hook_domain, container_of … */

/* ── Stubs for internal symbols not exported by the installed libfabric ── */

ssize_t fi_no_rx_size_left(struct fid_ep *ep) { return -FI_ENOSYS; }
ssize_t fi_no_tx_size_left(struct fid_ep *ep) { return -FI_ENOSYS; }

/* ── fi_send: the one function we actually trace ─────────────── */

static ssize_t
mytracer_send(struct fid_ep *ep, const void *buf, size_t len,
              void *desc, fi_addr_t dest_addr, void *context)
{
    struct hook_ep *myep = container_of(ep, struct hook_ep, ep);

    printf(">>> fi_send  ep=%p  buf=%p  len=%zu  dest=0x%lx  ctx=%p\n",
           (void *)ep, buf, len, (unsigned long)dest_addr, context);
    fflush(stdout);

    ssize_t ret = fi_send(myep->hep, buf, len, desc, dest_addr, context);

    printf("<<< fi_send  ret=%zd\n", ret);
    fflush(stdout);

    return ret;
}

/* ── Pass-through wrappers for all other msg ops ─────────────── */

static ssize_t
mytracer_recv(struct fid_ep *ep, void *buf, size_t len,
              void *desc, fi_addr_t src_addr, void *context)
{
    struct hook_ep *myep = container_of(ep, struct hook_ep, ep);
    return fi_recv(myep->hep, buf, len, desc, src_addr, context);
}

static ssize_t
mytracer_recvv(struct fid_ep *ep, const struct iovec *iov, void **desc,
               size_t count, fi_addr_t src_addr, void *context)
{
    struct hook_ep *myep = container_of(ep, struct hook_ep, ep);
    return fi_recvv(myep->hep, iov, desc, count, src_addr, context);
}

static ssize_t
mytracer_recvmsg(struct fid_ep *ep, const struct fi_msg *msg, uint64_t flags)
{
    struct hook_ep *myep = container_of(ep, struct hook_ep, ep);
    return fi_recvmsg(myep->hep, msg, flags);
}

static ssize_t
mytracer_sendv(struct fid_ep *ep, const struct iovec *iov, void **desc,
               size_t count, fi_addr_t dest_addr, void *context)
{
    struct hook_ep *myep = container_of(ep, struct hook_ep, ep);
    return fi_sendv(myep->hep, iov, desc, count, dest_addr, context);
}

static ssize_t
mytracer_sendmsg(struct fid_ep *ep, const struct fi_msg *msg, uint64_t flags)
{
    struct hook_ep *myep = container_of(ep, struct hook_ep, ep);
    return fi_sendmsg(myep->hep, msg, flags);
}

static ssize_t
mytracer_inject(struct fid_ep *ep, const void *buf, size_t len,
                fi_addr_t dest_addr)
{
    struct hook_ep *myep = container_of(ep, struct hook_ep, ep);
    return fi_inject(myep->hep, buf, len, dest_addr);
}

static ssize_t
mytracer_senddata(struct fid_ep *ep, const void *buf, size_t len,
                  void *desc, uint64_t data, fi_addr_t dest_addr, void *context)
{
    struct hook_ep *myep = container_of(ep, struct hook_ep, ep);
    return fi_senddata(myep->hep, buf, len, desc, data, dest_addr, context);
}

static ssize_t
mytracer_injectdata(struct fid_ep *ep, const void *buf, size_t len,
                    uint64_t data, fi_addr_t dest_addr)
{
    struct hook_ep *myep = container_of(ep, struct hook_ep, ep);
    return fi_injectdata(myep->hep, buf, len, data, dest_addr);
}

/* ── fi_ops_msg vtable ───────────────────────────────────────── */

static struct fi_ops_msg mytracer_msg_ops = {
    .size       = sizeof(struct fi_ops_msg),
    .recv       = mytracer_recv,
    .recvv      = mytracer_recvv,
    .recvmsg    = mytracer_recvmsg,
    .send       = mytracer_send,      /* <── our traced wrapper */
    .sendv      = mytracer_sendv,
    .sendmsg    = mytracer_sendmsg,
    .inject     = mytracer_inject,
    .senddata   = mytracer_senddata,
    .injectdata = mytracer_injectdata,
};

/* ── Per-endpoint init callback: swap msg vtable ─────────────── */

static int mytracer_ep_init(struct fid *fid)
{
    struct fid_ep *ep = container_of(fid, struct fid_ep, fid);
    ep->msg = &mytracer_msg_ops;
    return 0;
}

/* ── Forward declaration (defined below, referenced in fabric fn) */
struct hook_prov_ctx mytracer_ctx;

/* ── Fabric creation: called once per fi_fabric() ────────────── */

static int
mytracer_fabric(struct fi_fabric_attr *attr,
                struct fid_fabric **fabric, void *context)
{
    struct fi_provider *hprov = context;
    struct hook_fabric *fab;

    printf("[mytracer] hook installed over provider: %s\n", hprov->name);
    fflush(stdout);

    fab = calloc(1, sizeof *fab);
    if (!fab)
        return -FI_ENOMEM;

    hook_fabric_init(fab, HOOK_TRACE, attr->fabric, hprov,
                     &hook_fabric_fid_ops, &mytracer_ctx);
    *fabric = &fab->fabric;
    return 0;
}

/* ── Provider context ────────────────────────────────────────── */

struct hook_prov_ctx mytracer_ctx = {
    .prov = {
        .version    = OFI_VERSION_DEF_PROV,
        .fi_version = OFI_VERSION_LATEST,
        .name       = "ofi_hook_mytracer",
        .getinfo    = NULL,
        .fabric     = mytracer_fabric,
        .cleanup    = NULL,
    },
};

/* ── DSO entry point ─────────────────────────────────────────── */

FI_EXT_INI
{
    mytracer_ctx.ini_fid[FI_CLASS_EP] = mytracer_ep_init;
    return &mytracer_ctx.prov;
}
