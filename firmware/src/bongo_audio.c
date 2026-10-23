/* SPDX-License-Identifier: GPL-3.0-only */
/* Called only by the existing audio/keyboard paths. No drawing, allocation,
 * flash writes, hardware accesses or main-loop locks in these callbacks. */
static int bc_part_audible(uint32_t part)
{
    return bongo.solo?bongo.solo==part+1u:!trk[part].p[P_MUTE];
}
static void bongo_hit(track_t *t, uint32_t note)
{
    uint32_t i = (uint32_t)(t - trk);
    if (!bongo.enabled || i >= 4u) return;
    bongo.note[i] = (uint8_t)note;
    bongo.hit[i] = fm1_ms | 1u;
    bongo.count[i]++;
    if (i == 3u && (note == 35u || note == 36u) && t->p[P_LEVEL] && !(pf.mute & 8u))
        bongo.kick_at = fm1_ms;
}

/* Give the kick's transient room, then let the sustained bass bloom. The
 * gain is smoothed per sample; neither stored mixer levels nor notes change.
 * Both stereo components follow the same envelope, including spread voices. */
static void bongo_bass_mix(track_t *t, int32_t *b, int32_t *side, uint32_t n)
{
    if (!bongo.enabled) return;
    uint32_t part=(uint32_t)(t-trk);
    if(part>=4u) return;
    int32_t mix=bongo.mix_gain[part];
    int32_t goal=bc_part_audible(part)?4096:0;
    if(mix!=4096 || goal!=4096) for(uint32_t i=0;i<n;i++) {
        int32_t d=goal-mix;
        mix+=d>0?(d+63)/64:(d-63)/64;
        b[i]=((b[i]>>2)*mix)>>10;
        if(side) side[i]=((side[i]>>2)*mix)>>10;
    }
    bongo.mix_gain[part]=mix;
    if(t!=&trk[2]) return;
    uint32_t age = fm1_ms - bongo.kick_at;
    int32_t target = 4096, gain = bongo.bass_duck;
    if (bongo.kick_at && age < 160u)
        target = age < 12u ? 1843 : 1843 + (int32_t)(age - 12u) * 2253 / 148;
    if (target == 4096 && gain == 4096) return;
    for (uint32_t i = 0; i < n; i++) {
        int32_t delta = target - gain;
        gain += delta > 0 ? (delta + 63) / 64 : (delta - 63) / 64;
        b[i] = ((b[i] >> 2) * gain) >> 10;
        if (side) side[i] = ((side[i] >> 2) * gain) >> 10;
    }
    bongo.bass_duck = gain;
}

static void bongo_live(track_t *t, uint32_t note)
{
    static const uint8_t lick[7]={0,2,3,5,2,10,0};
    static const uint8_t coffee[4]={0,9,5,4}, orbit[5]={0,3,7,0,7};
    if(!bongo.enabled || t!=trk) return;
    bongo.live_at=fm1_ms|1u; bongo.live_count++;
    if(drum_track(t)) return;
    if(fm1_ms-bongo.motif_at>1800u) bongo.motif_n=0;
    bongo.motif_at=fm1_ms;
    for(uint32_t i=0;i<6u;i++) bongo.motif[i]=bongo.motif[i+1u];
    bongo.motif[6]=(uint8_t)(note%12u);
    if(bongo.motif_n<7u) bongo.motif_n++;
    const uint8_t *patterns[3]={lick,coffee,orbit};
    const uint8_t lengths[3]={7,4,5};
    for(uint32_t k=0;k<3u;k++) {
        uint32_t n=lengths[k], start=7u-n, match=bongo.motif_n>=n;
        for(uint32_t i=0;i<n && match;i++)
            if((bongo.motif[start+i]+12u-bongo.motif[start])%12u!=patterns[k][i]) match=0;
        if(match) {
            bongo.egg=(uint8_t)(k+1u); bongo.egg_at=fm1_ms|1u;
            if(!k) bongo.jazz_at=bongo.egg_at;
            bongo.motif_n=0;
            break;
        }
    }
}

static void bongo_step_note(step_t *s, uint32_t note, uint32_t vel)
{
    memset(s, 0, sizeof *s);
    s->time = ST_NOTE; s->n = 1; s->note[0] = (uint8_t)note; s->vel = (uint8_t)vel;
}

static void bongo_bass_ties(void)
{
    uint32_t held = 0;
    for (uint32_t i=0; i<16u; i++) {
        step_t *bs=&trk[2].step[i];
        if (bs->time==ST_NOTE) held=3;
        else if (held && bongo.band) { bs->time=ST_TIE; held--; }
    }
}

