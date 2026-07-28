// pk.h — a tiny p5.js-flavored layer over Butano (GBA).
//
// GBA is tile/sprite hardware, not an immediate-mode pixel canvas, so "drawing"
// here means: a background color, text, and lightweight sprite "actors" (Box).
// Everything else — the loop, time, input, and math — maps closely to p5.
//
// Coordinate system: Butano's, i.e. the ORIGIN IS THE SCREEN CENTER, y grows
// downward. Screen is 240x160, so x in [-120,120], y in [-80,80].
//
// Usage (see src/main.cpp):
//     void pk::setup()  { ...create actors, seed state... }
//     void pk::update() { ...runs every frame at 60fps... }
//     int  main()       { pk::run(); }

#ifndef PK_H
#define PK_H

#include "bn_core.h"
#include "bn_math.h"
#include "bn_fixed.h"
#include "bn_color.h"
#include "bn_keypad.h"
#include "bn_random.h"
#include "bn_vector.h"
#include "bn_display.h"
#include "bn_fixed_point.h"
#include "bn_string_view.h"
#include "bn_bg_palettes.h"
#include "bn_music.h"
#include "bn_music_item.h"
#include "bn_sound_item.h"
#include "bn_sprite_ptr.h"
#include "bn_sprite_palette_ptr.h"
#include "bn_sprite_text_generator.h"

#include "bn_sprite_items_square.h"            // generated from graphics/square.bmp
#include "bn_sprite_items_circle.h"            // generated from graphics/circle.bmp (size sheet)
#include "common_variable_8x16_sprite_font.h"  // font for pk::Text

namespace pk
{
    using bn::fixed;
    using bn::color;

    // Own key enum (not an alias) so pk::pressed/released don't collide with
    // bn::keypad::pressed/released via argument-dependent lookup.
    enum class key { A, B, SELECT, START, RIGHT, LEFT, UP, DOWN, R, L };
    inline bn::keypad::key_type _kt(key k)
    {
        switch(k)
        {
            case key::A:      return bn::keypad::key_type::A;
            case key::B:      return bn::keypad::key_type::B;
            case key::SELECT: return bn::keypad::key_type::SELECT;
            case key::START:  return bn::keypad::key_type::START;
            case key::RIGHT:  return bn::keypad::key_type::RIGHT;
            case key::LEFT:   return bn::keypad::key_type::LEFT;
            case key::UP:     return bn::keypad::key_type::UP;
            case key::DOWN:   return bn::keypad::key_type::DOWN;
            case key::R:      return bn::keypad::key_type::R;
            default:          return bn::keypad::key_type::L;
        }
    }

    // ---- color --------------------------------------------------------------
    // GBA color is 15-bit BGR555 (5 bits/channel). hex() converts a normal
    // 0xRRGGBB web color down to that gamut.
    constexpr color hex(unsigned rgb)
    {
        return color(int((rgb >> 16) & 0xff) >> 3,
                     int((rgb >>  8) & 0xff) >> 3,
                     int( rgb        & 0xff) >> 3);
    }

    // The vulpes palette (github.com/ejfox/vulpes.nvim, "reddishnovember" dark).
    // Note: colors are squeezed into GBA's 5-bit gamut, so the neons read a
    // touch softer than on a true-color display.
    namespace vulpes
    {
        constexpr color bg          = hex(0x000000);  // pure black
        constexpr color bg_alt      = hex(0x0d0d0d);
        constexpr color bg_cursor   = hex(0x2a1520);
        constexpr color fg          = hex(0xf2cfdf);  // soft pink-white
        constexpr color fg_dim      = hex(0xc490a8);
        constexpr color fg_dark     = hex(0x735865);
        constexpr color base        = hex(0xe60067);  // the signature vulpes pink
        constexpr color base_bright = hex(0xff277d);
        constexpr color pink        = base;
        constexpr color hot_pink    = hex(0xff0095);
        constexpr color magenta     = hex(0xff24ab);
        constexpr color teal        = hex(0x6eedf7);  // signature teal comments
        constexpr color amber       = hex(0xffaa00);  // warning / accent
        constexpr color red         = hex(0xff001e);
        constexpr color chartreuse  = hex(0xb4d455);  // git-add pop
        constexpr color white       = hex(0xffffff);
    }

