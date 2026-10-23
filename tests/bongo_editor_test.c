/* SPDX-License-Identifier: GPL-3.0-only */
#include "../web/emu/felucca_web.c"
#include <assert.h>
static void run(unsigned ms){for(unsigned n=ms*441/10;n;){unsigned m=n>128?128:n;web_render(m);n-=m;}}
static void press(unsigned b){web_buttons(1u<<b);run(25);web_buttons(0);run(25);}
static void turn(unsigned role,int n){web_enc(role,n);run(80);}
static void same_track(const proj_trk_t *a,const proj_trk_t *b){assert(a->engine==b->engine&&a->preset==b->preset);assert(!memcmp(a->p,b->p,sizeof a->p));assert(!memcmp(a->step,b->step,sizeof a->step));}
int main(int argc,char **argv){
 web_nor_erase();
 if(argc>1){FILE*f=fopen("build/editor-nor.bin","rb");assert(f);assert(fread(nor,1,sizeof nor,f)==sizeof nor);fclose(f);}
 web_boot();run(600);
 if(argc>1){project_store_t want,have;project_t p;FILE*f=fopen("build/editor-expected.bin","rb");assert(f);assert(fread(&want,1,sizeof want,f)==sizeof want);fclose(f);project_capture(&p);assert(proj_pack(&have,&p));assert(!memcmp(&want,&have,sizeof want));puts("PASS: all four edited sounds and every sequence byte survive cold boot");return 0;}
 for(unsigned t=0;t<4;t++){trk[t].p[P_LEVEL]=70+4*t;trk[t].p[P_PAN]=(int)t*8-12;}
 for(unsigned part=0;part<4;part++){
  web_set_page(BC_SOUND_PAGE);run(50);turn(EN_K1,(int)part-bongo.edit_part);assert(bongo.edit_part==part&&song.sel==part);
  for(unsigned engine=0;engine<NENG_SHOWN;engine++){
   project_t before,after;project_capture(&before);unsigned old=trk[part].eng_req;
   turn(EN_ALGO,1);assert(trk[part].eng_req==eng_step(old,1));turn(EN_PRESET,1);
   for(unsigned page=7;page<=10;page++) {web_set_page(page);run(20);for(unsigned k=0;k<4;k++){turn(EN_K1+k,2);unsigned id=bc_edit_id(k);const param_desc_t*d=track_desc(&trk[part],id);assert(trk[part].p[id]>=d->min&&trk[part].p[id]<=d->max);assert(str_len(web_edit_label(k)));}}
   project_capture(&after);
   for(unsigned t=0;t<4;t++)if(t!=part)same_track(&before.t[t],&after.t[t]);
   assert(!memcmp(before.t[part].step,after.t[part].step,sizeof before.t[part].step));
   assert(before.t[part].p[P_LEVEL]==after.t[part].p[P_LEVEL] && before.t[part].p[P_PAN]==after.t[part].p[P_PAN]);
   assert(before.t[part].p[P_MUTE]==after.t[part].p[P_MUTE]);
   web_set_page(BC_SOUND_PAGE);run(20);
  }
  unsigned count=bongo.count[part];web_keys(1u<<7);run(120);assert(kb_trk[7]==part);web_keys(0);run(500);assert(bongo.count[part]>count);
 }
 puts("PASS: all four parts, all 13 engines, preset browsing and every sound/envelope/effects knob; other parts and sequences untouched");
 web_set_page(9);run(25);press(B_REC);assert(bongo.help==0 && song.sel==0 && bongo.record==1);press(B_REC);press(B_PLAY);assert(!song.playing);
 puts("PASS: panel keys audition the selected cat; REC returns safely to the lead");
 web_set_page(7);bc_edit_part(2);run(50);set_engine_of(&trk[2],3);run(50);trk[2].p[P_E1]=4;assert(str_eq(bc_edit_label(2),"WAV#"));trk[2].p[P_E1]=0;assert(str_eq(bc_edit_label(2),"DUTY"));
 puts("PASS: mode-dependent knob names follow the actual sound");
 web_set_page(0);run(500);project_t expected;project_store_t wire;project_capture(&expected);assert(proj_pack(&wire,&expected));
 press(B_SAVE);for(unsigned wait=0;bongo.save_pending && wait<600;wait++)run(50);assert(!bongo.save_pending);project_store_t saved;assert(st_load(OBJ_AUTOSAVE,&saved,sizeof saved)>0);assert(!memcmp(&wire,&saved,sizeof wire));
 FILE*f=fopen("build/editor-expected.bin","wb");assert(f);fwrite(&wire,1,sizeof wire,f);fclose(f);f=fopen("build/editor-nor.bin","wb");assert(f);fwrite(nor,1,sizeof nor,f);fclose(f);
 puts("PASS: SAVE commits all edited sounds without rewriting the recorded sequence");
}
