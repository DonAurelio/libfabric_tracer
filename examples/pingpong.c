/*
 * Minimal libfabric loopback ping-pong.
 *
 * Opens a single RDM endpoint over tcp, sends a message to itself,
 * and reads it back. Designed to be the smallest possible program
 * that exercises the send/recv/CQ path so the hook_trace output
 * is easy to read.
 *
 * Run with ofi_hook_trace active:
 *   FI_HOOK=trace FI_LOG_LEVEL=trace ./pingpong
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <rdma/fabric.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_eq.h>

#define MSG "hello from hook_trace"
#define BUF_SIZE 64

static void die(const char *msg, int ret)
{
    fprintf(stderr, "%s: %s (%d)\n", msg, fi_strerror(-ret), ret);
    exit(1);
}

int main(void)
{
    struct fi_info        *hints, *info;
    struct fid_fabric     *fabric  = NULL;
    struct fid_domain     *domain  = NULL;
    struct fid_av         *av      = NULL;
    struct fid_cq         *cq      = NULL;
    struct fid_ep         *ep      = NULL;
    struct fi_av_attr      av_attr = {0};
    struct fi_cq_attr      cq_attr = {0};
    fi_addr_t              self_addr;
    struct fi_cq_msg_entry cqe;
    char                   send_buf[BUF_SIZE];
    char                   recv_buf[BUF_SIZE];
    int                    ret;
    size_t                 addrlen = 0;
    void                  *addr;

    hints = fi_allocinfo();
    if (!hints) die("fi_allocinfo", -FI_ENOMEM);

    hints->ep_attr->type    = FI_EP_RDM;
    hints->caps             = FI_MSG;
    hints->mode             = FI_CONTEXT;
    hints->fabric_attr->prov_name = strdup("tcp");

    ret = fi_getinfo(FI_VERSION(2, 0), NULL, NULL, 0, hints, &info);
    fi_freeinfo(hints);
    if (ret) die("fi_getinfo", ret);

    printf("[app] provider: %s\n", info->fabric_attr->prov_name);

    ret = fi_fabric(info->fabric_attr, &fabric, NULL);
    if (ret) die("fi_fabric", ret);

    ret = fi_domain(fabric, info, &domain, NULL);
    if (ret) die("fi_domain", ret);

    av_attr.type  = FI_AV_MAP;
    av_attr.count = 1;
    ret = fi_av_open(domain, &av_attr, &av, NULL);
    if (ret) die("fi_av_open", ret);

    cq_attr.format = FI_CQ_FORMAT_MSG;
    cq_attr.size   = 16;
    ret = fi_cq_open(domain, &cq_attr, &cq, NULL);
    if (ret) die("fi_cq_open", ret);

    ret = fi_endpoint(domain, info, &ep, NULL);
    if (ret) die("fi_endpoint", ret);

    ret = fi_ep_bind(ep, &av->fid, 0);
    if (ret) die("fi_ep_bind(av)", ret);

    ret = fi_ep_bind(ep, &cq->fid, FI_TRANSMIT | FI_RECV);
    if (ret) die("fi_ep_bind(cq)", ret);

    ret = fi_enable(ep);
    if (ret) die("fi_enable", ret);

    /* resolve our own address so we can send to ourselves */
    ret = fi_getname(&ep->fid, NULL, &addrlen);
    if (ret != -FI_ETOOSMALL) die("fi_getname(size)", ret);
    addr = malloc(addrlen);
    ret = fi_getname(&ep->fid, addr, &addrlen);
    if (ret) die("fi_getname", ret);

    ret = fi_av_insert(av, addr, 1, &self_addr, 0, NULL);
    if (ret != 1) die("fi_av_insert", ret < 0 ? ret : -FI_EOTHER);
    free(addr);

    /* post recv first, then send */
    memset(recv_buf, 0, BUF_SIZE);
    struct fi_context rx_ctx = {0}, tx_ctx = {0};

    ret = fi_recv(ep, recv_buf, BUF_SIZE, NULL, self_addr, &rx_ctx);
    if (ret) die("fi_recv", ret);

    strncpy(send_buf, MSG, BUF_SIZE - 1);
    do {
        ret = fi_send(ep, send_buf, strlen(send_buf) + 1, NULL, self_addr, &tx_ctx);
    } while (ret == -FI_EAGAIN);
    if (ret) die("fi_send", ret);

    printf("[app] sent:     \"%s\"\n", send_buf);

    /* drain two CQ completions: one send, one recv */
    int completions = 0;
    while (completions < 2) {
        ret = fi_cq_read(cq, &cqe, 1);
        if (ret == -FI_EAGAIN) continue;
        if (ret < 0) die("fi_cq_read", ret);
        if (cqe.flags & FI_RECV)
            printf("[app] received: \"%s\"\n", recv_buf);
        completions++;
    }

    fi_close(&ep->fid);
    fi_close(&cq->fid);
    fi_close(&av->fid);
    fi_close(&domain->fid);
    fi_close(&fabric->fid);
    fi_freeinfo(info);

    printf("[app] done.\n");
    return 0;
}
