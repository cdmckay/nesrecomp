/* input_test.c - the windowed cycle host's input actions (cyc_input.h) and
 * their persistence (cyc_settings.h): defaults, the text forms of every
 * binding kind, keyboard and controller evaluation, shortcut ownership,
 * controller assignment, and a config.ini round trip (every binding changed,
 * saved, reloaded into a fresh default set, compared field by field), plus
 * damaged and foreign files, an older file's move to new defaults (bindings
 * and VSync), Nintendo pads by position, and which controllers a hidden
 * window opens. Links SDL for its key and controller names and for
 * virtual pads; no window or real device is opened. */
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>

#include "cyc_input.h"
#include "cyc_settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while (0)

static uint8_t keys[SDL_NUM_SCANCODES];

static CycInputFrame frame(int pads)
{
    CycInputFrame f;
    memset(&f, 0, sizeof(f));
    f.keys = keys;
    f.key_count = SDL_NUM_SCANCODES;
    f.pad_count = pads;
    return f;
}

static void text_forms(void)
{
    char t[64];
    int v;
    cyc_key_text(SDL_SCANCODE_Z, t, sizeof(t));
    CHECK(!strcmp(t, "Z") && cyc_key_parse(t, &v) && v == SDL_SCANCODE_Z);
    cyc_key_text(SDL_SCANCODE_RSHIFT, t, sizeof(t));
    CHECK(!strcmp(t, "Right Shift") && cyc_key_parse(t, &v) && v == SDL_SCANCODE_RSHIFT);
    cyc_key_text(0, t, sizeof(t));
    CHECK(!strcmp(t, "None") && cyc_key_parse(t, &v) && v == 0);
    CHECK(cyc_key_parse("rshift", &v) && v == SDL_SCANCODE_RSHIFT);       /* the runner's alias */
    CHECK(cyc_key_parse("  escape", &v) && v == SDL_SCANCODE_ESCAPE);
    CHECK(!cyc_key_parse("NoSuchKey", &v));
    /* every scancode SDL names round-trips */
    for (int sc = 1; sc < SDL_NUM_SCANCODES; ++sc) {
        const char *name = SDL_GetScancodeName((SDL_Scancode)sc);
        if (!name || !*name) continue;
        cyc_key_text(sc, t, sizeof(t));
        int back = -1;
        CHECK(cyc_key_parse(t, &back));
        CHECK(back == (int)SDL_GetScancodeFromName(name));
    }
    /* every controller button, every axis direction, chords */
    for (int b = 0; b < SDL_CONTROLLER_BUTTON_MAX; ++b) {
        cyc_pad_text(CYC_PAD_BUTTON(b), t, sizeof(t));
        CHECK(cyc_pad_parse(t, &v) && v == CYC_PAD_BUTTON(b));
    }
    for (int a = 0; a < SDL_CONTROLLER_AXIS_MAX; ++a)
        for (int pos = 0; pos < 2; ++pos) {
            cyc_pad_text(CYC_PAD_AXIS(a, pos), t, sizeof(t));
            CHECK(cyc_pad_parse(t, &v) && v == CYC_PAD_AXIS(a, pos));
        }
    uint32_t chord = (1u << SDL_CONTROLLER_BUTTON_BACK) | (1u << SDL_CONTROLLER_BUTTON_START);
    cyc_pad_text(CYC_PAD_COMBO(chord), t, sizeof(t));
    CHECK(!strcmp(t, "back+start") && cyc_pad_parse(t, &v) && v == CYC_PAD_COMBO(chord));
    CHECK(cyc_pad_parse("lefttrigger+", &v) && v == CYC_PAD_AXIS(SDL_CONTROLLER_AXIS_TRIGGERLEFT, 1));
    CHECK(cyc_pad_parse("LeftX-", &v) && v == CYC_PAD_AXIS(SDL_CONTROLLER_AXIS_LEFTX, 0));
    CHECK(cyc_pad_parse("a, b", &v) && v == CYC_PAD_BUTTON(SDL_CONTROLLER_BUTTON_A));   /* runner list: first */
    CHECK(cyc_pad_parse("a+a", &v) && v == CYC_PAD_BUTTON(SDL_CONTROLLER_BUTTON_A));     /* one button, once */
    CHECK(cyc_pad_parse("none", &v) && v == 0 && cyc_pad_parse("", &v) && v == 0);
    CHECK(!cyc_pad_parse("paddle9", &v) && !cyc_pad_parse("a+paddle9", &v) && !cyc_pad_parse("nosuchaxis+", &v));
    /* hints */
    CycBinding hb = { SDL_SCANCODE_D, CYC_PAD_BUTTON(SDL_CONTROLLER_BUTTON_LEFTSHOULDER) };
    cyc_binding_hint(&hb, t, sizeof(t));
    CHECK(!strcmp(t, "D / LB"));
    hb.key = 0;
    cyc_binding_hint(&hb, t, sizeof(t));
    CHECK(!strcmp(t, "LB"));
    hb.pad = CYC_PAD_AXIS(SDL_CONTROLLER_AXIS_TRIGGERRIGHT, 1);
    cyc_binding_hint(&hb, t, sizeof(t));
    CHECK(!strcmp(t, "RT"));
    hb.pad = 0;
    cyc_binding_hint(&hb, t, sizeof(t));
    CHECK(!strcmp(t, "DISK"));
}

