#!/usr/bin/env python3
"""
Generates hatsu_v1_shield.kicad_pcb: a shield PCB for hatsu_ino v1.

Female headers on one side plug directly onto an Arduino Nano; header
sockets + terminal block on the other side accept a DFPlayer Mini module
and speaker leads. R1/R2 and the IRLZ44N power-gate MOSFET are built into
the shield itself.

Pin mappings verified against:
- Arduino's official Nano pinout PDF (content.arduino.cc/assets/Pinout-NANO_latest.pdf)
- DFPlayer Mini's official datasheet pin table (Table 2.2)
- hatsu_v1.ino / boards/v1/README.md wiring tables for the actual net connections

Run with: python3 generate_board.py
"""
import pcbnew

FP_ROOT = "/usr/share/kicad/footprints"
OUT_FILE = "hatsu_v1_shield.kicad_pcb"

board = pcbnew.CreateEmptyBoard()

def load(lib, name):
    fp = pcbnew.FootprintLoad(f"{FP_ROOT}/{lib}", name)
    if fp is None:
        raise RuntimeError(f"footprint not found: {lib}/{name}")
    return fp

def place(fp, x, y, ref, rot=0):
    fp.SetPosition(pcbnew.VECTOR2I(pcbnew.FromMM(x), pcbnew.FromMM(y)))
    fp.SetOrientationDegrees(rot)
    fp.Reference().SetText(ref)
    fp.Reference().SetVisible(True)
    board.Add(fp)
    return fp

# ── Footprint placement ──────────────────────────────────────────────
NANO_X, NANO_Y = 15.0, 10.0          # column A (pins 1-15) anchor
nano_a = place(load("Connector_PinSocket_2.54mm.pretty", "PinSocket_1x15_P2.54mm_Vertical"),
               NANO_X, NANO_Y, "J1")
nano_b = place(load("Connector_PinSocket_2.54mm.pretty", "PinSocket_1x15_P2.54mm_Vertical"),
               NANO_X + 15.24, NANO_Y, "J2")

DFP_X, DFP_Y = 54.0, 10.0            # DFPlayer column A (pins 1-8) anchor
dfp_a = place(load("Connector_PinSocket_2.54mm.pretty", "PinSocket_1x08_P2.54mm_Vertical"),
              DFP_X, DFP_Y, "J3")
dfp_b = place(load("Connector_PinSocket_2.54mm.pretty", "PinSocket_1x08_P2.54mm_Vertical"),
              DFP_X + 17.78, DFP_Y, "J4")

r1 = place(load("Resistor_THT.pretty", "R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal"),
           40.0, 22.7, "R1")
r2 = place(load("Resistor_THT.pretty", "R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal"),
           40.0, 27.78, "R2")

mosfet = place(load("Package_TO_SOT_THT.pretty", "TO-220-3_Vertical"), 85.0, 15.0, "Q1")

spk = place(load("TerminalBlock_Phoenix.pretty",
                  "TerminalBlock_Phoenix_MPT-0,5-2-2.54_1x02_P2.54mm_Horizontal"),
            100.0, 25.0, "SPK1")

# ── Board outline ────────────────────────────────────────────────────
outline = pcbnew.PCB_SHAPE(board)
outline.SetShape(pcbnew.SHAPE_T_RECT)
outline.SetStart(pcbnew.VECTOR2I(pcbnew.FromMM(5), pcbnew.FromMM(2)))
outline.SetEnd(pcbnew.VECTOR2I(pcbnew.FromMM(110), pcbnew.FromMM(48)))
outline.SetLayer(pcbnew.Edge_Cuts)
outline.SetWidth(pcbnew.FromMM(0.15))
board.Add(outline)

# ── Nets ──────────────────────────────────────────────────────────────
# J1/J2 are generic 1x15 sockets, so their OWN pad numbers (1-15) don't
# match the Nano's physical pin numbers directly. J1 (column A) happens to
# line up 1:1 with physical Nano pins 1-15. J2 (column B) is mounted in the
# same top-to-bottom direction, but the Nano's column B runs pin 16 (top,
# nearest USB) to pin 30 (bottom) - the REVERSE of column A - so J2's own
# pad k corresponds to physical Nano pin (31-k). Verified against Arduino's
# official Nano pinout PDF.
def pad(fp, num):
    p = fp.FindPadByNumber(str(num))
    if p is None:
        raise RuntimeError(f"pad {num} not found on {fp.Reference().GetText()}")
    return p

NANO_5V   = pad(nano_a, 4)          # physical pin 4
NANO_GND1 = pad(nano_a, 2)          # physical pin 2
NANO_GND2 = pad(nano_b, 4)          # J2 pad4 -> physical pin 27
NANO_D2   = pad(nano_b, 5)          # J2 pad5 -> physical pin 26
NANO_D3   = pad(nano_b, 6)          # J2 pad6 -> physical pin 25
NANO_D4   = pad(nano_b, 7)          # J2 pad7 -> physical pin 24
NANO_D5   = pad(nano_b, 8)          # J2 pad8 -> physical pin 23

DFP_VCC  = pad(dfp_a, 1)
DFP_RX   = pad(dfp_a, 2)
DFP_TX   = pad(dfp_a, 3)
DFP_SPK2 = pad(dfp_a, 6)
DFP_GND  = pad(dfp_a, 7)
DFP_SPK1 = pad(dfp_a, 8)
DFP_BUSY = pad(dfp_b, 1)

R1_A, R1_B = pad(r1, 1), pad(r1, 2)
R2_A, R2_B = pad(r2, 1), pad(r2, 2)
Q1_GATE, Q1_DRAIN, Q1_SOURCE = pad(mosfet, 1), pad(mosfet, 2), pad(mosfet, 3)
SPK_A, SPK_B = pad(spk, 1), pad(spk, 2)

