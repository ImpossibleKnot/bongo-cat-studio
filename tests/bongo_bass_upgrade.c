/* SPDX-License-Identifier: GPL-3.0-only */
/* Apply the requested bass sound to a captured jam, offline. No device I/O. */
#include "../web/emu/felucca_web.c"
#include <assert.h>
int main(int argc,char **argv) {
 assert(argc==3);uint8_t raw[3840];project_t before,after;project_store_t packed;
 FILE *f=fopen(argv[1],"rb");assert(f);assert(fread(raw,1,sizeof raw,f)==sizeof raw);fclose(f);
 web_nor_erase();web_boot();assert(proj_unpack(&before,raw,sizeof raw));assert(!project_restore_runtime(&before));
 bc_bass_tone();bongo_bass_ties();trk[2].p[P_LEVEL]=110;project_capture(&after);
 for(unsigned i=0;i<4;i++)if(i!=2) {
  assert(before.t[i].engine==after.t[i].engine && before.t[i].preset==after.t[i].preset);
  assert(!memcmp(before.t[i].p,after.t[i].p,sizeof before.t[i].p));
  if(memcmp(before.t[i].step,after.t[i].step,sizeof before.t[i].step)) { for(unsigned j=0;j<64;j++) if(memcmp(&before.t[i].step[j],&after.t[i].step[j],sizeof(step_t))) fprintf(stderr,"CHANGED track %u step %u: time %u>%u n %u>%u notes %u,%u>%u,%u\n",i,j,before.t[i].step[j].time,after.t[i].step[j].time,before.t[i].step[j].n,after.t[i].step[j].n,before.t[i].step[j].note[0],before.t[i].step[j].note[1],after.t[i].step[j].note[0],after.t[i].step[j].note[1]); return 1; }
 }
 assert(!memcmp(before.g,after.g,sizeof before.g));assert(!memcmp(before.name,after.name,sizeof before.name));
 assert(!memcmp(&before.chain,&after.chain,sizeof before.chain));assert(!memcmp(&before.motion,&after.motion,sizeof before.motion));
 for(unsigned i=0;i<NSTEP;i++)if(before.t[2].step[i].time==ST_NOTE)
  assert(!memcmp(&before.t[2].step[i],&after.t[2].step[i],sizeof(step_t)));
 assert(proj_pack(&packed,&after));f=fopen(argv[2],"wb");assert(f);assert(fwrite(packed.raw,1,sizeof packed.raw,f)==sizeof packed.raw);fclose(f);
 puts("PASS: only bass tone/level and ties changed; every other track, note onset, global, studio choice and automation preserved");
}