static void defaults(void)
{
    CycBindings b;
    cyc_bindings_default(&b);
    CHECK(b.source[0] == 1 && b.source[1] == 2);
    CHECK(b.button[0][0].key == SDL_SCANCODE_Z && b.button[0][1].key == SDL_SCANCODE_X);
    CHECK(b.button[0][2].key == SDL_SCANCODE_BACKSLASH && b.button[0][3].key == SDL_SCANCODE_RETURN);
    CHECK(b.button[1][0].key == 0);                          /* player 2 has no keyboard layout */
    CHECK(b.shortcut[CYC_SC_DISK].key == SDL_SCANCODE_D && b.shortcut[CYC_SC_DISK].pad == 0);
    CHECK(b.shortcut[CYC_SC_MENU].key == SDL_SCANCODE_ESCAPE);
    /* layout 2: the Menu on the right stick's press, Rewind and Fast-forward on the shoulders */
    CHECK(b.shortcut[CYC_SC_MENU].pad == CYC_PAD_BUTTON(SDL_CONTROLLER_BUTTON_RIGHTSTICK));
    CHECK(b.shortcut[CYC_SC_FAST_FORWARD].pad == CYC_PAD_BUTTON(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER));
    CHECK(b.shortcut[CYC_SC_REWIND].pad == CYC_PAD_BUTTON(SDL_CONTROLLER_BUTTON_LEFTSHOULDER));
    CHECK(b.shortcut[CYC_SC_STATE_MENU].key == SDL_SCANCODE_F7 && b.shortcut[CYC_SC_STATE_MENU].pad == 0);
    CHECK(b.shortcut[CYC_SC_REWIND].key == SDL_SCANCODE_F8);
    /* no shortcut leaves a trigger or Back held for a game to read */
    for (int s = 0; s < CYC_SC_COUNT; ++s) {
        int pad = b.shortcut[s].pad;
        CHECK(!CYC_PAD_IS_AXIS(pad) && !CYC_PAD_IS_COMBO(pad) && pad != CYC_PAD_BUTTON(SDL_CONTROLLER_BUTTON_BACK));
    }
    /* no shortcut default collides with a controller default, a dev key (F1-F6)
     * or another shortcut */
    for (int s = 0; s < CYC_SC_COUNT; ++s) {
        const CycBinding *sb = &b.shortcut[s];
        CHECK(!(sb->key >= SDL_SCANCODE_F1 && sb->key <= SDL_SCANCODE_F6));
        for (int p = 0; p < CYC_INPUT_PLAYERS; ++p)
            for (int i = 0; i < CYC_INPUT_BUTTONS; ++i) {
                CHECK(!sb->key || sb->key != b.button[p][i].key);
                CHECK(!sb->pad || sb->pad != b.button[p][i].pad);
            }
        for (int o = 0; o < s; ++o) {
            CHECK(!sb->key || sb->key != b.shortcut[o].key);
            CHECK(!sb->pad || sb->pad != b.shortcut[o].pad);
        }
    }
}

