/* SPDX-License-Identifier: GPL-3.0-only */
/* Musical invariants: scale-safe harmony, phrase-length recording, additive
 * overdub, undo of a full phrase, migration and mix persistence across sounds. */
#include "../web/emu/felucca_web.c"
#include <assert.h>
static void run(unsigned ms) { for(unsigned n=ms*441/10;n;) { unsigned m=n>128?128:n;web_render(m);n-=m; } }
static void press(unsigned b) { web_buttons(1u<<b);run(25);web_buttons(0);run(25); }
static void wait_record(unsigned state) { unsigned guard=0;while(bongo.record==state && guard++<2000)run(10); assert(guard<2000); }
static int inscale(unsigned n,unsigned root,unsigned minor) {
    static const unsigned masks[]={0xAB5,0x5AD}; return (masks[minor]>>((n+12-root)%12))&1;
}
int main(void) {
    web_nor_erase();web_boot();run(500);
    for(unsigned root=0;root<12;root++) for(unsigned minor=0;minor<2;minor++) for(unsigned prog=0;prog<4;prog++) {
        bongo.root=root;bongo.minor=minor;bongo.progression=prog;
        for(unsigned bar=1;bar<=8;bar++) {
            bongo.bars=bar;bongo_band_pattern();
            for(unsigned t=1;t<3;t++)for(unsigned step=0;step<16;step++)for(unsigned n=0;n<trk[t].step[step].n;n++)
                assert(inscale(trk[t].step[step].note[n],root,minor));
            assert(trk[1].step[0].n<=2);
        }
    }
    puts("PASS: all 12 keys, major/minor, all four progressions remain diatonic");
    bongo.root=0;bongo.minor=0;bongo.progression=1;bongo.assist=1;bc_scale();
    for(unsigned i=0;i<4;i++)trk[i].p[P_LEVEL]=37+i*8;
    bc_instrument(4);bc_style(2);run(50);
    for(unsigned i=0;i<4;i++)assert(trk[i].p[P_LEVEL]==37+(int)i*8);
    puts("PASS: sound/style changes preserve the four-part mixer");
    for(unsigned length=0;length<3;length++) {
        bc_length(length);bongo.overdub=0;
        press(B_REC);assert(song.playing && bongo.record==1 && !song.rec);
        run(1500);assert(bongo.record==1); /* A whole silent visual lead-in, not instant recording. */
        wait_record(1);assert(bongo.record==2 && trk[0].p[P_SLEN]==(16<<length));
        for(unsigned j=0;j<(1u<<length);j++) {
            web_midi(0x90,60+j*2,100);run(90);web_midi(0x80,60+j*2,0);run(210);
        }
        wait_record(2);assert(!song.rec);press(B_PLAY);assert(!song.playing);
        unsigned n=0;for(unsigned i=0;i<(16u<<length);i++)n+=trk[0].step[i].n;
        assert(n>0);
    }
    puts("PASS: complete 1/2/4-bar takes, visual lead-in and automatic loop playback");
    for(unsigned i=0;i<64;i++) bongo_step_note(&trk[0].step[i],60,80);
    step_t original[64];memcpy(original,trk[0].step,sizeof original);
    bongo.overdub=1;unsigned hits=bongo.count[0];press(B_REC);wait_record(1);run(2500);
    assert(bongo.count[0]>hits+10); /* Existing phrase plays while overdubbing. */
    web_midi(0x90,67,100);run(40);web_midi(0x80,67,0);wait_record(2);
    unsigned added=0;for(unsigned i=0;i<64;i++)for(unsigned j=0;j<trk[0].step[i].n;j++)added+=trk[0].step[i].note[j]==67;
    assert(added);press(B_LFO);assert(!memcmp(original,trk[0].step,sizeof original));
    press(B_PLAY);press(B_EDIT);for(unsigned i=0;i<64;i++)assert(!trk[0].step[i].n);
    press(B_LFO);assert(!memcmp(original,trk[0].step,sizeof original));
    puts("PASS: overdub adds notes while the phrase plays; undo restores all 64 steps");
    bc_length(0);assert(trk[0].p[P_SLEN]==16);press(B_LFO);assert(trk[0].p[P_SLEN]==64);
    /* Test v0.1's original 12-char metadata directly, with a tail canary. */
    uint32_t old=0x800000u|2u|(2u<<3)|(3u<<5)|(2u<<7)|(7u<<9)|(1u<<14)|(1u<<15)|(80u<<16);
    snprintf(proj_name,sizeof proj_name,"BONGO %06X",old);
    bongo_step_note(&trk[0].step[3],67,100);int level=trk[0].p[P_LEVEL];
    bongo_boot();assert(bongo.instrument==2 && bongo.style==2 && bongo.hat==3 && bongo.theme==2 && bongo.root==7);
    assert(bongo.length==0 && bongo.progression==0 && !bongo.assist && trk[0].step[3].note[0]==67 && trk[0].p[P_LEVEL]==level);
    assert(!memcmp(proj_name,"BC2",3));
    puts("PASS: v0.1 jam migrates without losing its notes, mix or character");
    return 0;
}
