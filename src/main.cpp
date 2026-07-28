// AMEN SLICER — breakbeat slicer for GBA. Atomic-purple theme on OLED black.
// Sync-aware clock: a MASTER phase always advances on the grid, so effects never
// knock the loop off-time (it stays locked to external gear at the same BPM).
// SELECT toggles EXPLAIN <-> ZEN. START taps the loop back to the 1 (re-align).
#include "pk.h"
#include "banks.h"

#include "bn_optional.h"
#include "bn_string.h"
#include "bn_sound_items.h"
#include "bn_sound_handle.h"

namespace T = pk::atomic;

namespace
{
    using pk::fixed; using pk::color;
    constexpr int STEPS = 16, NP = 16, TRAIL = 5, RES = banks::ENV_RES;
    constexpr fixed BPM_K = fixed(895.9);      // 15 * 59.7275 → BPM = BPM_K * speed / frames-per-16th

    // transport (sync clock)
    fixed speed = 1, acc = 0, sacc = 0, rot = 0, rot2 = 0;
    int step = 0, froz = 0, bank = 0, upd = 0, dnd = 0;
    bool zen = false, wasRep = false;

    // eased visual system
    fixed vspeed = 1, vstut = 0, vpitch = 1, flashF = 0, venv[RES] = { 0 };
    pk::Spring zoom;

    bn::vector<pk::Circle, STEPS> dots;
    bn::optional<pk::Circle>      vu;
    bn::vector<pk::Circle, RES>   ring, ring2;
    bn::vector<pk::Circle, TRAIL> comet;
    bn::vector<pk::Circle, NP>    parts;
    bn::optional<pk::Circle>      core;
    pk::Body pbody[NP];
    int      plife[NP] = { 0 };
    bn::optional<pk::Text> hud;
    bn::optional<bn::sound_handle> voice;      // last-played slice (for real tape-stop)

    fixed repeat_mult()
    {
        if(pk::down(pk::key::R)) return fixed(0.25);
        if(pk::down(pk::key::B)) return fixed(0.5);
        if(pk::down(pk::key::A)) return 1;
        if(pk::down(pk::key::L)) return 2;
        return 0;
    }
    const char* repeat_name()
    {
        if(pk::down(pk::key::R)) return "1/64";
        if(pk::down(pk::key::B)) return "1/32";
        if(pk::down(pk::key::A)) return "1/16";
        if(pk::down(pk::key::L)) return "1/8";
        return "off";
    }

    color mix(color a, color b, fixed t)
    {
        t = pk::clamp(t, fixed(0), fixed(1));
        auto L = [&](int x, int y) { return (fixed(x) + fixed(y - x) * t).integer(); };
        return color(L(a.red(), b.red()), L(a.green(), b.green()), L(a.blue(), b.blue()));
    }

    void build_explain()
    {
        ring.clear(); ring2.clear(); comet.clear(); parts.clear(); core.reset();
        dots.clear();
        for(int i = 0; i < STEPS; ++i) { dots.emplace_back(); dots[i].pos(pk::col(i, STEPS), -26); }
        vu.emplace(); vu->fill(T::base);
    }
    void build_zen()
    {
        dots.clear(); vu.reset();
        ring.clear();  for(int i = 0; i < RES; ++i)   ring.emplace_back();
        ring2.clear(); for(int i = 0; i < RES; ++i)   ring2.emplace_back();
        comet.clear(); for(int i = 0; i < TRAIL; ++i) comet.emplace_back();
        parts.clear(); for(int i = 0; i < NP; ++i)  { parts.emplace_back(); parts[i].show(false); plife[i] = 0; }
        core.emplace(); core->fill(T::base);
        zoom.reset(1); zoom.stiffness = fixed(0.22); zoom.damping = fixed(0.6);
        for(int i = 0; i < RES; ++i) venv[i] = fixed(banks::env[bank][i]) / 255;
    }

    void spawn_burst(int st, bool downbeat)
    {
        fixed amp = fixed(banks::env[bank][st * RES / STEPS]) / 255;
        int n = downbeat ? 8 : 3;
        for(int c = 0; c < n; ++c)
            for(int i = 0; i < NP; ++i)
                if(plife[i] <= 0)
                {
                    fixed a = pk::rnd(0, 360), spd = pk::rnd(fixed(1), fixed(3)) * (fixed(0.6) + amp);
                    fixed tang = pk::rnd(fixed(-2), fixed(2));
                    pbody[i] = { 0, 0, pk::cos(a) * spd - pk::sin(a) * tang,
                                       pk::sin(a) * spd + pk::cos(a) * tang };
                    plife[i] = 18 + (amp * 14).integer();
                    break;
                }
    }
}