static void evaluation(void)
{
    CycBindings b;
    cyc_bindings_default(&b);
    CycInputState st;
    memset(keys, 0, sizeof(keys));
    CycInputFrame f = frame(0);
    cyc_input_eval(&b, &f, &st);
    CHECK(!st.buttons[0] && !st.buttons[1] && st.player_pad[0] == -1 && st.player_pad[1] == -1);
    keys[SDL_SCANCODE_Z] = keys[SDL_SCANCODE_RETURN] = keys[SDL_SCANCODE_LEFT] = 1;
    cyc_input_eval(&b, &f, &st);
    CHECK(st.buttons[0] == (0x80 | 0x10 | 0x02) && st.buttons[1] == 0);
    keys[SDL_SCANCODE_RIGHT] = 1;                            /* left + right cancel */
    cyc_input_eval(&b, &f, &st);
    CHECK(st.buttons[0] == (0x80 | 0x10));
    keys[SDL_SCANCODE_UP] = keys[SDL_SCANCODE_DOWN] = 1;     /* ...and up + down */
    cyc_input_eval(&b, &f, &st);
    CHECK(st.buttons[0] == (0x80 | 0x10));
    keys[SDL_SCANCODE_DOWN] = 0;
    cyc_input_eval(&b, &f, &st);
    CHECK(st.buttons[0] == (0x80 | 0x10 | 0x08));
    memset(keys, 0, sizeof(keys));

    /* one controller: player 1 plays on it as well as the keyboard */
    f = frame(1);
    f.pad_guid[0] = "pad-a";
    f.pad_buttons[0] = (1u << SDL_CONTROLLER_BUTTON_A) | (1u << SDL_CONTROLLER_BUTTON_DPAD_UP);
    cyc_input_eval(&b, &f, &st);
    CHECK(st.player_pad[0] == 0 && st.player_pad[1] == -1);
    CHECK(st.buttons[0] == (0x80 | 0x08) && st.buttons[1] == 0);
    keys[SDL_SCANCODE_RETURN] = 1;                           /* the keyboard still counts */
    cyc_input_eval(&b, &f, &st);
    CHECK(st.buttons[0] == (0x80 | 0x10 | 0x08));
    memset(keys, 0, sizeof(keys));
    /* the left stick works the D-pad past the deadzone only */
    f.pad_buttons[0] = 0;
    f.pad_axes[0][SDL_CONTROLLER_AXIS_LEFTX] = 32767 * 29 / 100;
    cyc_input_eval(&b, &f, &st);
    CHECK(st.buttons[0] == 0);
    f.pad_axes[0][SDL_CONTROLLER_AXIS_LEFTX] = 32767 * 31 / 100;
    f.pad_axes[0][SDL_CONTROLLER_AXIS_LEFTY] = -32768;
    cyc_input_eval(&b, &f, &st);
    CHECK(st.buttons[0] == (0x01 | 0x08));
    memset(f.pad_axes, 0, sizeof(f.pad_axes));
    /* a second controller is player 2's */
    f = frame(2);
    f.pad_guid[0] = "pad-a";
    f.pad_guid[1] = "pad-b";
    f.pad_buttons[1] = 1u << SDL_CONTROLLER_BUTTON_A;
    cyc_input_eval(&b, &f, &st);
    CHECK(st.player_pad[0] == 0 && st.player_pad[1] == 1);
    CHECK(st.buttons[0] == 0 && st.buttons[1] == 0x80);
    /* player 2 naming the only controller keeps it (co-op on one pad) */
    snprintf(b.device[1], sizeof(b.device[1]), "pad-a");
    f = frame(1);
    f.pad_guid[0] = "pad-a";
    f.pad_buttons[0] = 1u << SDL_CONTROLLER_BUTTON_A;
    cyc_input_eval(&b, &f, &st);
    CHECK(st.player_pad[0] == -1 && st.player_pad[1] == 0);
    CHECK(st.buttons[0] == 0 && st.buttons[1] == 0x80);
    b.device[1][0] = 0;

    /* player 1 on a controller: its named device wins over order */
    b.source[0] = 2;
    snprintf(b.device[0], sizeof(b.device[0]), "pad-b");
    f = frame(2);
    f.pad_guid[0] = "pad-a";
    f.pad_guid[1] = "pad-b";
    f.pad_buttons[1] = 1u << SDL_CONTROLLER_BUTTON_X;        /* NES B on pad-b */
    f.pad_buttons[0] = 1u << SDL_CONTROLLER_BUTTON_START;
    cyc_input_eval(&b, &f, &st);
    CHECK(st.player_pad[0] == 1 && st.player_pad[1] == 0);
    CHECK(st.buttons[0] == 0x40 && st.buttons[1] == 0x10);
    /* a named device that is not connected: the first free one */
    snprintf(b.device[0], sizeof(b.device[0]), "gone");
    cyc_input_eval(&b, &f, &st);
    CHECK(st.player_pad[0] == 0 && st.player_pad[1] == 1);
    b.source[1] = 0;                                         /* none */
    cyc_input_eval(&b, &f, &st);
    CHECK(st.buttons[1] == 0 && st.player_pad[1] == -1);
    cyc_bindings_default(&b);

    /* shortcuts: from the keyboard and from any controller */
    f = frame(2);
    keys[SDL_SCANCODE_D] = 1;
    cyc_input_eval(&b, &f, &st);
    CHECK(st.shortcut[CYC_SC_DISK] && !st.shortcut[CYC_SC_MENU]);
    keys[SDL_SCANCODE_D] = 0;
    f.pad_buttons[1] = 1u << SDL_CONTROLLER_BUTTON_LEFTSHOULDER;   /* a pad no player reads */
    cyc_input_eval(&b, &f, &st);
    CHECK(st.shortcut[CYC_SC_REWIND] && !st.shortcut[CYC_SC_DISK]);
    f.pad_buttons[1] = 1u << SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
    cyc_input_eval(&b, &f, &st);
    CHECK(st.shortcut[CYC_SC_FAST_FORWARD]);
    f.pad_buttons[1] = 0;
    /* an axis shortcut, past half travel */
    b.shortcut[CYC_SC_FAST_FORWARD].pad = CYC_PAD_AXIS(SDL_CONTROLLER_AXIS_TRIGGERRIGHT, 1);
    f.pad_axes[0][SDL_CONTROLLER_AXIS_TRIGGERRIGHT] = 20000;
    cyc_input_eval(&b, &f, &st);
    CHECK(st.shortcut[CYC_SC_FAST_FORWARD]);
    f.pad_axes[0][SDL_CONTROLLER_AXIS_TRIGGERRIGHT] = 10000;       /* under half travel */
    cyc_input_eval(&b, &f, &st);
    CHECK(!st.shortcut[CYC_SC_FAST_FORWARD]);
    memset(f.pad_axes, 0, sizeof(f.pad_axes));
    cyc_bindings_default(&b);

    /* a key bound to a shortcut and a button is the shortcut's */
    b.shortcut[CYC_SC_DISK].key = SDL_SCANCODE_Z;
    f = frame(0);
    keys[SDL_SCANCODE_Z] = 1;
    cyc_input_eval(&b, &f, &st);
    CHECK(st.shortcut[CYC_SC_DISK] && st.buttons[0] == 0);
    keys[SDL_SCANCODE_Z] = 0;
    /* ...and a chord's buttons are the shortcut's only while the whole chord is held */
    cyc_bindings_default(&b);
    b.shortcut[CYC_SC_MENU].pad =
        CYC_PAD_COMBO((1u << SDL_CONTROLLER_BUTTON_BACK) | (1u << SDL_CONTROLLER_BUTTON_START));
    f = frame(1);                                            /* the lone pad is player 1's */
    f.pad_buttons[0] = 1u << SDL_CONTROLLER_BUTTON_START;
    cyc_input_eval(&b, &f, &st);
    CHECK(!st.shortcut[CYC_SC_MENU] && st.buttons[0] == 0x10);
    f.pad_buttons[0] |= 1u << SDL_CONTROLLER_BUTTON_BACK;
    cyc_input_eval(&b, &f, &st);
    CHECK(st.shortcut[CYC_SC_MENU] && st.buttons[0] == 0);
    /* an axis bound to a player button */
    cyc_bindings_default(&b);
    b.button[0][0].pad = CYC_PAD_AXIS(SDL_CONTROLLER_AXIS_TRIGGERLEFT, 1);
    f = frame(1);
    f.pad_axes[0][SDL_CONTROLLER_AXIS_TRIGGERLEFT] = 30000;
    cyc_input_eval(&b, &f, &st);
    CHECK(st.buttons[0] == 0x80);
    memset(keys, 0, sizeof(keys));
}

