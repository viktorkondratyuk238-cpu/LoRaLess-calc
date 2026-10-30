# Checks the PCB netlist against what the firmware expects.
# Run:  python -I check_board.py path\to\lora-calculator_jlcpcb.kicad_sch
# Prints PASS/FAIL per check and exits 1 if anything fails.
# It checks wiring only. It cannot tell whether the screen, keys or radio work on the board.
import os, sys, shutil, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

# netcheck.py opens a fixed filename in the current folder, so run it from a temp copy.
src = os.path.abspath(sys.argv[1])
work = tempfile.mkdtemp()
shutil.copy(src, os.path.join(work, "lora-calculator_jlcpcb_updated.kicad_sch"))
os.chdir(work)
import netcheck as n

failures = []

def check(name, ok, detail=""):
    print(("PASS " if ok else "FAIL ") + name + (("  (" + detail + ")") if detail and not ok else ""))
    if not ok:
        failures.append(name)

def net(ref, pin):
    return n.net_of(ref, pin)

def only_pin_on_net(ref, pin):
    """True if nothing else on the schematic shares this pin's net (i.e. it is left unconnected)."""
    target = net(ref, pin)
    others = [k for k in n.pin_nodes if k != (ref, pin) and net(*k) == target]
    return not others, others

# ---- ESP32-C3 (U4) to MCP23017 (U1) I2C bus. Firmware: SDA = IO4, SCL = IO5 ----
# U4 pin numbers from the WROOM-02U symbol: 3 = IO4, 4 = IO5. U1: 13 = SDA, 12 = SCK (I2C clock).
check("I2C SDA: ESP32 IO4 (U4.3) -> MCP23017 SDA (U1.13)", net("U4", "3") == net("U1", "13"),
      f"U4.3={net('U4','3')} U1.13={net('U1','13')}")
check("I2C SCL: ESP32 IO5 (U4.4) -> MCP23017 SCL (U1.12)", net("U4", "4") == net("U1", "12"),
      f"U4.4={net('U4','4')} U1.12={net('U1','12')}")

# ---- MCP23017 address 0x20: A0, A1, A2 (U1.15-17) tied to GND ----
for pin, name in (("15", "A0"), ("16", "A1"), ("17", "A2")):
    check(f"MCP address pin {name} (U1.{pin}) tied to GND", net("U1", pin) == "GND", net("U1", pin))

# ---- MCP23017 power and reset ----
check("MCP VDD (U1.9) on +3.3V", net("U1", "9") == "+3.3V", net("U1", "9"))
check("MCP VSS (U1.10) on GND", net("U1", "10") == "GND", net("U1", "10"))
check("MCP RESET (U1.18) on +3.3V (not held in reset)", net("U1", "18") == "+3.3V", net("U1", "18"))

# ---- Screen header J2: module labels are GND, VCC, SCL, SDA ----
check("Screen J2.1 is GND", net("J2", "1") == "GND", net("J2", "1"))
check("Screen J2.2 (VCC) is +3.3V", net("J2", "2") == "+3.3V", net("J2", "2"))
check("Screen J2.3 (SCL) is ESP32 IO5", net("J2", "3") == net("U4", "4"),
      f"J2.3={net('J2','3')} U4.4={net('U4','4')}")
check("Screen J2.4 (SDA) is ESP32 IO4", net("J2", "4") == net("U4", "3"),
      f"J2.4={net('J2','4')} U4.3={net('U4','3')}")

# ---- Keypad matrix: firmware uses rows GPA0-5 (U1.21-26) and columns GPB0-4 (U1.1-5) ----
ROW_PINS = ["21", "22", "23", "24", "25", "26"]
COL_PINS = ["1", "2", "3", "4", "5"]
ROW_NETS = [net("U1", p) for p in ROW_PINS]
COL_NETS = [net("U1", p) for p in COL_PINS]
check("Matrix row nets are named ROW_0..ROW_5", ROW_NETS == [f"ROW_{i}" for i in range(6)], str(ROW_NETS))
check("Matrix column nets are named COL_0..COL_4", COL_NETS == [f"COL_{i}" for i in range(5)], str(COL_NETS))

# Each key: switch pin 1 on a row, switch pin 2 -> diode -> a column. Collect (row, col) per key.
keys = {}
for (ref, pin) in list(n.pin_nodes):
    if ref.startswith("SW") and ref != "SW31" and pin == "1":
        row = net(ref, "1")
        sw2 = net(ref, "2")
        diodes = [k for k in n.pin_nodes if k[0].startswith("D") and k[1] == "1" and net(*k) == sw2]
        cols = []
        for d in diodes:
            other = [k for k in n.pin_nodes if k[0] == d[0] and k[1] == "2"]
            cols += [net(*k) for k in other]
        keys[ref] = (row, cols[0] if cols else None)

check("30 keys found in the matrix (SW1-SW30)", len(keys) == 30, f"found {len(keys)}")
pairs = [v for v in keys.values()]
check("Every key has a unique row/column pair", len(set(pairs)) == len(pairs) and None not in [c for _, c in pairs],
      str(sorted(pairs, key=str)[:6]) + " ...")
check("Every row/column combination is used (6 x 5 = 30)", len(set(pairs)) == 30 and
      set(pairs) == {(r, c) for r in ROW_NETS for c in COL_NETS}, "")

# ---- Unused expander pins: firmware drives these as outputs, so they must not be wired to anything ----
for p in ("6", "7", "8", "27", "28"):
    free, others = only_pin_on_net("U1", p)
    check(f"Unused MCP pin U1.{p} is not connected to any part", free, str(others))

print()
print(f"{len(failures)} check(s) failed" if failures else "All checks passed")
sys.exit(1 if failures else 0)
