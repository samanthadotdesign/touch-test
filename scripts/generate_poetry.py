#!/usr/bin/env python3
"""Validate the shared poetry database and generate firmware/browser constants.

Run from any directory. --input imports an exported sandbox database. --check
verifies committed generated artifacts without modifying them. No dependencies.
"""
import argparse
from functools import lru_cache
import hashlib
import json
import math
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
REFERENCE_LAYOUT_KEYS = ("scattered-phrases", "alternating-phrases", "open-phrases", "inset-phrases")
LAYOUTS = ("centered", "left", "stepped", "constellation", "cascade", "split", "wave", "columns", "random", "stepped-reverse", "stepped-zigzag", "split-mirrored", "split-three") + REFERENCE_LAYOUT_KEYS
MIN_MOTION_SPEED = .68
UNITS = ("letters", "words", "mixed")
FORMATIONS = ("cluster", "stream", "patches")
DEFAULT_FORMATIONS = ("cluster", "stream", "stream", "cluster", "patches", "patches", "cluster", "stream")
SHARED_BROWSER_SCRIPTS = (
    "poetry-data.js", "engine.js", "library-store.js", "display.js",
    "atmosphere.js", "background.bundle.js",
)
PAGE_SCRIPTS = {
    "sandbox/index.html": SHARED_BROWSER_SCRIPTS + ("simulator.js",),
    "prototype/index.html": tuple("../sandbox/" + name for name in SHARED_BROWSER_SCRIPTS) + ("prototype.js",),
}
RANGES = {
    "speed": (0.1, 2), "spread": (20, 100), "depth": (0, 180),
    "tilt": (0, 1.2), "spin": (0, 1), "perspective": (240, 800),
}


DISPLAY_ALIASES = str.maketrans({
    **dict.fromkeys("\u2018\u2019\u201a\u201b", "'"),
    **dict.fromkeys("\u201c\u201d\u201e\u201f", '"'),
    **dict.fromkeys("\u2010\u2011\u2012\u2013\u2014\u2212", "-"),
    "\u00a0": " ", "\u202f": " ", "\u2026": "...",
})


def normalize_display(value):
    """Map supported typography to bitmap glyphs without changing source text."""
    return value.translate(DISPLAY_ALIASES)


def ascii_text(value, label, limit, multiline=False, allow_empty=False, display_aliases=False):
    if not isinstance(value, str) or (not value.strip() and not (allow_empty and value == "")):
        raise ValueError(f"{label} must be a nonempty string")
    rendered = normalize_display(value) if display_aliases else value
    if len(rendered) > limit:
        raise ValueError(f"{label} exceeds {limit} display characters")
    if any(not (32 <= ord(c) <= 126 or multiline and c == "\n") for c in rendered):
        raise ValueError(f"{label} supports printable ASCII glyphs and supported typographic punctuation" + (" and newlines" if multiline else ""))
    return rendered


def wrapped_lines(text, size=1):
    """Mirror firmware wrapping: 168 px measure, 6 px character advance."""
    if size != 1:
        raise ValueError("poetry uses font size 1")
    lines = []
    capacity = 28
    for paragraph in text.split("\n"):
        current = ""
        for word in paragraph.split():
            if len(word) > capacity:
                return None
            candidate = (current + " " if current else "") + word
            if len(candidate) > capacity:
                lines.append(current)
                current = word
            else:
                current = candidate
        lines.append(current)
    return lines


