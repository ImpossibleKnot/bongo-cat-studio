/* SPDX-License-Identifier: GPL-3.0-only */
/* Original procedural cat artwork; no downloaded meme sprites or samples.
 * The same renderer, controls and musical state run on the FM-1 and in WASM. */
static const char *const BC_NAMES[] = {"BONGOS","SOFT KEYS","MEOW","CHIPTUNE","MARIMBA","DRUM KIT","BASS CAT","SPACE BELL"};
static const char *const BC_PRESET[] = {"HAND DRUM","SINE KEY","VOX LEAD","PULSE LD","MARIMBA","TIGHT KIT","SQR BASS","BELL"};
static const uint8_t BC_ENGINE[] = {9,0,5,3,9,10,0,2};
static const char *const BC_STYLE[] = {"LOFI","FUNK","ARCADE"};
static const char *const BC_ROOT[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
static const char *const BC_PROG[]={"HOLD","COZY","DREAM","JAZZ"};
static const char *const BC_COAT[]={"CREAM","TUXEDO","CALICO"};
static const char *const BC_HAT[]={"BARE EARS","HEADPHONES","BERET","SHADES","CROWN","FLOWER"};
static const char *const BC_THEME[]={"MIDNIGHT","MINT","STRAWBERRY","COFFEE"};
static const char *const BC_PAGE[]={"STUDIO","HARMONY","LOOPER","MIXER","DRESS UP","HELP","SOUNDS","TONE","COLOUR","ENVELOPE","EFFECTS","LIVE"};
static const char *const BC_PART[]={"YOUR CAT","KEYS CAT","BASS CAT","DRUM CAT"};
#define BC_SOUND_PAGE 6u
static uint32_t bc_seen_learned;

static void bc_say(const char *s)
{
    bongo.toast = s; bongo.toast_until = fm1_ms + 2400u;
}

static const char *const BC_CLICK[] = {"METRONOME: OFF", "METRONOME: RECORD", "METRONOME: ON"};
static void bc_metronome(uint32_t mode)
{
    mode = mode < 3u ? mode : 0u;
    fm1_irq_off();
    rp_put(RP_CLICK, mode);
    rp_put(RP_COUNTIN, 0); /* The studio uses its own next-bar learning cue. */
    if (!mode) { click_req = 0; clk.env = 0; }
    fm1_irq_on();
    bc_say(BC_CLICK[mode]);
    ui.force = 1;
}

static void bc_preset(track_t *t, uint32_t eng, const char *name)
{
    uint32_t p;
    set_engine_of(t, eng);
    for (p = 0; p < ENGINES[eng]->npresets; p++)
        if (str_eq(ENGINES[eng]->presets[p].name, name)) break;
    apply_preset_to(t, p < ENGINES[eng]->npresets ? p : 0u);
}

static void bc_instrument(uint32_t n)
{
    int16_t level=trk[0].p[P_LEVEL];
    bongo.instrument = (uint8_t)(n % 8u);
    bc_preset(trk, BC_ENGINE[bongo.instrument], BC_PRESET[bongo.instrument]);
    trk[0].p[P_LEVEL] = level;
    trk[0].p[P_TRANS] = bongo.instrument == 0u ? -12 : 0;
    if (bongo.instrument == 2u) {
        /* A short, bright /i/ -> /a/ formant sweep: a playable synthetic meow. */
        trk[0].p[P_E0] = 110; trk[0].p[P_E1] = 18; trk[0].p[P_E2] = 65;
        trk[0].p[P_ATK] = 0; trk[0].p[P_DEC] = 55;
        trk[0].p[P_SUS] = 12; trk[0].p[P_REL] = 35;
    }
    bc_say(BC_NAMES[bongo.instrument]);
}

/* One reversible sound gesture per part. A knob sweep is one gesture;
 * changing its parameter or pausing starts the next. Never copies notes/mix. */
typedef struct {
    int16_t p[P_COUNT];
    uint8_t engine,preset,user,slot,valid,group;
    uint8_t fm6[128];
    uint32_t at;
} bc_sound_copy_t;
static bc_sound_copy_t bc_sound_copy[4];
static void bc_sound_reset(void) { memset(bc_sound_copy,0,sizeof bc_sound_copy); }
static int bc_sound_page(void) { return bongo.help>=BC_SOUND_PAGE && bongo.help<=10u; }
static void bc_sound_checkpoint(track_t *t,uint32_t group)
{
    uint32_t part=(uint32_t)(t-trk); bc_sound_copy_t *s=&bc_sound_copy[part];
    if(!s->valid || !group || s->group!=group || fm1_ms-s->at>750u) {
        for(uint32_t i=0;i<P_COUNT;i++) s->p[i]=(int16_t)motion_base_value(t,i);
        s->engine=t->eng_req; s->preset=t->preset; s->user=t->user;
        s->slot=fm6_slot[part]; fm6_pack(fm6_patch[part],s->fm6);
        s->valid=1;
    }
    s->at=fm1_ms; s->group=(uint8_t)group;
}
static void bc_sound_swap(void)
{
    uint32_t part=bongo.edit_part; track_t *t=&trk[part]; bc_sound_copy_t *s=&bc_sound_copy[part];
    if(!s->valid) { bc_say("EDIT A SOUND TO PAWDO"); return; }
    uint8_t patch[128],v[FP_SIZE+1u],slot=fm6_slot[part];
    fm6_pack(fm6_patch[part],patch); fm6_unpack(s->fm6,v);
    fm1_irq_off();
    uint8_t e=t->eng_req,pr=t->preset,u=t->user;
    t->eng_req=s->engine; t->preset=s->preset; t->user=s->user;
    s->engine=e; s->preset=pr; s->user=u;
    for(uint32_t i=0;i<P_COUNT;i++) if(!param_kept(i)) {
        int16_t value=t->p[i]; t->p[i]=s->p[i]; s->p[i]=value;
    }
    panic_req|=(uint8_t)(1u<<part);
    fm1_irq_on();
    fm6_set_patch(part,v); fm6_slot[part]=s->slot; s->slot=slot;
    fm6_own_ok&=(uint8_t)~(1u<<part); memcpy(s->fm6,patch,sizeof patch);
    s->group=0; sync_reload=1; ui.force=1; bc_say("PAWDO: SOUND A / B");
}
static uint32_t bc_live_state(uint32_t part)
{
    return bongo.solo==part+1u?2u:trk[part].p[P_MUTE]?1u:0u;
}
static void bc_live_turn(uint32_t part,int32_t steps)
{
    uint32_t state=(uint32_t)clamp((int)bc_live_state(part)+steps,0,2);
    if(state==2u) bongo.solo=(uint8_t)(part+1u);
    else { trk[part].p[P_MUTE]=(int16_t)state; if(bongo.solo==part+1u)bongo.solo=0; }
    bongo.toast_until=0; ui.force=1;
}

static void bc_tap_tempo(void)
{
    if(song.g[G_CLOCK]) { bongo.tap_at=0; bc_say("TEMPO FOLLOWS MIDI CLOCK"); return; }
    uint32_t delta=fm1_ms-bongo.tap_at;
    if(bongo.tap_at && delta>=273u && delta<=1500u) {
        if(bongo.tap_n>=4u) { bongo.tap_sum=0; bongo.tap_n=0; }
        bongo.tap_sum+=delta; bongo.tap_n++;
        song.g[G_BPM]=(int16_t)clamp((int)((60000u*bongo.tap_n+bongo.tap_sum/2u)/bongo.tap_sum),40,220);
        bc_say("TEMPO FROM YOUR PAWS");
    } else { bongo.tap_sum=0; bongo.tap_n=0; bc_say("TAP ENV IN TIME"); }
    bongo.tap_at=fm1_ms|1u; ui.force=1;
}

static track_t *bc_edit_track(void) { return &trk[bongo.edit_part%NTRK]; }
static const char *bc_sound_name(const track_t *t)
{
    const engine_t *e=ENGINES[t->eng_req%NENGINES];
    return e->npresets?e->presets[t->preset%e->npresets].name:e->name;
}
static const char *bc_lead_name(void)
{
    return trk[0].eng_req==BC_ENGINE[bongo.instrument] && str_eq(bc_sound_name(trk),BC_PRESET[bongo.instrument])?
        BC_NAMES[bongo.instrument]:bc_sound_name(trk);
}
static void bc_edit_part(uint32_t part)
{
    if(bongo.record) { bc_say("FINISH TAKE TO CHANGE PART"); return; }
    bongo.edit_part=(uint8_t)(part%NTRK); ui.force=1;
}
static void bc_sound_engine(int32_t steps)
{
    track_t *t=bc_edit_track(); uint32_t e=t->eng_req;
    int32_t n=steps%(int32_t)NENG_SHOWN;
    while(n) { e=eng_step(e,n); n+=n>0?-1:1; }
    if(e!=t->eng_req) { bc_sound_checkpoint(t,0); set_engine_of(t,e); }
    bc_say(bc_sound_name(t)); ui.force=1;
}
static void bc_sound_preset(int32_t steps)
{
    track_t *t=bc_edit_track(); const engine_t *e=ENGINES[t->eng_req];
    int32_t count=(int32_t)preset_shown(t->eng_req);
    if(!count) return;
    int32_t rank=((int32_t)preset_rank(e,t->preset)+steps%count+count)%count;
    for(uint32_t p=0;p<e->npresets;p++) if(preset_orig(e,p)==p && !rank--) {
        if(p!=t->preset) { bc_sound_checkpoint(t,0); apply_preset_to(t,p); } break;
    }
    bc_say(bc_sound_name(t)); ui.force=1;
}
static uint32_t bc_edit_id(uint32_t knob)
{
    static const uint8_t envelope[]={P_ATK,P_DEC,P_SUS,P_REL};
    static const uint8_t effects[]={P_DIST,P_CHOR,P_DLY,P_REV};
    knob%=4u;
    return bongo.help==7u?P_E0+knob:bongo.help==8u?P_E4+knob:
        bongo.help==9u?envelope[knob]:effects[knob];
}
static const char *bc_edit_label(uint32_t knob)
{
    static const char *const sound[]={"PART","ENGINE","PRESET","LEVEL"};
    return bongo.help==BC_SOUND_PAGE?sound[knob%4u]:track_desc(bc_edit_track(),bc_edit_id(knob))->label;
}
static void bc_edit_knob(uint32_t knob,int32_t steps)
{
    track_t *t=bc_edit_track();
    if(bongo.help==BC_SOUND_PAGE) {
        if(knob==0u) bc_edit_part((uint32_t)clamp((int)bongo.edit_part+steps,0,3));
        if(knob==1u) bc_sound_engine(steps);
        if(knob==2u) bc_sound_preset(steps);
        if(knob==3u) t->p[P_LEVEL]=(int16_t)clamp(t->p[P_LEVEL]+steps*2,0,110);
    } else {
        uint32_t id=bc_edit_id(knob); const param_desc_t *d=track_desc(t,id);
        if(d->max>d->min) {
            int16_t value=(int16_t)param_turn(d,t->p[id],steps);
            if(value!=t->p[id]) { bc_sound_checkpoint(t,id+1u); t->p[id]=value; }
        }
    }
    ui.force=1;
}

static void bc_bass_tone(void)
{
    track_t *t=&trk[2];
    /* Audible upper harmonics, a rounded attack and a dry, sustained centre.
     * Used only when choosing a style; loading a jam keeps its own sound. */
    t->p[P_ATK]=4; t->p[P_DEC]=64; t->p[P_SUS]=100; t->p[P_REL]=45;
    t->p[P_DLY]=0; t->p[P_REV]=0;
    if (bongo.style==2u) {
        t->p[P_E0]=2; t->p[P_E1]=4; t->p[P_E2]=64; t->p[P_E7]=48; /* WRAM BAS2 */
    } else {
        t->p[P_E0]=1; t->p[P_E4]=62; t->p[P_E5]=12; t->p[P_E6]=0; /* warm low-pass square */
    }
}

static void bc_style(uint32_t n)
{
    int16_t levels[3]={trk[1].p[P_LEVEL],trk[2].p[P_LEVEL],trk[3].p[P_LEVEL]};
    bongo.style = (uint8_t)(n % 3u);
    bc_preset(&trk[1], 0, bongo.style == 2u ? "PLUCK" : "SINE KEY");
    bc_preset(&trk[2], bongo.style == 2u ? 3u : 0u, bongo.style == 2u ? "WAVE BASS" : "SUB BASS");
    bc_preset(&trk[3], 10, bongo.style == 1u ? "TIGHT KIT" : "DEEP KIT");
    trk[1].p[P_LEVEL] = levels[0]; trk[1].p[P_REL] = 40;
    trk[2].p[P_LEVEL] = levels[1]; bc_bass_tone();
    trk[3].p[P_LEVEL] = levels[2];
    bc_say(BC_STYLE[bongo.style]);
}

static void bc_undo_take(void)
{
    fm1_irq_off();
    memcpy(bongo.undo, trk[0].step, sizeof bongo.undo);
    bongo.undo_length=bongo.length;
    fm1_irq_on();
    bongo.undo_valid = 1;
}

static uint32_t bc_down(uint32_t b) { return (fm1_in.buttons >> panel.btn[b]) & 1u; }

static void bc_record_end(void)
{
    song.rec=0; bongo.record=0; bongo.count_in=0;
    trk[0].p[P_SLEN]=(int16_t)(16u<<bongo.length); trk[0].rh_n=0;
}
static void bc_scale(void)
{
    trk[0].p[P_ROOT]=bongo.root;
    trk[0].p[P_SCALE]=bongo.minor?2:1;
    trk[0].p[P_QUANT]=bongo.assist?1:0; /* SNAP: every physical key remains playable. */
    trk[0].p[P_SQNT]=bongo.quantize;
}
static void bc_length(int n)
{
    n=clamp(n,0,2);
    if(n==bongo.length) return;
    if(song.playing || transport_busy() || bongo.record) { bc_say("STOP TO CHANGE LENGTH"); return; }
    bc_undo_take();
    uint32_t old=16u<<bongo.length, length=16u<<(uint32_t)n;
    fm1_irq_off();
    if(length>old) for(uint32_t i=old;i<length;i++) trk[0].step[i]=trk[0].step[i%old];
    bongo.length=(uint8_t)n; trk[0].p[P_SLEN]=(int16_t)length;
    fm1_irq_on(); bc_say("LOOP RESIZED / LFO UNDO");
}
static int bongo_input(uint32_t pressed)
{
    uint32_t b,i,labels=0;
    int32_t s;
    if(!bongo.enabled || ui.uboot) return 0;
    song.sel=bc_sound_page() && !bongo.record?bongo.edit_part:0;
    song.grid=0; kb_mask=perf_mask=kb_lock=0;
    for(b=0;b<NB;b++) labels|=((pressed>>panel.btn[b])&1u)<<b;
    bongo.fill=(uint8_t)bc_down(B_FX);
    if(bongo.fill && !bongo.was_fill) bc_say("MAXIMUM CAT");
    bongo.was_fill=bongo.fill;
    if(bongo.learned!=bc_seen_learned) { bc_seen_learned=bongo.learned; bongo.home_at=fm1_ms; bc_say("WE MADE THAT TOGETHER!"); }
    if(labels&(1u<<B_HOME)) bongo.help=(uint8_t)((bongo.help+1u)%NELEM(BC_PAGE));
    if(labels&(1u<<B_PLAY)) {
        if(song.playing || transport_busy()) { transport_req=2; bc_record_end(); bc_say("TAKE FIVE"); }
        else { bongo.bars=0; transport_req=1; bc_say("LET'S JAM"); }
    }
    if(labels&(1u<<B_REC)) {
        if(bc_sound_page()) { bongo.help=0; song.sel=0; }
        if(bongo.record) { bc_record_end(); bc_say("TAKE FINISHED"); }
        else {
            bc_undo_take(); bongo.loop_on=1;
            fm1_irq_off();
            if(!song.playing) { bongo.count_in=1; trk[0].p[P_SLEN]=16; bongo.bars=0; transport_req=1; }
            bongo.record=1;
            fm1_irq_on(); bc_say("ARMED / WATCH THE BEATS");
        }
    }
    if(labels&(1u<<B_ARP)) { bongo.band^=1u; bc_say(bongo.band?"BAND JOINS NEXT BAR":"SOLO CAT"); }
    if(labels&(1u<<B_SEQ)) { bongo.loop_on^=1u; bc_say(bongo.loop_on?"LOOP ON":"LOOP MUTED"); }
    if(labels&(1u<<B_SCL)) { bongo.minor^=1u; bc_say(bongo.minor?"MINOR MOOD":"MAJOR MOOD"); }
    if(labels&(1u<<B_ENV)) { if(bongo.help==11u) bc_tap_tempo(); else { bongo.hat=(uint8_t)((bongo.hat+1u)%6u); bc_say(BC_HAT[bongo.hat]); } }
    if(labels&(1u<<B_GLO)) { bongo.theme=(uint8_t)((bongo.theme+1u)%4u); bc_say(BC_THEME[bongo.theme]); }
    if(labels&(1u<<B_EDIT)) {
        bc_undo_take(); fm1_irq_off(); bc_record_end();
        for(i=0;i<NSTEP;i++) { memset(&trk[0].step[i],0,sizeof(step_t)); trk[0].step[i].time=ST_REST; }
        fm1_irq_on(); bc_say("CLEAN SLATE / LFO UNDO");
    }
    if(labels&(1u<<B_LFO)) {
        if(bc_sound_page()) bc_sound_swap();
        else if(bongo.help==11u) { bongo.solo=0; bc_say("FULL BAND / SOLO OFF"); }
        else if(bongo.undo_valid) {
            fm1_irq_off(); bc_record_end();
            memcpy(trk[0].step,bongo.undo,sizeof bongo.undo);
            bongo.length=bongo.undo_length; trk[0].p[P_SLEN]=(int16_t)(16u<<bongo.length);
            trk[0].seq_idx%=16u<<bongo.length;
            fm1_irq_on(); bongo.undo_valid=0; bongo.loop_on=1; bc_say("PAWDO: RESTORED");
        } else bc_say("NOTHING TO PAWDO");
    }
    if(labels&(1u<<B_SAVE)) {
        transport_req=2; bc_record_end(); bongo.save_pending=1; bc_say("PAWS OFF KEYS TO SAVE");
    }
    if(labels&(1u<<B_OCTDN)) song.octave=(int8_t)clamp(song.octave-1,-3,3);
    if(labels&(1u<<B_OCTUP)) song.octave=(int8_t)clamp(song.octave+1,-3,3);
    s=panel_enc(EN_SELECT);
    if(s) bongo.help=(uint8_t)(((int)bongo.help+s%(int)NELEM(BC_PAGE)+(int)NELEM(BC_PAGE))%(int)NELEM(BC_PAGE));
    s=panel_enc(EN_PRESET);
    if(s) { if(bc_sound_page()) bc_sound_preset(s); else if(bongo.help!=11u) bc_instrument((uint32_t)((int)bongo.instrument+s%8+8)); }
    s=panel_enc(EN_ALGO);
    if(s) { if(bc_sound_page()) bc_sound_engine(s); else if(bongo.help!=11u) bc_style((uint32_t)((int)bongo.style+s%3+3)); }
    for(i=0;i<4u;i++) {
        s=panel_enc(EN_K1+i); if(!s) continue;
        if(bc_sound_page()) { bc_edit_knob(i,s); continue; }
        switch(bongo.help) {
        case 0:
            if(i==0u) song.g[G_BPM]=(int16_t)clamp(song.g[G_BPM]+s,40,220);
            if(i==1u) song.g[G_SWING]=(int16_t)clamp(song.g[G_SWING]+s*2,0,65);
            if(i==2u) bongo.density=(uint8_t)clamp((int)bongo.density+s*5,0,100);
            if(i==3u) for(b=0;b<4u;b++) trk[b].p[P_REV]=(int16_t)clamp(trk[b].p[P_REV]+s*2,0,90);
            break;
        case 1:
            if(i==0u) bongo.root=(uint8_t)(((int)bongo.root+s%12+12)%12);
            if(i==1u) bongo.minor=(uint8_t)clamp((int)bongo.minor+s,0,1);
            if(i==2u) bongo.progression=(uint8_t)clamp((int)bongo.progression+s,0,3);
            if(i==3u) bongo.assist=(uint8_t)clamp((int)bongo.assist+s,0,1);
            break;
        case 2:
            if(i==0u) bc_length((int)bongo.length+s);
            if(i==1u) { if(bongo.record) bc_say("FINISH TAKE TO CHANGE MODE"); else bongo.overdub=(uint8_t)clamp((int)bongo.overdub+s,0,1); }
            if(i==2u) bongo.quantize=(uint8_t)clamp((int)bongo.quantize+s,0,1);
            if(i==3u) bc_metronome((uint32_t)clamp((int)rp_get(RP_CLICK)+s,0,2));
            break;
        case 3: trk[i].p[P_LEVEL]=(int16_t)clamp(trk[i].p[P_LEVEL]+s*2,0,110); break;
        case 4:
            if(i==0u) bongo.coat=(uint8_t)clamp((int)bongo.coat+s,0,2);
            if(i==1u) bongo.hat=(uint8_t)clamp((int)bongo.hat+s,0,5);
            if(i==2u) bongo.theme=(uint8_t)clamp((int)bongo.theme+s,0,3);
            if(i==3u) bongo.motion=(uint8_t)clamp((int)bongo.motion+s,0,1);
            break;
        case 11: bc_live_turn(i,s); break;
        case 5: if(i==0u) bc_metronome((uint32_t)clamp((int)rp_get(RP_CLICK)+s,0,2)); break;
        }
        if(bongo.help==0u) { bongo.busy=(uint8_t)(i+1u); bongo.toast_until=0; bongo.knob_at=fm1_ms; }
    }
    song.sel=bc_sound_page() && !bongo.record?bongo.edit_part:0;
    bc_scale();
    if(fm1_ms-bongo.draw_at>=(bongo.fill && bongo.motion?66u:100u)) ui.force=1;
    if(fm1_ms-bongo.heat_at>=100u) {
        uint32_t count=bongo.live_count;
        bongo.heat=clamp(bongo.heat-4+(int)(count-bongo.seen_live)*12,0,100);
        bongo.seen_live=count; bongo.heat_at=fm1_ms;
    }
    bongo_save_poll(); return 1;
}

/* The cat gets a tiny stage and an actual 16-step tape. Geometry is clipped
 * against two 120-row strips, so it uses the existing canvas, not more RAM. */
static int bc_y;
static uint16_t bc_bg, bc_ink, bc_paper, bc_pink, bc_accent, bc_dim, bc_desk;
#define BC_RGB(r,g,b) ((uint16_t)(((r)>>3)<<11 | ((g)>>2)<<5 | ((b)>>3)))
static void bc_colours(void)
{
    static const uint16_t backgrounds[4] = {BC_RGB(24,27,45),BC_RGB(25,42,43),BC_RGB(57,29,42),BC_RGB(44,34,30)};
    static const uint16_t accents[4] = {BC_RGB(255,192,103),BC_RGB(139,232,197),BC_RGB(255,164,189),BC_RGB(226,189,145)};
    bc_bg = backgrounds[bongo.theme]; bc_accent = accents[bongo.theme];
    bc_ink = BC_RGB(33,33,48); bc_paper = BC_RGB(255,249,231);
    bc_pink = BC_RGB(247,145,171); bc_dim = BC_RGB(123,132,153); bc_desk = BC_RGB(57,61,80);
}
static void bc_rect(int x,int y,int w,int h,uint16_t c) { cv_rect(x,y-bc_y,w,h,c); }
static void bc_line(int x,int y,int xx,int yy,uint16_t c,int w) { cv_line_t(x,y-bc_y,xx,yy-bc_y,c,w); }
static void bc_text(int x,int y,const char *s,uint16_t c) { cv_text(x,y-bc_y,&AF_S,s,c); }
static void bc_mid(int y,const char *s,uint16_t c) { bc_text((240-text_w(&AF_S,s))/2,y,s,c); }
static void bc_ellipse(int x,int y,int rx,int ry,uint16_t c)
{
    int yy,xx;
    for (yy=-ry; yy<=ry; yy++) {
        if (y+yy < bc_y || y+yy >= bc_y+120) continue;
        for (xx=0; xx<rx && (xx+1)*(xx+1)*ry*ry <= rx*rx*(ry*ry-yy*yy); xx++);
        bc_rect(x-xx,y+yy,2*xx+1,1,c);
    }
}
static void bc_triangle(int x,int y,int w,int h,uint16_t c)
{
    int j;
    for (j=0;j<h;j++) bc_rect(x-w*j/h,y+j,2*w*j/h+1,1,c);
}
static void bc_star(int x,int y,uint16_t c)
{
    bc_line(x-3,y,x+3,y,c,1); bc_line(x,y-3,x,y+3,c,1);
}
static void bc_heart(int x,int y,uint16_t c)
{
    bc_ellipse(x-3,y,4,3,c); bc_ellipse(x+3,y,4,3,c);
    for(int j=0;j<7;j++) bc_rect(x-6+j,y+j,13-2*j,1,c);
}
static void bc_party(void)
{
    /* A bounded, deterministic particle field: no allocation or random work
     * in audio, and no full-screen flashes. CALM keeps the party still. */
    uint32_t tick=bongo.motion?fm1_ms/66u:0u;
    for (uint32_t i=0;i<18u;i++) {
        int side=(int)(i&1u), x=9+(int)((i*13u+tick*(1u+i%2u))%43u);
        int y=61+(int)((i*29u+tick*(2u+i%3u))%120u);
        if(side) x=240-x;
        uint16_t c=i%3u==0u?bc_pink:i%3u==1u?bc_accent:bc_paper;
        if(i%5u==0u) bc_heart(x,y,c);
        else if(i%3u==0u) bc_star(x,y,c);
        else bc_line(x,y,x+((tick+i)&1u?3:-3),y+4,c,2);
    }
    for(int i=0;i<6;i++) {
        int x=13+i*42;
        bc_line(x,58,x+28,61,bc_dim,1);
        bc_triangle(x+12,58,5,7,i&1?bc_pink:bc_accent);
    }
}
static void bc_secret(void)
{
    if(!bongo.egg_at || fm1_ms-bongo.egg_at>=5000u) return;
    uint32_t tick=bongo.motion?(fm1_ms-bongo.egg_at)/100u:0;
    if(bongo.egg==1u) {
        bc_rect(15,115,23,25,bc_accent); bc_star(26,128,bc_ink);
        bc_text(12,145,"JAZZ",bc_pink);
    } else if(bongo.egg==2u) {
        bc_rect(18,136,24,20,bc_paper); bc_ellipse(44,143,7,7,bc_paper);
        bc_ellipse(44,143,4,4,bc_bg); bc_heart(30,142,bc_pink);
        for(int j=0;j<3;j++) { int x=22+j*7,y=128-(int)(tick%7u);bc_line(x,y,x+2,y-5,bc_dim,1); }
        bc_text(13,173,"CAFE",bc_accent);
    } else if(bongo.egg==3u) {
        for(uint32_t j=0;j<8;j++) {
            int x=12+(int)((j*29u+tick*2u)%216u),y=68+(int)(j*19u%90u);
            if(x<57 || x>185) bc_star(x,y,j&1?bc_pink:bc_accent);
        }
        bc_ellipse(208,88,12,9,bc_pink);bc_line(190,94,225,80,bc_accent,2);
        bc_text(12,178,"PURRBIT",bc_accent);
    }
}
static const char *bc_secret_text(void)
{
    if(!bongo.egg_at || fm1_ms-bongo.egg_at>=5000u) return 0;
    return bongo.egg==1u?"JAZZ POLICE. PAWS UP.":bongo.egg==2u?"CAFE OPEN. LOFI ON TAP.":"ONE SMALL STEP FOR CAT.";
}

static void bc_cat(int bob,int left,int right,int sleepy)
{
    int x=120,y=109+bob,eye=(int)(bongo.note[0]%12u)/4-1;
    uint16_t fur=bongo.coat==1u?BC_RGB(74,76,94):bc_paper;
    /* A little window, plant and mug make this a studio the cat lives in. */
    bc_rect(16,68,30,35,bc_desk); bc_rect(19,71,24,29,bc_bg);
    bc_ellipse(34,80,5,5,bc_accent); bc_line(31,70,31,101,bc_desk,2);
    bc_line(18,87,43,87,bc_desk,2);
    bc_rect(205,137,14,12,bc_pink); bc_line(212,137,211,117,bc_accent,2);
    bc_ellipse(206,122,5,3,bc_accent); bc_ellipse(217,127,5,3,bc_accent);
    /* curved tail, ears, round body */
    bc_ellipse(182,133+bob,22,16,bc_ink); bc_ellipse(182,130+bob,17,12,fur);
    bc_ellipse(179,126+bob,12,9,bc_bg);
    bc_triangle(76,59+bob,17,40,bc_ink); bc_triangle(76,65+bob,12,31,fur);
    bc_triangle(165,59+bob,17,40,bc_ink); bc_triangle(165,65+bob,12,31,fur);
    bc_triangle(76,72+bob,7,20,bc_pink); bc_triangle(165,72+bob,7,20,bc_pink);
    bc_ellipse(x,y+7,62,46,bc_ink); bc_ellipse(x,y+5,58,43,fur);
    if(bongo.coat==1u) bc_ellipse(120,y+7,46,30,bc_paper);
    if(bongo.coat==2u) { bc_ellipse(153,y-25,16,12,bc_accent); bc_ellipse(70,y+6,7,18,BC_RGB(105,89,86)); }
    if (sleepy || (!bongo.fill && bongo.motion && fm1_ms%4300u<150u)) {
        bc_line(87,y-3,102,y-3,bc_ink,2); bc_line(137,y-3,152,y-3,bc_ink,2);
        if(sleepy) bc_text(180,72,"z z",bc_dim);
    } else if (bongo.heat>75 || bongo.fill) {
        bc_line(89,y-7,97,y-1,bc_ink,2); bc_line(97,y-1,89,y+5,bc_ink,2);
        bc_line(149,y-7,141,y-1,bc_ink,2); bc_line(141,y-1,149,y+5,bc_ink,2);
        bc_ellipse(173,98+bob,3,5,bc_accent);
    } else {
        bc_ellipse(96+eye,y-2,6,8,bc_ink); bc_ellipse(144+eye,y-2,6,8,bc_ink);
        bc_ellipse(94+eye,y-5,2,2,bc_paper); bc_ellipse(142+eye,y-5,2,2,bc_paper);
        bc_rect(98+eye,y+1,1,2,bc_paper); bc_rect(146+eye,y+1,1,2,bc_paper);
    }
    bc_ellipse(81,y+10,7,3,bc_pink); bc_ellipse(159,y+10,7,3,bc_pink);
    bc_triangle(120,y+8,3,4,bc_pink);
    bc_line(120,y+12,115,y+17,bc_ink,2); bc_line(115,y+17,110,y+14,bc_ink,2);
    bc_line(120,y+12,125,y+17,bc_ink,2); bc_line(125,y+17,130,y+14,bc_ink,2);
    if(bongo.fill) { bc_ellipse(120,y+24,8,6,bc_ink); bc_ellipse(120,y+27,5,2,bc_pink); }
    if (bongo.instrument==2 && fm1_ms-bongo.hit[0]<200u) bc_ellipse(120,y+23,5,5,bc_ink);
    if (bongo.hat==1u || bongo.fill) {
        bc_ellipse(120,80+bob,48,18,bc_accent); bc_ellipse(120,86+bob,42,18,bc_paper);
        bc_rect(69,87+bob,9,28,bc_ink); bc_rect(164,87+bob,9,28,bc_ink);
        bc_rect(68,90+bob,7,22,bc_accent); bc_rect(167,90+bob,7,22,bc_accent);
    } else if (bongo.hat==2u) {
        bc_ellipse(120,77+bob,29,9,bc_pink); bc_line(124,67+bob,128,62+bob,bc_pink,3);
    } else if (bongo.hat==3u) {
        bc_rect(83,y-8,26,14,bc_ink); bc_rect(132,y-8,26,14,bc_ink);
        bc_line(108,y-4,132,y-4,bc_ink,3);
        bc_line(89,y-5,96,y+1,bc_paper,1); bc_line(138,y-5,145,y+1,bc_paper,1);
    }
    if(bongo.hat==4u) {
        bc_rect(101,70+bob,38,8,bc_accent);
        for(int j=0;j<3;j++) { bc_triangle(105+j*15,59+bob,6,14,bc_accent); bc_ellipse(105+j*15,59+bob,2,2,bc_pink); }
    } else if(bongo.hat==5u) {
        for(int j=-1;j<=1;j++) { bc_ellipse(158+j*5,75+bob,4,4,bc_pink); bc_ellipse(158,75+bob+j*5,4,4,bc_pink); }
        bc_ellipse(158,75+bob,3,3,bc_accent);
    }
    /* instrument sits in front of the body */
    bc_rect(17,164,206,5,bc_accent); bc_rect(22,169,196,19,bc_desk);
    if (bongo.instrument==0u || bongo.instrument==5u) {
        bc_rect(49,148,54,27,bc_ink); bc_rect(53,150,46,23,bc_pink);
        bc_ellipse(76,149,28,10,bc_ink); bc_ellipse(76,147,25,8,bc_paper);
        bc_rect(137,152,54,24,bc_ink); bc_rect(141,154,46,20,bc_accent);
        bc_ellipse(164,153,28,10,bc_ink); bc_ellipse(164,151,25,8,bc_paper);
        bc_line(60,153,64,173,bc_paper,2); bc_line(91,153,87,173,bc_paper,2);
        bc_line(148,157,152,173,bc_paper,2); bc_line(179,157,175,173,bc_paper,2);
    } else if (bongo.instrument==6u) {
        bc_ellipse(86,157,28,17,bc_ink); bc_ellipse(86,157,24,14,bc_pink);
        bc_rect(88,149,102,13,bc_ink); bc_rect(91,152,102,7,bc_accent);
        for(int j=0;j<3;j++) bc_line(68,154+j*3,191,154+j*3,bc_paper,1);
    } else {
        bc_rect(40,146,160,33,bc_ink); bc_rect(44,150,152,25,bc_paper);
        for(int j=1;j<12;j++) bc_line(44+j*13,151,44+j*13,174,bc_ink,1);
        for(int j=0;j<11;j++) if (j%7!=2 && j%7!=6) bc_rect(52+j*13,150,7,13,bc_ink);
        if (fm1_ms-bongo.hit[0]<170u) bc_rect(44+(int)(bongo.note[0]%12u)*13,165,11,9,bc_pink);
    }
    /* paws visibly strike, then recover; positions follow pitch for keys */
    int lx = bongo.instrument==0u || bongo.instrument==5u ? 76 : 81;
    int rx = bongo.instrument==0u || bongo.instrument==5u ? 164 : 159;
    uint32_t age=fm1_ms-bongo.hit[0];
    int lift=bongo.motion && age<240u?(int)(240u-age)*14/240:0;
    int ly=134+(left?lift:0), ry=135+(right?lift:0);
    if(bongo.fill && bongo.motion) {
        /* The fill animates itself even when no lead key is being played. */
        int stroke=(int)((fm1_ms/45u)%8u);
        int reach=stroke<4?stroke*8:(7-stroke)*8;
        ly=151-reach; ry=127+reach;
        lx-=4; rx+=4;
        for(int j=0;j<3;j++) {
            bc_line(lx-22-j*3,ly-9,lx-18-j*3,ly+7,bc_accent,1);
            bc_line(rx+18+j*3,ry-9,rx+22+j*3,ry+7,bc_pink,1);
        }
    }
    bc_ellipse(lx,ly,16,10,bc_ink); bc_ellipse(lx,ly-1,13,8,bc_paper);
    bc_ellipse(rx,ry,16,10,bc_ink); bc_ellipse(rx,ry-1,13,8,bc_paper);
    for(int j=-1;j<=1;j++) {
        bc_line(lx+j*5,ly+3,lx+j*5,ly+6,bc_pink,1);
        bc_line(rx+j*5,ry+3,rx+j*5,ry+6,bc_pink,1);
    }
    if(left) { bc_star(47,133,bc_accent); bc_star(56,124,bc_pink); }
    if(right) { bc_star(195,137,bc_accent); bc_star(184,126,bc_pink); }
    if(bongo.motion && bongo.hit[0] && age<800u) {
        int rise=(int)age/28;
        bc_heart((bongo.count[0]&1u)?193:48,113-rise,bc_pink);
    }
    if(bongo.motion && bongo.home_at && fm1_ms-bongo.home_at<2800u) {
        int rise=(int)(fm1_ms-bongo.home_at)%900/30;
        bc_heart(54,92-rise,bc_pink); bc_heart(190,100-rise,bc_accent);
    }
}

static void bc_scene(void)
{
    char num[12], line[30];
    uint32_t now=fm1_ms, recent=now-bongo.hit[0]<170u && bongo.hit[0];
    int left=recent && !(bongo.count[0]&1u), right=recent && (bongo.count[0]&1u);
    int sleepy=now-bongo.live_at>16000u && !song.playing && !bongo.fill;
    int bob=0;
    if(bongo.motion) {
        uint32_t phase=song.playing?(uint32_t)(beat_pos*16u/(click_beat_len()?click_beat_len():1u)):(now/150u)%16u;
        static const int8_t wave[16]={0,1,2,3,3,3,2,1,0,-1,-2,-2,-2,-2,-1,0};
        bob=bongo.fill?wave[(now/45u)%16u]*3:wave[phase%16u];
    }
    if(bongo.fill) { left=bongo.motion?(now/90u)&1u:1; right=bongo.motion?!left:1; }
    bc_text(11,8,bongo.fill?"MAXIMUM CAT":bongo.help==4u?"DRESS UP":"BONGO CAT",bc_paper);
    fmt_int(num,song.g[G_BPM]); cv_text_r(228,8-bc_y,&AF_S,num,bc_accent,bc_bg);
    bc_text(11,24,bongo.help==4u?BC_COAT[bongo.coat]:bc_lead_name(),bc_accent);
    if(bongo.help==4u) cv_text_r(228,24-bc_y,&AF_S,BC_HAT[bongo.hat],bc_pink,bc_bg);
    if(bongo.help!=4u) { bc_rect(155,26,73,4,bc_desk); bc_rect(155,26,bongo.fill?73:bongo.heat*73/100,4,bc_pink); }
    const char *msg = bongo.record==2u ? "I'M LISTENING..." : bongo.record==1u ? "GET READY: 1   2   3   4" :
        bc_secret_text() ? bc_secret_text() :
        (int32_t)(bongo.toast_until-now)>0 ? bongo.toast :
        sleepy ? "WAKE ME WITH A NOTE" : bongo.heat>75 ? "PAWS IN THE POCKET" :
        song.playing ? "SMALL CAT. BIG GROOVE." : "PLAY A NOTE. I'M WITH YOU.";
    if(bongo.help==4u) msg="K1 COAT / K2 HAT / K3 ROOM";
    if(bongo.help==0u && bongo.busy && now-bongo.knob_at<1800u) {
        static const char *const label[]={"TEMPO ","SWING ","ENERGY ","ROOM "};
        int values[4]={song.g[G_BPM],song.g[G_SWING],bongo.density,trk[0].p[P_REV]};
        str_cpy(line,label[bongo.busy-1u],sizeof line); fmt_int(num,values[bongo.busy-1u]);
        str_cpy(line+str_len(line),num,sizeof line-str_len(line)); msg=line;
    }
    if(bongo.fill) msg="TINY PAWS. ABSOLUTE CHAOS.";
    bc_mid(43,msg?msg:"LET'S MAKE A LITTLE NOISE",bongo.record?bc_pink:bc_dim);
    if(bongo.record==1u) for(uint32_t i=0;i<4u;i++) bc_ellipse(89+(int)i*21,60,3,3,bongo.step/4u==i?bc_pink:bc_desk);
    if(bongo.fill) bc_party();
    bc_cat(bob,left,right,sleepy);
    bc_secret();
    for(uint32_t i=0;i<16u;i++) {
        uint32_t at=(trk[0].seq_idx/16u)*16u+i;
        if(!song.playing) at=i;
        int on=trk[0].step[at].time==ST_NOTE && (trk[0].step[at].n || trk[0].step[at].hit);
        uint16_t c=song.playing && bongo.step==i?bc_paper:on?(bongo.loop_on?bc_accent:bc_dim):bc_desk;
        bc_rect(12+(int)i*14,192,10,5,c);
    }
    for(uint32_t i=0;i<(1u<<bongo.length);i++) bc_rect(108+(int)i*8,199,5,2,song.playing && trk[0].seq_idx/16u==i?bc_pink:bc_dim);
    static const char *const role[3]={"KEYS","BASS","DRUMS"};
    for(uint32_t i=0;i<3u;i++) {
        int x=12+(int)i*77, active=bongo.hit[i+1] && now-bongo.hit[i+1]<160u;
        int jump=bongo.fill && bongo.motion?(int)((now/90u+i)%4u)*2:0;
        uint16_t c=bongo.band && trk[i+1u].p[P_LEVEL] && bc_part_audible(i+1u)?active?bc_accent:bc_dim:bc_desk;
        bc_triangle(x+6,202-jump,4,6,c); bc_triangle(x+17,202-jump,4,6,c);
        bc_ellipse(x+12,212-jump,10,7,c);
        bc_rect(x+8,210-jump,2,2,bc_ink); bc_rect(x+15,210-jump,2,2,bc_ink);
        bc_text(x+27,207,role[i],bongo.band && trk[i+1u].p[P_LEVEL] && bc_part_audible(i+1u)?bc_paper:bc_dim);
    }
    str_cpy(line,BC_STYLE[bongo.style],sizeof line);
    str_cpy(line+str_len(line)," / ",sizeof line-str_len(line));
    str_cpy(line+str_len(line),BC_ROOT[(song.playing?bongo.chord:bongo.root)%12u],sizeof line-str_len(line));
    if(song.playing?bongo.chord_minor:bongo.minor) str_cpy(line+str_len(line),"m",sizeof line-str_len(line));
    if(bongo.help==4u) {
        bc_text(11,225,BC_THEME[bongo.theme],bc_dim);
        cv_text_r(228,225-bc_y,&AF_S,bongo.motion?"K4: LIVELY":"K4: CALM",bc_accent,bc_bg);
    } else {
        bc_text(11,225,line,bc_dim);
        char state[16];
        if(song.playing) {
            fmt_int(state,(int)(trk[0].seq_idx/16u)+1); str_cpy(state+str_len(state),"/",sizeof state-str_len(state));
            fmt_int(num,1<<bongo.length); str_cpy(state+str_len(state),num,sizeof state-str_len(state));
            str_cpy(state+str_len(state),bongo.record==2u?" REC":bongo.record?" WAIT":" LIVE",sizeof state-str_len(state));
        } else str_cpy(state,"HOME: PAGES",sizeof state);
        cv_text_r(228,225-bc_y,&AF_S,state,bongo.record?bc_pink:bc_accent,bc_bg);
    }
}

static void bc_sound_editor(void)
{
    track_t *t=bc_edit_track(); char val[24],num[5]; const char *unit;
    bc_text(12,10,BC_PART[bongo.edit_part],bc_pink);
    cv_text_r(228,10-bc_y,&AF_S,BC_PAGE[bongo.help],bc_accent,bc_bg);
    bc_text(12,29,bc_sound_name(t),bc_paper); bc_rect(12,44,216,1,bc_desk);
    for(uint32_t i=0;i<4u;i++) {
        int y=55+(int)i*32; const char *value=val;
        fmt_int(num,(int)i+1);bc_text(12,y,num,bc_pink);bc_text(32,y,bc_edit_label(i),bc_dim);
        if(bongo.help==BC_SOUND_PAGE) {
            if(i==0u) value=BC_PART[bongo.edit_part];
            else if(i==1u) value=ENGINES[t->eng_req]->name;
            else if(i==2u) value=bc_sound_name(t);
            else { fmt_int(val,t->p[P_LEVEL]); if(!t->p[P_LEVEL] || t->p[P_MUTE]) value="MUTED"; }
        } else {
            uint32_t id=bc_edit_id(i);const param_desc_t *d=track_desc(t,id);
            param_format(d,t->p[id],val,&unit);
            if(*unit) { str_cpy(val+str_len(val)," ",sizeof val-str_len(val));str_cpy(val+str_len(val),unit,sizeof val-str_len(val)); }
        }
        cv_text_r(228,y-bc_y,&AF_S,value,bc_paper,bc_bg);
    }
    bc_mid(190,"PRESETS: SOUND / ALGO: ENGINE",bc_accent);
    bc_mid(207,bongo.record?"KEYS RECORD YOUR CAT":"KEYS PLAY THE SELECTED CAT",bc_dim);
    bc_mid(225,"LFO: SOUND A/B / SAVE: KEEP",bc_dim);
}

static void bc_help(void)
{
    char num[16];
    bc_text(12,10,"TINY STUDIO",bc_paper);
    cv_text_r(228,10-bc_y,&AF_S,BC_PAGE[bongo.help],bc_accent,bc_bg);
    bc_rect(12,30,216,1,bc_desk);
    if(bongo.help==5u) {
        const char *a[]={"SELECT / HOME: change page","SOUNDS K1: choose a cat","TONE / COLOUR: its synth","ENVELOPE / EFFECTS: shape","REC: lead take / LFO: undo","ARP: band / SEQ: loop mute","LIVE: mutes / solo / tap","SAVE: stop + remember"};
        for(uint32_t i=0;i<8u;i++) bc_text(12,44+(int)i*20,a[i],bc_paper);
        bc_text(12,211,"K1",bc_dim); bc_text(38,211,BC_CLICK[rp_get(RP_CLICK)],bc_accent); return;
    }
    const char *labels[4], *values[4]; char nums[4][16];
    if(bongo.help==1u) {
        labels[0]="KEY"; values[0]=BC_ROOT[bongo.root];
        labels[1]="MOOD"; values[1]=bongo.minor?"MINOR":"MAJOR";
        labels[2]="CHORDS"; values[2]=BC_PROG[bongo.progression];
        labels[3]="KEY ASSIST"; values[3]=bongo.assist?"IN KEY":"FREE";
    } else if(bongo.help==2u) {
        labels[0]="LENGTH"; values[0]=bongo.length==0u?"1 BAR":bongo.length==1u?"2 BARS":"4 BARS";
        labels[1]="RECORD"; values[1]=bongo.overdub?"OVERDUB":"REPLACE";
        labels[2]="TIMING"; values[2]=bongo.quantize?"QUANTIZED":"AS PLAYED";
        labels[3]="METRONOME"; values[3]=rp_get(RP_CLICK)==0u?"OFF":rp_get(RP_CLICK)==1u?"RECORD":"ON";
    } else {
        labels[0]="YOUR CAT"; labels[1]="KEYS CAT"; labels[2]="BASS CAT"; labels[3]="DRUM CAT";
        for(uint32_t i=0;i<4u;i++) {
            fmt_int(nums[i],trk[i].p[P_LEVEL]); values[i]=trk[i].p[P_LEVEL] && !trk[i].p[P_MUTE]?nums[i]:"MUTED";
            if(bongo.help==11u) values[i]=bc_live_state(i)==2u?"SOLO":bc_live_state(i)==1u?"MUTED":bongo.solo?"WAITING":"PLAY";
        }
    }
    for(uint32_t i=0;i<4u;i++) {
        int y=46+(int)i*36;
        fmt_int(num,(int)i+1); bc_text(12,y,num,bc_pink); bc_text(34,y,labels[i],bc_dim);
        cv_text_r(228,y-bc_y,&AF_S,values[i],bc_paper,bc_bg);
        if(bongo.help==3u) { bc_rect(34,y+19,194,3,bc_desk); bc_rect(34,y+19,trk[i].p[P_LEVEL]*194/110,3,bc_accent); }
    }
    const char *hint=bongo.help==1u?"A band that stays in your key":bongo.help==2u?"REC waits, listens, then loops":bongo.help==11u?"TURN: PLAY / MUTE / SOLO":"Four friends. Your balance.";
    bc_mid(198,hint,bc_accent);
    bc_mid(222,bongo.help==11u?"ENV: TAP TEMPO / LFO: UNSOLO":"HOME / SELECT: NEXT PAGE",bc_dim);
}

static void bongo_draw(void)
{
    if (fm1_ms-bongo.draw_at<66u && !ui.force) return;
    bongo.draw_at=fm1_ms; bc_colours();
    for(bc_y=0;bc_y<240;bc_y+=120) {
        cv_begin(240,120,bc_bg);
        if(bc_sound_page() && !bongo.fill) bc_sound_editor();
        else if(bongo.help && bongo.help!=4u && !bongo.fill) bc_help(); else bc_scene();
        if(bongo.help && bongo.help!=4u && !bongo.fill && (int32_t)(bongo.toast_until-fm1_ms)>0) {
            bc_rect(0,220,240,20,bc_bg); bc_mid(225,bongo.toast,bc_accent);
        }
        cv_blit(0,(uint32_t)bc_y);
    }
    ui.force=0;
}

static void bongo_leds(void)
{
    uint8_t on[FM1_NCOL]={0},dim[FM1_NCOL]={0};
    if (fm1_led_anim_on()) return;
    led_pos_init();
    for(uint32_t i=0;i<27u;i++) {
        uint32_t lit=(fm1_in.notes>>i)&1u;
        if(song.playing && key_place(i)==bongo.step && !key_black(i)) lit=1;
        if(bongo.fill && bongo.motion && (i+fm1_ms/70u)%9u<3u) lit=1;
        led_put(on,i+14u,(int)lit);
        led_put(dim,i+14u,!lit);
    }
    led_put(on,panel.btn[B_PLAY],song.playing);
    led_put(on,panel.btn[B_REC],bongo.record==2u || (bongo.record==1u && (fm1_ms/200u)%2u));
    led_put(on,panel.btn[B_ARP],bongo.band);
    led_put(on,panel.btn[B_SEQ],bongo.loop_on);
    led_put(on,panel.btn[B_FX],bongo.fill);
    for(uint32_t i=0;i<FM1_NCOL;i++) { fm1_led[i]=on[i]; fm1_led_dim[i]=dim[i]; fm1_led_breath[i]=0; fm1_led_mid[i]=0; }
}
