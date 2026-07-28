// AMEN SLICER — breakbeat slicer for GBA. Atomic-purple theme (clear-purple GBA
// on OLED black). Two modes: EXPLAIN (labelled control panel) and ZEN (a live
// generative viz of the break's real waveform). SELECT+START toggles them.
#include "pk.h"
#include "banks.h"

#include "bn_optional.h"
#include "bn_string.h"
#include "bn_sound_items.h"

namespace T = pk::atomic;      // theme

namespace
{
    using pk::fixed; using pk::color;
    constexpr int STEPS = 16, NP = 12, RES = banks::ENV_RES;

    fixed speed = 1, acc = 0, rot = 0;
    int step = 0, bank = 0, flash = 0, upd = 0, dnd = 0;
    bool zen = false;

    bn::vector<pk::Circle, STEPS> dots;      // explain: step row
    bn::optional<pk::Circle>      vu;        // explain: center pulse
    bn::vector<pk::Circle, RES>   ring;      // zen: waveform ring
    bn::vector<pk::Circle, NP>    parts;     // zen: hit particles
    bn::optional<pk::Circle>      core;      // zen: center pulse
    pk::Body pbody[NP];
    int      plife[NP] = { 0 };
    bn::optional<pk::Text> hud;

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
        ring.clear(); parts.clear(); core.reset();
        dots.clear();
        for(int i = 0; i < STEPS; ++i) { dots.emplace_back(); dots[i].pos(pk::col(i, STEPS), -26); }
        vu.emplace(); vu->fill(T::base);
    }
    void build_zen()
    {
        dots.clear(); vu.reset();
        ring.clear();
        for(int i = 0; i < RES; ++i) ring.emplace_back();
        parts.clear();
        for(int i = 0; i < NP; ++i) { parts.emplace_back(); parts[i].show(false); plife[i] = 0; }
        core.emplace(); core->fill(T::base);
    }

    void spawn_burst(int st, bool downbeat)      // bloom particles on a hit, scaled by loudness
    {
        fixed amp = fixed(banks::env[bank][st * RES / STEPS]) / 255;
        int n = downbeat ? 5 : 2;
        for(int c = 0; c < n; ++c)
            for(int i = 0; i < NP; ++i)
                if(plife[i] <= 0)
                {
                    fixed a = pk::rnd(0, 360), spd = pk::rnd(fixed(1), fixed(3)) * (fixed(0.5) + amp);
                    pbody[i] = { 0, 0, pk::cos(a) * spd, pk::sin(a) * spd };
                    plife[i] = 16 + (amp * 12).integer();
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
    // ---- transport controls (identical in both modes) ----
    fixed rmult = repeat_mult();
    bool  repeating = rmult > 0;
    fixed fxpitch = speed, fxrate = speed;

    if(repeating)   // hold a stutter + arrows = stacked breakdown FX
    {
        if(down(key::UP))   { upd = upd + 1 > 90 ? 90 : upd + 1; fixed r = 1 + fixed(upd) * fixed(0.03); fxpitch = speed * r; fxrate = speed * r; }
        else                  upd = 0;
        if(down(key::DOWN)) { dnd = dnd + 1 > 90 ? 90 : dnd + 1; fixed r = clamp(1 - fixed(dnd) * fixed(0.012), fixed(0.2), fixed(1)); fxpitch = speed * r; fxrate = speed * r; }
        else                  dnd = 0;
        if(down(key::RIGHT))  fxpitch = fxpitch * fixed(1.5);
        if(down(key::LEFT))   fxpitch = fxpitch * fixed(0.6);
    }
    else
    {
        upd = 0; dnd = 0;
        if(down(key::UP))       speed = clamp(speed + fixed(0.015), fixed(0.4), fixed(2.5));
        if(down(key::DOWN))     speed = clamp(speed - fixed(0.015), fixed(0.4), fixed(2.5));
        if(pressed(key::RIGHT)) { bank = (bank + 1) % banks::COUNT; acc = 0; }
        if(pressed(key::LEFT))  { bank = (bank + banks::COUNT - 1) % banks::COUNT; acc = 0; }
    }

    fixed interval = (repeating ? banks::step[bank] * rmult : banks::step[bank]) / fxrate;

    // mode toggle (SEL+START) vs back-to-1 (START) vs flavor (SEL)
    if(down(key::SELECT) && pressed(key::START)) { zen = ! zen; if(zen) build_zen(); else build_explain(); }
    else if(pressed(key::START))                 { step = 0; acc = interval; }
    if(pressed(key::SELECT) && ! down(key::START)) { bank = (bank + 1) % banks::COUNT; acc = 0; }

    // ---- clock ----
    acc += 1;
    if(acc >= interval)
    {
        acc -= interval;
        bool downbeat = (step == 0);
        banks::slices[bank][step]->play(repeating ? fixed(0.7) : fixed(0.9), clamp(fxpitch, fixed(0.1), fixed(6)), 0);
        flash = downbeat ? 8 : 4;
        if(zen) spawn_burst(step, downbeat);
        if(! repeating) step = (step + 1) % STEPS;
    }
    if(flash > 0) --flash;

    int bpm = (fixed(900) * speed / banks::step[bank] + fixed(0.5)).integer();

    // theme color, modulated live (shared circle palette → recolors everything)
    color c = mix(T::base, T::glow, fixed(flash) / 8);
    if(upd > 0) c = mix(c, T::cyan, fixed(upd) / 90);     // build  → cool
    if(dnd > 0) c = mix(c, T::dim,  fixed(dnd) / 90);     // stop   → dim

    if(zen)
    {
        // ---- ZEN: rotating real-waveform ring + particles + core pulse ----
        rot += fixed(0.5) + fixed(upd) * fixed(0.06) - fixed(dnd) * fixed(0.04);
        int playidx = step * RES / STEPS;
        for(int i = 0; i < RES; ++i)
        {
            fixed a = fixed(i) * 360 / RES + rot;
            fixed e = fixed(banks::env[bank][i]) / 255;                       // real amplitude 0..1
            fixed R = 26 + e * 30 + (flash > 0 ? fixed(flash) : fixed(0));    // radius = loudness (+pulse)
            fixed d = 2 + e * 5 + (i == playidx ? fixed(4) : fixed(0));       // playhead swells
            ring[i].pos(cos(a) * R, sin(a) * R).diameter(d);
        }
        core->pos(0, 0).diameter(flash > 0 ? map(fixed(flash), 0, 8, 4, 22) : 4).fill(c);
        for(int i = 0; i < NP; ++i)
        {
            if(plife[i] > 0) { pbody[i].integrate(); pbody[i].damp(fixed(0.9)); --plife[i];
                               parts[i].pos(pbody[i].x, pbody[i].y).diameter(map(fixed(plife[i]), 0, 24, 1, 6)).show(true); }
            else             parts[i].show(false);
        }

        hud->clear();
        hud->align_center();
        hud->print(0, 66, bn::string<24>(banks::name[bank]) + "   " + bn::to_string<8>(bpm) + " BPM");
        hud->align_right();
        hud->print(right - 4, top + 4, "info");
        hud->tint(T::dim);
    }
    else
    {
        // ---- EXPLAIN: labelled control panel ----
        for(int i = 0; i < STEPS; ++i)
            dots[i].radius(i == step ? 6 : (i == 0 ? 4 : 2));
        vu->pos(0, -2).radius(flash > 0 ? map(fixed(flash), 0, 8, 5, 18) : 5).fill(c);

        const char* brk = "";
        if(repeating) { if(down(key::UP)) brk = " build"; else if(down(key::DOWN)) brk = " stop";
                        else if(down(key::RIGHT)) brk = " up"; else if(down(key::LEFT)) brk = " down"; }

        bn::string<40> l2 = bn::string<40>("< ") + banks::name[bank] + " >  " + bn::to_string<8>(bpm) + " BPM";
        if(repeating) l2 = l2 + "  " + repeat_name() + brk;

        hud->clear();
        hud->align_center();
        hud->print(0, -50, "AMEN SLICER");
        hud->print(0,  26, l2);
        hud->print(0,  42, "UP/DN spd  SEL flav  ST=1");
        hud->print(0,  58, "A16 B32 L8 R64  +arrows");
        hud->align_right();
        hud->print(right - 4, top + 4, "zen>");
        hud->tint(T::fg);
    }
}

int main() { pk::run(); }