def reading_layouts():
    """Load the shared Figma phrase boxes used by all three implementations."""
    data = json.loads((ROOT / "data/reading-layouts.json").read_text())
    layouts = data.get("layouts")
    if not isinstance(layouts, list) or len(layouts) != len(REFERENCE_LAYOUT_KEYS):
        raise ValueError("reading-layouts.json must contain four reference layouts")
    for index, template in enumerate(layouts):
        if template.get("id") != index + 13 or template.get("key") != REFERENCE_LAYOUT_KEYS[index]:
            raise ValueError("reading layout IDs and keys must retain their shared order")
        boxes = template.get("boxes")
        if not isinstance(boxes, list) or not 1 <= len(boxes) <= 9:
            raise ValueError("reading templates need one to nine boxes")
        for box in boxes:
            if any(type(box.get(field)) is not int for field in ("x", "y", "width", "rows", "weight")):
                raise ValueError("reading box geometry and weights must be integers")
            if not (0 <= box["x"] < 240 and 0 <= box["y"] < 320 and
                    6 <= box["width"] <= 240 - box["x"] and
                    1 <= box["rows"] <= 22 and 1 <= box["weight"] <= 255 and
                    box["y"] + (box["rows"] - 1) * 14 + 8 <= 320):
                raise ValueError("reading box geometry must fit the display")
    return layouts