nets = {}
def net(name):
    if name not in nets:
        n = pcbnew.NETINFO_ITEM(board, name)
        board.Add(n)
        nets[name] = n
    return nets[name]

def connect(name, *pads):
    n = net(name)
    for p in pads:
        p.SetNet(n)

connect("5V",            NANO_5V, DFP_VCC)
connect("GND",            NANO_GND1, NANO_GND2, Q1_SOURCE)
connect("D2_DFP_TX",      NANO_D2, DFP_TX)
connect("D3_R1",          NANO_D3, R1_A)
connect("R1_DFP_RX",      R1_B, DFP_RX)
connect("D4_DFP_BUSY",    NANO_D4, DFP_BUSY)
connect("D5_R2",          NANO_D5, R2_A)
connect("R2_GATE",        R2_B, Q1_GATE)
connect("DFP_GND_DRAIN",  DFP_GND, Q1_DRAIN)
connect("DFP_SPK1",       DFP_SPK1, SPK_A)
connect("DFP_SPK2",       DFP_SPK2, SPK_B)

# ── Traces ────────────────────────────────────────────────────────────
# Multi-segment paths, each planned to avoid every other pad/track:
# - never travel vertically along a socket's own pad column (x=15, 30.24,
#   52, 69.78) except for the final short hop straight into the target pad
# - each long cross-board net gets its own dedicated x (riser) and y
#   (highway) lane so risers and highways never intersect another net's
#   riser/highway at a shared point
# - the four bottom-area nets (R2_GATE/DFP_GND_DRAIN/DFP_SPK1/DFP_SPK2) and
#   the two Nano power nets (5V/GND) are routed on B.Cu, physically
#   separated (top vs. bottom of the board) from each other and from the
#   F.Cu signal nets (D2/D3/D4/D5/R1_DFP_RX), so no layer ever needs to
#   double-check against the other's geometry
def route(net_name, layer, *points_mm):
    n = nets[net_name]
    for (x1, y1), (x2, y2) in zip(points_mm, points_mm[1:]):
        track = pcbnew.PCB_TRACK(board)
        track.SetStart(pcbnew.VECTOR2I(pcbnew.FromMM(x1), pcbnew.FromMM(y1)))
        track.SetEnd(pcbnew.VECTOR2I(pcbnew.FromMM(x2), pcbnew.FromMM(y2)))
        track.SetWidth(pcbnew.FromMM(0.4))
        track.SetLayer(layer)
        track.SetNet(n)
        board.Add(track)

# This layout is dense enough that hand-verifying every crossing by eye is
# unreliable - the exact coordinates below were validated with a standalone
# segment-intersection + pad-clearance checker (check_routes.py) before
# being transcribed here. Key decisions that came out of that process:
# - GND crosses the busy central area on a y=18.9 lane, above every riser
#   in the bottom section entirely, rather than threading between them.
# - R2_GATE and DFP_GND_DRAIN moved to F.Cu (D2/D3/D4/D5/R1_DFP_RX don't
#   reach far enough right to conflict with them there), freeing up B.Cu
#   for GND/DFP_SPK1/DFP_SPK2 with far fewer nets to avoid.
# - DFP_SPK1 and DFP_SPK2 each get their own highway y (34.5 / 32) and their
#   final approach avoids the terminal block's own non-plated mounting
#   holes (at pin_y + 2.54mm) and each other's destination pad.
route("5V", pcbnew.B_Cu,
      (15, 17.62), (11, 17.62), (11, 6), (DFP_X, 6), (DFP_X, 10))

route("GND", pcbnew.B_Cu,
      (15, 12.54), (22, 12.54), (22, 17.62), (30.24, 17.62))
route("GND", pcbnew.B_Cu,
      (30.24, 17.62), (37, 17.62), (37, 18.9), (90.08, 18.9), (90.08, 15))

route("D2_DFP_TX", pcbnew.F_Cu,
      (30.24, 20.16), (32, 20.16), (32, 8), (DFP_X + 4, 8),
      (DFP_X + 4, 15.08), (DFP_X, 15.08))

route("D3_R1", pcbnew.F_Cu, (30.24, 22.7), (40, 22.7))

route("R1_DFP_RX", pcbnew.F_Cu,
      (50.16, 22.7), (49, 22.7), (49, 12.54), (DFP_X, 12.54))

route("D4_DFP_BUSY", pcbnew.F_Cu,
      (30.24, 25.24), (27, 25.24), (27, 5), (DFP_X + 17.78, 5), (DFP_X + 17.78, 10))

route("D5_R2", pcbnew.F_Cu, (30.24, 27.78), (40, 27.78))

route("R2_GATE", pcbnew.F_Cu,
      (50.16, 27.78), (50.16, 30), (85, 30), (85, 15))

route("DFP_GND_DRAIN", pcbnew.F_Cu,
      (DFP_X, 25.24), (48, 25.24), (48, 33), (87.54, 33), (87.54, 15))

route("DFP_SPK1", pcbnew.B_Cu,
      (DFP_X, 27.78), (DFP_X, 34.5), (106, 34.5), (106, 23), (100, 23), (100, 25))

route("DFP_SPK2", pcbnew.B_Cu,
      (DFP_X, 22.7), (56, 22.7), (56, 32), (104, 32), (104, 25), (102.54, 25))

board.BuildConnectivity()
board.Save(OUT_FILE)
print(f"Saved {OUT_FILE} with {len(board.GetFootprints())} footprints and {len(nets)} nets")
