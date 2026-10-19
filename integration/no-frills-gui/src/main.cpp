#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>

namespace {
struct Palette {
  unsigned long bg{}, panel{}, panel2{}, border{}, text{}, muted{}, teal{}, red{}, yellow{}, clip{};
};

unsigned long color(Display* d, Colormap cmap, const char* name) {
  XColor exact{}, screen{};
  if (XAllocNamedColor(d, cmap, name, &screen, &exact)) return screen.pixel;
  return BlackPixel(d, DefaultScreen(d));
}

struct App {
  Display* display{};
  Window window{};
  GC gc{};
  int width{1280};
  int height{800};
  bool playing{false};
  bool recording{false};
  bool loop{false};
  int activeTab{0};
  int selectedTrack{0};
  std::array<bool, 4> mute{{false, false, false, false}};
  std::array<bool, 4> solo{{false, false, false, false}};
  std::array<bool, 4> arm{{false, false, false, false}};
  Palette p{};
};

void fill(App& a, int x, int y, int w, int h, unsigned long c) {
  XSetForeground(a.display, a.gc, c);
  XFillRectangle(a.display, a.window, a.gc, x, y,
                 static_cast<unsigned int>(std::max(0, w)),
                 static_cast<unsigned int>(std::max(0, h)));
}
void outline(App& a, int x, int y, int w, int h, unsigned long c) {
  XSetForeground(a.display, a.gc, c);
  XDrawRectangle(a.display, a.window, a.gc, x, y,
                 static_cast<unsigned int>(std::max(0, w)),
                 static_cast<unsigned int>(std::max(0, h)));
}
void line(App& a, int x1, int y1, int x2, int y2, unsigned long c) {
  XSetForeground(a.display, a.gc, c);
  XDrawLine(a.display, a.window, a.gc, x1, y1, x2, y2);
}
void text(App& a, int x, int y, const std::string& s, unsigned long c, bool bold = false) {
  XSetForeground(a.display, a.gc, c);
  const std::string rendered = bold ? s : s;
  XDrawString(a.display, a.window, a.gc, x, y, rendered.c_str(),
              static_cast<int>(rendered.size()));
}
void button(App& a, int x, int y, int w, int h, const char* label,
            bool active, unsigned long activeColor) {
  fill(a, x, y, w, h, active ? activeColor : a.p.panel2);
  outline(a, x, y, w, h, a.p.border);
  text(a, x + 10, y + h / 2 + 5, label, active ? a.p.bg : a.p.text, true);
}
void draw(App& a) {
  const int w = a.width, h = a.height;
  fill(a, 0, 0, w, h, a.p.bg);

  // Branding and transport bar.
  fill(a, 0, 0, w, 48, a.p.panel);
  text(a, 18, 29, "HUSH", a.p.teal, true);
  text(a, 82, 29, "COSMIC STUDIO  /  LOCAL SESSION", a.p.muted);
  button(a, w - 360, 9, 58, 30, "PLAY", a.playing, a.p.teal);
  button(a, w - 296, 9, 58, 30, "STOP", false, a.p.teal);
  button(a, w - 232, 9, 58, 30, "REC", a.recording, a.p.red);
  button(a, w - 168, 9, 58, 30, "LOOP", a.loop, a.p.yellow);
  text(a, w - 95, 29, "120 BPM", a.p.text, true);

  const int top = 58, bottomH = 176, leftW = 184, rightW = 226;
  const int contentH = h - top - bottomH - 12;
  const int contentW = w - leftW - rightW - 24;
  const int browserX = 8, centerX = browserX + leftW + 8;
  const int rightX = centerX + contentW + 8;
  const int timelineY = top + 42;
  const int trackH = std::max(58, (contentH - 42) / 4);

  // Browser.
  fill(a, browserX, top, leftW, contentH, a.p.panel);
  outline(a, browserX, top, leftW, contentH, a.p.border);
  text(a, browserX + 12, top + 22, "BROWSER", a.p.teal, true);
  line(a, browserX + 8, top + 31, browserX + leftW - 8, top + 31, a.p.border);
  const std::array<const char*, 5> folders{{"Projects", "Instruments", "Effects", "Samples", "Recent Files"}};
  for (std::size_t i = 0; i < folders.size(); ++i) {
    const int y = top + 58 + static_cast<int>(i) * 34;
    fill(a, browserX + 8, y - 16, leftW - 16, 26, i == 0 ? a.p.panel2 : a.p.panel);
    text(a, browserX + 20, y, folders[i], i == 0 ? a.p.teal : a.p.text);
  }
  text(a, browserX + 12, top + contentH - 18, "LOCAL ONLY", a.p.muted);

  // Arrangement header and ruler.
  fill(a, centerX, top, contentW, contentH, a.p.panel);
  outline(a, centerX, top, contentW, contentH, a.p.border);
  text(a, centerX + 12, top + 22, "ARRANGEMENT", a.p.teal, true);
  text(a, centerX + contentW - 178, top + 22, "SNAP  1 BAR   |   +/- ZOOM", a.p.muted);
  fill(a, centerX, top + 32, contentW, 26, a.p.panel2);
  for (int bar = 0; bar < 17; ++bar) {
    const int x = centerX + 168 + bar * std::max(32, (contentW - 176) / 16);
    if (x >= centerX + contentW - 4) break;
    line(a, x, top + 32, x, top + contentH, bar % 4 == 0 ? a.p.muted : a.p.border);
    if (bar % 4 == 0) text(a, x + 4, top + 49, std::to_string(bar + 1), a.p.muted);
  }

  const std::array<const char*, 4> names{{"DRUMS", "BASS", "SYNTH", "VOCALS"}};
  const std::array<unsigned long, 4> trackColors{{0, 0, 0, 0}};
  (void)trackColors;
  for (int i = 0; i < 4; ++i) {
    const int y = timelineY + i * trackH;
    fill(a, centerX, y, contentW, trackH, i == a.selectedTrack ? a.p.panel2 : a.p.panel);
    line(a, centerX, y + trackH - 1, centerX + contentW, y + trackH - 1, a.p.border);
    fill(a, centerX, y, 4, trackH, a.p.teal);
    text(a, centerX + 12, y + 18, names[static_cast<std::size_t>(i)], a.p.text, true);
    text(a, centerX + 12, y + 35, "GAIN  0.0 dB", a.p.muted);
    button(a, centerX + 12, y + 42, 24, 18, "M", a.mute[static_cast<std::size_t>(i)], a.p.red);
    button(a, centerX + 40, y + 42, 24, 18, "S", a.solo[static_cast<std::size_t>(i)], a.p.yellow);
    button(a, centerX + 68, y + 42, 24, 18, "R", a.arm[static_cast<std::size_t>(i)], a.p.red);

    const int clipX = centerX + 184 + (i == 2 ? 132 : i == 3 ? 246 : 0);
    const int clipW = std::max(72, std::min(230, contentW - (clipX - centerX) - 14));
    const unsigned long cc = i == 1 ? a.p.yellow : i == 3 ? a.p.red : a.p.teal;
    fill(a, clipX, y + 8, clipW, trackH - 16, a.p.clip);
    outline(a, clipX, y + 8, clipW, trackH - 16, cc);
    text(a, clipX + 8, y + 25, i == 3 ? "Vocal take" : "Audio clip", a.p.text);
    for (int k = 0; k < 28; ++k) {
      const int wx = clipX + 8 + k * std::max(2, (clipW - 16) / 28);
      const int amp = 3 + ((k * 17 + i * 11) % std::max(4, trackH / 3));
      line(a, wx, y + trackH / 2 - amp / 2, wx, y + trackH / 2 + amp / 2, cc);
    }
  }

  // Right-side rack and master output.
  fill(a, rightX, top, rightW, contentH, a.p.panel);
  outline(a, rightX, top, rightW, contentH, a.p.border);
  text(a, rightX + 12, top + 22, "PROCESSING RACK", a.p.teal, true);
  const std::array<const char*, 4> fx{{"Spectral Denoise", "Hiss Reduction", "Click Removal", "Hum Eliminator"}};
  for (int i = 0; i < 4; ++i) {
    const int y = top + 44 + i * 42;
    fill(a, rightX + 8, y, rightW - 16, 34, a.p.panel2);
    outline(a, rightX + 8, y, rightW - 16, 34, a.p.border);
    fill(a, rightX + 16, y + 10, 10, 10, a.p.teal);
    text(a, rightX + 34, y + 22, fx[static_cast<std::size_t>(i)], a.p.text);
  }
  const int masterY = top + contentH - 92;
  line(a, rightX + 8, masterY - 8, rightX + rightW - 8, masterY - 8, a.p.border);
  text(a, rightX + 12, masterY + 12, "MASTER OUT", a.p.teal, true);
  fill(a, rightX + 14, masterY + 24, rightW - 28, 12, a.p.panel2);
  fill(a, rightX + 14, masterY + 24, (rightW - 28) * 3 / 4, 12, a.p.teal);
  text(a, rightX + 14, masterY + 54, "L  -12.0 dB       R  -12.0 dB", a.p.muted);

  // Bottom tabs and mixer strip.
  const int bottomY = h - bottomH - 4;
  fill(a, 8, bottomY, w - 16, bottomH, a.p.panel);
  outline(a, 8, bottomY, w - 16, bottomH, a.p.border);
  const std::array<const char*, 5> tabs{{"MIXER", "PLUGINS", "MIDI", "FORENSICS", "DAW CHECKER"}};
  int tabX = 18;
  for (int i = 0; i < static_cast<int>(tabs.size()); ++i) {
    const int tw = i == 4 ? 124 : i == 3 ? 100 : 82;
    button(a, tabX, bottomY + 8, tw, 28, tabs[static_cast<std::size_t>(i)],
           a.activeTab == i, a.p.teal);
    tabX += tw + 6;
  }
  text(a, 22, bottomY + 58, "TRACK CHANNELS", a.p.teal, true);
  for (int i = 0; i < 4; ++i) {
    const int x = 22 + i * 142;
    text(a, x, bottomY + 82, names[static_cast<std::size_t>(i)], a.p.text, true);
    fill(a, x, bottomY + 92, 110, 8, a.p.panel2);
    fill(a, x, bottomY + 92, 72 - i * 7, 8, a.p.teal);
    text(a, x, bottomY + 120, "PAN  C     VOL  0 dB", a.p.muted);
  }
  text(a, w - 254, bottomY + 68, "SESSION  •  LOCAL", a.p.muted);
  text(a, w - 254, bottomY + 90, "No network / telemetry", a.p.muted);
  text(a, w - 254, bottomY + 112, "Engine adapter pending", a.p.yellow);
  XFlush(a.display);
}

bool inside(int x, int y, int rx, int ry, int rw, int rh) {
  return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}
} // namespace

