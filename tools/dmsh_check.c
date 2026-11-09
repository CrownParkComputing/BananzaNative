/* (bananza) Reads a dmsh_vk.lib with BananaNative's own reader (android/bananative/dm/gfx/vk_android/vka_shlib.c) as the phone
   would, lists what it holds, and runs spirv-val over every module. docs/VULKAN_SHARED_PLAN.md step 1's check.
   Build: tools/dmsh_check.sh. Usage: dmsh_check <dmsh_vk.lib> [--spv-dir <dir>] [--val] */
#include "../android/bananative/dm/gfx/vk_android/vka_shlib.h"
#include "../android/bananative/dm/gfx/nvn/dmsh_chain.h"
#include "../android/bananative/dm/gfx/nvn/dmsh_chain_vk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char *kind_name(int k) {
    static const char *n[] = {"?", "cbuf", "c1", "drvcb", "support", "sbuf", "sbuf_ubo", "ubo_ptr", "tex", "tbuf", "img", "samp", "samp_tex", "samp_id",
                              "tex_ubo", "samp_ubo", "tex_c2a", "samp_c2a", "tex_pool", "samp_pool", "gmem_map", "gmem_log", "shared_spill"};
    return k == 255 ? "UNKNOWN" : k >= 0 && k < (int)(sizeof n / sizeof *n) ? n[k] : "?";
}

int main(int argc, char **argv) {
    if (argc < 2) return fprintf(stderr, "usage: dmsh_check <dmsh_vk.lib> [--spv-dir <dir>] [--val]\n"), 2;
    const char *spv_dir = NULL;
    int val = 0;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--spv-dir") && i + 1 < argc) spv_dir = argv[++i];
        else if (!strcmp(argv[i], "--val")) val = 1;
    }
    vka_shlib L;
    char err[512];
    if (!vka_shlib_open(&L, argv[1], DMSH_CHAIN, DMSH_VK_CHAIN, NULL, DMSH_VK_ABI, err, sizeof err)) return fprintf(stderr, "refused: %s\n", err), 1;
    printf("ok: %u entries, %u bindings, %u modules, blob %llu MB; by_abi %d\n", L.h->entry_count, L.h->bind_count, L.h->mod_count,
           (unsigned long long)(L.h->blob_size >> 20), L.by_abi);
    int kinds[256] = {0}, stages[8] = {0}, unknown_entries = 0, bad = 0, invalid = 0;
    if (spv_dir) mkdir(spv_dir, 0755);
    for (unsigned i = 0; i < L.h->entry_count; i++) {
        const dmsh_vk_entry *e = &L.ent[i];
        if (e->stage < 8) stages[e->stage]++;
        const dmsh_vk_bind *b = vka_shlib_binds(&L, e);
        int unk = 0;
        for (unsigned k = 0; k < e->bind_count; k++) { kinds[b[k].kind]++; unk |= b[k].kind == DMSH_VK_KIND_UNKNOWN; }
        unknown_entries += unk;
        const dmsh_vk_module *m = vka_shlib_module(&L, e, DMSH_VK_MOD_BASE);
        if (!m) { bad++; continue; }
        size_t n;
        uint32_t *spv = vka_shlib_spirv(&L, m, &n, err, sizeof err);
        if (!spv) { bad++; if (bad < 5) fprintf(stderr, "entry %016llx: %s\n", (unsigned long long)e->key, err); continue; }
        if (spv_dir || val) {
            char path[1024];
            snprintf(path, sizeof path, "%s/%016llx.spv", spv_dir ? spv_dir : "/tmp", (unsigned long long)e->key);
            FILE *f = fopen(path, "wb");
            if (f) { fwrite(spv, 1, n, f); fclose(f); }
            if (val) {
                char cmd[1200];
                snprintf(cmd, sizeof cmd, "spirv-val --target-env vulkan1.3 --uniform-buffer-standard-layout '%s' >/dev/null 2>&1", path);
                if (system(cmd)) { invalid++; if (invalid <= 8) fprintf(stderr, "spirv-val fails: %016llx (stage %u)\n", (unsigned long long)e->key, e->stage); }
                if (!spv_dir) remove(path);
            }
        }
        free(spv);
    }
    printf("stages: vs %d tcs %d tes %d gs %d fs %d cs %d ts %d ms %d\n", stages[0], stages[1], stages[2], stages[3], stages[4], stages[5], stages[6], stages[7]);
    printf("entries with an UNKNOWN binding: %d; unreadable modules: %d%s\n", unknown_entries, bad, val ? "" : " (no --val: modules not validated)");
    if (val) printf("spirv-val failures: %d\n", invalid);
    printf("binding kinds:");
    for (int k = 0; k < 256; k++) if (kinds[k]) printf(" %s %d", kind_name(k), kinds[k]);
    printf("\n");
    vka_shlib_close(&L);
    return bad || invalid ? 1 : 0;
}
