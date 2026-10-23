/* SPDX-License-Identifier: GPL-3.0-only */
/* Bongo Cat's Tiny Studio. All musical scheduling stays in the audio ISR.
 * The UI only posts requests or changes small, bounded controls. */
typedef struct {
    volatile uint32_t hit[4], count[4], live_at, live_count, jazz_at;
    volatile uint8_t note[4], root, minor, record, band, fill, loop_on, density;
    volatile uint8_t step, instrument, style, hat, theme, enabled;
    volatile uint32_t bars, learned;
    volatile uint32_t kick_at;
    int32_t bass_duck; /* Q12, audio-owned; never part of the saved jam. */
    volatile uint8_t progression, assist, length, overdub, quantize, count_in, chord, chord_minor;
    uint8_t coat, motion, help, edit_part, save_pending, undo_valid, undo_length, was_fill, busy, motif[7];
    uint8_t voicing[2], motif_n;
    volatile uint8_t egg;
    volatile uint8_t solo; /* 0 all parts, 1..4 audition solo; never saved. */
    volatile uint32_t egg_at;
    uint32_t motif_at, tap_at, tap_sum;
    uint8_t tap_n;
    int32_t mix_gain[4];
    uint32_t draw_at, heat_at, seen_live, home_at, toast_until, knob_at;
    int32_t heat;
    const char *toast;
    step_t undo[64];
} bongo_t;
static bongo_t bongo;
static void bongo_hit(track_t *t, uint32_t note);
static void bongo_live(track_t *t, uint32_t note);
static void bongo_enter(track_t *t);
static void bongo_bass_mix(track_t *t, int32_t *b, int32_t *side, uint32_t n);
static int bongo_input(uint32_t pressed);
static void bongo_draw(void);
static void bongo_leds(void);
static void bongo_boot(void);
static void bongo_save_poll(void);
static void bc_sound_reset(void);
static int bc_part_audible(uint32_t part);
