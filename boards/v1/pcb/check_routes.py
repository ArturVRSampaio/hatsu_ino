#!/usr/bin/env python3
"""Standalone geometric sanity-checker for generate_board.py's route plan.
Checks every same-layer segment pair for intersection, and every segment
against every pad NOT on that net for near-miss (pad hole clearance).
Run BEFORE generate_board.py to catch routing mistakes cheaply.
"""

CLEARANCE = 0.9  # mm, conservative combined pad-radius+trace+clearance guard

# pad_name -> (x, y, net)
PADS = {
    "NANO_5V": (15, 17.62, "5V"), "NANO_GND1": (15, 12.54, "GND"),
    "NANO_GND2": (30.24, 17.62, "GND"), "NANO_D2": (30.24, 20.16, "D2_DFP_TX"),
    "NANO_D3": (30.24, 22.7, "D3_R1"), "NANO_D4": (30.24, 25.24, "D4_DFP_BUSY"),
    "NANO_D5": (30.24, 27.78, "D5_R2"),
    "DFP_VCC": (54, 10, "5V"), "DFP_RX": (54, 12.54, "R1_DFP_RX"),
    "DFP_TX": (54, 15.08, "D2_DFP_TX"), "DFP_DACR": (54, 17.62, None),
    "DFP_DACL": (54, 20.16, None), "DFP_SPK2": (54, 22.7, "DFP_SPK2"),
    "DFP_GND": (54, 25.24, "DFP_GND_DRAIN"), "DFP_SPK1": (54, 27.78, "DFP_SPK1"),
    "J4_BUSY": (71.78, 10, "D4_DFP_BUSY"), "J4_2": (71.78, 12.54, None),
    "J4_3": (71.78, 15.08, None), "J4_4": (71.78, 17.62, None),
    "J4_5": (71.78, 20.16, None), "J4_6": (71.78, 22.7, None),
    "J4_7": (71.78, 25.24, None), "J4_8": (71.78, 27.78, None),
    "R1_A": (40, 22.7, "D3_R1"), "R1_B": (50.16, 22.7, "R1_DFP_RX"),
    "R2_A": (40, 27.78, "D5_R2"), "R2_B": (50.16, 27.78, "R2_GATE"),
    "Q1_GATE": (85, 15, "R2_GATE"), "Q1_DRAIN": (87.54, 15, "DFP_GND_DRAIN"),
    "Q1_SOURCE": (90.08, 15, "GND"),
    "SPK_A": (100, 25, "DFP_SPK1"), "SPK_B": (102.54, 25, "DFP_SPK2"),
    "SPK_MOUNT1": (100, 27.54, None), "SPK_MOUNT2": (102.54, 27.54, None),
}
for k, (x, y, n) in list(PADS.items()):
    if "J2_" not in k and k.startswith("_"):
        pass
# J1 column (physical pins 1-15, pad matches 1:1)
for i in range(15):
    y = 10 + i * 2.54
    net = {2: "GND", 4: "5V"}.get(i + 1)
    PADS[f"J1_{i+1}"] = (15, y, net)
# J2 column (own pad k -> physical pin 31-k); only a few carry nets
J2_NET = {4: "GND", 5: "D2_DFP_TX", 6: "D3_R1", 7: "D4_DFP_BUSY", 8: "D5_R2"}
for k in range(1, 16):
    y = 10 + (k - 1) * 2.54
    PADS[f"J2_{k}"] = (30.24, y, J2_NET.get(k))

