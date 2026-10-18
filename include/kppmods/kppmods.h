#ifndef KPPMODS_H
#define KPPMODS_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define KPPMOD_ABI_V2 2u
/* Every pointer passed by the host stays valid until process exit. Plugins are
 * never unloaded. */
struct kppmod_context_v2 {
    uint32_t abi;
    uint32_t size;
    const char *id;
    const char *root;
    const char *process;
    const char *config;
    void *host_private;
};
struct kppmod_api_v2 {
    uint32_t abi;
    uint32_t size;
    void (*log)(const struct kppmod_context_v2 *, int priority, const char *message);
    /* Names are globally exclusive, and non-null target addresses cannot be
     * claimed under another name. Claim before installing a patch/interposer. */
    int (*claim_hook)(const struct kppmod_context_v2 *, const char *name,
                      const void *target);
};
/* Export this exact symbol. Return zero on success, errno-style code on failure.
 * Resolve dependencies and validate first; do not install irreversible patches
 * before validation. Failed plugins lose their registrations, but remain mapped.
 * Native crashes cannot be isolated inside a shared process. */
typedef int (*kppmod_init_fn_v2)(const struct kppmod_api_v2 *,
                                 const struct kppmod_context_v2 *);
int kppmod_init_v2(const struct kppmod_api_v2 *, const struct kppmod_context_v2 *);
#ifdef __cplusplus
}
#endif
#endif