    // ---- screen -------------------------------------------------------------
    inline constexpr int   width  = bn::display::width();   // 240
    inline constexpr int   height = bn::display::height();  // 160
    inline constexpr fixed left   = -width  / 2;
    inline constexpr fixed right  =  width  / 2;
    inline constexpr fixed top    = -height / 2;
    inline constexpr fixed bottom =  height / 2;
    inline constexpr bn::fixed_point center = bn::fixed_point(0, 0);

    // Safe area — inset from the edges so UI isn't clipped by the Miyoo Mini's
    // overscan/scaling on its 640x480 panel. Default inset 8px; nudge as needed.
    inline constexpr fixed SAFE        = 8;
    inline constexpr fixed safe_left   = left   + SAFE;
    inline constexpr fixed safe_right  = right  - SAFE;
    inline constexpr fixed safe_top    = top    + SAFE;
    inline constexpr fixed safe_bottom = bottom - SAFE;

    // Grid layout inside the safe area. col(i,n)/row(j,m) give evenly spaced
    // coordinates (i in [0,n), edges inclusive); cell() combines them.
    inline fixed col(int i, int n) { return n <= 1 ? fixed(0) : safe_left + (safe_right - safe_left) * i / (n - 1); }
    inline fixed row(int j, int m) { return m <= 1 ? fixed(0) : safe_top  + (safe_bottom - safe_top) * j / (m - 1); }
    inline bn::fixed_point cell(int i, int n, int j, int m) { return bn::fixed_point(col(i, n), row(j, m)); }

    // ---- time (60fps fixed step) -------------------------------------------
    inline int frame = 0;                                   // like p5 frameCount
    inline fixed seconds()        { return fixed(frame) / 60; }
    inline bool  every(int n)     { return n > 0 && (frame % n) == 0; }

    // ---- randomness ---------------------------------------------------------
    inline bn::random _rng;
    inline fixed random()                       { return fixed::from_data(_rng.get_int(1 << 12)); } // [0,1)
    inline fixed rnd(fixed hi)                   { return random() * hi; }                            // [0,hi)
    inline fixed rnd(fixed lo, fixed hi)         { return lo + random() * (hi - lo); }                // [lo,hi)
    inline int   rndi(int lo, int hi)            { return lo + _rng.get_int(hi - lo + 1); }           // [lo,hi]
    inline bool  chance(fixed p)                 { return random() < p; }

    // ---- math (p5-shaped) ---------------------------------------------------
    inline fixed clamp(fixed v, fixed lo, fixed hi) { return v < lo ? lo : (v > hi ? hi : v); }
    inline fixed lerp(fixed a, fixed b, fixed t)    { return a + (b - a) * t; }
    inline fixed map(fixed v, fixed a, fixed b, fixed c, fixed d)
    {
        if(a == b) return c;
        return c + (v - a) * (d - c) / (b - a);
    }
    inline fixed sin(fixed deg)  { return bn::degrees_sin(bn::safe_degrees_angle(deg)); } // [-1,1]
    inline fixed cos(fixed deg)  { return bn::degrees_cos(bn::safe_degrees_angle(deg)); }
    // oscillate between lo..hi with the given period in frames
    inline fixed wave(int period, fixed lo, fixed hi)
    {
        fixed s = sin(fixed(frame) * 360 / period);   // -1..1
        return map(s, -1, 1, lo, hi);
    }

    // ---- juice (game feel) --------------------------------------------------
    // Springy follow: ease `cur` toward `target` by `rate` each frame (0..1).
    // Small rate = loose/laggy, big rate = snappy. This is the secret to "alive".
    inline fixed approach(fixed cur, fixed target, fixed rate) { return cur + (target - cur) * rate; }
    inline fixed ease_in(fixed t)     { return t * t; }
    inline fixed ease_out(fixed t)    { fixed u = 1 - t; return 1 - u * u; }
    inline fixed ease_in_out(fixed t) { return t < fixed(0.5) ? 2 * t * t : 1 - (2 - 2 * t) * (2 - 2 * t) / 2; }
    // pop: 0→1 with a little overshoot (ease-out-back) over `dur` frames since birth.
    // Perfect for spawn-in joy: things that bounce past their size then settle.
    inline fixed pop(int age, int dur)
    {
        if(age >= dur) return 1;
        fixed t = fixed(age) / dur, s = fixed(1.70158), u = t - 1;
        return 1 + (s + 1) * u * u * u + s * u * u;
    }

