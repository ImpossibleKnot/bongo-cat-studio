/* SPDX-License-Identifier: GPL-3.0-only */
#include "../web/emu/felucca_web.c"
#include <assert.h>
static double render_ms(unsigned ms)
{
    double energy=0;
    for(unsigned n=ms*441/10;n;) {
        unsigned m=n>128?128:n; web_render(m); n-=m;
        for(unsigned i=0;i<m;i++) energy+=(double)out_l[i]*out_l[i]+(double)out_r[i]*out_r[i];
    }
    return energy;
}
static void button(unsigned b)
{
    web_buttons(1u<<b);render_ms(25);web_buttons(0);render_ms(25);
}
int main(int argc,char **argv)
{
    web_nor_erase();
    if(argc>1) {
        FILE *f=fopen("build/bongo-click-on-nor.bin","rb");assert(f);
        assert(fread(nor,1,sizeof nor,f)==sizeof nor);fclose(f);
    }
    web_boot();render_ms(600);
    assert(click_mode==CLICK_OFF && cin_bars==0);
    if(argc>1) { puts("PASS: cold boot silences inherited ON metronome and two-bar count-in");return 0; }
    bongo.band=0;bongo_band_pattern();
    button(B_PLAY);assert(song.playing);
    assert(render_ms(1200)==0);
    web_set_metronome(CLICK_ON);
    assert(render_ms(1200)>.01); /* DAC path, which actually contains the click. */
    int tempo=song.g[G_BPM];
    web_enc(EN_SELECT,2);render_ms(40);web_enc(EN_K4,-2);render_ms(40);
    assert(click_mode==CLICK_OFF && cin_bars==0 && song.g[G_BPM]==tempo);
    assert(song.playing && bongo.loop_on && !bongo.band);
    assert(render_ms(1200)==0);
    puts("PASS: actual click audio ON, LOOPER/K4 OFF, silence with transport and loop still enabled");
    bongo.band=1;bongo_band_pattern();render_ms(2500);
    web_set_metronome(CLICK_ON);render_ms(100);
    web_set_metronome(CLICK_OFF);
    assert(bongo.band && song.playing && bongo.loop_on);
    assert(render_ms(1200)>.01);
    web_enc(EN_SELECT,-2);render_ms(40);assert(!bongo.help);
    web_enc(EN_K1,1);render_ms(40);assert(song.g[G_BPM]==tempo+1);
    puts("PASS: dedicated browser control leaves band audio running; K1 on the studio page still changes tempo");
    button(B_PLAY);render_ms(3000);
    rp_put(RP_CLICK,CLICK_ON);rp_put(RP_COUNTIN,2);settings_save();render_ms(1000);
    assert(rp_get(RP_CLICK)==CLICK_ON && rp_get(RP_COUNTIN)==2);
    FILE *f=fopen("build/bongo-click-on-nor.bin","wb");assert(f);
    assert(fwrite(nor,1,sizeof nor,f)==sizeof nor);fclose(f);
    return 0;
}
