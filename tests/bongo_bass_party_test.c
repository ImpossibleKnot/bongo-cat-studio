/* SPDX-License-Identifier: GPL-3.0-only */
#include "../web/emu/felucca_web.c"
#include <assert.h>
static double energy;
static void run(unsigned ms) {
 for(unsigned n=ms*441/10;n;) {unsigned m=n>128?128:n;web_render(m);for(unsigned i=0;i<m;i++)energy+=out_l[i]*out_l[i]+out_r[i]*out_r[i];n-=m;}
}
static void frame(const char *name) {
 ui.force=1;bongo_draw();FILE *f=fopen(name,"wb");assert(f);fprintf(f,"P6\n240 240\n255\n");
 for(unsigned i=0;i<240*240;i++){unsigned c=(web_fb[i]>>8)|(web_fb[i]<<8);unsigned char rgb[3]={((c>>11)&31)*255/31,((c>>5)&63)*255/63,(c&31)*255/31};fwrite(rgb,3,1,f);}fclose(f);
}
int main(void) {
 web_nor_erase();web_boot();run(600);
 for(unsigned style=0;style<3;style++) for(unsigned density=0;density<=100;density+=5) {
  bongo.style=style;bongo.density=density;bongo_band_pattern();unsigned held=0;
  for(unsigned i=0;i<16;i++) {
   int onset=i==0||i==8||(density>25&&i==6)||(density>55&&i==10)||(density>80&&i==15);
   assert((trk[2].step[i].time==ST_NOTE)==onset);
   if(onset){assert(trk[2].step[i].n==1);held=3;}
   else {assert(trk[2].step[i].time==(held?ST_TIE:ST_REST));if(held)held--;}
  }
 }
 bongo.band=0;bongo_band_pattern();for(unsigned i=0;i<16;i++)assert(trk[2].step[i].time==ST_REST);
 puts("PASS: sustained bass preserves all onset positions at every style/energy; band-off stays silent");
 bongo.band=1;bongo.density=0;bc_style(2);trk[2].p[P_LEVEL]=110;
 for(unsigned i=0;i<4;i++)if(i!=2)trk[i].p[P_LEVEL]=0;
 bongo_band_pattern();run(200);web_buttons(1u<<B_PLAY);run(25);web_buttons(0);run(175);energy=0;run(220);
 assert(energy>1);printf("PASS: actual sequencer bass still sounds 200-420 ms after its onset, energy %.3f\n",energy);
 web_buttons(1u<<B_PLAY);run(25);web_buttons(0);run(500);
 int32_t a[128],side[128];bongo.kick_at=fm1_ms;bongo.bass_duck=4096;
 for(unsigned i=0;i<128;i++)a[i]=side[i]=12000;
 bongo_bass_mix(&trk[1],a,side,128);for(unsigned i=0;i<128;i++)assert(a[i]==12000);
 bongo_bass_mix(&trk[2],a,side,128);assert(a[0]>11000 && a[127]<7000);for(unsigned i=0;i<128;i++)assert(a[i]==side[i]);
 fm1_ms+=250;for(unsigned k=0;k<5;k++){for(unsigned i=0;i<128;i++)a[i]=12000;bongo_bass_mix(&trk[2],a,0,128);}assert(bongo.bass_duck==4096);
 trk[3].p[P_LEVEL]=0;bongo.kick_at=0;bongo_hit(&trk[3],36);assert(!bongo.kick_at);
 puts("PASS: smooth bass-only kick duck, stereo symmetry, full recovery and no duck from muted kick");
 bongo.help=3;bongo.motion=1;web_buttons(1u<<B_FX);run(170);assert(bongo.fill && bongo.help==3);
 frame("build/maximum-cat-a.ppm");uint16_t first[240*240];memcpy(first,web_fb,sizeof first);
 run(140);frame("build/maximum-cat-b.ppm");unsigned changed=0;for(unsigned i=0;i<240*240;i++)changed+=first[i]!=web_fb[i];assert(changed>2000);
 bongo.motion=0;run(200);frame("build/maximum-cat-calm.ppm");memcpy(first,web_fb,sizeof first);run(200);ui.force=1;bongo_draw();assert(!memcmp(first,web_fb,sizeof first));
 web_buttons(0);run(100);assert(!bongo.fill && bongo.help==3);
 printf("PASS: Maximum Cat moves without lead notes (%u changed pixels), CALM stays still, release restores page\n",changed);
 return 0;
}
