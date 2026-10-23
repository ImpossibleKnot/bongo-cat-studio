/* SPDX-License-Identifier: GPL-3.0-only */
/* Integration tests: the real firmware, UI, audio, sequencer and NOR storage. */
#include "../web/emu/felucca_web.c"
#include <assert.h>

static double energy;
static float peak;
static void run_ms(unsigned ms)
{
    unsigned n=ms*441u/10u;
    while(n) {
        unsigned m=n>128?128:n;
        web_render(m);
        for(unsigned j=0;j<m;j++) {
            assert(isfinite(out_l[j]) && isfinite(out_r[j]));
            float a=fabsf(out_l[j]); if(a>peak) peak=a;
            energy+=(double)out_l[j]*out_l[j]+(double)out_r[j]*out_r[j];
        }
        n-=m;
    }
}
static void press(unsigned b) { web_buttons(1u<<b); run_ms(20); web_buttons(0); run_ms(25); }
static void note(unsigned n) { web_midi(0x90,n,100); run_ms(80); web_midi(0x80,n,0); run_ms(80); }
static void frame(const char *path)
{
    ui.force=1; bongo_draw();
    FILE *f=fopen(path,"wb"); assert(f);
    fprintf(f,"P6\n240 240\n255\n");
    for(unsigned i=0;i<240*240;i++) {
        unsigned c=(web_fb[i]>>8)|(web_fb[i]<<8);
        unsigned char rgb[3]={((c>>11)&31)*255/31,((c>>5)&63)*255/63,(c&31)*255/31};
        fwrite(rgb,3,1,f);
    }
    fclose(f);
}
int main(int argc,char **argv)
{
    web_nor_erase();
    if(argc>1) {
        FILE *f=fopen("build/bongo-nor.bin","rb"); assert(f);
        assert(fread(nor,1,sizeof nor,f)==sizeof nor); fclose(f);
    }
    web_boot(); run_ms(600);
    assert(bongo.enabled && !song.playing);
    if(argc>1) {
        assert(bongo.instrument==2 && bongo.style==2 && bongo.hat==3 && bongo.theme==2);
        assert(bongo.root==7 && !bongo.minor && !bongo.band && !bongo.loop_on && bongo.density==80);
        assert(bongo.length==2 && bongo.coat==2 && bongo.motion && bongo.assist && bongo.progression==3 && bongo.overdub);
        assert(trk[0].p[P_SLEN]==64);
        assert(song.g[G_BPM]==123 && trk[0].step[3].n==1 && trk[0].step[3].note[0]==67);
        puts("PASS: cold boot restores loop, instrument, tempo, band, key, hat, theme and knobs");
        return 0;
    }
    assert(bongo.instrument==1 && song.g[G_BPM]==108);
    frame("build/bongo-idle.ppm");
    for(unsigned i=0;i<8;i++) {
        web_enc(EN_PRESET,(int)i-(int)bongo.instrument); run_ms(30);
        assert(bongo.instrument==i && trk[0].eng_req==BC_ENGINE[i]);
        assert(str_eq(ENGINES[BC_ENGINE[i]]->presets[trk[0].preset].name,BC_PRESET[i]));
        double before=energy; note(i==5?36:60); run_ms(200);
        assert(energy-before>0.001);
    }
    puts("PASS: all eight named presets produce finite, non-silent audio");
    web_enc(EN_PRESET,1-(int)bongo.instrument); run_ms(30);
    unsigned old=bongo.live_count;
    web_keys(1u<<7); run_ms(120); web_keys(0); run_ms(100);
    assert(bongo.live_count>old);
    const unsigned lick[]={62,64,65,67,64,60,62};
    for(unsigned i=0;i<7;i++) note(lick[i]);
    assert(bongo.jazz_at && bongo.root==0);
    frame("build/bongo-jazz.ppm");
    bc_length(0);
    press(B_PLAY); assert(song.playing);
    run_ms(400); press(B_REC); assert(bongo.record==1);
    unsigned guard=0;
    while(bongo.record==1 && guard++<300) run_ms(10);
    assert(bongo.record==2 && song.rec==1);
    for(unsigned i=0;i<6;i++) note(60+i%3*2);
    guard=0;
    while(bongo.record==2 && guard++<300) run_ms(10);
    assert(!bongo.record && !song.rec && bongo.learned==1);
    unsigned count=0;
    for(unsigned i=0;i<16;i++) count+=trk[0].step[i].n>0;
    assert(count>=3);
    old=bongo.count[0]; run_ms(2500); assert(bongo.count[0]>old);
    frame("build/bongo-playing.ppm");
    press(B_SEQ); old=bongo.count[0]; run_ms(2500); assert(bongo.count[0]==old);
    press(B_SEQ);
    step_t captured[16]; memcpy(captured,trk[0].step,sizeof captured);
    press(B_EDIT);
    for(unsigned i=0;i<16;i++) assert(!trk[0].step[i].n);
    press(B_LFO); assert(!memcmp(captured,trk[0].step,sizeof captured));
    puts("PASS: physical keys, jazz recognition, one-bar learning, playback, mute, clear and undo");
    for(unsigned s=0;s<3;s++) {
        web_enc(EN_ALGO,(int)s-(int)bongo.style); run_ms(30); assert(bongo.style==s);
        run_ms(2400); assert(bongo.count[1] && bongo.count[2] && bongo.count[3]);
    }
    web_buttons(1u<<B_FX); run_ms(500); assert(bongo.fill);
    frame("build/bongo-chaos.ppm");
    web_buttons(0); run_ms(60); assert(!bongo.fill);
    press(B_PLAY); assert(!song.playing);
    press(B_HOME); assert(bongo.help==1); frame("build/bongo-help1.ppm");
    press(B_HOME); assert(bongo.help==2); frame("build/bongo-help2.ppm");
    web_enc(EN_SELECT,-2); run_ms(30); assert(!bongo.help);
    puts("PASS: three backing styles, momentary fill release and help navigation");
    /* Mix control changes, transport, rendering and note edges for a minute. */
    for(unsigned i=0;i<400;i++) {
        web_enc(EN_PRESET,(i%3)-1); web_enc(EN_K1,(i%5)-2);
        if(!(i%11)) press(B_ENV);
        if(!(i%13)) press(B_GLO);
        if(!(i%29)) press(B_PLAY);
        if(!(i%41)) press(B_REC);
        if(!(i%47)) press(B_EDIT);
        note(53+i%27);
        assert(bongo.instrument<8 && bongo.theme<4 && bongo.hat<6);
    }
    if(song.playing) press(B_PLAY);
    bc_length(2); bongo.coat=2; bongo.motion=1; bongo.assist=1; bongo.progression=3; bongo.overdub=1;
    bc_instrument(2); bc_style(2); bongo.hat=3; bongo.theme=2; bongo.root=7;
    bongo.minor=0; bongo.band=0; bongo.loop_on=0; bongo.density=80;
    song.g[G_BPM]=123; bongo_step_note(&trk[0].step[3],67,100);
    press(B_SAVE); run_ms(6000);
    assert(!bongo.save_pending && as.writes>0);
    FILE *f=fopen("build/bongo-nor.bin","wb"); assert(f);
    assert(fwrite(nor,1,sizeof nor,f)==sizeof nor); fclose(f);
    printf("PASS: 400 mixed gestures, save to NOR, audio peak %.4f; frames %u\n",peak,web_draws);
    return 0;
}