/* Diatonic harmony stays in the chosen key. Bass supplies the root; two
 * upper notes voice-lead between chords, leaving voices for the player's part. */
static void bongo_band_pattern(void)
{
    static const uint8_t major[7]={0,2,4,5,7,9,11}, minor[7]={0,2,3,5,7,8,10};
    static const uint8_t progression[4][4]={{0,0,0,0},{0,5,3,4},{0,3,5,4},{1,4,0,5}};
    const uint8_t *scale=bongo.minor?minor:major;
    uint32_t bar=bongo.bars?bongo.bars-1u:0u;
    uint32_t degree=progression[bongo.progression][bar%4u], root=(bongo.root+scale[degree])%12u;
    uint32_t third=(scale[(degree+2u)%7u]+12u-scale[degree])%12u;
    uint32_t fifth=(scale[(degree+4u)%7u]+12u-scale[degree])%12u;
    uint32_t dense=bongo.density, style=bongo.style;
    uint32_t target[2]={(root+third)%12u,(root+fifth)%12u};
    bongo.chord=(uint8_t)root; bongo.chord_minor=(uint8_t)(third==3u);
    for(uint32_t v=0;v<2u;v++) {
        int best=60+(int)target[v], distance=100;
        int previous=bongo.voicing[v]?bongo.voicing[v]:(v?67:60);
        for(int n=48+(int)target[v];n<=79;n+=12) {
            int d=n>previous?n-previous:previous-n;
            if(d<distance) { best=n; distance=d; }
        }
        bongo.voicing[v]=(uint8_t)best;
    }
    for (uint32_t i=0;i<16u;i++) {
        step_t *ch=&trk[1].step[i], *bs=&trk[2].step[i], *dr=&trk[3].step[i];
        memset(ch,0,sizeof *ch); ch->time=ST_REST;
        memset(bs,0,sizeof *bs); bs->time=ST_REST;
        memset(dr,0,sizeof *dr); dr->time=ST_REST;
        if (!bongo.band) continue;
        if (i==(style==1u?2u:0u) || i==8u || (dense>70u && i==14u)) {
            bongo_step_note(ch,bongo.voicing[0],i==0u?64u:53u);
            ch->n=2; ch->note[1]=bongo.voicing[1];
        }
        if (i==0u || i==8u || (dense>25u && i==6u) || (dense>55u && i==10u) || (dense>80u && i==15u))
            bongo_step_note(bs,36u+root+(i==6u?fifth:i==15u?12u:0u),i%8u?74u:94u);
        dr->time=ST_NOTE; dr->vel=(uint8_t)(i%4u==0u?88u:i%2u==0u?66u:48u);
        if(i==0u || i==8u || (style==1u && (i==6u || i==11u))) dr->hit|=1u;
        if(i==4u || i==12u) dr->hit|=2u;
        if(!(i%2u) || (dense>65u && ((i+bar)%4u==3u))) dr->hit|=8u;
        if(i==14u && dense>35u) dr->hit=(uint8_t)((dr->hit&~8u)|16u);
        if(bar%4u==3u && i>=14u && dense>55u) dr->hit|=32u;
        if(!dr->hit) dr->time=ST_REST;
    }
    /* Keep every bass onset and pitch. Sustain for up to a quarter note,
     * stopping early for the next note; ties never cross the bar boundary. */
    bongo_bass_ties();
}

static void bongo_enter(track_t *t)
{
    uint32_t idx;
    if (!bongo.enabled || t != trk) return;
    idx = t->seq_idx % 16u;
    bongo.step = (uint8_t)idx;
    if (!idx) {
        bongo.bars++;
        if (!t->seq_idx) {
            if (bongo.record==1u && bongo.count_in) bongo.count_in=0;
            else if (bongo.record==1u) {
                if(!bongo.overdub) {
                    memset(trk[0].step,0,sizeof trk[0].step);
                    for(uint32_t i=0;i<NSTEP;i++) trk[0].step[i].time=ST_REST;
                }
                trk[0].p[P_SLEN]=(int16_t)(16u<<bongo.length);
                bongo.bars=1;
                song.rec=1u; bongo.record=2;
            } else if (bongo.record==2u) {
                song.rec=0; bongo.record=0; bongo.learned++;
                bongo.loop_on=1; trk[0].rh_n=0;
            }
        }
        bongo_band_pattern();
    }
    /* Momentary fills do not overwrite the drummer's saved groove. */
    if (bongo.fill && bongo.band) {
        trk_note_on(&trk[3], (idx & 1u) ? 38u : 45u, 82u + idx * 2u);
    }
}