typedef struct { int loads; char value[32]; } GameKeys;

static void logical_seats(void)
{
#if CYC_INPUT_PLAYERS == 4
    CycBindings b;
    CycInputState st;
    cyc_bindings_default(&b);
    CycInputFrame f = frame(4);
    const char *guids[] = {"one", "two", "three", "four"};
    const int buttons[] = {SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_X,
                           SDL_CONTROLLER_BUTTON_DPAD_LEFT, SDL_CONTROLLER_BUTTON_DPAD_RIGHT};
    const uint8_t expected[] = {0x80, 0x40, 0x02, 0x01};
    for (int p = 0; p < 4; ++p) {
        b.source[p] = 2;
        snprintf(b.device[p], sizeof b.device[p], "%s", guids[p]);
        f.pad_guid[p] = guids[p];
        f.pad_buttons[p] = 1u << buttons[p];
    }
    cyc_input_eval(&b, &f, &st);
    for (int p = 0; p < 4; ++p) CHECK(st.player_pad[p] == p && st.buttons[p] == expected[p]);
    /* Enumeration order changes when devices reconnect; GUID ownership wins. */
    for (int p = 0; p < 4; ++p) {
        f.pad_guid[3-p] = guids[p];
        f.pad_buttons[3-p] = 1u << buttons[p];
    }
    cyc_input_eval(&b, &f, &st);
    for (int p = 0; p < 4; ++p) CHECK(st.player_pad[p] == 3-p && st.buttons[p] == expected[p]);
    /* With one device removed the other three retain their named ownership. */
    f.pad_count = 3;
    cyc_input_eval(&b, &f, &st);
    CHECK(st.player_pad[0] == -1 && st.buttons[0] == 0);
    for (int p = 1; p < 4; ++p) CHECK(st.player_pad[p] == 3-p && st.buttons[p] == expected[p]);
    /* Four keyboard seats may have independent bindings too. */
    f = frame(0);
    memset(keys, 0, sizeof keys);
    for (int p = 0; p < 4; ++p) {
        b.source[p] = 1;
        memset(b.button[p], 0, sizeof b.button[p]);
        b.button[p][p].key = SDL_SCANCODE_1 + p;
        keys[SDL_SCANCODE_1+p] = 1;
    }
    cyc_input_eval(&b, &f, &st);
    for (int p = 0; p < 4; ++p) CHECK(st.buttons[p] == (0x80 >> p));
    memset(keys, 0, sizeof keys);
#endif
}
static void game_load(void *ctx, const char *key, const char *value)
{
    GameKeys *g = ctx;
    if (!strcmp(key, "HudMode")) { g->loads++; snprintf(g->value, sizeof(g->value), "%s", value); }
}
static void game_save(void *ctx, FILE *f) { fprintf(f, "HudMode = %s\n", ((GameKeys *)ctx)->value); }

