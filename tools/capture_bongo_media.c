/* SPDX-License-Identifier: GPL-3.0-only */
/* README footage from the actual firmware renderer and factory state.
 * Build after generating build/gen assets:
 * clang -O1 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src 
 *   tools/capture_bongo_media.c -lm -o build/capture_bongo_media
 * mkdir -p build/media; build/capture_bongo_media build/media
 * Each file is 96 packed RGB frames, 240x240 pixels, 16 fps (six seconds).
 */
#include "../web/emu/felucca_web.c"
#include <assert.h>
static void run(unsigned ms) {
    for (unsigned n=ms*441/10;n;) {
        unsigned m=n>128?128:n;web_render(m);n-=m;
    }
}
static void press(unsigned b) {web_buttons(1u<<b);run(25);web_buttons(0);run(25);}
static void frame(FILE *f) {
    ui.force=1;bongo_draw();
    for(unsigned i=0;i<240*240;i++) {
        unsigned c=(web_fb[i]>>8)|(web_fb[i]<<8);
        unsigned char rgb[3]={((c>>11)&31)*255/31,((c>>5)&63)*255/63,(c&31)*255/31};
        assert(fwrite(rgb,3,1,f)==1);
    }
}
int main(int argc,char **argv) {
    assert(argc==2);web_nor_erase();web_boot();run(600);
    bc_style(0);bc_instrument(1);bongo.help=0;bongo.motion=1;
    bongo.coat=2;bongo.hat=1;bongo.theme=0;bongo.band=1;bongo.loop_on=1;
    bongo.progression=1;bongo.minor=1;bongo.root=0;bongo.density=55;
    bongo_band_pattern();song.g[G_BPM]=120;run(2500);press(B_PLAY);
    const char *names[]={"studio.rgb","maximum-cat.rgb","cat-cafe.rgb"};
    const unsigned notes[]={60,63,67,70,67,65,63,58};
    for(unsigned scene=0;scene<3;scene++) {
        if(scene==1)web_buttons(1u<<B_FX);
        if(scene==2) {
            web_buttons(0);press(B_PLAY);bongo.theme=3;bongo.hat=2;bongo.assist=0;
            run(2000);const unsigned phrase[]={60,69,65,64};
            for(unsigned i=0;i<4;i++){web_midi(0x90,phrase[i],90);run(130);web_midi(0x80,phrase[i],0);run(90);}
        }
        char path[1024];snprintf(path,sizeof path,"%s/%s",argv[1],names[scene]);
        FILE *f=fopen(path,"wb");assert(f);
        for(unsigned n=0;n<96;n++) {
            if(scene<2 && n%8==0)web_midi(0x90,notes[(n/8)%8],90);
            if(scene<2 && n%8==5)web_midi(0x80,notes[(n/8)%8],0);
            run(n%2?63:62);frame(f);
        }
        fclose(f);
    }
    return 0;
}