int main() {
  App a;
  a.display = XOpenDisplay(nullptr);
  if (!a.display) {
    std::fprintf(stderr, "HUSH GUI: cannot open X display. Run inside an X11 desktop session.\n");
    return 1;
  }
  const int screen = DefaultScreen(a.display);
  const Colormap cmap = DefaultColormap(a.display, screen);
  a.p = {
    color(a.display, cmap, "#11141a"), color(a.display, cmap, "#191e27"),
    color(a.display, cmap, "#222a35"), color(a.display, cmap, "#34404e"),
    color(a.display, cmap, "#e7edf5"), color(a.display, cmap, "#9aa8b8"),
    color(a.display, cmap, "#5eead4"), color(a.display, cmap, "#fb7185"),
    color(a.display, cmap, "#facc15"), color(a.display, cmap, "#182c32")
  };

  a.window = XCreateSimpleWindow(a.display, RootWindow(a.display, screen),
                                 40, 40, static_cast<unsigned int>(a.width),
                                 static_cast<unsigned int>(a.height), 1,
                                 a.p.border, a.p.bg);
  XStoreName(a.display, a.window, "HUSH Cosmic Studio — No-Frills GUI");
  XSelectInput(a.display, a.window, ExposureMask | StructureNotifyMask | ButtonPressMask | KeyPressMask);
  XSizeHints hints{};
  hints.flags = PMinSize;
  hints.min_width = 900;
  hints.min_height = 620;
  XSetWMNormalHints(a.display, a.window, &hints);
  a.gc = XCreateGC(a.display, a.window, 0, nullptr);
  XMapWindow(a.display, a.window);

  bool running = true;
  while (running) {
    XEvent e{};
    XNextEvent(a.display, &e);
    if (e.type == Expose) {
      if (e.xexpose.count == 0) draw(a);
    } else if (e.type == ConfigureNotify) {
      a.width = std::max(900, e.xconfigure.width);
      a.height = std::max(620, e.xconfigure.height);
      draw(a);
    } else if (e.type == KeyPress) {
      const KeySym key = XLookupKeysym(&e.xkey, 0);
      if (key == XK_Escape || key == XK_q) running = false;
      if (key == XK_space) a.playing = !a.playing;
      if (key == XK_r) a.recording = !a.recording;
      draw(a);
    } else if (e.type == ButtonPress) {
      const int x = e.xbutton.x, y = e.xbutton.y;
      const int w = a.width, h = a.height;
      if (inside(x, y, w - 360, 9, 58, 30)) a.playing = !a.playing;
      else if (inside(x, y, w - 296, 9, 58, 30)) { a.playing = false; a.recording = false; }
      else if (inside(x, y, w - 232, 9, 58, 30)) a.recording = !a.recording;
      else if (inside(x, y, w - 168, 9, 58, 30)) a.loop = !a.loop;
      else {
        const int top = 58, bottomH = 176, leftW = 184, rightW = 226;
        const int contentH = h - top - bottomH - 12;
        const int contentW = w - leftW - rightW - 24;
        const int centerX = 8 + leftW + 8;
        const int timelineY = top + 42;
        const int trackH = std::max(58, (contentH - 42) / 4);
        for (int i = 0; i < 4; ++i) {
          const int yy = timelineY + i * trackH;
          if (inside(x, y, centerX, yy, contentW, trackH)) a.selectedTrack = i;
          if (inside(x, y, centerX + 12, yy + 42, 24, 18)) a.mute[static_cast<std::size_t>(i)] = !a.mute[static_cast<std::size_t>(i)];
          if (inside(x, y, centerX + 40, yy + 42, 24, 18)) a.solo[static_cast<std::size_t>(i)] = !a.solo[static_cast<std::size_t>(i)];
          if (inside(x, y, centerX + 68, yy + 42, 24, 18)) a.arm[static_cast<std::size_t>(i)] = !a.arm[static_cast<std::size_t>(i)];
        }
        const int bottomY = h - bottomH - 4;
        const std::array<int, 5> tabWidths{{82, 82, 82, 100, 124}};
        int tabX = 18;
        for (int i = 0; i < 5; ++i) {
          if (inside(x, y, tabX, bottomY + 8, tabWidths[static_cast<std::size_t>(i)], 28)) a.activeTab = i;
          tabX += tabWidths[static_cast<std::size_t>(i)] + 6;
        }
      }
      draw(a);
    }
  }

  XFreeGC(a.display, a.gc);
  XDestroyWindow(a.display, a.window);
  XCloseDisplay(a.display);
  return 0;
}
