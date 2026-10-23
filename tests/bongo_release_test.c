/* SPDX-License-Identifier: GPL-3.0-only */
#include "../web/emu/felucca_web.c"
#include <assert.h>
static void run(unsigned ms){for(unsigned n=ms*441/10;n;){unsigned m=n>128?128:n;web_render(m);for(unsigned i=0;i<m;i++)assert(isfinite(out_l[i])&&isfinite(out_r[i]));n-=m;}}
static void press(unsigned b){web_buttons(1u<<b);run(25);web_buttons(0);run(25);}
static void turn(unsigned k,int n){web_enc(k,n);run(40);}
static void sound_same(unsigned p,const project_t *want){
 project_t got;project_capture(&got);
 assert(got.t[p].engine==want->t[p].engine&&got.t[p].preset==want->t[p].preset);
 for(unsigned i=0;i<P_COUNT;i++)if(!param_kept(i))assert(got.t[p].p[i]==want->t[p].p[i]);
 assert(!memcmp(got.fm6[p],want->fm6[p],128));
}
static void frame(const char *path){
 ui.force=1;bongo_draw();FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n240 240\n255\n");
 for(unsigned i=0;i<240*240;i++){unsigned c=(web_fb[i]>>8)|(web_fb[i]<<8);unsigned char rgb[3]={((c>>11)&31)*255/31,((c>>5)&63)*255/63,(c&31)*255/31};fwrite(rgb,3,1,f);}fclose(f);
}
int main(void){
 web_nor_erase();web_boot();run(600);
 for(unsigned part=0;part<4;part++){
  web_set_edit_part(part);run(50);
  for(unsigned engine=0;engine<NENG_SHOWN;engine++){
   project_t a,b,after;project_capture(&a);turn(EN_ALGO,1);project_capture(&b);
   press(B_LFO);sound_same(part,&a);press(B_LFO);sound_same(part,&b);
   project_capture(&after);
   for(unsigned p=0;p<4;p++){
    assert(!memcmp(a.t[p].step,after.t[p].step,sizeof a.t[p].step));
    assert(a.t[p].p[P_LEVEL]==after.t[p].p[P_LEVEL]);
    assert(a.t[p].p[P_PAN]==after.t[p].p[P_PAN]&&a.t[p].p[P_MUTE]==after.t[p].p[P_MUTE]);
    if(p!=part)assert(!memcmp(&a.t[p],&after.t[p],sizeof a.t[p]));
   }
  }
  web_set_page(9);run(800);project_t a,b;project_capture(&a);
  turn(EN_K1,2);turn(EN_K1,2);turn(EN_K1,2);project_capture(&b);
  press(B_LFO);sound_same(part,&a);press(B_LFO);sound_same(part,&b);
 }
 /* Own FM6 voice must survive A/B even when no factory slot matches it. */
 web_set_edit_part(2);set_engine_of(&trk[2],ENGI_FM6);run(80);
 uint8_t patch[FP_SIZE+1u];memcpy(patch,fm6_patch[2],sizeof patch);patch[FP_NAME]='Z';patch[FP_FB]=5;fm6_set_patch(2,patch);fm6_adopt(2);run(80);
 project_t fm;project_capture(&fm);turn(EN_ALGO,1);press(B_LFO);sound_same(2,&fm);
 puts("PASS: all parts/13 engines A/B, grouped knob undo, own FM6 patch, mix and all sequences preserved");
 web_set_page(11);run(80);int16_t levels[4];for(unsigned p=0;p<4;p++)levels[p]=trk[p].p[P_LEVEL];
 trk[0].p[P_MUTE]=1;turn(EN_K1,1);assert(bongo.solo==1&&bc_part_audible(0));
 assert(trk[0].p[P_MUTE]==1);press(B_LFO);assert(!bongo.solo&&!bc_part_audible(0));
 turn(EN_K1,-1);assert(!trk[0].p[P_MUTE]);turn(EN_K3,2);assert(bongo.solo==3);
 for(unsigned p=0;p<4;p++){
  int32_t a[128],b[128];for(unsigned j=0;j<5;j++){for(unsigned i=0;i<128;i++)a[i]=b[i]=12000;bongo_bass_mix(&trk[p],a,b,128);}
  for(unsigned i=0;i<128;i++)assert(a[i]==b[i]);assert(p==2?a[127]==12000:a[127]==0);
  assert(trk[p].p[P_LEVEL]==levels[p]);
 }
 frame("build/release-live.ppm");press(B_LFO);assert(!bongo.solo);
 for(unsigned p=0;p<4;p++)assert(bc_part_audible(p));
 song.g[G_CLOCK]=0;unsigned hat=bongo.hat;press(B_ENV);run(450);press(B_ENV);run(450);press(B_ENV);
 assert(song.g[G_BPM]>=119&&song.g[G_BPM]<=121&&bongo.hat==hat);
 song.g[G_CLOCK]=1;unsigned bpm=song.g[G_BPM];press(B_ENV);assert(song.g[G_BPM]==bpm);song.g[G_CLOCK]=0;
 puts("PASS: solo overrides/restores prior mutes; smooth stereo isolation; levels kept; tap tempo respects external clock");
 web_set_page(0);bc_instrument(1);run(300);bongo.motion=0;project_t before,after;project_capture(&before);
 static const uint8_t phrases[3][7]={{62,64,65,67,64,60,62},{60,69,65,64},{69,72,76,81,76}};
 static const unsigned lengths[]={7,4,5};
 const char *images[]={"build/release-jazz.ppm","build/release-cafe.ppm","build/release-orbit.ppm"};
 for(unsigned k=0;k<3;k++)for(unsigned transpose=0;transpose<12;transpose++){
  run(2000);bongo.egg=0;bongo.egg_at=0;
  for(unsigned i=0;i<lengths[k];i++){web_midi(0x90,phrases[k][i]+transpose,90);run(70);web_midi(0x80,phrases[k][i]+transpose,0);run(60);}
  assert(bongo.egg==k+1&&bc_secret_text());
  project_capture(&after);assert(!memcmp(before.t,after.t,sizeof before.t));assert(!memcmp(before.g,after.g,sizeof before.g));
  if(!transpose){run(300);bongo.heat=0;frame(images[k]);uint16_t f[240*240];memcpy(f,web_fb,sizeof f);run(60);ui.force=1;bongo_draw();assert(!memcmp(f,web_fb,sizeof f));}
 }
 /* A motif must be played as a phrase, not collected over minutes. */
 bongo.egg_at=0;for(unsigned i=0;i<4;i++){bongo_live(trk,phrases[1][i]);run(1900);}assert(!bongo.egg_at);
 puts("PASS: three original visual secrets in all keys; no saved music changes; CALM still; phrase timeout");
 bongo.motion=1;web_set_page(2);press(B_REC);unsigned mode=bongo.overdub;turn(EN_K2,1);assert(bongo.overdub==mode);press(B_REC);press(B_PLAY);
 puts("PASS: record replace/overdub mode cannot change halfway through a take");
 bc_style(0);bc_instrument(1);bongo.band=1;bongo.loop_on=1;web_set_page(0);press(B_PLAY);
 unsigned seed=13;
 for(unsigned i=0;i<2000;i++){
  seed=seed*1664525u+1013904223u;unsigned p=(seed>>16)%4u;
  if(i%37==0){web_set_edit_part(p);turn(EN_ALGO,1);}
  if(i%11==0){web_set_page(11);turn(EN_K1+p,(seed&1)?2:-2);}
  if(i%7==0){web_midi(0x90,48+(i%36),80);run(30);web_midi(0x80,48+(i%36),0);}
  if(i%17==0){web_buttons(1u<<B_FX);run(30);web_buttons(0);}
  run(100);
 }
 press(B_PLAY);bongo.solo=0;
 puts("PASS: 2000 mixed live gestures under ASan/UBSan with finite stereo output");
}