    // ---- physics ------------------------------------------------------------
    // A 1D spring: pulls `pos` toward a target with overshoot + settle. The juicy
    // alternative to approach() — stiffness = pull strength, damping = friction.
    // (stiffness ~0.15-0.3, damping ~0.7-0.85 feels springy but stable.)
    struct Spring
    {
        fixed pos = 0, vel = 0;
        fixed stiffness = fixed(0.2), damping = fixed(0.78);
        fixed update(fixed target)
        {
            vel += (target - pos) * stiffness;
            vel *= damping;
            pos += vel;
            return pos;
        }
        void reset(fixed p) { pos = p; vel = 0; }
    };

    // A 2D point mass for particles/projectiles: velocity integration, gravity,
    // damping, and bounce off a box (restitution 0..1 = bounciness).
    struct Body
    {
        fixed x = 0, y = 0, vx = 0, vy = 0;
        void integrate()      { x += vx; y += vy; }
        void gravity(fixed g) { vy += g; }
        void damp(fixed d)    { vx *= d; vy *= d; }
        void kick(fixed ax, fixed ay) { vx += ax; vy += ay; }
        void bounce(fixed lo_x, fixed hi_x, fixed lo_y, fixed hi_y, fixed restitution)
        {
            if(x < lo_x) { x = lo_x; vx = -vx * restitution; }
            if(x > hi_x) { x = hi_x; vx = -vx * restitution; }
            if(y < lo_y) { y = lo_y; vy = -vy * restitution; }
            if(y > hi_y) { y = hi_y; vy = -vy * restitution; }
        }
    };

    // Distance between two points (for circle collisions etc.).
    inline fixed dist(fixed ax, fixed ay, fixed bx, fixed by)
    {
        fixed dx_ = ax - bx, dy_ = ay - by;
        return bn::sqrt(dx_ * dx_ + dy_ * dy_);
    }

    // ---- input (p5-shaped) --------------------------------------------------
    inline bool down(key k)     { return bn::keypad::held(_kt(k)); }      // held this frame
    inline bool pressed(key k)  { return bn::keypad::pressed(_kt(k)); }   // went down this frame
    inline bool released(key k) { return bn::keypad::released(_kt(k)); }  // came up this frame
    inline int  dx() { return (down(key::RIGHT) ? 1 : 0) - (down(key::LEFT) ? 1 : 0); } // -1/0/1
    inline int  dy() { return (down(key::DOWN)  ? 1 : 0) - (down(key::UP)   ? 1 : 0); } // -1/0/1

    // ---- drawing: background ------------------------------------------------
    inline void background(color c) { bn::bg_palettes::set_transparent_color(c); }

    // ---- drawing: text (retained; clear() then print() each frame) ----------
    // Coordinates are center-origin like everything else.
    class Text
    {
    public:
        Text() { _gen.set_left_alignment(); _gen.set_z_order(0); } // top layer (Boxes sit behind)
        void clear() { _sprites.clear(); }
        Text& align_left()   { _gen.set_left_alignment();   return *this; }
        Text& align_center() { _gen.set_center_alignment(); return *this; }
        void print(fixed x, fixed y, const bn::string_view& s) { _gen.generate(x, y, s, _sprites); }
        void print(const bn::fixed_point& p, const bn::string_view& s) { _gen.generate(p, s, _sprites); }
        // Recolor the text's glyph fill, keeping its black outline. The common
        // fonts store the fill at palette index 14 (outline at 12) — so we tint
        // 14 only, NOT a full-palette fade (which would merge outline+fill into a
        // mushy blob). Call after your print()s for the frame.
        void tint(color c) { if(! _sprites.empty()) { bn::sprite_palette_ptr p = _sprites[0].palette(); p.set_color(14, c); } }
    private:
        bn::sprite_text_generator     _gen{common::variable_8x16_sprite_font};
        bn::vector<bn::sprite_ptr, 96> _sprites;
    };

    // ---- drawing: Box — a movable sprite "actor" (8x8 white square) ----------
    // Squares share one palette, so tinting is global by design; keep them white
    // and set the mood with background(). Move/scale/rotate are per-box.
    class Box
    {
    public:
        explicit Box(fixed x = 0, fixed y = 0)
            : _sp(bn::sprite_items::square.create_sprite(x, y)) { _sp.set_z_order(1); } // behind Text