static void persistence(const char *dir)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/input_test_config.ini", dir);
    remove(path);
    CycSettings s, back;
    cyc_settings_default(&s);
    CHECK(!cyc_settings_load(&s, path, NULL, NULL));               /* no file: defaults stay */
    /* change every field */
    s.window_scale = 5; s.fullscreen = 2; s.integer_scale = 0; s.linear_filter = 1; s.vsync = 0;
    s.audio_enabled = 0; s.volume = 35; s.skip_launcher = 1;
    s.rewind_enabled = 0; s.rewind_depth = 150; s.rewind_interval = 15;
    s.hdpack_enabled = 0;
    snprintf(s.hdpack_dir,sizeof s.hdpack_dir,"F:/HD Packs/Zelda");
    s.zapper_mouse = 0; s.zapper_crosshair = 0;
    s.fds_hle.auto_swap = 1; s.fds_hle.fast_load = 0;
    for (int p = 0; p < CYC_INPUT_PLAYERS; ++p) {
        s.bind.source[p] = p == 0 ? 2 : 0;
        snprintf(s.bind.device[p], sizeof(s.bind.device[p]), "03000000de280000ff1100000100000%d", p);
        s.bind.deadzone[p] = 12 + p;
        for (int i = 0; i < CYC_INPUT_BUTTONS; ++i) {
            s.bind.button[p][i].key = SDL_SCANCODE_A + i + p * 8;
            s.bind.button[p][i].pad = i % 3 == 0 ? CYC_PAD_BUTTON(i + p)
                                    : i % 3 == 1 ? CYC_PAD_AXIS(i % 6, p)
                                                 : CYC_PAD_COMBO((1u << i) | (1u << (i + 3)));
        }
    }
    for (int i = 0; i < CYC_SC_COUNT; ++i) {
        s.bind.shortcut[i].key = SDL_SCANCODE_F1 + i;
        s.bind.shortcut[i].pad = i == 2 ? 0 : CYC_PAD_BUTTON(SDL_CONTROLLER_BUTTON_Y + i);
    }
    CHECK(cyc_settings_save(&s, path, NULL));
    cyc_settings_default(&back);
    CHECK(cyc_settings_load(&back, path, stderr, NULL));
    CHECK(back.window_scale == 5 && back.fullscreen == 2 && back.integer_scale == 0 && back.linear_filter == 1);
    CHECK(back.vsync == 0 && !back.migrated);                  /* VSync off, saved now: the player's */
    CHECK(back.audio_enabled == 0 && back.volume == 35 && back.skip_launcher == 1);
    CHECK(back.rewind_enabled == 0 && back.rewind_depth == 150 && back.rewind_interval == 15);
    CHECK(!back.hdpack_enabled && !strcmp(back.hdpack_dir,s.hdpack_dir));
    CHECK(!back.zapper_mouse && !back.zapper_crosshair && back.zapper_keys == 3);
    CHECK(back.fds_hle.auto_swap == 1 && back.fds_hle.fast_load == 0);
    CHECK(!memcmp(&back.bind, &s.bind, sizeof(s.bind)));
    /* the file is what a person can read and edit */
    FILE *f = fopen(path, "r");
    char all[8192];
    size_t n = fread(all, 1, sizeof(all) - 1, f);
    fclose(f);
    all[n] = 0;
    CHECK(strstr(all, "[Keyboard.Shortcuts]\ndisk = F1\n"));
    CHECK(strstr(all, "[Gamepad.Shortcuts]\ndisk = y\n"));
    CHECK(strstr(all, "[FDS]") && strstr(all, "AutoSwap = on\n") && strstr(all, "FastLoad = off\n"));
    /* "default" goes back to saying nothing */
    s.fds_hle = NES_FDS_HLE_ASK_NONE;
    CHECK(cyc_settings_save(&s, path, NULL));
    cyc_settings_default(&back);
    back.fds_hle.auto_swap = 1;
    cyc_settings_load(&back, path, NULL, NULL);
    CHECK(back.fds_hle.auto_swap == -1 && back.fds_hle.fast_load == -1);

    /* a game's view mode and its own [Game] keys round-trip through its hooks */
    s.view_mode = 2;
    GameKeys gk = { 0 };
    snprintf(gk.value, sizeof(gk.value), "wide");
    CycSettingsGame game = { &gk, game_load, game_save, NULL };
    CHECK(cyc_settings_save(&s, path, &game));
    GameKeys gk2 = { 0 };
    CycSettingsGame game2 = { &gk2, game_load, game_save, NULL };
    cyc_settings_default(&back);
    CHECK(cyc_settings_load(&back, path, stderr, &game2));
    CHECK(back.view_mode == 2 && gk2.loads == 1 && !strcmp(gk2.value, "wide"));
    CHECK(!memcmp(&back.bind, &s.bind, sizeof(s.bind)));      /* the game's keys disturbed nothing */

    /* damaged values keep their defaults and are reported; unknown sections
     * and keys are ignored; the runner's keybinds.ini names parse */
    f = fopen(path, "w");
    fputs("[Display]\nWindowScale = lots\nFullscreen = 9\n[Unknown]\nx = y\n"
          "[Keyboard.Player1]\na = NoSuchKey\nb = rshift\n[Gamepad.Player1]\nstart = a, b\nup = paddle9\n"
          "[Keyboard.Player7]\na = Q\n[FDS]\nAutoSwap = maybe\nFastLoad = on\n"
          "[Zapper]\nMouse = false\nCrosshair = true\n"
          "[Rewind]\nEnabled = lots\nDepth = 900\nInterval = -3\n"
          "[Keyboard.Shortcuts]\nsave_state = F8\nload_state = F9\n", f);
    fclose(f);
    char logpath[1024];
    snprintf(logpath, sizeof(logpath), "%s/input_test_log.txt", dir);
    FILE *log = fopen(logpath, "w");
    cyc_settings_default(&back);
    CHECK(cyc_settings_load(&back, path, log, NULL));
    fclose(log);
    CycSettings d;
    cyc_settings_default(&d);
    CHECK(back.window_scale == d.window_scale && back.fullscreen == 2);         /* clamped */
    CHECK(back.bind.button[0][0].key == d.bind.button[0][0].key);             /* kept */
    CHECK(back.bind.button[0][1].key == SDL_SCANCODE_RSHIFT);
    CHECK(back.bind.button[0][3].pad == CYC_PAD_BUTTON(SDL_CONTROLLER_BUTTON_A));
    CHECK(back.bind.button[0][4].pad == d.bind.button[0][4].pad);
    CHECK(back.fds_hle.auto_swap == -1 && back.fds_hle.fast_load == 1);
    CHECK(!back.zapper_mouse && back.zapper_crosshair && back.zapper_keys == 3);
    CHECK(back.rewind_enabled == 1 && back.rewind_depth == 240 && back.rewind_interval == 1);
    CHECK(back.bind.shortcut[CYC_SC_STATE_MENU].key == SDL_SCANCODE_F7);
    CHECK(back.bind.shortcut[CYC_SC_REWIND].key == SDL_SCANCODE_F8);
    log = fopen(logpath, "r");
    n = fread(all, 1, sizeof(all) - 1, log);
    fclose(log);
    all[n] = 0;
    CHECK(strstr(all, "WindowScale = lots") && strstr(all, "a = NoSuchKey") && strstr(all, "up = paddle9") &&
          strstr(all, "AutoSwap = maybe"));
    remove(path);
    remove(logpath);
}

