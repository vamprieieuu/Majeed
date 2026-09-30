/*
    MAJEED - FINAL (v5)  |  DITHER + GRAIN + VHS NOISE
    Target : Adobe After Effects 23.2.1+ (ScriptUI dockable panel)

    INSTALL
      Copy to  ...\Support Files\Scripts\ScriptUI Panels\   (Windows)
               /Applications/Adobe After Effects <ver>/Scripts/ScriptUI Panels/   (macOS)
      Restart AE, then  Window > MAJEED_FINAL_v4.jsx
      (Preferences > Scripting & Expressions > "Allow Scripts to Write Files and Access Network" ON)

    WHAT CHANGED vs v4
      - NEW dedicated "VHS Noise" page (own controls, own effects, all animated):
          Static (fine RGB noise), Tape Dashes (horizontal streak noise), Vertical Streaks,
          Color Bleed (horizontal chroma smear), Tracking (horizontal tearing), Jitter,
          Scanlines, Flicker.  Built from the three VHS reference textures.
      - Algorithms that were exact duplicates of each other now have their own threshold
        shape (Dispersed, Circuit, Clock, Rekt Block, Mosaic Halftone, Z-Modulation).
      - Fixed Rotate helper keys in the halftone map.

    WHAT CHANGED vs v3
      - Effects are applied DIRECTLY to the selected layer(s) as native AE effects.
        No adjustment solids, no dust solid, no per-layer precomps.
        (Pattern/ordered algorithms need ONE shared, hidden helper layer "MAJEED Map"
         per comp - it is the threshold matrix. Removed automatically by REMOVE.)
      - Live edits update effect VALUES IN PLACE. The effect stack is only rebuilt when the
        structure changes (e.g. blur goes 0 -> >0, algorithm switches error<->pattern).
        The panel is never closed / rebuilt; no alerts are shown while editing live.
      - Dither defaults to PRESERVE ORIGINAL COLORS. Black & White is an explicit mode.
      - Grain is real film-style grain (Add Grain) with animated seed, not halftone.
      - REMOVED: Texture, Dust, Print, Film, Reserved.  Halftone is subtle by default.

    HONEST LIMITS (please read)
      - Error-diffusion algorithms (Floyd-Steinberg, Atkinson, Sierra ...) cannot be run
        pixel-by-pixel inside native AE. They render as a tuned noise-threshold dither
        (marked "~"). Pattern/ordered algorithms use a true threshold matrix.
      - The plugin (DitherTone, Photoshop) contains NO VHS code. VHS Noise was therefore
        designed from the three reference textures, using native AE effects only.
      - VHS: no per-channel RGB offset is possible with native effects, so none is offered
        (Color Bleed smears R/B horizontally instead). Vertical Streaks are luminance streaks.
      - UNTESTED in AE (written without AE available). Two menu indices may need a one-number
        tweak if your build differs: FRACTAL_BLEND (Fractal Noise blending mode) and
        VBLIND_DIR (scanline direction) - both are constants near the top of the script.
      - If "Add Grain" is not available in your AE build, Grain falls back to plain animated
        Add Noise (size / softness / shadows / highlights controls are then inactive).
*/

