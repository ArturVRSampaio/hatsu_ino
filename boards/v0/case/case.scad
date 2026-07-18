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

// ── Text labels ────────────────────────────────────────────────────
lid_label_text  = "hatsu_ino v0";
lid_label_size  = 6;     // mm, character height
lid_label_h     = 0.6;   // mm, embossed (raised) on the lid's outer face

sig_text        = "ArturVRSampaio";
sig_size        = 3;     // mm, character height
sig_depth       = 0.6;   // mm, engraved (recessed) into the base floor

// ── Enclosure interior (adjust to taste once parts are confirmed) ────
// Floor plan: the speaker occupies its own zone (X 0-46), fully separate
// from the electronics zone (X 46-92) where the Nano/SD/PAM stack along Y.
// This split-by-X layout is what keeps the speaker ring from intersecting
// any of the boards — do not shrink in_w below what the three stacked
// board heights + gaps require (18 + 24 + 18 + margins ≈ 74mm minimum).
in_l = 92;  // interior length (X)
in_w = 80;  // interior width  (Y) — sized to stack Nano/SD/PAM without overlapping the speaker
in_h = 26;  // interior height (Z), measured from floor top face — sized for a 1-2W speaker (deeper magnet/frame than a 0.5W speaker)

// ── Arduino Nano ───────────────────────────────────────────────────
// The Mini-USB connector is on the board's SHORT edge, not the long side —
// the Nano sits flush against the far (X = in_l) wall so that edge, and
// the USB cutout below, actually line up.
nano_l = 45;
nano_w = 18;
nano_usb_w = 8.5;   // Mini-USB connector opening width
nano_usb_h = 4.0;   // Mini-USB connector opening height
nano_pos_x = in_l - nano_l;   // flush against the far wall
nano_pos_y = 5;

// ── SD card module (microSD) ──────────────────────────────────────
// Also flush against the far wall, same side as the USB cutout, so the
// card-eject edge lines up with the microSD slot cut below it.
sd_l = 42;
sd_w = 24;
sd_card_slot_w = 15;  // microSD width + finger clearance (NOT full-size SD)
sd_card_slot_h = 3.0;
sd_pos_x = in_l - sd_l;
sd_pos_y = 28;

// ── PAM8403 amp board ─────────────────────────────────────────────
// No external connector needed, so it just needs to sit in the
// electronics zone (X >= 46) without overlapping the Nano/SD Y-bands.
pam_l = 32;
pam_w = 18;
pam_pos_x = 47;
pam_pos_y = 57;

// ── Speaker (40mm 8Ω, 1-2W) ─────────────────────────────────────────
// Centered in its own zone (X 0-46), clear of the electronics zone.
spk_d       = 40 + 1.0;  // + fit clearance
spk_lip_d   = spk_d - 6; // retention lip the speaker rim rests on
spk_pos_x   = 23;
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

        // microSD card slot — cut through the far wall, aligned with the SD module
        translate([wall + in_l - 1, wall + sd_pos_y + (sd_w - sd_card_slot_w) / 2,
                   floor_t + pcb_t + 1])
            cube([wall + 2, sd_card_slot_w, sd_card_slot_h + tol]);

        // USB access — cut through the far wall (same wall as the SD slot,
        // different Y band), aligned with the Nano's short edge
        translate([wall + in_l - 1, wall + nano_pos_y + (nano_w - nano_usb_w) / 2,
                   floor_t + pcb_t])
            cube([wall + 2, nano_usb_w + tol, nano_usb_h + tol]);

        // signature — engraved into the floor's open front strip (Y 0-5,
        // before the Nano/speaker-ring rows start), visible with the lid off
        translate([wall + in_l / 2, wall + 2.5, floor_t - sig_depth])
            linear_extrude(height = sig_depth + 0.1)
                text(sig_text, size = sig_size, halign = "center", valign = "center",
                     font = "Liberation Sans");
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

    // logo — embossed on the outer face, centered over the electronics
    // zone (the speaker grille occupies the other half of the lid)
    translate([wall + (46 + in_l) / 2, wall + in_w / 2, lid_t])
        linear_extrude(height = lid_label_h)
            text(lid_label_text, size = lid_label_size, halign = "center", valign = "center",
                 font = "Liberation Sans:style=Bold");
}

if (part == "base") {
    base();
} else if (part == "lid") {
    translate([0, out_w + 10, 0]) lid();
} else {
    base();
    translate([0, out_w + 10, 0]) lid();
}