/* A config.ini an older build wrote (layout 1, no BindingsLayout): every
 * controller shortcut still on layout 1's default moves to the current one,
 * once; one the player changed stays; the game's loaded() sees the whole file
 * and can ask for a save. */
typedef struct { int loads, loaded; bool move; } LayoutGame;
static void layout_load(void *ctx, const char *key, const char *value)
{
    (void)key; (void)value;
    ((LayoutGame *)ctx)->loads++;
}
static void layout_save(void *ctx, FILE *f) { (void)ctx; fprintf(f, "HudMode = wide\n"); }
static bool layout_loaded(void *ctx)
{
    LayoutGame *g = ctx;
    g->loaded++;
    return g->move;
}
static void migration(const char *dir)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/input_test_layout1.ini", dir);
    /* as a build before layout 2 wrote it: every shortcut on its default but
     * Fast-forward, which the player moved to Y */
    FILE *f = fopen(path, "w");
    fputs("[Input]\nPlayer1Source = 1\n"
          "[Gamepad.Player1]\na = a\nb = x\nselect = back\nstart = start\n"
          "[Keyboard.Shortcuts]\ndisk = D\nmenu = Escape\nfast_forward = Tab\nscreenshot = F12\nfullscreen = F11\n"
          "state_menu = F7\nrewind = F8\n"
          "[Gamepad.Shortcuts]\ndisk = leftshoulder\nmenu = rightshoulder\nfast_forward = y\nscreenshot = none\n"
          "fullscreen = none\nstate_menu = back+leftstick\nrewind = back+rightstick\n"
          "[Game]\nViewMode = 1\nHudMode = wide\n", f);
    fclose(f);
    CycSettings s, d;
    cyc_settings_default(&d);
    cyc_settings_default(&s);
    LayoutGame g = { 0 };
    CycSettingsGame game = { &g, layout_load, layout_save, layout_loaded };
    CHECK(cyc_settings_load(&s, path, stderr, &game));
    CHECK(s.migrated && g.loads == 1 && g.loaded == 1);
    CHECK(s.bind.shortcut[CYC_SC_DISK].pad == 0);
    CHECK(s.bind.shortcut[CYC_SC_MENU].pad == CYC_PAD_BUTTON(SDL_CONTROLLER_BUTTON_RIGHTSTICK));
    CHECK(s.bind.shortcut[CYC_SC_FAST_FORWARD].pad == CYC_PAD_BUTTON(SDL_CONTROLLER_BUTTON_Y));   /* the player's */
    CHECK(s.bind.shortcut[CYC_SC_STATE_MENU].pad == 0);
    CHECK(s.bind.shortcut[CYC_SC_REWIND].pad == CYC_PAD_BUTTON(SDL_CONTROLLER_BUTTON_LEFTSHOULDER));
    for (int i = 0; i < CYC_SC_COUNT; ++i) CHECK(s.bind.shortcut[i].key == d.bind.shortcut[i].key);
    CHECK(!memcmp(s.bind.button, d.bind.button, sizeof(s.bind.button)));   /* the player's buttons as they were */
    /* saved: the file says layout 2 */
    CHECK(cyc_settings_save(&s, path, &game));
    f = fopen(path, "r");
    char all[8192];
    size_t n = fread(all, 1, sizeof(all) - 1, f);
    fclose(f);
    all[n] = 0;
    CHECK(strstr(all, "BindingsLayout = 2\n") && strstr(all, "[Gamepad.Shortcuts]\ndisk = none\nmenu = rightstick\n"));
    CHECK(strstr(all, "state_menu = none\nrewind = leftshoulder\n"));
    /* in layout 2 a binding the player sets back to layout 1's default stays there */
    s.bind.shortcut[CYC_SC_MENU].pad = CYC_PAD_BUTTON(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
    CHECK(cyc_settings_save(&s, path, &game));
    CycSettings back;
    cyc_settings_default(&back);
    LayoutGame g2 = { 0 };
    CycSettingsGame game2 = { &g2, layout_load, layout_save, layout_loaded };
    CHECK(cyc_settings_load(&back, path, stderr, &game2));
    CHECK(!back.migrated && g2.loaded == 1);
    CHECK(!memcmp(&back.bind, &s.bind, sizeof(s.bind)));
    /* the game alone can ask for the save */
    g2.move = true;
    cyc_settings_default(&back);
    CHECK(cyc_settings_load(&back, path, NULL, &game2) && back.migrated);
    /* a layout 1 file whose player put the NES A on the left shoulder: Rewind's
     * move there gives way, and Rewind keeps F8 alone */
    f = fopen(path, "w");
    fputs("[Gamepad.Player1]\na = leftshoulder\n[Gamepad.Shortcuts]\nrewind = back+rightstick\nmenu = rightshoulder\n", f);
    fclose(f);
    cyc_settings_default(&back);
    CHECK(cyc_settings_load(&back, path, NULL, NULL) && back.migrated);
    CHECK(back.bind.shortcut[CYC_SC_REWIND].pad == 0 && back.bind.shortcut[CYC_SC_REWIND].key == SDL_SCANCODE_F8);
    CHECK(back.bind.shortcut[CYC_SC_MENU].pad == CYC_PAD_BUTTON(SDL_CONTROLLER_BUTTON_RIGHTSTICK));
    CHECK(back.moved_shortcuts >> CYC_SC_REWIND & 1);
    /* ...and a game's own action on a moved shortcut's new button wins too */
    int game_pads[] = { CYC_PAD_BUTTON(SDL_CONTROLLER_BUTTON_RIGHTSTICK) };
    cyc_settings_yield_moved(&back, game_pads, 1);
    CHECK(back.bind.shortcut[CYC_SC_MENU].pad == 0);
    /* a shortcut the load did not move stays, whatever is on its button */
    back.bind.shortcut[CYC_SC_FAST_FORWARD].pad = CYC_PAD_BUTTON(SDL_CONTROLLER_BUTTON_Y);
    back.moved_shortcuts = 0;
    int y_pad[] = { CYC_PAD_BUTTON(SDL_CONTROLLER_BUTTON_Y) };
    cyc_settings_yield_moved(&back, y_pad, 1);
    CHECK(back.bind.shortcut[CYC_SC_FAST_FORWARD].pad == CYC_PAD_BUTTON(SDL_CONTROLLER_BUTTON_Y));
    /* no file: the defaults, nothing to move, loaded() not called */
    remove(path);
    LayoutGame g3 = { 0 };
    CycSettingsGame game3 = { &g3, layout_load, layout_save, layout_loaded };
    cyc_settings_default(&back);
    CHECK(!cyc_settings_load(&back, path, NULL, &game3) && !back.migrated && g3.loaded == 0);
}

