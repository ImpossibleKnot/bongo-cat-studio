/* SPDX-License-Identifier: GPL-3.0-only */
/* Reuses Felucca's CRC-checked, redundant autosave. The 12-character project
 * name is uppercase-normalised by project.c: BC2 plus nine BASE32 characters
 * is a versioned 45-bit tag. No new flash addresses or project-slot writes. */
static void bc_metadata(void)
{
    static const char alphabet[]="0123456789ABCDEFGHIJKLMNOPQRSTUV";
    uint64_t v=((uint64_t)0xBCu<<32) | bongo.instrument | (bongo.style<<3) |
        (bongo.coat<<5) | (bongo.hat<<7) | (bongo.theme<<10) | (bongo.root<<12) |
        (bongo.minor<<16) | (bongo.band<<17) | (bongo.loop_on<<18) |
        ((bongo.density/5u)<<19) | (bongo.progression<<24) | (bongo.assist<<26) |
        (bongo.length<<27) | (bongo.overdub<<29) | (bongo.quantize<<30) | ((uint32_t)bongo.motion<<31);
    memcpy(proj_name,"BC2",3);
    for(uint32_t i=0;i<9u;i++) proj_name[3u+i]=alphabet[(v>>(40u-5u*i))&31u];
    proj_name[12]=0;
}
static void bongo_boot(void)
{
    uint32_t i,legacy=0,old=0,restored=0;
    uint64_t v=0;
    if(!memcmp(proj_name,"BC2",3)) {
        restored=1;
        for(i=3;i<12u;i++) {
            char c=proj_name[i]; uint32_t digit;
            if(c>='0' && c<='9') digit=(uint32_t)(c-'0');
            else if(c>='A' && c<='V') digit=(uint32_t)(c-'A'+10);
            else { restored=0; break; }
            v=(v<<5)|digit;
        }
        restored=restored && (v>>32)==0xBCu && ((v>>3)&3u)<3u && ((v>>5)&3u)<3u &&
            ((v>>7)&7u)<6u && ((v>>12)&15u)<12u && ((v>>19)&31u)<=20u && ((v>>27)&3u)<3u;
    } else if(!memcmp(proj_name,"BONGO ",6)) {
        legacy=1;
        for(i=6;i<12u;i++) {
            char c=proj_name[i];
            if(c>='0' && c<='9') old=(old<<4)|(uint32_t)(c-'0');
            else if(c>='A' && c<='F') old=(old<<4)|(uint32_t)(c-'A'+10);
            else { legacy=0; break; }
        }
        legacy=legacy && (old&0x800000u) && ((old>>3)&3u)<3u && ((old>>7)&3u)<3u &&
            ((old>>9)&15u)<12u && ((old>>16)&127u)<=100u;
        restored=legacy;
    }
    memset(&bongo,0,sizeof bongo);
    bongo.bass_duck=4096;
    for(i=0;i<4u;i++) bongo.mix_gain[i]=4096;
    bc_sound_reset();
    bongo.enabled=1; bongo.band=1; bongo.loop_on=1; bongo.minor=1; bongo.density=55;
    bongo.progression=1; bongo.assist=1; bongo.length=1; bongo.quantize=1; bongo.motion=1;
    bongo.live_at=fm1_ms;
    song.sel=0; song.grid=0; song.rec=0; song.playing=0;
    bc_metronome(CLICK_OFF); /* Quiet on EVERY boot, including inherited saved settings. */
    ui_prefs &= ~PREF_RESTORE_OFF;
    if(!restored) {
        for(i=0;i<G_COUNT;i++) song.g[i]=GP[i].def;
        song.octave=0; memset(&chain_config,0,sizeof chain_config);
        for(i=0;i<4u;i++) track_defaults(&trk[i]);
        bc_instrument(1); bc_style(0);
        trk[0].p[P_LEVEL]=88; trk[1].p[P_LEVEL]=46; trk[2].p[P_LEVEL]=82; trk[3].p[P_LEVEL]=68;
        song.g[G_BPM]=108; song.g[G_SWING]=18; song.g[G_CLOCK]=0;
    } else if(legacy) {
        bongo.instrument=old&7u; bongo.style=(old>>3)&3u; bongo.hat=(old>>5)&3u;
        bongo.theme=(old>>7)&3u; bongo.root=(old>>9)&15u; bongo.minor=(old>>13)&1u;
        bongo.band=(old>>14)&1u; bongo.loop_on=(old>>15)&1u; bongo.density=(old>>16)&127u;
        bongo.length=0; bongo.progression=0; bongo.assist=0;
    } else {
        bongo.instrument=v&7u; bongo.style=(v>>3)&3u; bongo.coat=(v>>5)&3u;
        bongo.hat=(v>>7)&7u; bongo.theme=(v>>10)&3u; bongo.root=(v>>12)&15u;
        bongo.minor=(v>>16)&1u; bongo.band=(v>>17)&1u; bongo.loop_on=(v>>18)&1u;
        bongo.density=(uint8_t)(((v>>19)&31u)*5u); bongo.progression=(v>>24)&3u;
        bongo.assist=(v>>26)&1u; bongo.length=(v>>27)&3u; bongo.overdub=(v>>29)&1u;
        bongo.quantize=(v>>30)&1u; bongo.motion=(v>>31)&1u;
    }
    for(i=0;i<4u;i++) { trk[i].p[P_SLEN]=i?16:(int16_t)(16u<<bongo.length); trk[i].p[P_SDIV]=2; }
    bc_scale();
    if(!restored) bongo_band_pattern();
    else {
        /* Loading must not regenerate any of the saved sequence. Remember
         * the last voicing; the normal bar callback writes the next chord. */
        for(i=0;i<16u;i++) if(trk[1].step[i].time==ST_NOTE && trk[1].step[i].n>=2u) {
            bongo.voicing[0]=trk[1].step[i].note[0];
            bongo.voicing[1]=trk[1].step[i].note[1];
            break;
        }
    }
    bc_metadata();
    bc_say(restored?"WELCOME BACK, MAESTRO":"A LITTLE BAND. ALL YOURS."); ui.force=1;
}

static void bongo_save_poll(void)
{
    bc_metadata();
    if(!bongo.save_pending || !autosave_quiet()) return;
    bongo.save_pending=0;
    project_capture(&proj_scratch); proj_wire_gen++;
    if(!proj_pack(&proj_wire,&proj_scratch)) { bc_say("SAVE FORMAT ERROR"); return; }
#if FELUCCA_FLASH
    if(!flash_ok || st_save(OBJ_AUTOSAVE,&proj_wire,sizeof proj_wire)) { bc_say("SAVE FAILED"); return; }
    as.saved=as.seen=autosave_sig(); as.last=as.t=fm1_ms; as.wrote=1; as.writes++;
    bc_say("SAVED. CAT REMEMBERS.");
#else
    bc_say("PREVIEW ONLY / NOT SAVED");
#endif
}