        // Higher z draws first (further back); lower z draws on top. Default 1.
        Box& layer(int z) { _sp.set_z_order(z); return *this; }

        Box& move(fixed dx_, fixed dy_) { _sp.set_position(_sp.x() + dx_, _sp.y() + dy_); return *this; }
        Box& pos(fixed x, fixed y)      { _sp.set_position(x, y);   return *this; }
        Box& pos(const bn::fixed_point& p) { _sp.set_position(p);   return *this; }
        // Squares share one palette, so fill() recolors ALL square actors at
        // once (by design — one accent color reads as intentional). Set it once.
        Box& fill(color c)              { bn::sprite_palette_ptr p = _sp.palette(); p.set_fade(c, 1); return *this; }
        Box& size(fixed s)              { _sp.set_scale(s);         return *this; } // 8px * s
        Box& size(fixed w, fixed h)     { _sp.set_scale(w, h);      return *this; }
        Box& angle(fixed deg)           { _sp.set_rotation_angle(bn::safe_degrees_angle(deg)); return *this; }
        Box& show(bool v = true)        { _sp.set_visible(v);       return *this; }

        fixed x() const { return _sp.x(); }
        fixed y() const { return _sp.y(); }
        bn::sprite_ptr& sprite() { return _sp; }

    private:
        bn::sprite_ptr _sp;
    };

    // ---- audio --------------------------------------------------------------
    // Drop files in audio/: *.wav -> bn::sound_items::name (SFX),
    // *.mod/.xm/.it/.s3m -> bn::music_items::name (music). Pass those here.
    inline void sfx(const bn::sound_item& s, fixed vol = 1)                      { s.play(vol); }
    inline void music(const bn::music_item& m, fixed vol = 1, bool loop = true)  { m.play(vol, loop); }
    inline void music_stop()          { if(bn::music::playing()) bn::music::stop(); }
    inline void music_volume(fixed v) { if(bn::music::playing()) bn::music::set_volume(v); }

    // ---- drawing: Circle — the joy primitive --------------------------------
    // A circle sprite whose SIZE is picked from a pre-baked sheet of radii (no
    // hardware scaling → no 32-affine-matrix limit → hundreds are cheap). Circles
    // are radially symmetric so they never need rotation. Like Box, all Circles
    // share one palette (fill() = one global accent).
    class Circle
    {
    public:
        static constexpr int SIZES = 16;      // frames in graphics/circle.bmp
        static constexpr int STEP  = 2;       // px of diameter per frame (2..32)

        explicit Circle(fixed x = 0, fixed y = 0)
            : _sp(bn::sprite_items::circle.create_sprite(x, y, 0)) { _sp.set_z_order(1); }

        Circle& pos(fixed x, fixed y)      { _sp.set_position(x, y); return *this; }
        Circle& pos(const bn::fixed_point& p) { _sp.set_position(p); return *this; }
        Circle& move(fixed dx_, fixed dy_) { _sp.set_position(_sp.x() + dx_, _sp.y() + dy_); return *this; }
        Circle& radius(fixed r)            { return diameter(r * 2); }
        Circle& diameter(fixed d)          { int f = (d / STEP + fixed(0.5)).integer() - 1;
                                             f = f < 0 ? 0 : (f >= SIZES ? SIZES - 1 : f);
                                             if(f != _frame) { _frame = f; _sp.set_item(bn::sprite_items::circle, f); }
                                             return *this; }  // skip the tile swap when size is unchanged
        Circle& fill(color c)              { bn::sprite_palette_ptr p = _sp.palette(); p.set_fade(c, 1); return *this; }
        Circle& show(bool v = true)        { _sp.set_visible(v); return *this; }
        Circle& layer(int z)               { _sp.set_z_order(z); return *this; }

        fixed x() const { return _sp.x(); }
        fixed y() const { return _sp.y(); }
        bn::sprite_ptr& sprite() { return _sp; }

    private:
        bn::sprite_ptr _sp;
        int _frame = 0;
    };

    // ---- the loop -----------------------------------------------------------
    // Provide these two; call pk::run() from main().
    void setup();
    void update();

    inline void run()
    {
        bn::core::init();
        setup();
        while(true)
        {
            update();
            bn::core::update();
            ++frame;
        }
    }
}

#endif
