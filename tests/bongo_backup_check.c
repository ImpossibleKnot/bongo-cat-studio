/* SPDX-License-Identifier: GPL-3.0-only */
/* Offline migration of an actual CRC-validated device backup's project payload.
 * Usage: bongo-backup-check project-object-0.bin. Never writes to hardware. */
#include "../web/emu/felucca_web.c"
#include <assert.h>
int main(int argc,char **argv) {
    assert(argc==2 || argc==3);project_t before,after;uint8_t raw[3840];
    FILE *f=fopen(argv[1],"rb");assert(f);assert(fread(raw,1,sizeof raw,f)==sizeof raw);fclose(f);
    web_nor_erase();web_boot();assert(proj_unpack(&before,raw,sizeof raw));
    assert(!project_restore_runtime(&before));project_capture(&after);
    printf("Metadata: %.12s -> %.12s\n",before.name,after.name);
    assert(!memcmp(after.name,"BC2",3));
    assert(!memcmp(before.t[0].step,after.t[0].step,sizeof before.t[0].step));
    for(unsigned i=0;i<4;i++) { assert(before.t[i].engine==after.t[i].engine); assert(before.t[i].preset==after.t[i].preset); assert(before.t[i].p[P_LEVEL]==after.t[i].p[P_LEVEL]); }
    assert(before.g[G_BPM]==after.g[G_BPM] && before.g[G_SWING]==after.g[G_SWING]);
    assert(bongo.save_pending);
    for(unsigned n=0;n<1000;n++) web_render(128);
    assert(!bongo.save_pending && as.writes);
    project_store_t stored; project_t saved;
    assert(st_load(OBJ_AUTOSAVE,&stored,sizeof stored)>0);
    assert(proj_unpack(&saved,stored.raw,sizeof stored));
    assert(!memcmp(saved.t[0].step,before.t[0].step,sizeof before.t[0].step));
    assert(!memcmp(saved.name,after.name,sizeof saved.name));
    if(argc==3) { assert(proj_pack(&stored,&after)); FILE *out=fopen(argv[2],"wb");assert(out);assert(fwrite(stored.raw,1,sizeof stored,out)==sizeof stored);fclose(out); }
    puts("PASS: runtime restore also commits the migrated jam to redundant autosave");
    puts("PASS: offline migration of the actual device backup preserves all 64 lead steps, instruments/mix, tempo and swing");
}
