#!/usr/bin/env python3
"""
Install RetroWrite II onto a ProDOS disk image that has Apple II DeskTop.

  python3 install_rw2.py IMAGE.po [--ac ac.jar] [--files DIR] [--slot N]

- Copies RW2.SYSTEM (loader), RW2.MAIN, RW2.LC, RW2.FONTS, RW2.OVL1-9, RW2.OVLA-C, RW2.SPLASH
  and SPELL.DICT into /<VOL>/RETROWRITE, with the TUTORIAL document and a
  starter GLOSSARY.
- If the disk has no BASIC.SYSTEM at its top level, copies the first one found
  elsewhere on the disk into RETROWRITE (for Apple menu > BASIC.SYSTEM).
- Adds a DeskTop shortcut "RetroWrite II" to LOCAL/SELECTOR.LIST
  (first free slot of the primary list, normally Open-Apple-8),
  keeping the file's type ($F1) and aux type.

Requires Java and AppleCommander's command-line ac.jar (1.9.0 tested).
"""
import argparse, json, os, re, subprocess, sys, tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import setaux

NAME = "RetroWrite II"
FLAGS = 0xC0                 # DeskTop: never copy to RAMCard
NUM_PRIMARY = 8
NUM_SECONDARY = 16
NAME_SLOT = 16
PATH_SLOT = 64
PATHS_AT = 2 + (NUM_PRIMARY + NUM_SECONDARY) * NAME_SLOT     # 386
FILE_LEN = PATHS_AT + (NUM_PRIMARY + NUM_SECONDARY) * PATH_SLOT  # 1922


def ac(jar, *args, stdin=None):
    r = subprocess.run(["java", "-jar", jar, *args], input=stdin,
                       capture_output=True)
    return r.returncode, r.stdout, r.stderr


def volume_name(jar, image):
    _, out, _ = ac(jar, "-lj", image)
    d = json.loads(out)
    return d["disks"][0]["diskName"].strip("/")


def list_detail(jar, image):
    _, out, _ = ac(jar, "-ll", image)
    return out.decode("latin-1")


def aux_of(jar, image, name):
    """Find aux type of a file in -ll listing (first match by name)."""
    for line in list_detail(jar, image).splitlines():
        parts = line.split()
        if parts and parts[0] == name:
            m = re.findall(r"\$([0-9A-F]{4})", line)
            if m:
                return int(m[0], 16)
    return None


