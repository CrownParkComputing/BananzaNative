"""Changes to the lifted C for Donkey Kong Bananza (v458752), applied by lift.py after it writes the code.

- Window size: the game's window (the nvnWindow textures its upscaler's output goes to, and the render targets scale with it)
  is the constant pair two leaf functions of main return: main+0x5bf770 (2880) and main+0x5bf780 (1620), `mov w0,#imm; ret`.
  Lifted, they are `c->x[0] = 0xb40ULL;` / `0x654ULL`. They read the host's hos_dkb_window_w / _h instead (host/hos_main.c:
  HOS_WINDOW_SIZE, 1920x1080 by default), as BananaNative's loader writes DM_WINDOW_SIZE into the same two functions.
  Another game, or another build of this one, has no such functions: nothing is changed, and lift.py says so.
"""
import glob
import os
import re
import sys

SITES = (("f_805df770", "0xb40ULL", "hos_dkb_window_w"), ("f_805df780", "0x654ULL", "hos_dkb_window_h"))


def apply(gen):
    done = 0
    for path in sorted(glob.glob(os.path.join(gen, "code_*.c"))):
        with open(path, encoding="utf-8", newline="") as f:
            s = f.read()
        out = s
        for fn, const, var in SITES:
            pat = re.compile(r"(void %s\(Ctx \*__restrict A64_NOESC c\) \{\n    c->x\[0\] = )%s;" % (fn, re.escape(const)))
            out, n = pat.subn(r"\g<1>(uint64_t)%s;" % var, out)
            done += n
        if out != s:
            out = "extern uint32_t hos_dkb_window_w, hos_dkb_window_h;   /* (nx2/dkb_patches.py) */\n" + out
            with open(path, "w", encoding="utf-8", newline="") as f:
                f.write(out)
    print("dkb_patches: window size functions patched: %d of 2%s" % (done, "" if done == 2 else " (not this game, or another build)"))


if __name__ == "__main__":
    apply(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "work", "sa", "gen"))
