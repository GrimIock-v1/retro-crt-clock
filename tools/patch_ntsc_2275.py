Import("env")

from pathlib import Path

libdeps = Path(env.subst("$PROJECT_LIBDEPS_DIR"))
env_name = env.subst("$PIOENV")
video_out = libdeps / env_name / "ESP32CompositeColorVideo" / "src" / "video_out.h"

if not video_out.exists():
    raise RuntimeError("ESP32CompositeColorVideo video_out.h not found at %s" % video_out)

text = video_out.read_text()

old_define = "#define NTSC_COLOR_CLOCKS_PER_SCANLINE 228       // really 227.5 for NTSC but want to avoid half phase fiddling for now"
new_define = "#define NTSC_SAMPLES_PER_SCANLINE ((_samples_per_cc * 455) / 2) // 227.5 color clocks"

old_width = "_line_width = NTSC_COLOR_CLOCKS_PER_SCANLINE*_samples_per_cc;"
new_width = "_line_width = NTSC_SAMPLES_PER_SCANLINE;"

if new_define in text and new_width in text:
    print("NTSC 227.5 patch already applied:", video_out)
else:
    if old_define not in text:
        raise RuntimeError("Expected NTSC scanline define not found; pinned library changed")
    if old_width not in text:
        raise RuntimeError("Expected NTSC line-width assignment not found; pinned library changed")

    text = text.replace(old_define, new_define)
    text = text.replace(old_width, new_width)
    video_out.write_text(text)
    print("Applied experimental NTSC 227.5-color-clock scanline patch:", video_out)
