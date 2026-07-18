// hatsu_ino v0 — 3D-printable enclosure
//
// All dimensions below are typical values for common hobbyist breakout
// boards, NOT measured from your specific parts. Measure your actual
// Nano, SD module, PAM8403, and speaker with calipers and adjust the
// values in this section before printing — module dimensions vary
// between suppliers.
//
// Render/export:
//   openscad -o base.stl -D 'part="base"' case.scad
//   openscad -o lid.stl  -D 'part="lid"'  case.scad
//   openscad -o preview.png -D 'part="both"' case.scad   (for a quick look)

part = "both"; // "base", "lid", or "both" — overridable with -D part="..."

// ── General ──────────────────────────────────────────────────────────
wall      = 2.2;   // shell wall thickness
tol       = 0.3;   // clearance added to slots/pockets so parts actually fit
pcb_t     = 1.6;   // standard PCB thickness (Nano / SD module / PAM8403)
floor_t   = 2.0;   // base floor thickness
lid_t     = 2.0;   // lid plate thickness

// ── Enclosure interior (adjust to taste once parts are confirmed) ────
in_l = 92;  // interior length (X)
in_w = 58;  // interior width  (Y)
in_h = 24;  // interior height (Z), measured from floor top face

// ── Arduino Nano ───────────────────────────────────────────────────
nano_l = 45;
nano_w = 18;
nano_usb_w = 8.5;   // Mini-USB connector opening width
nano_usb_h = 4.0;   // Mini-USB connector opening height
nano_pos_x = in_l - nano_l - 6;   // Nano sits near the USB-accessible wall
nano_pos_y = 6;

// ── SD card module ────────────────────────────────────────────────
sd_l = 42;
sd_w = 24;
sd_card_slot_w = 26;  // opening width for the card itself to slide out
sd_card_slot_h = 3.0;
sd_pos_x = 6;
sd_pos_y = in_w - sd_w - 6;

// ── PAM8403 amp board ─────────────────────────────────────────────
pam_l = 32;
pam_w = 18;
pam_pos_x = 6;
pam_pos_y = 6;

// ── Speaker (40mm 8Ω) ──────────────────────────────────────────────
spk_d       = 40 + 1.0;  // + fit clearance
spk_lip_d   = spk_d - 6; // retention lip the speaker rim rests on
spk_pos_x   = in_l - 24;
spk_pos_y   = in_w / 2;
grille_hole_d = 3;
grille_spacing = 6;

// ── Screw bosses (base <-> lid, M3 self-tapping) ──────────────────
boss_od = 7;
boss_id = 2.6;  // pilot hole for M3 self-tapper
boss_inset = 6; // distance of boss center from each corner

// ── Derived exterior dimensions ────────────────────────────────────
out_l = in_l + 2 * wall;
out_w = in_w + 2 * wall;
out_h = in_h + floor_t;

module pcb_edge_rails(board_l, board_w, pos_x, pos_y, rail_h = 4) {
    // Two raised rails that grip the long edges of a PCB, holderless-mount style
    rail_w = 1.6;
    translate([pos_x, pos_y, floor_t])
        cube([board_l, rail_w, rail_h]);
    translate([pos_x, pos_y + board_w - rail_w, floor_t])
        cube([board_l, rail_w, rail_h]);
}

module screw_boss(h) {
    difference() {
        cylinder(h = h, d = boss_od, $fn = 32);
        cylinder(h = h + 1, d = boss_id, $fn = 32);
    }
}

module screw_bosses(h) {
    for (pos = [
        [boss_inset, boss_inset],
        [out_l - boss_inset, boss_inset],
        [boss_inset, out_w - boss_inset],
        [out_l - boss_inset, out_w - boss_inset]
    ]) {
        translate([pos[0], pos[1], floor_t])
            screw_boss(h);
    }
}

module base() {
    difference() {
        union() {
            // outer shell
            cube([out_l, out_w, out_h]);
            screw_bosses(in_h - 2);
        }
        // hollow interior
        translate([wall, wall, floor_t])
            cube([in_l, in_w, in_h + 1]);

        // SD card slot — cut through the side wall, aligned with the module
        translate([-1, wall + sd_pos_y + (sd_w - sd_card_slot_w) / 2,
                   floor_t + pcb_t + 1])
            cube([wall + 2, sd_card_slot_w, sd_card_slot_h + tol]);

        // USB access — cut through the end wall, aligned with the Nano
        translate([wall + nano_pos_x + (nano_l - nano_usb_w) / 2, -1,
                   floor_t + pcb_t])
            cube([nano_usb_w + tol, wall + 2, nano_usb_h + tol]);
    }

    translate([wall, wall, 0]) {
        pcb_edge_rails(nano_l, nano_w, nano_pos_x, nano_pos_y);
        pcb_edge_rails(sd_l, sd_w, sd_pos_x, sd_pos_y);
        pcb_edge_rails(pam_l, pam_w, pam_pos_x, pam_pos_y);

        // speaker retention lip — speaker rim rests on this ring, facing up into the lid grille
        translate([spk_pos_x, spk_pos_y, floor_t])
            difference() {
                cylinder(h = 3, d = spk_d + 4, $fn = 64);
                cylinder(h = 4, d = spk_lip_d, $fn = 64);
            }
    }
}

module speaker_grille() {
    // radial ring pattern of holes over the speaker position
    for (r = [spk_d/2 - 4 : -grille_spacing : 4]) {
        count = max(6, floor(2 * 3.14159 * r / grille_spacing));
        for (i = [0 : count - 1]) {
            a = i * 360 / count;
            translate([spk_pos_x + r * cos(a), spk_pos_y + r * sin(a), -1])
                cylinder(h = lid_t + 2, d = grille_hole_d, $fn = 16);
        }
    }
}

module lid() {
    difference() {
        cube([out_l, out_w, lid_t]);
        translate([wall, wall, 0])
            speaker_grille();
        for (pos = [
            [boss_inset, boss_inset],
            [out_l - boss_inset, boss_inset],
            [boss_inset, out_w - boss_inset],
            [out_l - boss_inset, out_w - boss_inset]
        ]) {
            translate([pos[0], pos[1], -1])
                cylinder(h = lid_t + 2, d = boss_id + 0.4, $fn = 32);
        }
    }
}

if (part == "base") {
    base();
} else if (part == "lid") {
    translate([0, out_w + 10, 0]) lid();
} else {
    base();
    translate([0, out_w + 10, 0]) lid();
}