/* VSync: on by default. A config.ini from before (no [Display]
 * DisplayDefaults) said VSync = 0 because that was the default, so its load
 * turns it on and asks for a save, once; the saved file says defaults 2, and
 * a VSync = 0 set after that stays. */
static void write_text(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");
    CHECK(f);
    fputs(text, f);
    fclose(f);
}
static void vsync_default(const char *dir)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/input_test_vsync.ini", dir);
    remove(path);
    CycSettings s;
    /* a fresh profile: on, and no file to move */
    cyc_settings_default(&s);
    CHECK(s.vsync == 1);
    CHECK(!cyc_settings_load(&s, path, NULL, NULL) && s.vsync == 1 && !s.migrated);
    /* as a build before wrote it, on its default: on, once, and saved so */
    write_text(path, "[Display]\nWindowScale = 3\nFullscreen = 0\nIntegerScale = 1\nLinearFilter = 0\nVSync = 0\n"
                     "HdPackEnabled = 1\nHdPackDir = \n[Input]\nBindingsLayout = 2\n");
    cyc_settings_default(&s);
    CHECK(cyc_settings_load(&s, path, stderr, NULL) && s.vsync == 1 && s.migrated);
    CHECK(s.window_scale == 3 && s.moved_shortcuts == 0);             /* nothing else moved */
    CHECK(cyc_settings_save(&s, path, NULL));
    FILE *f = fopen(path, "r");
    char all[8192];
    size_t n = fread(all, 1, sizeof(all) - 1, f);
    fclose(f);
    all[n] = 0;
    CHECK(strstr(all, "VSync = 1\n") && strstr(all, "\nDisplayDefaults = 2\n"));
    /* loaded again: on, nothing to move */
    cyc_settings_default(&s);
    CHECK(cyc_settings_load(&s, path, stderr, NULL) && s.vsync == 1 && !s.migrated);
    /* the player turns it off: it stays off */
    s.vsync = 0;
    CHECK(cyc_settings_save(&s, path, NULL));
    for (int i = 0; i < 2; ++i) {
        cyc_settings_default(&s);
        CHECK(cyc_settings_load(&s, path, stderr, NULL) && s.vsync == 0 && !s.migrated);
        CHECK(cyc_settings_save(&s, path, NULL));
    }
    /* an older file already on stays on and gets the marker; one without
     * the key (a hand-written one) takes the default */
    write_text(path, "[Display]\nVSync = 1\n[Input]\nBindingsLayout = 2\n");
    cyc_settings_default(&s);
    CHECK(cyc_settings_load(&s, path, stderr, NULL) && s.vsync == 1 && s.migrated);
    write_text(path, "[Display]\nWindowScale = 1\n");
    cyc_settings_default(&s);
    CHECK(cyc_settings_load(&s, path, stderr, NULL) && s.vsync == 1 && s.window_scale == 1);
    /* defaults that are no number read as 1's, and move; later ones keep the player's off */
    write_text(path, "[Display]\nVSync = 0\nDisplayDefaults = new\n[Input]\nBindingsLayout = 2\n");
    cyc_settings_default(&s);
    CHECK(cyc_settings_load(&s, path, NULL, NULL) && s.vsync == 1 && s.migrated);
    write_text(path, "[Display]\nVSync = 0\nDisplayDefaults = 3\n[Input]\nBindingsLayout = 2\n");
    cyc_settings_default(&s);
    CHECK(cyc_settings_load(&s, path, NULL, NULL) && s.vsync == 0 && !s.migrated);
    remove(path);
}