(function (thisObj) {

    var TAG = "MAJEED";
    var MARK = "MAJEED|";
    var LABEL_W = 84;
    var UI = {};
    var FNT = null;
    var BUSY = false;
    var PAGES = [];
    var PAGE_NAMES = ["Dither", "Color", "Preprocess", "Grain", "VHS Noise"];

    // Compound Arithmetic "Operator" popup index that means ADD.
    // If the dither pattern ever looks wrong on your build, change this one number.
    var COMPOUND_OP_ADD = 2;
    // Fractal Noise "Blending Mode" popup index used for VHS streaks/dashes (Overlay). Tune if needed.
    var FRACTAL_BLEND = 6;
    // Venetian Blinds "Direction" that gives HORIZONTAL scanlines. If lines look vertical, use 0.
    var VBLIND_DIR = 90;

    // ------------------------------------------------------------ algorithms
    // name, kind, matrix size n, rotation, strength (error-diffusion only), description
    var ALGOS = [];
    function A(name, kind, n, rot, k, desc) {
        ALGOS.push({ name: name, kind: kind, n: n, rot: rot, k: k, desc: desc, exact: (desc.charAt(0) !== "~") });
    }
    A("Floyd-Steinberg", "err", 0, 0, 1.00, "~ Error diffusion 7-3-5-1/16: organic grain. Stand-in: noise-threshold.");
    A("Jarvis-Judice-Ninke", "err", 0, 0, 0.90, "~ 3-row kernel (/48): smoother, fewer worm patterns.");
    A("Stucki", "err", 0, 0, 0.93, "~ 3-row kernel (/42): like JJN but crisper.");
    A("Burkes", "err", 0, 0, 0.95, "~ 2-row kernel (/32): clean and fast.");
    A("Atkinson", "err", 0, 0, 1.20, "~ Diffuses only 6/8 of the error: punchy contrast.");
    A("Row Modulation", "lineH", 4, 0, 0, "~ Horizontal line screen: tone = line thickness.");
    A("Column Modulation", "lineV", 4, 0, 0, "~ Vertical line screen.");
    A("Dispersed Modulation", "disp", 4, 0, 0, "~ Dispersed, scattered dots (not a Bayer grid).");
    A("Medium Modulation", "lineH", 6, 0, 0, "~ Medium-pitch line screen.");
    A("Heavy Modulation", "lineH", 8, 0, 0, "~ Coarse line screen.");
    A("Circuit Modulation", "circuit", 8, 0, 0, "~ Square rings with checker breaks: circuit-board look.");
    A("Tilt Modulation", "diag", 6, 0, 0, "~ Diagonal line screen.");
    A("Grid Modulation", "grid", 6, 0, 0, "~ Grid screen.");
    A("Sierra", "err", 0, 0, 0.97, "~ 3-row kernel (/32).");
    A("Sierra Two Row", "err", 0, 0, 0.92, "~ 2-row Sierra (/16).");
    A("Sierra Lite", "err", 0, 0, 1.08, "~ 2-1-1 (/4): fastest, slightly directional.");
    A("Skip Neighbours", "err", 0, 0, 1.05, "~ Diffusion skipping the adjacent pixel: coarser grain.");
    A("Skip1 Neighbours", "err", 0, 0, 1.10, "~ Skips 1 pixel: coarser grain.");
    A("Skip2 Neighbours", "err", 0, 0, 1.15, "~ Skips 2 pixels: coarsest grain.");
    A("Matrix", "dot", 4, 45, 0, "~ Small dot matrix, rotated 45 deg.");
    A("Knit", "diag2", 4, 0, 0, "~ Knitted diagonal weave.");
    A("Cross Square", "cross", 6, 0, 0, "~ Cross-shaped clusters.");
    A("Serpentine", "err", 0, 0, 0.85, "~ Floyd-Steinberg scanning back and forth.");
    A("Rekt Block", "rect", 8, 0, 0, "~ Wide rectangular block clusters.");
    A("Variable Hatch", "diag", 8, 0, 0, "~ Diagonal hatching.");
    A("Bayer 2x2", "bayer", 2, 0, 0, "Exact ordered dither, 2x2 matrix: very coarse.");
    A("Bayer 4x4", "bayer", 4, 0, 0, "Exact ordered dither, 4x4: the classic crosshatch look.");
    A("Bayer 8x8", "bayer", 8, 0, 0, "Exact ordered dither, 8x8: fine, smooth gradients.");
    A("Halftone", "dot", 8, 0, 0, "Round-dot halftone screen at 0 deg. Subtle by default (8 levels, colors kept).");
    A("Halftone 22.5\u00b0", "dot", 8, 22.5, 0, "Round-dot halftone screen rotated 22.5 deg.");
    A("Halftone 45\u00b0", "dot", 8, 45, 0, "Round-dot halftone at 45 deg (newspaper angle).");
    A("Star", "star", 8, 0, 0, "~ Diamond / star clusters.");
    A("Grid", "grid", 8, 0, 0, "~ Grid screen.");
    A("Vertical Stitch", "stitchV", 6, 0, 0, "~ Vertical dashed lines.");
    A("Horizontal Stitch", "stitchH", 6, 0, 0, "~ Horizontal dashed lines.");
    A("Clock", "clock", 8, 0, 0, "~ Sweeping radial (clock-hand) clusters.");
    A("Cyber", "grid", 4, 0, 0, "~ Tight digital grid.");
    A("Bi-thread", "bithread", 6, 0, 0, "~ Two crossing diagonal threads.");
    A("Bytewav", "wave", 8, 0, 0, "~ Wavy line screen.");
    A("Bitslash", "diag", 4, 0, 0, "~ Fine diagonal slashes.");
    A("Mosaic Halftone", "sq", 6, 0, 0, "~ Small square-dot halftone (finer than Square Halftone).");
    A("Z-Modulation", "zig", 8, 0, 0, "~ Zig-zag line screen.");
    A("Square Halftone", "sq", 8, 0, 0, "Square-dot halftone screen.");
    A("H Beam Modulation", "beamH", 8, 0, 0, "~ Wide horizontal beams.");
    A("V Beam Modulation", "beamV", 8, 0, 0, "~ Wide vertical beams.");
    A("Xerox Grain", "err", 0, 0, 1.30, "~ Heavy random grain, photocopy style.");

    var ALGO_NAMES = [];
    (function () { for (var i = 0; i < ALGOS.length; i++) ALGO_NAMES.push(ALGOS[i].name); })();

    function algoIndex(name) {
        for (var i = 0; i < ALGOS.length; i++) if (ALGOS[i].name === name) return i;
        return 0;
    }

    var DITHER_MODES = ["Preserve Original Colors", "Black & White"];

    var BASE = {
        algorithm: "Bayer 4x4",
        ditherMode: 0,
        dither: 100, tones: 8, ditherSize: 2, ditherContrast: 100, spread: 100, pixelate: 0,
        hue: 0, saturation: 0, invert: 0, gradeBias: 0, limitPalette: 0, indexColors: 16, tonalOn: 0,
        denoise: 0, blur: 0, sharpStrength: 0, sharpRadius: 1,
        grainOn: 0, grainAmount: 35, grainSize: 30, grainSoftness: 40, grainColor: 25,
        grainShadows: 100, grainHighlights: 60, grainSpeed: 100, grainSeed: 0,
        vhsOn: 0, vhsStatic: 35, vhsDashes: 30, vhsStreaks: 25, vhsBleed: 30, vhsTracking: 20,
        vhsJitter: 15, vhsScanlines: 30, vhsScanSize: 3, vhsFlicker: 20, vhsSpeed: 100, vhsSeed: 0
    };

    var PRESETS = {
        "Default (colors kept)": {},
        "Soft": { algorithm: "Floyd-Steinberg", tones: 12, ditherSize: 1, dither: 70 },
        "Subtle Halftone": { algorithm: "Halftone 45\u00b0", tones: 10, ditherSize: 3, dither: 55 },
        "Newsprint B&W": { algorithm: "Halftone 45\u00b0", ditherMode: 1, tones: 2, ditherSize: 3, dither: 100 },
        "Retro 4-tone": { algorithm: "Bayer 4x4", tones: 4, ditherSize: 2 },
        "Film Grain": { dither: 0, grainOn: 1, grainAmount: 40, grainSize: 30, grainColor: 25 },
        "VHS Tape": { dither: 0, vhsOn: 1 },
        "Dither + Grain": { algorithm: "Bayer 8x8", tones: 8, dither: 60, grainOn: 1, grainAmount: 30 }
    };

    // ------------------------------------------------------------ helpers
    function activeComp() {
        var c = app.project && app.project.activeItem;
        return (c && c instanceof CompItem) ? c : null;
    }
    function num(v, fb) { v = parseFloat(v); return isNaN(v) ? fb : v; }
    function clamp(v, a, b) { return Math.max(a, Math.min(b, v)); }
    function roundTo(v, dec) { var m = Math.pow(10, dec); return Math.round(v * m) / m; }
    function isHelper(L) { return L && L.name && L.name.indexOf(TAG + " ") === 0; }
    function isTarget(L) { try { return L.comment && String(L.comment).indexOf(MARK) === 0; } catch (_) { return false; } }
    function fx(layer) { return layer.property("ADBE Effect Parade"); }
    function setStatus(t) { try { UI.status.text = t; } catch (_) {} }

    function addFx(layer, matchNames, displayName) {
        var p = fx(layer);
        if (!p) return null;
        if (!(matchNames instanceof Array)) matchNames = [matchNames];
        for (var i = 0; i < matchNames.length; i++) {
            try {
                var e = p.addProperty(matchNames[i]);
                if (e) { if (displayName) { try { e.name = displayName; } catch (_) {} } return e; }
            } catch (_) {}
        }
        return null;
    }

    function getFx(layer, name) {
        try { return fx(layer).property(name); } catch (_) { return null; }
    }

    // property lookup by match name, display name, or (deep) inside groups
    function findProp(e, key) {
        var p = null;
        try { p = e.property(key); } catch (_) {}
        if (p) return p;
        var k = String(key).toLowerCase();
        try {
            for (var i = 1; i <= e.numProperties; i++) {
                var q = e.property(i);
                if (q && q.name && q.name.toLowerCase() === k) return q;
            }
            for (var j = 1; j <= e.numProperties; j++) {
                var g = e.property(j);
                if (g && g.numProperties > 0) {
                    var r = findProp(g, key);
                    if (r) return r;
                }
            }
        } catch (_) {}
        return null;
    }

    // property `name` inside group `group` (deep)
    function findIn(e, group, name) {
        var g = findProp(e, group);
        return g ? findProp(g, name) : null;
    }

    function setP(p, value) {
        if (!p) return false;
        try {
            var v = value;
            if (typeof v === "number") {
                try { if (p.hasMin && v < p.minValue) v = p.minValue; } catch (_) {}
                try { if (p.hasMax && v > p.maxValue) v = p.maxValue; } catch (_) {}
            }
            p.setValue(v);
            return true;
        } catch (_) { return false; }
    }

    function set(e, keys, value) {
        if (!e) return false;
        if (!(keys instanceof Array)) keys = [keys];
        for (var i = 0; i < keys.length; i++) {
            var p = findProp(e, keys[i]);
            if (p && setP(p, value)) return true;
        }
        return false;
    }

    // ------------------------------------------------------------ threshold matrices
    function bayer(n) {
        var m = [[0]], size = 1, x, y;
        var add = [0, 2, 3, 1];
        while (size < n) {
            var s2 = size * 2, nm = [];
            for (y = 0; y < s2; y++) {
                nm[y] = [];
                for (x = 0; x < s2; x++) {
                    var q = (x < size ? 0 : 1) + (y < size ? 0 : 2);
                    nm[y][x] = m[y % size][x % size] * 4 + add[q];
                }
            }
            m = nm; size = s2;
        }
        var flat = [];
        for (y = 0; y < n; y++) for (x = 0; x < n; x++) flat.push(m[y][x]);
        return flat;
    }

    function keyOf(kind, n, x, y) {
        var c = (n - 1) / 2, dx = x - c, dy = y - c, d2 = dx * dx + dy * dy;
        var h = Math.max(2, Math.floor(n / 2));
        switch (kind) {
            case "dot": return d2;
            case "disp": return (x * 5 + y * 3 + x * y) % (n * n);
            case "circuit": return Math.max(Math.abs(dx), Math.abs(dy)) * 10 + ((x + y) % 2);
            case "rect": return Math.max(Math.abs(dx) * 2, Math.abs(dy)) * 100 + d2;
            case "clock": return Math.atan2(dy, dx) + 0.001 * d2;
            case "beamH": return Math.abs(dy) + 0.001 * Math.abs(dx);
            case "beamV": return Math.abs(dx) + 0.001 * Math.abs(dy);
            case "zig": return (x + Math.abs((y % (2 * h)) - h)) % n;
            case "sq": return Math.max(Math.abs(dx), Math.abs(dy)) * 100 + d2;
            case "star": return Math.abs(dx) + Math.abs(dy) + 0.001 * d2;
            case "cross": return Math.min(Math.abs(dx), Math.abs(dy)) + 0.001 * d2;
            case "lineH": return y;
            case "lineV": return x;
            case "diag": return (x + y) % n;
            case "diag2": return (x - y + n) % n;
            case "bithread": return Math.min((x + y) % n, (x - y + n) % n);
            case "grid": return Math.min(x % h, y % h) + 0.001 * ((x % h) + (y % h));
            case "wave": return ((y + Math.round(Math.sin(x * 2 * Math.PI / n) * n / 4)) % n + n) % n;
            case "stitchV": return (x + (y % 2) * Math.floor(n / 2)) % n;
            case "stitchH": return (y + (x % 2) * Math.floor(n / 2)) % n;
        }
        return d2;
    }

    // n*n thresholds in 0..1 (rank based, ties share the average rank)
    function thresholds(kind, n) {
        var keys = [], x, y, i, j;
        if (kind === "bayer") keys = bayer(n);
        else for (y = 0; y < n; y++) for (x = 0; x < n; x++) keys.push(keyOf(kind, n, x, y));
        var out = [], total = keys.length;
        for (i = 0; i < total; i++) {
            var lt = 0, eq = 0;
            for (j = 0; j < total; j++) {
                if (keys[j] < keys[i] - 1e-9) lt++;
                else if (Math.abs(keys[j] - keys[i]) <= 1e-9) eq++;
            }
            out.push((lt + eq / 2) / total);
        }
        return out;
    }

    function parseHex(t) {
        var m = /#?([0-9a-fA-F]{6})/.exec(String(t));
        if (!m) return null;
        return [parseInt(m[1].substr(0, 2), 16) / 255, parseInt(m[1].substr(2, 2), 16) / 255, parseInt(m[1].substr(4, 2), 16) / 255, 1];
    }

    // ------------------------------------------------------------ read UI
    function readUI() {
        var ai = UI.algorithm.selection ? UI.algorithm.selection.index : algoIndex(BASE.algorithm);
        var bw = (UI.ditherMode.selection ? UI.ditherMode.selection.index : 0) === 1;
        var limit = !!UI.limitPalette.value;
        var tones = clamp(Math.round(num(UI.tones.text, 8)), 2, 64);
        if (limit) {
            var idx = clamp(Math.round(num(UI.indexColors.text, 16)), 2, 256);
            tones = bw ? clamp(idx, 2, 64) : clamp(Math.round(Math.pow(idx, 1 / 3)), 2, 64);
        }
        return {
            algo: ALGOS[ai],
            bw: bw,
            levels: tones,
            dither: clamp(num(UI.dither.text, 100), 0, 100),
            cell: clamp(Math.round(num(UI.ditherSize.text, 2)), 1, 32),
            ditherContrast: clamp(num(UI.ditherContrast.text, 100), 0, 200),
            spread: clamp(num(UI.spread.text, 100), 0, 200),
            pixelate: !!UI.pixelate.value,

            hue: clamp(num(UI.hue.text, 0), -180, 180),
            saturation: clamp(num(UI.saturation.text, 0), -100, 100),
            invert: clamp(num(UI.invert.text, 0), 0, 100) >= 50,
            gradeBias: clamp(num(UI.gradeBias.text, 0), -100, 100),
            tonal: !!UI.tonalOn.value,
            tShadows: parseHex(UI.tShadows.text) || [0, 0, 0, 1],
            tMids: parseHex(UI.tMids.text) || [0.5, 0.5, 0.5, 1],
            tHighs: parseHex(UI.tHighs.text) || [1, 1, 1, 1],

            denoise: clamp(num(UI.denoise.text, 0), -100, 100),
            blur: clamp(num(UI.blur.text, 0), 0, 100),
            sharpStrength: clamp(num(UI.sharpStrength.text, 0), 0, 10),
            sharpRadius: clamp(num(UI.sharpRadius.text, 1), 0.1, 20),

            grainOn: !!UI.grainOn.value,
            grainAmount: clamp(num(UI.grainAmount.text, 35), 0, 100),
            grainSize: clamp(num(UI.grainSize.text, 30), 0, 100),
            grainSoftness: clamp(num(UI.grainSoftness.text, 40), 0, 100),
            grainColor: clamp(num(UI.grainColor.text, 25), 0, 100),
            grainShadows: clamp(num(UI.grainShadows.text, 100), 0, 200),
            grainHighlights: clamp(num(UI.grainHighlights.text, 60), 0, 200),
            grainSpeed: clamp(num(UI.grainSpeed.text, 100), 0, 100),
            grainSeed: clamp(Math.round(num(UI.grainSeed.text, 0)), 0, 10000),

            vhsOn: !!UI.vhsOn.value,
            vhsStatic: clamp(num(UI.vhsStatic.text, 0), 0, 100),
            vhsDashes: clamp(num(UI.vhsDashes.text, 0), 0, 100),
            vhsStreaks: clamp(num(UI.vhsStreaks.text, 0), 0, 100),
            vhsBleed: clamp(num(UI.vhsBleed.text, 0), 0, 100),
            vhsTracking: clamp(num(UI.vhsTracking.text, 0), 0, 100),
            vhsJitter: clamp(num(UI.vhsJitter.text, 0), 0, 100),
            vhsScanlines: clamp(num(UI.vhsScanlines.text, 0), 0, 100),
            vhsScanSize: clamp(Math.round(num(UI.vhsScanSize.text, 3)), 1, 20),
            vhsFlicker: clamp(num(UI.vhsFlicker.text, 0), 0, 100),
            vhsSpeed: clamp(num(UI.vhsSpeed.text, 100), 0, 100),
            vhsSeed: clamp(Math.round(num(UI.vhsSeed.text, 0)), 0, 10000)
        };
    }

    // structure signature: only things that change WHICH effects exist / their order
    function makeSig(s, hasAddGrain) {
        var a = s.algo;
        var isErr = (a.kind === "err");
        var needHS = (s.hue !== 0 || s.saturation !== 0 || s.bw);
        return [
            "v5",
            isErr ? "E" : "P",
            s.denoise < 0 ? "M" : (s.denoise > 0 ? "N" : "-"),
            s.blur > 0 ? "B" : "-",
            s.sharpStrength > 0 ? "S" : "-",
            needHS ? "H" : "-",
            s.invert ? "I" : "-",
            (s.pixelate && s.cell > 1) ? "X" : "-",
            s.dither > 0 ? "D" : "-",
            s.tonal ? "T" : "-",
            s.grainOn ? (hasAddGrain ? "G" : "g") : "-",
            "|V",
            (s.vhsOn && s.vhsJitter > 0) ? "J" : "-",
            (s.vhsOn && s.vhsTracking > 0) ? "T" : "-",
            (s.vhsOn && s.vhsBleed > 0) ? "B" : "-",
            (s.vhsOn && s.vhsStreaks > 0) ? "V" : "-",
            (s.vhsOn && s.vhsDashes > 0) ? "D" : "-",
            (s.vhsOn && s.vhsStatic > 0) ? "N" : "-",
            (s.vhsOn && s.vhsScanlines > 0) ? "S" : "-",
            (s.vhsOn && s.vhsFlicker > 0) ? "F" : "-"
        ].join("");
    }

    // ------------------------------------------------------------ map (shared helper for pattern algorithms)
    function findItem(name) {
        for (var i = 1; i <= app.project.numItems; i++) {
            var it = app.project.item(i);
            if (it instanceof CompItem && it.name === name) return it;
        }
        return null;
    }

    // "MAJEED Cell": one tile of the threshold matrix as gray squares (single shape layer)
    function getCellComp(c, a, ds) {
        var n = a.n, size = n * ds;
        var sig = a.kind + n + "|" + ds;
        var cc = findItem(TAG + " Cell");
        if (cc && cc.comment === sig && cc.width === size) return cc;
        if (cc) {
            while (cc.numLayers > 0) cc.layer(1).remove();
            cc.width = size; cc.height = size; cc.duration = Math.max(c.duration, 1);
        } else {
            cc = app.project.items.addComp(TAG + " Cell", size, size, 1, Math.max(c.duration, 1), c.frameRate);
        }
        var th = thresholds(a.kind, n), x, y, g, grp, r, f;
        var sl = cc.layers.addShape();
        sl.name = "cell";
        var root = sl.property("ADBE Root Vectors Group");
        for (y = 0; y < n; y++) {
            for (x = 0; x < n; x++) {
                g = clamp(th[y * n + x], 0, 1);
                grp = root.addProperty("ADBE Vector Group");
                r = grp.property("ADBE Vectors Group").addProperty("ADBE Vector Shape - Rect");
                r.property("ADBE Vector Rect Size").setValue([ds, ds]);
                r.property("ADBE Vector Rect Position").setValue([(x + 0.5) * ds - size / 2, (y + 0.5) * ds - size / 2]);
                f = grp.property("ADBE Vectors Group").addProperty("ADBE Vector Graphic - Fill");
                f.property("ADBE Vector Fill Color").setValue([g, g, g, 1]);
            }
        }
        sl.property("ADBE Transform Group").property("ADBE Position").setValue([size / 2, size / 2]);
        cc.comment = sig;
        return cc;
    }

    // "MAJEED Map": comp-sized tiled matrix, Exposure-scaled to the quantizer step
    function getMapComp(c, a, ds) {
        var cell = getCellComp(c, a, ds);
        var sig = a.kind + a.n + "|" + ds + "|" + a.rot + "|" + c.width + "x" + c.height;
        var mc = findItem(TAG + " Map");
        if (mc && mc.comment === sig) return mc;
        if (mc) {
            while (mc.numLayers > 0) mc.layer(1).remove();
            mc.width = c.width; mc.height = c.height; mc.duration = Math.max(c.duration, 1);
        } else {
            mc = app.project.items.addComp(TAG + " Map", c.width, c.height, 1, Math.max(c.duration, 1), c.frameRate);
        }
        var L = mc.layers.add(cell);
        L.name = "tiles";
        L.transform.position.setValue([c.width / 2, c.height / 2]);
        var t = addFx(L, "ADBE Tile", "Tile");
        var mult = a.rot ? 1.7 : 1.05;
        set(t, ["Output Width", "ADBE Tile-0004"], c.width * mult / (a.n * ds) * 100);
        set(t, ["Output Height", "ADBE Tile-0005"], c.height * mult / (a.n * ds) * 100);
        if (a.rot) {
            var g2 = addFx(L, "ADBE Geometry2", "Rotate");
            if (g2) {
                set(g2, ["Uniform Scaling", "Uniform Scale"], 1);
                set(g2, ["Rotation"], a.rot);
            }
        }
        var ex = addFx(L, ["ADBE Exposure2", "ADBE Exposure"], "Step");
        mc.comment = sig;
        return mc;
    }

    function updateMapStep(mapComp, step) {
        try {
            var L = mapComp.layer("tiles");
            var ex = L && getFx(L, "Step");
            if (ex) set(ex, ["Exposure", "ADBE Exposure2-0003"], Math.log(clamp(step, 0.001, 1)) / Math.LN2);
        } catch (_) {}
    }

    function ensureMapLayer(c, mapComp) {
        var i, L = null;
        for (i = 1; i <= c.numLayers; i++) {
            if (c.layer(i).name === TAG + " Map") { L = c.layer(i); break; }
        }
        if (L && L.source !== mapComp) { try { L.remove(); } catch (_) {} L = null; }
        if (!L) {
            L = c.layers.add(mapComp);
            L.name = TAG + " Map";
            try { L.startTime = 0; L.inPoint = 0; L.outPoint = c.duration; } catch (_) {}
            try { L.enabled = false; } catch (_) {}
            try { L.moveToEnd(); } catch (_) {}
        }
        return L;
    }

    // ------------------------------------------------------------ per-layer effect stack
    function hasEffectAddGrain() { return GRAIN_MODE === "add"; }
    var GRAIN_MODE = null;   // "add" | "noise", probed once on first use

    function probeGrain(L) {
        if (GRAIN_MODE) return GRAIN_MODE;
        var e = addFx(L, ["ADBE Add Grain", "ADBE Grain2"], "MAJEED probe");
        if (e) { try { e.remove(); } catch (_) {} GRAIN_MODE = "add"; }
        else GRAIN_MODE = "noise";
        return GRAIN_MODE;
    }

    function clearFx(L) {
        var p = fx(L), j, e;
        if (!p) return;
        for (j = p.numProperties; j >= 1; j--) {
            e = p.property(j);
            if (e && e.name && e.name.indexOf(TAG) === 0) { try { e.remove(); } catch (_) {} }
        }
        try { L.comment = ""; } catch (_) {}
    }

    function buildStack(L, s, sig) {
        clearFx(L);
        var a = s.algo, isErr = (a.kind === "err"), e;
        var needHS = (s.hue !== 0 || s.saturation !== 0 || s.bw);

        if (s.denoise < 0) addFx(L, "ADBE Median", TAG + " Denoise");
        if (s.blur > 0) addFx(L, "ADBE Gaussian Blur 2", TAG + " Blur");
        if (s.denoise > 0) addFx(L, "ADBE Noise2", TAG + " Noise");
        if (s.sharpStrength > 0) addFx(L, "ADBE Unsharp Mask2", TAG + " Sharpen");
        if (needHS) addFx(L, "ADBE HUE SATURATION", TAG + " Color");
        if (s.invert) addFx(L, "ADBE Invert", TAG + " Invert");
        addFx(L, "ADBE Brightness & Contrast 2", TAG + " Contrast");
        if (s.pixelate && s.cell > 1) addFx(L, "ADBE Mosaic", TAG + " Pixelate");

        if (s.dither > 0) {
            if (isErr) {
                addFx(L, "ADBE Noise2", TAG + " Dither Noise");
            } else {
                addFx(L, "ADBE Compound Arithmetic", TAG + " Dither Map");
            }
        }
        addFx(L, "ADBE Posterize", TAG + " Levels");
        if (s.tonal) addFx(L, "ADBE Tritone", TAG + " Tonal");

        if (s.grainOn) {
            if (GRAIN_MODE === "add") addFx(L, ["ADBE Add Grain", "ADBE Grain2"], TAG + " Grain");
            else addFx(L, "ADBE Noise2", TAG + " Grain");
        }

        // VHS Noise: distortion first, then noise layers, then scanlines / flicker
        if (s.vhsOn) {
            if (s.vhsJitter > 0) addFx(L, "ADBE Geometry2", TAG + " VHS Jitter");
            if (s.vhsTracking > 0) addFx(L, "ADBE Wave Warp", TAG + " VHS Tracking");
            if (s.vhsBleed > 0) addFx(L, "ADBE Channel Blur", TAG + " VHS Bleed");
            if (s.vhsStreaks > 0) addFx(L, "ADBE Fractal Noise", TAG + " VHS Streaks");
            if (s.vhsDashes > 0) addFx(L, "ADBE Fractal Noise", TAG + " VHS Dashes");
            if (s.vhsStatic > 0) addFx(L, "ADBE Noise2", TAG + " VHS Static");
            if (s.vhsScanlines > 0) addFx(L, "ADBE Venetian Blinds", TAG + " VHS Scanlines");
            if (s.vhsFlicker > 0) addFx(L, "ADBE Brightness & Contrast 2", TAG + " VHS Flicker");
        }
        try { L.comment = MARK + sig; } catch (_) {}
    }

    function updateValues(L, c, s, mapLayer) {
        var a = s.algo, e;
        var step = 1 / s.levels;                                   // one quantizer step (0..1)
        var A_ = step * (s.spread / 100) * (s.dither / 100);       // dither amplitude

        e = getFx(L, TAG + " Blur");
        if (e) set(e, ["Blurriness", "ADBE Gaussian Blur 2-0001"], Math.max(0.1, s.blur * 0.05));

        e = getFx(L, TAG + " Denoise");
        if (e) set(e, ["Radius", "ADBE Median-0001"], clamp(Math.round(Math.abs(s.denoise) / 25) + 1, 1, 8));

        e = getFx(L, TAG + " Noise");
        if (e) {
            set(e, ["Amount of Noise", "ADBE Noise2-0001"], clamp(s.denoise * 0.15, 0, 100));
            set(e, ["Use Color Noise", "ADBE Noise2-0002"], 0);
            set(e, ["Clip Result Values", "ADBE Noise2-0003"], 1);
        }

        e = getFx(L, TAG + " Sharpen");
        if (e) {
            set(e, ["Amount", "ADBE Unsharp Mask2-0001"], s.sharpStrength * 30);
            set(e, ["Radius", "ADBE Unsharp Mask2-0002"], s.sharpRadius);
        }

        e = getFx(L, TAG + " Color");
        if (e) {
            set(e, ["Master Hue", "ADBE HUE SATURATION-0004"], s.hue);
            set(e, ["Master Saturation", "ADBE HUE SATURATION-0005"], s.bw ? -100 : clamp(s.saturation, -100, 100));
        }

        e = getFx(L, TAG + " Contrast");
        if (e) {
            set(e, ["Brightness", "ADBE Brightness & Contrast 2-0001"], clamp(s.gradeBias * 0.5, -100, 100));
            set(e, ["Contrast", "ADBE Brightness & Contrast 2-0002"], clamp(s.ditherContrast - 100, -100, 100));
        }

        e = getFx(L, TAG + " Pixelate");
        if (e) {
            set(e, ["Horizontal Blocks", "ADBE Mosaic-0001"], clamp(Math.round(c.width / s.cell), 1, 2000));
            set(e, ["Vertical Blocks", "ADBE Mosaic-0002"], clamp(Math.round(c.height / s.cell), 1, 2000));
            set(e, ["Sharp Colors", "ADBE Mosaic-0003"], 1);
        }

        e = getFx(L, TAG + " Dither Noise");
        if (e) {
            set(e, ["Amount of Noise", "ADBE Noise2-0001"], clamp(100 * A_ * a.k, 0, 100));
            set(e, ["Use Color Noise", "ADBE Noise2-0002"], s.bw ? 0 : 1);
            set(e, ["Clip Result Values", "ADBE Noise2-0003"], 1);
        }

        e = getFx(L, TAG + " Dither Map");
        if (e && mapLayer) {
            set(e, ["Second Source Layer", "ADBE Compound Arithmetic-0001"], mapLayer.index);
            set(e, ["Operator", "ADBE Compound Arithmetic-0002"], COMPOUND_OP_ADD);
            set(e, ["Overflow Behavior", "ADBE Compound Arithmetic-0004"], 1);
            set(e, ["Stretch Second Source to Fit", "ADBE Compound Arithmetic-0005"], 0);
            set(e, ["Blend With Original", "ADBE Compound Arithmetic-0006"], 0);
        }

        e = getFx(L, TAG + " Levels");
        if (e) set(e, ["Level", "ADBE Posterize-0001"], clamp(s.levels, 2, 255));

        e = getFx(L, TAG + " Tonal");
        if (e) {
            set(e, ["Highlights", "ADBE Tritone-0001"], s.tHighs);
            set(e, ["Midtones", "ADBE Tritone-0002"], s.tMids);
            set(e, ["Shadows", "ADBE Tritone-0003"], s.tShadows);
        }

        e = getFx(L, TAG + " Grain");
        if (e) updateGrain(e, s);

        if (s.vhsOn) updateVhs(L, s);
    }

    // set an expression only when it changed (cheap live edits)
    function setExpr(p, expr) {
        if (!p) return;
        try { if (p.expression !== expr) p.expression = expr; } catch (_) {}
    }

    function updateVhs(L, s) {
        var e, p;
        // frames between random changes: speed 100 -> every frame, 50 -> every 2nd ...
        var R = s.vhsSpeed > 0 ? Math.max(1, Math.round(100 / Math.max(1, s.vhsSpeed))) : 0;
        var seedExpr = "seedRandom(Math.floor(timeToFrames(time)/" + Math.max(1, R) + ")*31+" + s.vhsSeed + ",true);";
        var evo = R > 0 ? "time*" + roundTo(s.vhsSpeed * 3.6, 1) + "+" + (s.vhsSeed * 17) : "" + (s.vhsSeed * 17);

        e = getFx(L, TAG + " VHS Jitter");
        if (e) {
            p = findProp(e, "Position");
            if (p) {
                if (R > 0) setExpr(p, seedExpr + "var a=" + roundTo(s.vhsJitter * 0.5, 1) + ";var b=(random()<0.3)?1:0.12;[value[0]+random(-a,a)*b, value[1]];");
                else setExpr(p, "");
            }
        }

        e = getFx(L, TAG + " VHS Tracking");
        if (e) {
            set(e, ["Wave Type"], 1);
            set(e, ["Direction"], 90);
            set(e, ["Wave Width"], 260);
            set(e, ["Wave Speed"], 0.3 + s.vhsSpeed / 60);
            p = findProp(e, "Wave Height");
            if (p) {
                setP(p, s.vhsTracking * 0.3);
                setExpr(p, R > 0 ? "value*(0.35+0.65*Math.abs(Math.sin(time*2.3)*Math.sin(time*0.7+" + s.vhsSeed + ")))" : "");
            }
        }

        e = getFx(L, TAG + " VHS Bleed");
        if (e) {
            set(e, ["Red Blurriness"], s.vhsBleed * 0.4);
            set(e, ["Green Blurriness"], 0);
            set(e, ["Blue Blurriness"], s.vhsBleed * 0.3);
            set(e, ["Alpha Blurriness"], 0);
            set(e, ["Blur Dimensions"], 2);   // horizontal only
        }

        e = getFx(L, TAG + " VHS Streaks");   // vertical streaks (reference pattern 3)
        if (e) vhsFractal(e, s, 20, 3000, 220, -10, s.vhsStreaks * 0.5, R > 0 ? "time*" + roundTo(s.vhsSpeed * 0.9, 1) + "+" + s.vhsSeed : "" + s.vhsSeed);

        e = getFx(L, TAG + " VHS Dashes");    // horizontal tape dashes (reference pattern 7)
        if (e) vhsFractal(e, s, 500, 10, 420, -45, s.vhsDashes * 0.6, R > 0 ? evo : "" + s.vhsSeed);

        e = getFx(L, TAG + " VHS Static");    // fine RGB static (reference texture 2)
        if (e) {
            set(e, ["Amount of Noise", "ADBE Noise2-0001"], clamp(s.vhsStatic * 0.6, 0, 100));
            set(e, ["Use Color Noise", "ADBE Noise2-0002"], 1);
            set(e, ["Clip Result Values", "ADBE Noise2-0003"], 1);
        }

        e = getFx(L, TAG + " VHS Scanlines");
        if (e) {
            set(e, ["Direction"], VBLIND_DIR);
            set(e, ["Width"], s.vhsScanSize);
            set(e, ["Feather"], 0);
            set(e, ["Transition Completion"], clamp(s.vhsScanlines * 0.4, 0, 60));
        }

        e = getFx(L, TAG + " VHS Flicker");
        if (e) {
            p = findProp(e, "Brightness");
            if (p) {
                setP(p, 0);
                setExpr(p, R > 0 ? seedExpr + "value+random(-" + roundTo(s.vhsFlicker * 0.15, 1) + "," + roundTo(s.vhsFlicker * 0.15, 1) + ");" : "");
            }
            set(e, ["Contrast"], 0);
        }
    }

    // Fractal Noise used as an overlay streak generator
    function vhsFractal(e, s, w, h, contrast, bright, opacity, evoExpr) {
        set(e, ["Contrast"], contrast);
        set(e, ["Brightness"], bright);
        set(e, ["Uniform Scaling"], 0);
        set(e, ["Scale Width"], w);
        set(e, ["Scale Height"], h);
        set(e, ["Opacity"], clamp(opacity, 0, 100));
        set(e, ["Blending Mode"], FRACTAL_BLEND);
        var ev = findProp(e, "Evolution");
        if (ev) setExpr(ev, evoExpr);
    }

    function updateGrain(e, s) {
        if (GRAIN_MODE !== "add") {
            // fallback: Add Noise re-randomises every frame by itself
            set(e, ["Amount of Noise", "ADBE Noise2-0001"], clamp(s.grainAmount * 0.35, 0, 100));
            set(e, ["Use Color Noise", "ADBE Noise2-0002"], s.grainColor > 20 ? 1 : 0);
            set(e, ["Clip Result Values", "ADBE Noise2-0003"], 1);
            return;
        }
        // Add Grain: viewing mode must be "Final Output" or nothing is applied to the image
        set(e, ["Viewing Mode"], 3);
        set(e, ["Intensity"], s.grainAmount / 50);
        set(e, ["Size"], 0.4 + (s.grainSize / 100) * 2.2);
        set(e, ["Softness"], (s.grainSoftness / 100) * 2);
        set(e, ["Color Saturation"], (s.grainColor / 100) * 2);
        // luminance-dependent grain
        var sh = findIn(e, "Shadows", "Intensity");
        var hi = findIn(e, "Highlights", "Intensity");
        if (sh) setP(sh, s.grainShadows / 100);
        if (hi) setP(hi, s.grainHighlights / 100);
        // temporal motion: new seed every (1/speed) frames, plus the effect's own animation speed
        set(e, ["Animation Speed"], s.grainSpeed / 100);
        var seed = findProp(e, "Random Seed") || findProp(e, "Seed");
        if (seed) {
            try {
                if (s.grainSpeed > 0) {
                    var rate = Math.max(1, Math.round(100 / Math.max(1, s.grainSpeed)));
                    seed.expression = "Math.floor(timeToFrames(time)/" + rate + ")*7919+" + s.grainSeed;
                } else {
                    seed.expression = "";
                    seed.setValue(s.grainSeed);
                }
            } catch (_) {}
        }
    }

    // ------------------------------------------------------------ apply / live / remove
    function targetsFrom(c, useSelection) {
        var out = [], i, L, sel;
        if (useSelection) {
            sel = c.selectedLayers;
            for (i = 0; i < sel.length; i++) {
                L = sel[i];
                if (isHelper(L)) continue;
                if (L.property("ADBE Effect Parade")) out.push(L);
            }
        } else {
            for (i = 1; i <= c.numLayers; i++) if (isTarget(c.layer(i))) out.push(c.layer(i));
        }
        return out;
    }

    function run(useSelection, quiet) {
        if (BUSY) return;
        var c = activeComp();
        if (!c) { if (!quiet) alert("Open a composition first."); return; }
        var targets = targetsFrom(c, useSelection);
        if (!targets.length) { if (!quiet) alert("Select a layer first.\nThe effects are applied directly to it."); return; }

        var s;
        try { s = readUI(); } catch (err) { if (!quiet) alert("MAJEED - could not read controls:\n" + err.toString()); return; }

        BUSY = true;
        if (!quiet) app.beginUndoGroup("MAJEED - Apply");
        try {
            if (s.grainOn) probeGrain(targets[0]);
            var sig = makeSig(s, GRAIN_MODE === "add");
            var isErr = (s.algo.kind === "err");
            var mapLayer = null, mapComp = null;
            if (!isErr && s.dither > 0) {
                mapComp = getMapComp(c, s.algo, s.cell);
                updateMapStep(mapComp, (1 / s.levels) * (s.spread / 100) * (s.dither / 100));
                mapLayer = ensureMapLayer(c, mapComp);
            }
            for (var i = 0; i < targets.length; i++) {
                var L = targets[i];
                var cur = "";
                try { cur = String(L.comment); } catch (_) {}
                if (cur !== MARK + sig) buildStack(L, s, sig);
                updateValues(L, c, s, mapLayer);
            }
            // if a map is no longer needed, drop the helper layer
            if (isErr || s.dither <= 0) removeMapLayer(c);

            var msg = s.algo.name + (s.algo.exact ? " [exact]" : " [approx]") + " | " + (s.bw ? "B&W" : "colors kept") + " | " + s.levels + " levels";
            if (s.grainOn) msg += " | grain: " + (GRAIN_MODE === "add" ? "Add Grain" : "noise fallback");
            if (s.vhsOn) msg += " | VHS";
            setStatus(msg);
        } catch (err2) {
            setStatus("Error: " + err2.toString());
            if (!quiet) { try { alert("MAJEED error:\n" + err2.toString() + "\nLine: " + err2.line); } catch (_) {} }
        }
        if (!quiet) app.endUndoGroup();
        BUSY = false;
    }

    function removeMapLayer(c) {
        for (var i = c.numLayers; i >= 1; i--) {
            var L = c.layer(i);
            if (isHelper(L)) { try { L.locked = false; } catch (_) {} try { L.remove(); } catch (_) {} }
        }
    }

    function applyFull() { run(true, false); }

    function liveUpdate() {
        if (BUSY) return;
        try {
            if (!UI.live || !UI.live.value) return;
            var c = activeComp();
            if (!c) return;
            run(false, true);
        } catch (_) {}
    }

    function removeFull() {
        var c = activeComp();
        if (!c) return;
        app.beginUndoGroup("MAJEED - Remove");
        var i, L;
        for (i = 1; i <= c.numLayers; i++) {
            L = c.layer(i);
            if (isTarget(L) || !isHelper(L)) { try { clearFx(L); } catch (_) {} }
        }
        removeMapLayer(c);
        try {
            for (i = app.project.numItems; i >= 1; i--) {
                var it = app.project.item(i);
                if (it instanceof CompItem && (it.name === TAG + " Map" || it.name === TAG + " Cell") && it.usedIn.length === 0) it.remove();
            }
            for (i = app.project.numItems; i >= 1; i--) {
                var it2 = app.project.item(i);
                if (it2 instanceof CompItem && it2.name === TAG + " Cell" && it2.usedIn.length === 0) it2.remove();
            }
        } catch (_) {}
        app.endUndoGroup();
        setStatus("Removed.");
    }

    // ------------------------------------------------------------ UI builders
    function sf(c) {
        try { if (FNT) c.graphics.font = FNT; } catch (_) {}
        return c;
    }

    function setVal(ctrl, v) {
        if (ctrl.type === "checkbox") { ctrl.value = !!v; return; }
        ctrl.text = String(v);
        if (ctrl.slider) ctrl.slider.value = v;
    }

    function updateInfo() {
        try {
            var i = UI.algorithm.selection ? UI.algorithm.selection.index : 0;
            UI.info.text = ALGOS[i].desc;
        } catch (_) {}
    }

    function loadPreset(name) {
        var over = PRESETS[name];
        if (!over) return;
        var k;
        for (k in BASE) {
            if (k === "algorithm" || k === "ditherMode") continue;
            if (UI[k]) setVal(UI[k], BASE[k]);
        }
        for (k in over) {
            if (k === "algorithm" || k === "ditherMode") continue;
            if (UI[k]) setVal(UI[k], over[k]);
        }
        UI.algorithm.selection = algoIndex(over.algorithm ? over.algorithm : BASE.algorithm);
        UI.ditherMode.selection = (over.ditherMode !== undefined) ? over.ditherMode : BASE.ditherMode;
        updateInfo();
        liveUpdate();
    }

    function grp(parent) {
        var g = parent.add("group");
        g.orientation = "row";
        g.alignment = ["fill", "top"];
        g.alignChildren = ["left", "center"];
        g.spacing = 3;
        g.margins = 0;
        return g;
    }

    // slider + number box
    function row(parent, key, label, min, max, dec) {
        var def = BASE[key];
        var g = grp(parent);
        var t = sf(g.add("statictext", undefined, label));
        t.preferredSize = [LABEL_W, 14];
        var s = g.add("slider", undefined, def, min, max);
        s.preferredSize = [70, 14];
        s.alignment = ["fill", "center"];
        var e = sf(g.add("edittext", undefined, String(def)));
        e.preferredSize = [38, 16];
        e.slider = s;
        e.label = t;
        s.onChanging = function () { e.text = String(roundTo(s.value, dec)); };
        s.onChange = function () { e.text = String(roundTo(s.value, dec)); liveUpdate(); };
        e.onChange = function () {
            var v = clamp(num(e.text, def), min, max);
            e.text = String(roundTo(v, dec));
            s.value = v;
            liveUpdate();
        };
        UI[key] = e;
        return e;
    }

    function dropRow(parent, key, label, items, selIndex) {
        var g = grp(parent);
        var t = sf(g.add("statictext", undefined, label));
        t.preferredSize = [LABEL_W, 14];
        var d = sf(g.add("dropdownlist", undefined, items));
        d.alignment = ["fill", "center"];
        d.preferredSize = [100, 18];
        d.selection = selIndex;
        d.onChange = function () {
            if (key === "algorithm") updateInfo();
            liveUpdate();
        };
        UI[key] = d;
        return d;
    }

    function checkRow(parent, key, label) {
        var c = sf(parent.add("checkbox", undefined, label));
        c.value = !!BASE[key];
        c.onClick = function () { liveUpdate(); };
        UI[key] = c;
        return c;
    }

    function hexRow(parent, key, label, def) {
        var g = grp(parent);
        sf(g.add("statictext", undefined, label)).preferredSize = [LABEL_W, 14];
        var e = sf(g.add("edittext", undefined, def));
        e.alignment = ["fill", "center"];
        e.preferredSize = [100, 16];
        e.onChange = function () { liveUpdate(); };
        UI[key] = e;
        return e;
    }

    function section(parent, title) {
        var p = parent.add("panel", undefined, title);
        p.orientation = "column";
        p.alignment = ["fill", "top"];
        p.alignChildren = ["fill", "top"];
        p.spacing = 1;
        p.margins = [5, 9, 5, 3];
        sf(p);
        return p;
    }

    function newPage(stack) {
        var g = stack.add("group");
        g.orientation = "column";
        g.alignment = ["fill", "top"];
        g.alignChildren = ["fill", "top"];
        g.spacing = 3;
        g.margins = 0;
        PAGES.push(g);
        return g;
    }

    function showPage(idx) {
        idx = clamp(idx, 0, PAGES.length - 1);
        for (var i = 0; i < PAGES.length; i++) PAGES[i].visible = (i === idx);
        UI.pageDrop.selection = idx;
    }

    function buildUI(obj) {
        var w = (obj instanceof Panel) ? obj : new Window("palette", "MAJEED - FINAL v5", undefined, { resizeable: true });
        try { FNT = ScriptUI.newFont(w.graphics.font.name, ScriptUI.FontStyle.REGULAR, 9); } catch (_) { FNT = null; }

        w.orientation = "column";
        w.alignChildren = ["fill", "top"];
        w.spacing = 3;
        w.margins = 4;

        // ---- row 1: preset / apply / remove
        var bar = grp(w);
        var names = [];
        for (var pn in PRESETS) names.push(pn);
        var presetDrop = sf(bar.add("dropdownlist", undefined, names));
        presetDrop.selection = 0;
        presetDrop.preferredSize = [110, 20];
        presetDrop.onChange = function () { if (presetDrop.selection) loadPreset(presetDrop.selection.text); };

        var apply = sf(bar.add("button", undefined, "APPLY"));
        apply.preferredSize = [60, 20];
        apply.onClick = applyFull;

        var remove = sf(bar.add("button", undefined, "REMOVE"));
        remove.preferredSize = [60, 20];
        remove.onClick = removeFull;

        // ---- row 2: page navigation + live
        var nav = grp(w);
        var prevB = sf(nav.add("button", undefined, "<"));
        prevB.preferredSize = [22, 20];
        UI.pageDrop = sf(nav.add("dropdownlist", undefined, PAGE_NAMES));
        UI.pageDrop.alignment = ["fill", "center"];
        UI.pageDrop.preferredSize = [120, 20];
        var nextB = sf(nav.add("button", undefined, ">"));
        nextB.preferredSize = [22, 20];
        UI.live = sf(nav.add("checkbox", undefined, "Live"));
        UI.live.value = true;

        prevB.onClick = function () { showPage((UI.pageDrop.selection ? UI.pageDrop.selection.index : 0) - 1); };
        nextB.onClick = function () { showPage((UI.pageDrop.selection ? UI.pageDrop.selection.index : 0) + 1); };
        UI.pageDrop.onChange = function () { if (UI.pageDrop.selection) showPage(UI.pageDrop.selection.index); };

        // ---- pages (stacked, only one visible)
        var stack = w.add("group");
        stack.orientation = "stack";
        stack.alignment = ["fill", "top"];
        stack.alignChildren = ["fill", "top"];
        stack.margins = 0;

        // PAGE 1: Dither
        var p1 = newPage(stack);
        var gen = section(p1, "Dither");
        dropRow(gen, "ditherMode", "Color Mode", DITHER_MODES, BASE.ditherMode);
        dropRow(gen, "algorithm", "Algorithm", ALGO_NAMES, algoIndex(BASE.algorithm));
        UI.info = sf(gen.add("statictext", undefined, "", { multiline: true }));
        UI.info.alignment = ["fill", "top"];
        UI.info.preferredSize = [230, 40];
        row(gen, "dither", "Amount", 0, 100, 0);
        row(gen, "tones", "Levels", 2, 64, 0);
        row(gen, "ditherSize", "Scale", 1, 32, 0);
        row(gen, "ditherContrast", "Contrast", 0, 200, 0);
        row(gen, "spread", "Spread", 0, 200, 0);
        checkRow(gen, "pixelate", "Also pixelate image to Scale");

        // PAGE 2: Color
        var p2 = newPage(stack);
        var color = section(p2, "Color / Grade");
        row(color, "hue", "Hue", -180, 180, 0);
        row(color, "saturation", "Saturation", -100, 100, 0);
        row(color, "invert", "Invert (>=50)", 0, 100, 0);
        row(color, "gradeBias", "Grade Bias", -100, 100, 0);
        checkRow(color, "limitPalette", "Limit palette (Indexed colors)");
        row(color, "indexColors", "Indexed Colors", 2, 256, 0);
        var tone = section(p2, "Tonal Mapping");
        checkRow(tone, "tonalOn", "Apply tonal colors");
        hexRow(tone, "tHighs", "Highlights", "#FFFFFF");
        hexRow(tone, "tMids", "Midtones", "#808080");
        hexRow(tone, "tShadows", "Shadows", "#000000");

        // PAGE 3: Preprocess (from the plugin: Denoise|Noise, Blur, Sharpen strength/radius)
        var p3 = newPage(stack);
        var pre = section(p3, "Preprocess");
        row(pre, "denoise", "Denoise | Noise", -100, 100, 0);
        row(pre, "blur", "Blur", 0, 100, 0);
        row(pre, "sharpStrength", "Sharp Strength", 0, 10, 1);
        row(pre, "sharpRadius", "Sharp Radius", 0.1, 20, 1);

        // PAGE 4: Grain
        var p4 = newPage(stack);
        var grain = section(p4, "Film Grain");
        checkRow(grain, "grainOn", "Enable grain");
        row(grain, "grainAmount", "Amount", 0, 100, 0);
        row(grain, "grainSize", "Size", 0, 100, 0);
        row(grain, "grainSoftness", "Softness", 0, 100, 0);
        row(grain, "grainColor", "Color Grain", 0, 100, 0);
        row(grain, "grainShadows", "Shadow Grain", 0, 200, 0);
        row(grain, "grainHighlights", "Highlight Grain", 0, 200, 0);
        row(grain, "grainSpeed", "Motion Speed", 0, 100, 0);
        row(grain, "grainSeed", "Random Seed", 0, 10000, 0);

        // PAGE 5: VHS Noise (own implementation; independent of Dither / Grain)
        var p5 = newPage(stack);
        var vhs = section(p5, "VHS Noise");
        checkRow(vhs, "vhsOn", "Enable VHS Noise");
        var vn = section(p5, "Noise (animated)");
        row(vn, "vhsStatic", "Static (RGB)", 0, 100, 0);
        row(vn, "vhsDashes", "Tape Dashes", 0, 100, 0);
        row(vn, "vhsStreaks", "Vertical Streaks", 0, 100, 0);
        var vd = section(p5, "Signal / Analog");
        row(vd, "vhsBleed", "Color Bleed", 0, 100, 0);
        row(vd, "vhsTracking", "Tracking", 0, 100, 0);
        row(vd, "vhsJitter", "Jitter", 0, 100, 0);
        row(vd, "vhsScanlines", "Scanlines", 0, 100, 0);
        row(vd, "vhsScanSize", "Scanline Size", 1, 20, 0);
        row(vd, "vhsFlicker", "Flicker", 0, 100, 0);
        var vt = section(p5, "Motion");
        row(vt, "vhsSpeed", "Motion Speed", 0, 100, 0);
        row(vt, "vhsSeed", "Random Seed", 0, 10000, 0);

        // ---- status line
        UI.status = sf(w.add("statictext", undefined, "Select a layer, then APPLY.", { truncate: "end" }));
        UI.status.alignment = ["fill", "bottom"];

        w.onResizing = w.onResize = function () { this.layout.resize(); };

        updateInfo();
        showPage(0);
        w.layout.layout(true);
        return w;
    }

    var ROOT = buildUI(thisObj);
    if (ROOT instanceof Window) {
        ROOT.center();
        ROOT.show();
    } else {
        ROOT.layout.layout(true);
    }

})(this);