ROUTES = {
    "5V": ("B", [(15, 17.62), (11, 17.62), (11, 6), (54, 6), (54, 10)]),
    "GND_A": ("B", [(15, 12.54), (22, 12.54), (22, 17.62), (30.24, 17.62)]),
    "GND_B": ("B", [(30.24, 17.62), (37, 17.62), (37, 18.9), (90.08, 18.9), (90.08, 15)]),
    "D2_DFP_TX": ("F", [(30.24, 20.16), (32, 20.16), (32, 8), (58, 8), (58, 15.08), (54, 15.08)]),
    "D3_R1": ("F", [(30.24, 22.7), (40, 22.7)]),
    "R1_DFP_RX": ("F", [(50.16, 22.7), (49, 22.7), (49, 12.54), (54, 12.54)]),
    "D4_DFP_BUSY": ("F", [(30.24, 25.24), (27, 25.24), (27, 5), (71.78, 5), (71.78, 10)]),
    "D5_R2": ("F", [(30.24, 27.78), (40, 27.78)]),
    "R2_GATE": ("F", [(50.16, 27.78), (50.16, 30), (85, 30), (85, 15)]),
    "DFP_GND_DRAIN": ("F", [(54, 25.24), (48, 25.24), (48, 33), (87.54, 33), (87.54, 15)]),
    "DFP_SPK1": ("B", [(54, 27.78), (54, 34.5), (106, 34.5), (106, 23), (100, 23), (100, 25)]),
    "DFP_SPK2": ("B", [(54, 22.7), (56, 22.7), (56, 32), (104, 32), (104, 25), (102.54, 25)]),
}
NET_OF = {"GND_A": "GND", "GND_B": "GND"}

def seg_intersect(p1, p2, p3, p4):
    def ccw(a, b, c):
        return (c[1]-a[1])*(b[0]-a[0]) - (b[1]-a[1])*(c[0]-a[0])
    d1, d2, d3, d4 = ccw(p3,p4,p1), ccw(p3,p4,p2), ccw(p1,p2,p3), ccw(p1,p2,p4)
    if ((d1>0)!=(d2>0)) and ((d3>0)!=(d4>0)):
        return True
    # collinear-overlap check (axis-aligned only, good enough here)
    if p1[0]==p2[0]==p3[0]==p4[0]:
        lo1,hi1=sorted([p1[1],p2[1]]); lo2,hi2=sorted([p3[1],p4[1]])
        return max(lo1,lo2) < min(hi1,hi2)
    if p1[1]==p2[1]==p3[1]==p4[1]:
        lo1,hi1=sorted([p1[0],p2[0]]); lo2,hi2=sorted([p3[0],p4[0]])
        return max(lo1,lo2) < min(hi1,hi2)
    return False

def point_seg_dist(px, py, a, b):
    ax, ay = a; bx, by = b
    dx, dy = bx-ax, by-ay
    if dx == 0 and dy == 0:
        return ((px-ax)**2 + (py-ay)**2) ** 0.5
    t = max(0, min(1, ((px-ax)*dx + (py-ay)*dy) / (dx*dx+dy*dy)))
    cx, cy = ax+t*dx, ay+t*dy
    return ((px-cx)**2 + (py-cy)**2) ** 0.5

problems = 0
names = list(ROUTES.keys())
for i, n1 in enumerate(names):
    layer1, pts1 = ROUTES[n1]
    net1 = NET_OF.get(n1, n1)
    for n2 in names[i+1:]:
        layer2, pts2 = ROUTES[n2]
        net2 = NET_OF.get(n2, n2)
        if layer1 != layer2 or net1 == net2:
            continue
        for a1, a2 in zip(pts1, pts1[1:]):
            for b1, b2 in zip(pts2, pts2[1:]):
                if seg_intersect(a1, a2, b1, b2):
                    print(f"CROSS  {n1} x {n2}  seg {a1}-{a2} vs {b1}-{b2}")
                    problems += 1

for rname, (layer, pts) in ROUTES.items():
    net = NET_OF.get(rname, rname)
    for a, b in zip(pts, pts[1:]):
        for pname, (px, py, pnet) in PADS.items():
            if pnet == net:
                continue
            d = point_seg_dist(px, py, a, b)
            if d < CLEARANCE:
                print(f"NEAR-PAD  {rname} seg {a}-{b}  ~{pname}({px},{py}, net={pnet})  dist={d:.2f}mm")
                problems += 1

print(f"\n{problems} potential problem(s) found")
