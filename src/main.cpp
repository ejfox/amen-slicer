// AMEN SLICER — a tiny breakbeat slicer for GBA.
//   plays a 16-slice break on a loop
//   UP/DOWN : Amiga stretch (repitch — speeds up/slows down pitch AND tempo)
//   A/B/L/R : beat-repeat stutter at 1/8, 1/16, 1/32, 1/64 (hold)
//   START   : reset speed to 1x
#include "pk.h"

#include "bn_optional.h"
#include "bn_string.h"
#include "bn_sound_items.h"

namespace
{
    constexpr int STEPS = 16;
    constexpr bn::fixed BASE_STEP = bn::fixed(6.5217);   // frames per 16th @ 1x (138 BPM)

    const bn::sound_item* slices[STEPS] = {
        &bn::sound_items::slice00, &bn::sound_items::slice01, &bn::sound_items::slice02, &bn::sound_items::slice03,
        &bn::sound_items::slice04, &bn::sound_items::slice05, &bn::sound_items::slice06, &bn::sound_items::slice07,
        &bn::sound_items::slice08, &bn::sound_items::slice09, &bn::sound_items::slice10, &bn::sound_items::slice11,
        &bn::sound_items::slice12, &bn::sound_items::slice13, &bn::sound_items::slice14, &bn::sound_items::slice15,
    };

    bn::fixed speed = 1;
    bn::fixed acc   = 0;
    int step  = 0;
    int flash = 0;                                        // VU pulse timer

    bn::vector<pk::Circle, STEPS> dots;                  // the 16 step indicators
    bn::optional<pk::Circle>      vu;                    // center pulse on each hit
    bn::optional<pk::Text>        hud;

    // held FX button → beat-repeat interval in 16th-note units (0 = off)
    bn::fixed repeat_mult()
    {
        if(pk::down(pk::key::R)) return bn::fixed(0.25); // 1/64
        if(pk::down(pk::key::L)) return bn::fixed(0.5);  // 1/32
        if(pk::down(pk::key::B)) return 1;               // 1/16
        if(pk::down(pk::key::A)) return 2;               // 1/8
        return 0;
    }
    const char* repeat_name()
    {
        if(pk::down(pk::key::R)) return "1/64";
        if(pk::down(pk::key::L)) return "1/32";
        if(pk::down(pk::key::B)) return "1/16";
        if(pk::down(pk::key::A)) return "1/8";
        return "--";
    }
}

void pk::setup()
{
    background(vulpes::bg);
    hud.emplace();
    vu.emplace();
    vu->fill(vulpes::base);                               // shared palette → all circles pink
    for(int i = 0; i < STEPS; ++i) { dots.emplace_back(); dots[i].pos(col(i, STEPS), -14); }
}

void pk::update()
{
    // --- Amiga stretch: repitch the whole break (pitch + tempo together) ---
    if(down(key::UP))       speed = clamp(speed + fixed(0.015), fixed(0.4), fixed(2.5));
    if(down(key::DOWN))     speed = clamp(speed - fixed(0.015), fixed(0.4), fixed(2.5));
    if(pressed(key::START)) speed = 1;

    // --- clock: trigger a slice when the accumulator crosses the step interval ---
    fixed rmult     = repeat_mult();
    bool  repeating = rmult > 0;
    fixed interval  = (repeating ? BASE_STEP * rmult : BASE_STEP) / speed;

    acc += 1;
    if(acc >= interval)
    {
        acc -= interval;
        slices[step]->play(repeating ? fixed(0.7) : fixed(0.9), speed, 0);   // play at stretch speed
        flash = 4;
        if(! repeating) step = (step + 1) % STEPS;        // beat-repeat freezes on the current slice
    }
    if(flash > 0) --flash;

    // --- visuals: step row + center VU pulse + readout ---
    for(int i = 0; i < STEPS; ++i)
        dots[i].radius(i == step ? 5 : 2);
    vu->pos(0, 18).radius(flash > 0 ? map(fixed(flash), 0, 4, 4, 15) : 4);

    hud->clear();
    hud->align_center();
    hud->print(0, -42, "AMEN SLICER");
    hud->print(0,  38, bn::string<24>("speed ") + bn::to_string<8>(speed) + "x");
    hud->print(0,  54, bn::string<24>("FX ") + repeat_name());
    hud->tint(vulpes::teal);
}

int main() { pk::run(); }