def phrase_partition(text, template):
    """Return contiguous word-group endpoints, or None when no full fit exists.

    Candidates follow global weighted endpoints, nearest first with a lower-end
    tie break. Only actual source newlines affect wrapping inside each box.
    """
    text = normalize_display(text)
    staged_lines = wrapped_lines(text)
    if staged_lines is None or len(staged_lines) > 97:
        return None
    words = list(re.finditer(r"[^ \n]+", text))
    boxes = template["boxes"]
    count = len(words)
    if not words or count > 96 or len(text) > 512 or any(len(word[0]) > 28 for word in words):
        return None
    if count < len(boxes) or sum(len(word[0]) for word in words) > sum(box["width"] // 6 * box["rows"] for box in boxes):
        return None
    total_weight = sum(box["weight"] for box in boxes)
    cumulative = 0
    targets = []
    for box in boxes:
        cumulative += box["weight"]
        targets.append((2 * count * cumulative + total_weight) // (2 * total_weight))

    @lru_cache(None)
    def fits(box_index, start, end):
        columns, rows = boxes[box_index]["width"] // 6, boxes[box_index]["rows"]
        row, used = 1, 0
        for index in range(start, end):
            length = len(words[index][0])
            if length > columns:
                return False
            if index > start:
                breaks = text[words[index - 1].end():words[index].start()].count("\n")
                if breaks:
                    row += breaks
                    used = 0
            if used and used + 1 + length > columns:
                row += 1
                used = 0
            if row > rows:
                return False
            used += length + (1 if used else 0)
        return True

    @lru_cache(None)
    def solve(box_index, start):
        if box_index == len(boxes):
            return () if start == count else None
        last = count - (len(boxes) - box_index - 1)
        candidates = sorted(range(start + 1, last + 1), key=lambda end: (abs(end - targets[box_index]), end))
        for end in candidates:
            if fits(box_index, start, end):
                suffix = solve(box_index + 1, end)
                if suffix is not None:
                    return (end,) + suffix
        return None
    result = solve(0, 0)
    return list(result) if result is not None else None


def layout_size(text, layout="random"):
    """Check the same baseline and per-arrangement eligibility as the device.

    Random needs any concrete layout to fit; the engine chooses among eligible
    arrangements at runtime. No text is truncated to force a fit.
    """
    if layout not in LAYOUTS:
        raise ValueError("unknown poem layout")
    lines = wrapped_lines(text)
    if lines is None:
        raise ValueError("poem contains a word longer than the 28-character measure")
    if len(lines) > 97:
        raise ValueError("poem exceeds the 97 wrapped-line parser limit")
    if layout in REFERENCE_LAYOUT_KEYS:
        template = reading_layouts()[REFERENCE_LAYOUT_KEYS.index(layout)]
        if phrase_partition(text, template) is None:
            raise ValueError("poem cannot fill every reference phrase box without overflow")
        return 1
    words = [word for line in lines for word in line.split()]
    if len(text) > 512 or not words or len(words) > 96:
        raise ValueError("poem needs 1 to 96 words and at most 512 characters")
    if layout == "random":
        for concrete in (name for name in LAYOUTS if name != "random"):
            try:
                return layout_size(text, concrete)
            except ValueError:
                pass
        raise ValueError("poem cannot fit any supported reading layout")
    widths = [len(line) * 6 for line in lines]
    count, line_count = len(words), len(lines)
    pitch = max(12, min(14, 252 // (line_count - 1))) if line_count > 1 else 14
    body_height = (line_count - 1) * pitch + 8
    if layout in ("centered", "left", "stepped", "stepped-reverse", "stepped-zigzag") and body_height + 20 + 8 > 288:
        raise ValueError("poem cannot fit the 240x320 display at font size 1")
    if layout in ("stepped", "stepped-reverse", "stepped-zigzag"):
        if layout == "stepped":
            indents = [i * 8 for i in range(line_count)]
        elif layout == "stepped-reverse":
            indents = [(line_count - 1 - i) * 8 for i in range(line_count)]
        else:
            indents = [(i % 4 if i % 4 <= 2 else 4 - i % 4) * 12 for i in range(line_count)]
        if max(indent + width for indent, width in zip(indents, widths)) > 216:
            raise ValueError("stepped poem exceeds the 216-pixel group width")
    if layout == "constellation":
        if (count + 2) // 3 > 12:
            raise ValueError("constellation allows at most 12 rows (36 words)")
        columns = [max((len(word) * 6 for i, word in enumerate(words) if i % 3 == col), default=0)
                   for col in range(3)]
        if sum(columns) + 36 > 216:
            raise ValueError("constellation columns exceed 216 pixels")
    if layout == "cascade" and count > 20:
        raise ValueError("cascade allows at most 20 words")
    if layout in ("split", "split-mirrored"):
        first = (line_count + 1) // 2
        total_height = (first * 14 - 6) + 48 + ((line_count - first) * 14 - 6)
        if line_count < 2 or total_height > 248:
            raise ValueError("split needs two or more lines with a combined height of at most 248 pixels")
    if layout == "split-three":
        counts = [sum(i * 3 // line_count == group for i in range(line_count)) for group in range(3)]
        if line_count < 3 or sum(count * 14 - 6 for count in counts) + 64 > 248:
            raise ValueError("split-three needs three or more lines with a combined height of at most 248 pixels")
    if layout == "wave" and line_count > 7:
        raise ValueError("wave allows at most seven wrapped lines")
    if layout == "columns":
        half = (count + 1) // 2
        if half > 17 or count - half < 1:
            raise ValueError("columns needs 2 to 34 words")
        left = max(len(word) * 6 for word in words[:half])
        right = max(len(word) * 6 for word in words[half:])
        if left + 24 + right > 216:
            raise ValueError("columns exceed 216 pixels")
    return 1


def validate(data):
    if not isinstance(data, dict):
        raise ValueError("database must be an object")
    if "version" in data and (type(data["version"]) is not int or data["version"] not in (1, 2, 3, 4)):
        raise ValueError("database version must be 1, 2, 3, or 4")
    for key in ("poems", "animations"):
        if not isinstance(data.get(key), list) or not data[key]:
            raise ValueError(f"{key} must be a nonempty array")
        ids = set()
        for i, entry in enumerate(data[key]):
            label = f"{key}[{i}]"
            if not isinstance(entry, dict):
                raise ValueError(f"{label} must be an object")
            ident = entry.get("id")
            if not isinstance(ident, str) or not re.fullmatch(r"[a-z0-9][a-z0-9_-]{0,59}", ident):
                raise ValueError(f"{label}.id must be a lowercase identifier (up to 60 characters)")
            if ident in ids:
                raise ValueError(f"duplicate {key} id: {ident}")
            ids.add(ident)
            if key == "poems":
                ascii_text(entry.get("title"), f"{label}.title", 60, allow_empty=True, display_aliases=True)
                # Library labels do not establish a poem/book title for display.
                # Older exports lack this optional metadata and show author only.
                entry.setdefault("workTitle", "")
                ascii_text(entry["workTitle"], f"{label}.workTitle", 60, allow_empty=True, display_aliases=True)
                display_text = ascii_text(entry.get("text"), f"{label}.text", 512, multiline=True, display_aliases=True)
                ascii_text(entry.get("by"), f"{label}.by", 35, display_aliases=True)
                if len(display_text.split()) > 96:
                    raise ValueError(f"{label}.text exceeds 96 words")
                poem_layout = entry.get("layout", "random")
                if poem_layout not in LAYOUTS:
                    raise ValueError(f"{label}.layout must be one of: {', '.join(LAYOUTS)}")
                entry["layout"] = poem_layout
                try:
                    layout_size(display_text, poem_layout)
                except ValueError as exc:
                    raise ValueError(f"{label}: {exc}") from exc
            else:
                ascii_text(entry.get("name"), f"{label}.name", 60)
                if type(entry.get("kind")) is not int or not 0 <= entry["kind"] <= 7:
                    raise ValueError(f"{label}.kind must be an integer from 0 to 7")
                unit = entry.get("unit", "mixed")
                if unit not in UNITS:
                    raise ValueError(f"{label}.unit must be letters, words, or mixed")
                entry["unit"] = unit
                formation = entry.get("formation", DEFAULT_FORMATIONS[entry["kind"]])
                if formation not in FORMATIONS:
                    raise ValueError(f"{label}.formation must be cluster, stream, or patches")
                entry["formation"] = formation
                for field, (low, high) in RANGES.items():
                    value = entry.get(field)
                    if type(value) not in (int, float) or not math.isfinite(value) or not low <= value <= high:
                        raise ValueError(f"{label}.{field} must be a finite number from {low} to {high}")
                entry["speed"] = max(MIN_MOTION_SPEED, entry["speed"])
    data["version"] = 4
    return data


def cpp_string(value):
    return json.dumps(value, ensure_ascii=True)


def cpp_float(value):
    literal = repr(float(value))
    return literal + "f"


def generate(data):
    references = reading_layouts()
    reading_header = [
        "// Generated by scripts/generate_poetry.py from data/reading-layouts.json.",
        "// " + json.loads((ROOT / "data/reading-layouts.json").read_text())["source"],
        "#pragma once", "#include <stdint.h>", "",
        "struct PoetryReadingBox { int16_t x, y; uint16_t width; uint8_t rows, weight; };",
        "struct PoetryReadingTemplate { uint8_t id, count; PoetryReadingBox boxes[9]; };",
        "constexpr uint8_t POETRY_REFERENCE_LAYOUT_COUNT = 4;",
        "constexpr uint8_t POETRY_MAX_LAYOUT = 16;",
        "static const PoetryReadingTemplate POETRY_READING_TEMPLATES[4] = {",
    ]
    for template in references:
        boxes = ["{" + ", ".join(str(box[field]) for field in ("x", "y", "width", "rows", "weight")) + "}" for box in template["boxes"]]
        reading_header.append("  {" + str(template["id"]) + ", " + str(len(boxes)) + ", {" + ", ".join(boxes) + "}},")
    reading_header += ["};", ""]
    header = [
        "// Generated by scripts/generate_poetry.py. Edit data/poetry.json instead.",
        "#pragma once", "#include <stdint.h>", "",
        "// Poem.title is the normalized display workTitle, not the library title label.",
        "struct Poem { const char *id; const char *title; const char *text; const char *by; uint8_t layout; };",
        "static const Poem POEMS[] = {",
    ]
    for poem in data["poems"]:
        fields = [cpp_string(poem["id"]),
                  cpp_string(normalize_display(poem.get("workTitle", ""))),
                  cpp_string(normalize_display(poem["text"])),
                  cpp_string(normalize_display(poem["by"]))]
        fields.append(str(LAYOUTS.index(poem.get("layout", "random"))))
        header.append("  {" + ", ".join(fields) + "},")
    header += ["};", "static const int POEM_COUNT = sizeof(POEMS) / sizeof(POEMS[0]);", "",
               "struct MotionPreset { const char *id; const char *name; uint8_t kind; float speed, spread, depth, tilt, spin, perspective; uint8_t unit; uint8_t formation; };",
               "static const MotionPreset MOTIONS[] = {"]
    for motion in data["animations"]:
        fields = [cpp_string(motion["id"]), cpp_string(motion["name"]), str(motion["kind"])]
        fields += [cpp_float(motion[k]) for k in RANGES]
        fields.append(str(UNITS.index(motion.get("unit", "mixed"))))
        fields.append(str(FORMATIONS.index(motion.get("formation", DEFAULT_FORMATIONS[motion["kind"]]))))
        header.append("  {" + ", ".join(fields) + "},")
    header += ["};", "static const int MOTION_COUNT = sizeof(MOTIONS) / sizeof(MOTIONS[0]);", ""]

    font = json.loads((ROOT / "data/font5x7.json").read_text())
    glyphs = font["glyphs"]
    if len(glyphs) != 95 or any(len(g) != 5 or any(type(b) is not int or not 0 <= b <= 255 for b in g) for g in glyphs):
        raise ValueError("data/font5x7.json must contain 95 glyphs of five bytes")
    browser = "// Generated by scripts/generate_poetry.py. Edit data/poetry.json instead.\n"
    browser += "globalThis.POETRY_DATA = " + json.dumps(data, ensure_ascii=False, indent=2) + ";\n\n"
    browser += "globalThis.POETRY_READING_LAYOUTS = " + json.dumps(references, ensure_ascii=False, separators=(",", ":")) + ";\n\n"
    # Keep poem-edit provenance independent from motion/template releases.
    # Older source copies of presets are migrated separately by library-store.
    revision = hashlib.sha256(json.dumps(data["poems"], sort_keys=True, ensure_ascii=False).encode()).hexdigest()
    browser += "globalThis.POETRY_SOURCE_REVISION = " + json.dumps(revision) + ";\n\n"
    browser += "/* ASCII font from Adafruit GFX; see data/font5x7.json for provenance.\n" + font["license"] + "\n*/\n"
    browser += "globalThis.POETRY_FONT = " + json.dumps(glyphs, separators=(",", ":")) + ";\n"
    outputs = {ROOT / "src/poetry_data.h": "\n".join(header), ROOT / "src/reading_layouts.h": "\n".join(reading_header), ROOT / "sandbox/poetry-data.js": browser}
    # Give changed app scripts new URLs when an embedded preview reuses its
    # cache. Preserve every other byte of the current hand-edited pages, and
    # hash generated data from this run rather than its previous on-disk copy.
    generated_scripts = {path.resolve(): content.encode() for path, content in outputs.items()
                         if path.suffix == ".js"}
    for relative, assets in PAGE_SCRIPTS.items():
        path = ROOT / relative
        content = path.read_text()
        for asset in assets:
            local = (path.parent / asset).resolve()
            payload = generated_scripts[local] if local in generated_scripts else local.read_bytes()
            asset_revision = hashlib.sha256(payload).hexdigest()[:12]
            pattern = re.compile(
                r'(?P<prefix><script\b[^>]*?\bsrc\s*=\s*)(?P<quote>[\"\x27])'
                + re.escape(asset) + r'(?:\?[^\"\x27]*)?(?P=quote)', re.IGNORECASE)
            def versioned(match):
                return match["prefix"] + match["quote"] + asset + "?rev=" + asset_revision + match["quote"]
            content, replacements = pattern.subn(versioned, content)
            if replacements != 1:
                raise ValueError(f"{relative} must load {asset} exactly once")
        outputs[path] = content
    return outputs


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=ROOT / "data/poetry.json", help="JSON database to validate and generate from")
    parser.add_argument("--check", action="store_true", help="check generated outputs match the database without writing")
    args = parser.parse_args(argv)
    try:
        data = validate(json.loads(args.input.read_text()))
        outputs = generate(data)
        stale = []
        for path, content in outputs.items():
            if args.check:
                if not path.exists() or path.read_text() != content:
                    stale.append(str(path.relative_to(ROOT)))
            else:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(content)
        if stale:
            raise ValueError("generated files are stale: " + ", ".join(stale))
    except (OSError, ValueError, KeyError, TypeError) as exc:
        print(f"Poetry data error: {exc}", file=sys.stderr)
        return 1
    verb = "Checked" if args.check else "Generated"
    print(f"{verb} {len(data['poems'])} poems and {len(data['animations'])} animations for firmware and browser.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
