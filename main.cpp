#include <iostream>
#include <cassert>
#include <rdma/fi_cm.h>
#include <rdma/fi_errno.h>
#include <rdma/fi_rma.h>

#define FI_SAFECALL(x)                                                    \
  {                                                                       \
    int err = (x);                                                        \
    if (err < 0) err = -err;                                              \
    if (err) {                                                            \
      fprintf(stderr, "err : %s (%s:%d)\n",                               \
              fi_strerror(err), __FILE__, __LINE__);                      \
      abort();                                                            \
    }                                                                     \
  }                                                                       \
  while (0)                                                               \
    ;

const int LCI_SERVER_MAX_CQES = 4096;

typedef struct server_t {
    struct fi_info* info;
    struct fid_fabric* fabric;
    struct fid_domain* domain;
    struct fid_ep* ep;
    struct fid_cq* cq;
    struct fid_av* av;
    fi_addr_t *peer_addrs;
} server_t __attribute__((aligned(64)));

void init(server_t *server)
{
    // Create hint.
//    char *p = getenv("LCI_OFI_PROVIDER_HINT");
    char p[] = "cxi";
    char *prov_name_hint = nullptr;
    if (p != nullptr) {
        prov_name_hint = (char*) malloc(strlen(p) + 1);
        strcpy(prov_name_hint, p);
    }
    struct fi_info* hints;
    hints = fi_allocinfo();
    hints->fabric_attr->prov_name = prov_name_hint;
    hints->ep_attr->type = FI_EP_RDM;
//    hints->domain_attr->mr_mode = FI_MR_VIRT_ADDR | FI_MR_ALLOCATED | FI_MR_PROV_KEY | FI_MR_LOCAL;
    hints->domain_attr->mr_mode = FI_MR_ENDPOINT | FI_MR_VIRT_ADDR | FI_MR_ALLOCATED | FI_MR_PROV_KEY | FI_MR_LOCAL;
    hints->domain_attr->threading = FI_THREAD_SAFE;
    hints->domain_attr->control_progress = FI_PROGRESS_MANUAL;
    hints->domain_attr->data_progress = FI_PROGRESS_MANUAL;
    hints->caps = FI_RMA | FI_TAGGED;
    hints->mode = FI_LOCAL_MR;

    // Create info.
    FI_SAFECALL(fi_getinfo(FI_VERSION(1, 6), NULL, NULL, 0, hints, &server->info));
    printf("Provider name: %s\n", server->info->fabric_attr->prov_name);
    printf("MR mode hints: [%s]\n", fi_tostr(&(hints->domain_attr->mr_mode), FI_TYPE_MR_MODE));
    printf("MR mode provided: [%s]\n", fi_tostr(&(server->info->domain_attr->mr_mode), FI_TYPE_MR_MODE));
    printf("Thread mode: %s\n", fi_tostr(&(server->info->domain_attr->threading), FI_TYPE_THREADING));
    printf("Control progress mode: %s\n", fi_tostr(&(server->info->domain_attr->control_progress), FI_TYPE_PROGRESS));
    printf("Data progress mode: %s\n", fi_tostr(&(server->info->domain_attr->data_progress), FI_TYPE_PROGRESS));
    printf("Capacities: %s\n", fi_tostr(&(server->info->caps), FI_TYPE_CAPS));
    printf("Mode: %s\n", fi_tostr(&(server->info->mode), FI_TYPE_MODE));
    assert(server->info->domain_attr->cq_data_size >= 4);
    assert(server->info->domain_attr->mr_key_size <= 8);
    fi_freeinfo(hints);

    // Create libfabric obj.
    FI_SAFECALL(fi_fabric(server->info->fabric_attr, &server->fabric, NULL));

    // Create domain.
    FI_SAFECALL(fi_domain(server->fabric, server->info, &server->domain, NULL));

    // Create end-point;
    FI_SAFECALL(fi_endpoint(server->domain, server->info, &server->ep, NULL));

    // Create cq.
    struct fi_cq_attr cq_attr;
    memset(&cq_attr, 0, sizeof(struct fi_cq_attr));
    cq_attr.format = FI_CQ_FORMAT_TAGGED;
    cq_attr.size = LCI_SERVER_MAX_CQES;
    FI_SAFECALL(fi_cq_open(server->domain, &cq_attr, &server->cq, NULL));

    // Bind my ep to cq.
    FI_SAFECALL(fi_ep_bind(server->ep, (fid_t)server->cq, FI_TRANSMIT | FI_RECV));

    struct fi_av_attr av_attr;
    memset(&av_attr, 0, sizeof(av_attr));
    av_attr.type = FI_AV_MAP;
    FI_SAFECALL(fi_av_open(server->domain, &av_attr, &server->av, NULL));
    FI_SAFECALL(fi_ep_bind(server->ep, (fid_t)server->av, 0));
    FI_SAFECALL(fi_enable(server->ep));

//    // Now exchange end-point address.
//    // assume the size of the raw address no larger than 128 bits.
//    const int EP_ADDR_LEN = 6;
//    size_t addrlen = 0;
//    fi_getname((fid_t)server->ep, NULL, &addrlen);
//    printf("addrlen = %lu\n", addrlen);
//    LCM_Assert(addrlen <= 8 * EP_ADDR_LEN, "addrlen = %lu\n", addrlen);
//    uint64_t my_addr[EP_ADDR_LEN];
//    FI_SAFECALL(fi_getname((fid_t)server->ep, my_addr, &addrlen));
//
//    server->peer_addrs = LCIU_malloc(sizeof(fi_addr_t) * LCI_NUM_PROCESSES);
//    char key[256];
//    sprintf(key, "LCI_KEY_%d_%d", device_id, LCI_RANK);
//    char value[256];
//    const char* PARSE_STRING = "%016lx-%016lx-%016lx-%016lx-%016lx-%016lx";
//    sprintf(value, PARSE_STRING,
//            my_addr[0], my_addr[1], my_addr[2], my_addr[3], my_addr[4], my_addr[5]);
//    lcm_pm_publish(key, value);
//    lcm_pm_barrier();
//
//    for (int i = 0; i < LCI_NUM_PROCESSES; i++) {
//        if (i != LCI_RANK) {
//            sprintf(key, "LCI_KEY_%d_%d", device_id, i);
//            lcm_pm_getname(key, value);
//            uint64_t peer_addr[EP_ADDR_LEN];
//
//            sscanf(value, PARSE_STRING,
//                   &peer_addr[0], &peer_addr[1], &peer_addr[2], &peer_addr[3], &peer_addr[4], &peer_addr[5]);
//            int ret = fi_av_insert(server->av, (void*)peer_addr, 1, &server->peer_addrs[i], 0, NULL);
//            LCM_Assert(ret == 1, "fi_av_insert failed! ret = %d\n", ret);
//        } else {
//            int ret = fi_av_insert(server->av, (void*)my_addr, 1, &server->peer_addrs[i], 0, NULL);
//            LCM_Assert(ret == 1, "fi_av_insert failed! ret = %d\n", ret);
//        }
//    }
//
//    lcm_pm_barrier();
}

void finalize(server_t *server)
{
//    free(server->peer_addrs);
    FI_SAFECALL(fi_close((struct fid*) &server->ep->fid));
    FI_SAFECALL(fi_close((struct fid*) &server->cq->fid));
    FI_SAFECALL(fi_close((struct fid*) &server->av->fid));
    FI_SAFECALL(fi_close((struct fid*) &server->domain->fid));
    FI_SAFECALL(fi_close((struct fid*) &server->fabric->fid));
    fi_freeinfo(server->info);
}

int main() {
    server_t server;
    init(&server);
    std::cout << "Hello, World!" << std::endl;
    finalize(&server);
    return 0;
}