def exists(jar, image, path):
    rc, out, _ = ac(jar, "-g", image, path)
    return rc == 0 and len(out) > 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("image")
    ap.add_argument("--ac", default=os.path.join(os.path.dirname(__file__), "ac.jar"))
    ap.add_argument("--files", default=os.path.dirname(os.path.abspath(__file__)))
    ap.add_argument("--slot", type=int, default=None,
                    help="primary shortcut slot 1-8 (default: first free)")
    a = ap.parse_args()
    jar, img = a.ac, a.image

    vol = volume_name(jar, img)
    print(f"Volume: /{vol}")

    # 1. program files
    for fname, ftype, aux in (("RW2.SYSTEM", "SYS", "0x2000"), ("RW2.MAIN", "BIN", "0x4000"),
                              ("RW2.LC", "BIN", "0xD400"), ("RW2.FONTS", "BIN", "0x0000"),
                              ("RW2.OVL1", "BIN", "0x0000"), ("RW2.OVL2", "BIN", "0x0000"),
                              ("RW2.OVL3", "BIN", "0x0000"), ("RW2.OVL4", "BIN", "0x0000"),
                              ("RW2.OVL5", "BIN", "0x0000"), ("RW2.OVL6", "BIN", "0x0000"),
                              ("RW2.OVL7", "BIN", "0x0000"), ("RW2.OVL8", "BIN", "0x0000"),
                              ("RW2.OVL9", "BIN", "0x0000"), ("RW2.OVLA", "BIN", "0x0000"),
                              ("RW2.OVLB", "BIN", "0x0000"), ("RW2.OVLC", "BIN", "0x0000"),
                              ("RW2.SPLASH", "BIN", "0x0000"), ("SPELL.DICT", "BIN", "0x0000"),
                              ("TUTORIAL", "GWP", "0x5257"), ("GLOSSARY", "TXT", "0x0000")):
        src = os.path.join(a.files, fname)
        data = open(src, "rb").read()
        ac(jar, "-d", img, f"RETROWRITE/{fname}")
        rc, out, err = ac(jar, "-p", img, f"RETROWRITE/{fname}", ftype, aux, stdin=data)
        if rc != 0 or not exists(jar, img, f"RETROWRITE/{fname}"):
            sys.exit(f"Failed to write {fname}: {err.decode()}")
        if ftype == "GWP":                 # AppleCommander leaves the aux type at 0
            setaux.setaux(img, f"RETROWRITE/{fname}", int(aux, 0))
        print(f"Installed /{vol}/RETROWRITE/{fname} ({len(data)} bytes)")

    # 1b. BASIC.SYSTEM for the Apple menu, copied from elsewhere on the same
    #     disk when there is none at the top level or in RETROWRITE
    _, out, _ = ac(jar, "-l", img)
    stack, paths = [], []
    for line in out.decode("latin-1").splitlines()[1:]:
        if not line.startswith("  ") or not line.strip():
            continue
        parts = line.split()
        name = parts[1] if parts[0] == "*" else parts[0]     # * marks a locked file
        depth = line.index(name) - 2                         # 1 space per level
        stack = stack[:depth] + [name]
        paths.append("/".join(stack).upper())
    if "BASIC.SYSTEM" in paths or "RETROWRITE/BASIC.SYSTEM" in paths:
        print("BASIC.SYSTEM is available for the Apple menu.")
    else:
        found = next((p for p in paths if p.endswith("/BASIC.SYSTEM")), None)
        rc, data, _ = ac(jar, "-g", img, found) if found else (1, b"", b"")
        if rc == 0 and data:
            ac(jar, "-p", img, "RETROWRITE/BASIC.SYSTEM", "SYS", "0x2000", stdin=data)
            print(f"Copied /{vol}/{found} to /{vol}/RETROWRITE/BASIC.SYSTEM")
        else:
            print("No BASIC.SYSTEM on this disk: copy one into RETROWRITE to use Apple menu > BASIC.SYSTEM.")

    # 2. DeskTop shortcut
    rc, sel, _ = ac(jar, "-g", img, "LOCAL/SELECTOR.LIST")
    if rc != 0 or not sel:
        print("No LOCAL/SELECTOR.LIST found - creating a new one.")
        sel = bytes(FILE_LEN)
        aux = 0
    else:
        aux = aux_of(jar, img, "SELECTOR.LIST") or 0
    sel = bytearray(sel) + bytes(max(0, FILE_LEN - len(sel)))
    nprim, nsec = sel[0], sel[1]
    path = f"/{vol}/RETROWRITE/RW2.SYSTEM".upper()

    def slot_name(i):
        o = 2 + i * NAME_SLOT
        return sel[o + 1:o + 1 + sel[o]].decode("latin-1")

    def slot_path(i):
        o = PATHS_AT + i * PATH_SLOT
        return sel[o + 1:o + 1 + sel[o]].decode("latin-1")

    print("Existing shortcuts:")
    for i in range(nprim):
        print(f"  OA-{i + 1}  {slot_name(i):15} {slot_path(i)}")
    for i in range(nsec):
        print(f"  (list)  {slot_name(8 + i):15} {slot_path(8 + i)}")

    # already present?
    for i in list(range(nprim)) + [8 + j for j in range(nsec)]:
        if slot_path(i).upper() == path:
            print(f"Shortcut already present in slot {i + 1}; updating name.")
            idx = i
            break
    else:
        if a.slot:
            idx = a.slot - 1
            if idx < nprim:
                sys.exit(f"Slot {a.slot} is in use ({slot_name(idx)}).")
            if idx != nprim:
                sys.exit(f"Primary shortcuts must be contiguous; next free is {nprim + 1}.")
        elif nprim < NUM_PRIMARY:
            idx = nprim
        elif nsec < NUM_SECONDARY:
            idx = 8 + nsec
        else:
            sys.exit("SELECTOR.LIST is full.")
        if idx < 8:
            sel[0] = idx + 1
        else:
            sel[1] = idx - 8 + 1

    o = 2 + idx * NAME_SLOT
    nm = NAME.encode()
    sel[o:o + NAME_SLOT] = bytes([len(nm)]) + nm.ljust(14, b"\0") + bytes([FLAGS])
    o = PATHS_AT + idx * PATH_SLOT
    pb = path.encode()
    sel[o:o + PATH_SLOT] = bytes([len(pb)]) + pb.ljust(PATH_SLOT - 1, b"\0")

    ac(jar, "-d", img, "LOCAL/SELECTOR.LIST")
    rc, _, err = ac(jar, "-p", img, "LOCAL/SELECTOR.LIST", "$F1", f"0x{aux:04X}", stdin=bytes(sel))
    if rc != 0:
        sys.exit(f"Failed to write SELECTOR.LIST: {err.decode()}")
    where = f"Open-Apple-{idx + 1}" if idx < 8 else "the Shortcuts > Run a Shortcut list"
    print(f"Added shortcut '{NAME}' -> {path} as {where}.")

    has_docs = exists(jar, img, "DOCUMENTS") or "DOCUMENTS" in list_detail(jar, img)
    if not has_docs:
        print(f"Note: /{vol}/DOCUMENTS not found; RetroWrite II will start in /{vol}.")

    # 3. the sample letter from the manual's Quick Tour (never overwrites yours)
    src = os.path.join(a.files, "LETTER")
    if os.path.exists(src):
        dest = "DOCUMENTS/LETTER" if has_docs else "RETROWRITE/LETTER"
        if exists(jar, img, dest):
            print(f"/{vol}/{dest} already exists; sample letter not copied.")
        else:
            rc, _, err = ac(jar, "-p", img, dest, "GWP", "0x5257", stdin=open(src, "rb").read())
            if rc == 0:
                setaux.setaux(img, dest, 0x5257)
                print(f"Installed the sample letter as /{vol}/{dest}")


if __name__ == "__main__":
    main()