/* Controllers by position: a Nintendo-layout pad's lower face button reports
 * as A once cyc_input_sdl_hints() has run, as on every other pad; sdl2-compat
 * otherwise reports it by its printed letter, B. A virtual Switch Pro
 * Controller (Nintendo's USB ids) stands in for one. */
static int south_press(void)
{
    SDL_VirtualJoystickDesc desc;
    SDL_zero(desc);
    desc.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    desc.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
    desc.naxes = SDL_CONTROLLER_AXIS_MAX;
    desc.nbuttons = SDL_CONTROLLER_BUTTON_MAX;
    desc.vendor_id = 0x057E;
    desc.product_id = 0x2009;
    desc.name = "Nintendo Switch Pro Controller";
    int index = SDL_JoystickAttachVirtualEx(&desc);
    CHECK(index >= 0);
    SDL_Joystick *joy = SDL_JoystickOpen(index);
    SDL_GameController *pad = SDL_GameControllerOpen(index);
    CHECK(joy && pad);
    SDL_Event e;
    SDL_PumpEvents();
    while (SDL_PollEvent(&e)) {}
    SDL_JoystickSetVirtualButton(joy, 0, 1);   /* a virtual pad's button 0: the lower face button */
    SDL_PumpEvents();
    int button = -1;
    while (SDL_PollEvent(&e))
        if (e.type == SDL_CONTROLLERBUTTONDOWN) button = e.cbutton.button;
    SDL_GameControllerClose(pad);
    SDL_JoystickClose(joy);
    SDL_JoystickDetachVirtual(index);
    return button;
}
static void positional(void)
{
    if (SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) != 0) {
        printf("input_test: no SDL controllers here (%s): positional pads not checked\n", SDL_GetError());
        return;
    }
    int labelled = south_press();
    cyc_input_sdl_hints();
    int placed = south_press();
    SDL_Quit();
    CHECK(placed == SDL_CONTROLLER_BUTTON_A);
    printf("input_test: a Switch Pro's lower face button reads %s by its label, %s by its place\n",
           labelled >= 0 ? SDL_GameControllerGetStringForButton((SDL_GameControllerButton)labelled) : "nothing",
           SDL_GameControllerGetStringForButton((SDL_GameControllerButton)placed));
}

/* Which pads a window opens (cyc_input_opens_pad): a visible window every
 * game controller, a hidden one only virtual ones. The rule for every kind
 * of device; then SDL's answer for a virtual pad, which both open, and for
 * each real pad on this machine, which only a visible window opens. */
static void hidden_pads(void)
{
    for (int gc = 0; gc < 2; ++gc)
        for (int virt = 0; virt < 2; ++virt) {
            CHECK(cyc_input_pad_opens(gc, virt, false) == (bool)gc);          /* visible: every controller */
            CHECK(cyc_input_pad_opens(gc, virt, true) == (gc && virt));       /* hidden: virtual ones only */
        }
    if (SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) != 0) {
        printf("input_test: no SDL controllers here (%s): hidden pads checked by rule only\n", SDL_GetError());
        return;
    }
    SDL_VirtualJoystickDesc desc;
    SDL_zero(desc);
    desc.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    desc.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
    desc.naxes = SDL_CONTROLLER_AXIS_MAX;
    desc.nbuttons = SDL_CONTROLLER_BUTTON_MAX;
    desc.name = "NESRecomp virtual controller";
    int index = SDL_JoystickAttachVirtualEx(&desc);
    CHECK(index >= 0);
    CHECK(cyc_input_opens_pad(index, true) && cyc_input_opens_pad(index, false));
    int real = 0;
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_JoystickIsVirtual(i) || !SDL_IsGameController(i)) continue;
        CHECK(!cyc_input_opens_pad(i, true) && cyc_input_opens_pad(i, false));
        printf("input_test: a hidden window leaves %s closed; a visible one opens it\n", SDL_JoystickNameForIndex(i));
        ++real;
    }
    SDL_JoystickDetachVirtual(index);
    SDL_Quit();
    printf("input_test: a hidden window opens the virtual pad and %s\n",
           real ? "none of this machine's real pads" : "no real pad (none connected to check against)");
}

int main(int argc, char **argv)
{
    text_forms();
    defaults();
    evaluation();
    logical_seats();
    persistence(argc > 1 ? argv[1] : ".");
    migration(argc > 1 ? argv[1] : ".");
    vsync_default(argc > 1 ? argv[1] : ".");
    positional();
    hidden_pads();
    printf("input_test: %u checks passed\n", checks);
    return 0;
}