void pk::setup()
{
    background(T::bg);
    hud.emplace();
    build_explain();
}

void pk::update()
{
    // ---- controls ----
    fixed rmult = repeat_mult();
    bool  repeating = rmult > 0;
    fixed fxpitch = speed;

    if(repeating)   // hold a stutter + arrows = breakdown FX (these NEVER touch the master tempo)
    {
        if(down(key::UP))   upd = upd + 1 > 90 ? 90 : upd + 1; else upd = 0;   // build (accelerates the stutter)
        if(down(key::DOWN)) dnd = dnd + 1 > 90 ? 90 : dnd + 1; else dnd = 0;   // tape-stop (ramps the voice)
        fxpitch = speed * (1 + fixed(upd) * fixed(0.03));
        if(down(key::RIGHT)) fxpitch = fxpitch * fixed(1.5);
        if(down(key::LEFT))  fxpitch = fxpitch * fixed(0.6);
    }
    else
    {
        upd = 0; dnd = 0;
        if(down(key::UP))       speed = clamp(speed + fixed(0.01), fixed(0.4), fixed(2.5));   // set tempo (sync)
        if(down(key::DOWN))     speed = clamp(speed - fixed(0.01), fixed(0.4), fixed(2.5));
        if(pressed(key::RIGHT)) { bank = (bank + 1) % banks::COUNT; zoom.vel += fixed(0.3); }
        if(pressed(key::LEFT))  { bank = (bank + banks::COUNT - 1) % banks::COUNT; zoom.vel += fixed(0.3); }
    }

    if(pressed(key::SELECT)) { zen = ! zen; if(zen) build_zen(); else build_explain(); zoom.vel += fixed(0.5); }

    fixed masterInterval = banks::step[bank] / speed;    // grid step — ONLY speed sets tempo

    // START = re-align phase to the 1 (tap on the external downbeat to lock in)
    if(pressed(key::START)) { step = 0; acc = 0; sacc = 0; flashF = 1; zoom.vel += fixed(0.7); }

    if(repeating && ! wasRep) froz = step;               // freeze the slice when a stutter starts
    wasRep = repeating;
    bool stopping = repeating && down(key::DOWN);

    // ---- MASTER CLOCK: always advances so the loop stays locked to the grid ----
    bool masterTick = false;
    acc += 1;
    if(acc >= masterInterval) { acc -= masterInterval; step = (step + 1) % STEPS; masterTick = true; }

    // ---- audio (an overlay on the master phase) ----
    bool downbeat = false;
    auto hitv = [&](int idx)
    {
        voice = banks::slices[bank][idx]->play(repeating ? fixed(0.7) : fixed(0.9), clamp(fxpitch, fixed(0.1), fixed(6)), 0);
        downbeat = (idx == 0);
        flashF = downbeat ? 1 : fixed(0.6);
        if(zen) { spawn_burst(idx, downbeat); zoom.vel += downbeat ? fixed(0.6) : fixed(0.28); }
    };

    if(stopping)                                         // ramp the live slice to a real halt
    {
        if(voice && voice->active())
            voice->set_speed(clamp(voice->speed() * fixed(0.9), fixed(0.02), fixed(6)));
        sacc = 0;
    }
    else if(repeating)                                  // stutter the frozen slice; master keeps time underneath
    {
        fixed stutInterval = masterInterval * rmult / (1 + fixed(upd) * fixed(0.04));
        sacc += 1;
        if(sacc >= stutInterval) { sacc -= stutInterval; hitv(froz); }
    }
    else                                                // normal: play the grid
    {
        sacc = 0;
        if(masterTick) hitv(step);
    }

    // ---- ease the whole visual system toward its targets (nothing hard-cuts) ----
    vspeed = approach(vspeed, speed, fixed(0.08));
    fixed stutTarget = repeating ? clamp(map(rmult, 2, fixed(0.25), fixed(0.3), 1), fixed(0.3), 1) : fixed(0);
    vstut  = approach(vstut, stutTarget, fixed(0.12));
    vpitch = approach(vpitch, fxpitch, fixed(0.1));
    flashF = approach(flashF, 0, fixed(0.16));
    for(int i = 0; i < RES; ++i)
        venv[i] = approach(venv[i], fixed(banks::env[bank][i]) / 255, fixed(0.09));

    int bpm = (BPM_K * speed / banks::step[bank] + fixed(0.5)).integer();

    fixed drift = (sin(fixed(frame) * fixed(1.4)) + 1) / 2;
    color c = mix(T::base, T::bright, drift);
    c = mix(c, T::glow, flashF);
    c = mix(c, T::cyan, clamp(vstut * fixed(0.7) + fixed(upd) / 120, fixed(0), fixed(0.9)));
    c = mix(c, T::dim,  fixed(dnd) / 100);
    c = mix(c, vpitch > 1 ? T::glow : T::deep, clamp((vpitch > 1 ? vpitch - 1 : 1 - vpitch) * fixed(0.5), fixed(0), fixed(0.4)));

    if(zen)
    {
        fixed Z = clamp(zoom.update(1), fixed(0.4), fixed(2)) * (1 - vstut * fixed(0.15));
        rot  += fixed(0.3) + vspeed * fixed(0.5) + vstut * fixed(0.8) + fixed(upd) * fixed(0.06) - fixed(dnd) * fixed(0.05);
        rot2 -= fixed(0.5) + vspeed * fixed(0.35) + vstut * fixed(0.5);
        fixed phase = (fixed(step) + acc / masterInterval) / STEPS;    // continuous grid playhead
        fixed scale = fixed(0.85) + vpitch * fixed(0.15);

        for(int i = 0; i < RES; ++i)
        {
            fixed eo = venv[i], ei = venv[RES - 1 - i];
            fixed ao = fixed(i) * 360 / RES + rot, ai = fixed(i) * 360 / RES + rot2;
            ring [i].pos(cos(ao) * (30 + eo * 28) * Z * scale, sin(ao) * (30 + eo * 28) * Z * scale).diameter(2 + eo * 5);
            ring2[i].pos(cos(ai) * (12 + ei * 14) * Z * scale, sin(ai) * (12 + ei * 14) * Z * scale).diameter(1 + ei * 3);
        }
        fixed ca = phase * 360 + rot, cr = 62 * Z * scale;
        for(int k = 0; k < TRAIL; ++k)
        { fixed ka = ca - fixed(k) * 9; comet[k].pos(cos(ka) * cr, sin(ka) * cr).diameter(7 - k); }

        core->pos(0, 0).diameter(map(flashF, 0, 1, 5 * Z, 22)).fill(c);
        for(int i = 0; i < NP; ++i)
        {
            if(plife[i] > 0) { pbody[i].kick(-pbody[i].x * fixed(0.01), -pbody[i].y * fixed(0.01));
                               pbody[i].damp(fixed(0.95)); pbody[i].integrate(); --plife[i];
                               parts[i].pos(pbody[i].x, pbody[i].y).diameter(map(fixed(plife[i]), 0, 28, 1, 6)).show(true); }
            else             parts[i].show(false);
        }

        hud->clear();
        hud->align_center();
        hud->print(0, 68, bn::string<24>(banks::name[bank]) + "   " + bn::to_string<8>(bpm) + " BPM");
        hud->align_right();
        hud->print(right - 4, top + 4, "SEL=info");
        hud->tint(mix(T::dim, T::bright, drift));
    }
    else
    {
        for(int i = 0; i < STEPS; ++i)
            dots[i].radius(i == step ? 6 : (i == 0 ? 4 : 2));    // grid playhead keeps moving (shows the lock)
        vu->pos(0, -2).radius(map(flashF, 0, 1, 5, 18)).fill(c);

        const char* brk = "";
        if(repeating) { if(down(key::UP)) brk = " build"; else if(down(key::DOWN)) brk = " stop";
                        else if(down(key::RIGHT)) brk = " up"; else if(down(key::LEFT)) brk = " down"; }

        bn::string<40> l2 = bn::string<40>("< ") + banks::name[bank] + " >  " + bn::to_string<8>(bpm) + " BPM";
        if(repeating) l2 = l2 + "  " + repeat_name() + brk;

        hud->clear();
        hud->align_center();
        hud->print(0, -50, "AMEN SLICER");
        hud->print(0,  26, l2);
        hud->print(0,  42, "UP/DN spd  L/R flav  ST=1");
        hud->print(0,  58, "A16 B32 L8 R64  +arrows");
        hud->align_right();
        hud->print(right - 4, top + 4, "SEL=zen");
        hud->tint(T::fg);
    }
    (void)masterTick; (void)downbeat;
}

int main() { pk::run(); }
