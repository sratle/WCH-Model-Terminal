#!/usr/bin/env python3
"""
Extract ASCII glyphs and FontAwesome icons from LVGL font files
to MiniUI font format and bitmap arrays.

Usage:
    python extract_lvgl_font.py --input LVGL/src/font/lv_font_montserrat_12.c --output-dir Common/Common/MiniUI/font --size 12
    python extract_lvgl_font.py --input LVGL/src/font/lv_font_montserrat_16.c --output-dir Common/Common/MiniUI/font --size 16
"""

import re
import sys
import os
import argparse
from typing import List, Tuple, Optional


class GlyphInfo:
    """Represents a single glyph"""
    def __init__(self, unicode: int, width: int, height: int, x_offset: int, 
                 y_offset: int, advance: int, bitmap: List[int], bitmap_index: int):
        self.unicode = unicode
        self.width = width
        self.height = height
        self.x_offset = x_offset
        self.y_offset = y_offset
        self.advance = advance
        self.bitmap = bitmap
        self.bitmap_index = bitmap_index


def parse_glyph_bitmap(content: str) -> List[int]:
    """Parse the glyph_bitmap array from LVGL font file"""
    # Find the start of glyph_bitmap array
    start_match = re.search(r'static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap\[\] = \{', content)
    if not start_match:
        raise ValueError("Could not find glyph_bitmap array start")
    
    start_pos = start_match.end()
    
    # Find the matching closing brace by counting braces
    brace_count = 1
    end_pos = start_pos
    while brace_count > 0 and end_pos < len(content):
        if content[end_pos] == '{':
            brace_count += 1
        elif content[end_pos] == '}':
            brace_count -= 1
        end_pos += 1
    
    bitmap_str = content[start_pos:end_pos-1]
    # Remove comments
    bitmap_str = re.sub(r'/\*.*?\*/', '', bitmap_str, flags=re.DOTALL)
    # Extract hex values
    values = re.findall(r'0x([0-9a-fA-F]{2})', bitmap_str)
    return [int(v, 16) for v in values]


def parse_glyph_dsc(content: str) -> List[dict]:
    """Parse the glyph_dsc array from LVGL font file"""
    # Find the start of glyph_dsc array
    start_match = re.search(r'static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc\[\] = \{', content)
    if not start_match:
        raise ValueError("Could not find glyph_dsc array start")
    
    start_pos = start_match.end()
    
    # Find the matching closing brace by counting braces
    brace_count = 1
    end_pos = start_pos
    while brace_count > 0 and end_pos < len(content):
        if content[end_pos] == '{':
            brace_count += 1
        elif content[end_pos] == '}':
            brace_count -= 1
        end_pos += 1
    
    dsc_str = content[start_pos:end_pos-1]
    # Remove comments
    dsc_str = re.sub(r'/\*.*?\*/', '', dsc_str, flags=re.DOTALL)
    
    # Parse each entry - match the exact format: {.bitmap_index = N, .adv_w = N, .box_w = N, .box_h = N, .ofs_x = N, .ofs_y = N}
    entries = []
    pattern = r'\{\.bitmap_index\s*=\s*(\d+),\s*\.adv_w\s*=\s*(\d+),\s*\.box_w\s*=\s*(\d+),\s*\.box_h\s*=\s*(\d+),\s*\.ofs_x\s*=\s*(-?\d+),\s*\.ofs_y\s*=\s*(-?\d+)\}'
    
    for m in re.finditer(pattern, dsc_str):
        entries.append({
            'bitmap_index': int(m.group(1)),
            'adv_w': int(m.group(2)),
            'box_w': int(m.group(3)),
            'box_h': int(m.group(4)),
            'ofs_x': int(m.group(5)),
            'ofs_y': int(m.group(6)),
        })
    
    return entries


def parse_font_metrics(content: str) -> dict:
    """Parse line_height and base_line from LVGL font file"""
    line_height_match = re.search(r'\.line_height\s*=\s*(\d+)', content)
    base_line_match = re.search(r'\.base_line\s*=\s*(\d+)', content)
    
    line_height = int(line_height_match.group(1)) if line_height_match else 0
    base_line = int(base_line_match.group(1)) if base_line_match else 0
    
    return {
        'line_height': line_height,
        'base_line': base_line,
        'baseline_from_top': line_height - base_line
    }


def parse_cmaps(content: str) -> List[dict]:
    """Parse the cmaps array to get unicode to glyph_id mapping"""
    # Find the start of cmaps array
    start_match = re.search(r'static const lv_font_fmt_txt_cmap_t cmaps\[\] = \{', content)
    if not start_match:
        raise ValueError("Could not find cmaps array start")
    
    start_pos = start_match.end()
    
    # Find the matching closing brace by counting braces
    brace_count = 1
    end_pos = start_pos
    while brace_count > 0 and end_pos < len(content):
        if content[end_pos] == '{':
            brace_count += 1
        elif content[end_pos] == '}':
            brace_count -= 1
        end_pos += 1
    
    cmap_str = content[start_pos:end_pos-1]
    
    # Find unicode_list if present
    unicode_list_match = re.search(r'static const uint16_t unicode_list_1\[\] = \{([^}]+)\}', content)
    unicode_list = None
    if unicode_list_match:
        list_str = unicode_list_match.group(1)
        unicode_list = [int(v.strip(), 0) for v in re.findall(r'0x[0-9a-fA-F]+|\d+', list_str)]
    
    # Parse each cmap entry - use a more precise pattern to match each struct
    cmaps = []
    
    # Match each cmap struct (between { and })
    # Need to handle nested braces in the struct
    struct_pattern = r'\{([^{}]*(?:\{[^{}]*\}[^{}]*)*)\}'
    
    for m in re.finditer(struct_pattern, cmap_str):
        struct_str = m.group(1)
        
        # Extract range_start
        range_start_match = re.search(r'\.range_start\s*=\s*(\d+)', struct_str)
        range_start = int(range_start_match.group(1)) if range_start_match else 0
        
        # Extract range_length
        range_length_match = re.search(r'\.range_length\s*=\s*(\d+)', struct_str)
        range_length = int(range_length_match.group(1)) if range_length_match else 0
        
        # Extract glyph_id_start
        glyph_id_start_match = re.search(r'\.glyph_id_start\s*=\s*(\d+)', struct_str)
        glyph_id_start = int(glyph_id_start_match.group(1)) if glyph_id_start_match else 0
        
        # Check if it has unicode_list (sparse type)
        # The struct has "unicode_list = unicode_list_1" for sparse, "unicode_list = NULL" for continuous
        has_unicode_list = 'unicode_list_1' in struct_str
        
        if has_unicode_list:
            cmaps.append({
                'range_start': range_start,
                'range_length': range_length,
                'glyph_id_start': glyph_id_start,
                'type': 'sparse',
                'unicode_list': unicode_list
            })
        else:
            cmaps.append({
                'range_start': range_start,
                'range_length': range_length,
                'glyph_id_start': glyph_id_start,
                'type': 'continuous',
                'unicode_list': None
            })
    
    return cmaps


def build_glyph_table(bitmap: List[int], glyph_dsc: List[dict], cmaps: List[dict], baseline_from_top: int) -> List[GlyphInfo]:
    """Build a table of all glyphs with their unicode codepoints
    
    LVGL renders glyphs at: glyph_y = line_top + baseline_from_top - box_h - ofs_y
    MiniUI renders glyphs at: gy = y + baseline + y_offset
    
    For these to match (with baseline = baseline_from_top):
        y_offset = -box_h - ofs_y
    """
    glyphs = []
    
    for cmap in cmaps:
        if cmap['type'] == 'continuous':
            for i in range(cmap['range_length']):
                unicode = cmap['range_start'] + i
                glyph_id = cmap['glyph_id_start'] + i
                if glyph_id < len(glyph_dsc):
                    dsc = glyph_dsc[glyph_id]
                    glyph_bitmap = extract_glyph_bitmap(bitmap, dsc)
                    y_offset = -dsc['box_h'] - dsc['ofs_y']
                    glyphs.append(GlyphInfo(
                        unicode=unicode,
                        width=dsc['box_w'],
                        height=dsc['box_h'],
                        x_offset=dsc['ofs_x'],
                        y_offset=y_offset,
                        advance=(dsc['adv_w'] + 8) // 16,
                        bitmap=glyph_bitmap,
                        bitmap_index=dsc['bitmap_index']
                    ))
        elif cmap['type'] == 'sparse' and cmap['unicode_list']:
            for i, offset in enumerate(cmap['unicode_list']):
                unicode = cmap['range_start'] + offset
                glyph_id = cmap['glyph_id_start'] + i
                if glyph_id < len(glyph_dsc):
                    dsc = glyph_dsc[glyph_id]
                    glyph_bitmap = extract_glyph_bitmap(bitmap, dsc)
                    y_offset = -dsc['box_h'] - dsc['ofs_y']
                    glyphs.append(GlyphInfo(
                        unicode=unicode,
                        width=dsc['box_w'],
                        height=dsc['box_h'],
                        x_offset=dsc['ofs_x'],
                        y_offset=y_offset,
                        advance=(dsc['adv_w'] + 8) // 16,
                        bitmap=glyph_bitmap,
                        bitmap_index=dsc['bitmap_index']
                    ))
    
    return glyphs


def extract_glyph_bitmap(bitmap: List[int], dsc: dict) -> List[int]:
    """Extract bitmap data for a single glyph from the global bitmap array"""
    bpp = 4  # LVGL montserrat fonts use 4bpp
    width = dsc['box_w']
    height = dsc['box_h']
    
    if width == 0 or height == 0:
        return []
    
    total_bits = width * bpp * height
    total_bytes = (total_bits + 7) // 8
    
    start_idx = dsc['bitmap_index']
    end_idx = start_idx + total_bytes
    
    if end_idx > len(bitmap):
        end_idx = len(bitmap)
    
    return bitmap[start_idx:end_idx]


def convert_4bpp_to_1bpp(bitmap_4bpp: List[int], width: int, height: int) -> List[int]:
    """Convert 4bpp bitmap to 1bpp (threshold at 50%)
    
    LVGL stores 4bpp glyph bitmaps as a continuous bit stream
    (no row padding), packed MSB-first within each byte.
    """
    if not bitmap_4bpp or width == 0 or height == 0:
        return []
    
    result = []
    bit_idx = 0
    
    for row in range(height):
        for col in range(width):
            byte_idx = bit_idx // 8
            shift = 4 - (bit_idx % 8)
            if shift < 0:
                shift += 8
                byte_idx += 1
            
            if byte_idx < len(bitmap_4bpp):
                pixel = (bitmap_4bpp[byte_idx] >> shift) & 0x0F
            else:
                pixel = 0
            
            bit = 1 if pixel >= 8 else 0
            
            byte_pos = (row * width + col) // 8
            bit_pos = 7 - ((row * width + col) % 8)
            
            if byte_pos >= len(result):
                result.append(0)
            
            if bit:
                result[byte_pos] |= (1 << bit_pos)
            
            bit_idx += 4
    
    return result


def convert_4bpp_to_1bpp_row_aligned(bitmap_4bpp: List[int], width: int, height: int) -> List[int]:
    """Convert 4bpp bitmap to 1bpp with row-aligned byte packing.
    
    Each row starts at a byte boundary (compatible with ui_draw_icon).
    """
    if not bitmap_4bpp or width == 0 or height == 0:
        return []
    
    bytes_per_row = (width + 7) // 8
    result = [0] * (bytes_per_row * height)
    bit_idx = 0
    
    for row in range(height):
        for col in range(width):
            byte_idx = bit_idx // 8
            shift = 4 - (bit_idx % 8)
            if shift < 0:
                shift += 8
                byte_idx += 1
            
            if byte_idx < len(bitmap_4bpp):
                pixel = (bitmap_4bpp[byte_idx] >> shift) & 0x0F
            else:
                pixel = 0
            
            bit = 1 if pixel >= 8 else 0
            
            if bit:
                row_byte_pos = row * bytes_per_row + col // 8
                result[row_byte_pos] |= (0x80 >> (col % 8))
            
            bit_idx += 4
    
    return result


def generate_miniui_font(glyphs: List[GlyphInfo], font_size: int, output_path: str, font_name: str, line_height: int, baseline_from_top: int):
    """Generate MiniUI font file"""
    # Filter ASCII glyphs (32-126) and special chars
    ascii_glyphs = [g for g in glyphs if 32 <= g.unicode <= 126]
    # Also include degree symbol (0xB0) and bullet (0x2022) if present
    special_glyphs = [g for g in glyphs if g.unicode in [0xB0, 0x2022]]
    all_glyphs = sorted(ascii_glyphs + special_glyphs, key=lambda g: g.unicode)
    
    if not all_glyphs:
        print(f"Warning: No ASCII glyphs found for {font_name}")
        return
    
    c_file = os.path.join(output_path, f"font_{font_name}.c")
    h_file = os.path.join(output_path, f"font_{font_name}.h")
    
    # Generate .c file
    with open(c_file, 'w') as f:
        f.write(f"/********************************** (C) COPYRIGHT *******************************\n")
        f.write(f"* File Name          : font_{font_name}.c\n")
        f.write(f"* Description        : MiniUI font {font_size}px extracted from LVGL.\n")
        f.write(f"*                      ASCII 32-126 + degree + bullet. 1bpp format.\n")
        f.write(f"********************************************************************************/\n")
        f.write(f"#include \"font_{font_name}.h\"\n\n")
        
        # Generate bitmap arrays for each glyph
        for glyph in all_glyphs:
            if glyph.width == 0 or glyph.height == 0:
                continue
            
            # Convert 4bpp to 1bpp (continuous bit stream for draw_glyph)
            bitmap_1bpp = convert_4bpp_to_1bpp(glyph.bitmap, glyph.width, glyph.height)
            
            if not bitmap_1bpp:
                bitmap_1bpp = [0x00]
            
            # Sanitize unicode for C identifier
            if glyph.unicode == 0xB0:
                suffix = "degree"
            elif glyph.unicode == 0x2022:
                suffix = "bullet"
            else:
                suffix = f"u{glyph.unicode:04X}"
            
            f.write(f"/* U+{glyph.unicode:04X} '{chr(glyph.unicode) if 32 <= glyph.unicode <= 126 else '?'}' */\n")
            f.write(f"static const uint8_t glyph_{suffix}_bitmap[] = {{\n    ")
            
            for i, b in enumerate(bitmap_1bpp):
                if i > 0 and i % 12 == 0:
                    f.write("\n    ")
                f.write(f"0x{b:02X}, ")
            
            f.write("\n};\n\n")
        
        # Generate glyph table
        f.write("/* Glyph table */\n")
        f.write(f"static const ui_glyph_t {font_name}_glyphs[] = {{\n")
        
        for glyph in all_glyphs:
            if glyph.unicode == 0xB0:
                suffix = "degree"
            elif glyph.unicode == 0x2022:
                suffix = "bullet"
            else:
                suffix = f"u{glyph.unicode:04X}"
            
            if glyph.width == 0 or glyph.height == 0:
                # Space character
                f.write(f"    {{{glyph.unicode}, {glyph.width}, {glyph.height}, {glyph.x_offset}, {glyph.y_offset}, {glyph.advance}, NULL}},\n")
            else:
                f.write(f"    {{{glyph.unicode}, {glyph.width}, {glyph.height}, {glyph.x_offset}, {glyph.y_offset}, {glyph.advance}, glyph_{suffix}_bitmap}},\n")
        
        f.write("};\n\n")
        
        # Font metrics from LVGL source
        # baseline = distance from top of line to baseline
        # height = line_height (includes ascent + descent + line gap)
        
        # Font descriptor
        f.write(f"/* Font descriptor */\n")
        f.write(f"const ui_font_t font_{font_name} = {{\n")
        f.write(f"    .glyphs = {font_name}_glyphs,\n")
        f.write(f"    .glyph_count = {len(all_glyphs)},\n")
        f.write(f"    .height = {line_height},\n")
        f.write(f"    .baseline = {baseline_from_top},\n")
        f.write(f"    .bpp = 1,\n")
        f.write(f"}};\n")
    
    # Generate .h file
    with open(h_file, 'w') as f:
        f.write(f"#ifndef __FONT_{font_name.upper()}_H\n")
        f.write(f"#define __FONT_{font_name.upper()}_H\n\n")
        f.write(f"#ifdef __cplusplus\n")
        f.write(f"extern \"C\" {{\n")
        f.write(f"#endif\n\n")
        f.write(f"#include \"../miniui_types.h\"\n\n")
        f.write(f"/* Font {font_size}px declaration */\n")
        f.write(f"extern const ui_font_t font_{font_name};\n\n")
        f.write(f"#ifdef __cplusplus\n")
        f.write(f"}}\n")
        f.write(f"#endif\n\n")
        f.write(f"#endif /* __FONT_{font_name.upper()}_H */\n")
    
    print(f"Generated MiniUI font: {c_file} ({len(all_glyphs)} glyphs)")


def generate_icon_bitmaps(glyphs: List[GlyphInfo], font_size: int, output_path: str):
    """Generate icon bitmap arrays for FontAwesome symbols"""
    # Filter icon glyphs (unicode >= 0xF000)
    icon_glyphs = [g for g in glyphs if g.unicode >= 0xF000]
    
    if not icon_glyphs:
        print("No icon glyphs found")
        return
    
    icon_base = f"ui_icons_{font_size}"
    c_file = os.path.join(output_path, f"{icon_base}.c")
    h_file = os.path.join(output_path, f"{icon_base}.h")
    
    # Icon name mapping (from lv_symbol_def.h)
    # Maps unicode codepoint to readable name
    icon_names = {
        0xF001: "AUDIO",
        0xF008: "VIDEO",
        0xF00B: "LIST",
        0xF00C: "OK",
        0xF00D: "CLOSE",
        0xF011: "POWER",
        0xF013: "SETTINGS",
        0xF015: "HOME",
        0xF019: "DOWNLOAD",
        0xF01C: "DRIVE",
        0xF021: "REFRESH",
        0xF026: "MUTE",
        0xF027: "VOLUME_MID",
        0xF028: "VOLUME_MAX",
        0xF03E: "IMAGE",
        0xF043: "TINT",
        0xF048: "PREV",
        0xF04B: "PLAY",
        0xF04C: "PAUSE",
        0xF04D: "STOP",
        0xF051: "NEXT",
        0xF052: "EJECT",
        0xF053: "LEFT",
        0xF054: "RIGHT",
        0xF067: "PLUS",
        0xF068: "MINUS",
        0xF06E: "EYE_OPEN",
        0xF070: "EYE_CLOSE",
        0xF071: "WARNING",
        0xF074: "SHUFFLE",
        0xF077: "UP",
        0xF078: "DOWN",
        0xF079: "LOOP",
        0xF07B: "DIRECTORY",
        0xF093: "UPLOAD",
        0xF095: "CALL",
        0xF0C4: "CUT",
        0xF0C5: "COPY",
        0xF0C7: "SAVE",
        0xF0C9: "BARS",
        0xF0E0: "ENVELOPE",
        0xF0E7: "CHARGE",
        0xF0EA: "PASTE",
        0xF0F3: "BELL",
        0xF11C: "KEYBOARD",
        0xF124: "GPS",
        0xF158: "FILE",
        0xF1EB: "WIFI",
        0xF240: "BATTERY_FULL",
        0xF241: "BATTERY_3",
        0xF242: "BATTERY_2",
        0xF243: "BATTERY_1",
        0xF244: "BATTERY_EMPTY",
        0xF287: "USB",
        0xF293: "BLUETOOTH",
        0xF2ED: "TRASH",
        0xF304: "EDIT",
        0xF55A: "BACKSPACE",
        0xF7C2: "SD_CARD",
        0xF8A2: "NEW_LINE",
    }
    
    # Generate .c file
    with open(c_file, 'w') as f:
        f.write("/********************************** (C) COPYRIGHT *******************************\n")
        f.write("* File Name          : ui_icons.c\n")
        f.write(f"* Description        : FontAwesome icon bitmaps {font_size}px. 1bpp format.\n")
        f.write("********************************************************************************/\n")
        f.write(f'#include "ui_icons_{font_size}.h"\n\n')
        
        for glyph in icon_glyphs:
            name = icon_names.get(glyph.unicode, f"U{glyph.unicode:04X}")
            
            if glyph.width == 0 or glyph.height == 0:
                continue
            
            # Convert 4bpp to 1bpp (row-aligned for ui_draw_icon)
            bitmap_1bpp = convert_4bpp_to_1bpp_row_aligned(glyph.bitmap, glyph.width, glyph.height)
            
            if not bitmap_1bpp:
                continue
            
            f.write(f"/* {name} ({glyph.width}x{glyph.height}) */\n")
            f.write(f"const uint8_t icon_{name.lower()}_{font_size}_bitmap[] = {{\n    ")
            
            for i, b in enumerate(bitmap_1bpp):
                if i > 0 and i % 12 == 0:
                    f.write("\n    ")
                f.write(f"0x{b:02X}, ")
            
            f.write("\n};\n\n")
    
    # Generate .h file
    with open(h_file, 'w') as f:
        f.write(f"#ifndef __UI_ICONS_{font_size}_H\n")
        f.write(f"#define __UI_ICONS_{font_size}_H\n\n")
        f.write("#ifdef __cplusplus\n")
        f.write("extern \"C\" {\n")
        f.write("#endif\n\n")
        f.write("#include <stdint.h>\n\n")
        
        for glyph in icon_glyphs:
            name = icon_names.get(glyph.unicode, f"U{glyph.unicode:04X}")
            f.write(f"/* {name} icon bitmap */\n")
            f.write(f"extern const uint8_t icon_{name.lower()}_{font_size}_bitmap[];\n")
            f.write(f"#define ICON_{name}_{font_size}_WIDTH  {glyph.width}\n")
            f.write(f"#define ICON_{name}_{font_size}_HEIGHT {glyph.height}\n\n")
        
        f.write("#ifdef __cplusplus\n")
        f.write("}\n")
        f.write("#endif\n\n")
        f.write(f"#endif /* __UI_ICONS_{font_size}_H */\n")
    
    print(f"Generated icon bitmaps: {c_file} ({len(icon_glyphs)} icons)")


def main():
    parser = argparse.ArgumentParser(description='Extract LVGL font to MiniUI format')
    parser.add_argument('--input', required=True, help='Input LVGL font .c file')
    parser.add_argument('--output-dir', required=True, help='Output directory for MiniUI font files')
    parser.add_argument('--size', type=int, required=True, help='Font size (12 or 16)')
    
    args = parser.parse_args()
    
    # Read input file
    with open(args.input, 'r', encoding='utf-8') as f:
        content = f.read()
    
    print(f"Parsing {args.input}...")
    
    # Parse LVGL font data
    bitmap = parse_glyph_bitmap(content)
    print(f"  Bitmap size: {len(bitmap)} bytes")
    
    glyph_dsc = parse_glyph_dsc(content)
    print(f"  Glyph descriptors: {len(glyph_dsc)}")
    
    cmaps = parse_cmaps(content)
    print(f"  CMAP entries: {len(cmaps)}")
    
    metrics = parse_font_metrics(content)
    print(f"  Line height: {metrics['line_height']}, Base line: {metrics['base_line']}, Baseline from top: {metrics['baseline_from_top']}")
    
    # Build glyph table
    glyphs = build_glyph_table(bitmap, glyph_dsc, cmaps, metrics['baseline_from_top'])
    print(f"  Total glyphs: {len(glyphs)}")
    
    # Categorize
    ascii_count = len([g for g in glyphs if 32 <= g.unicode <= 126])
    icon_count = len([g for g in glyphs if g.unicode >= 0xF000])
    print(f"  ASCII glyphs: {ascii_count}")
    print(f"  Icon glyphs: {icon_count}")
    
    # Generate output
    font_name = f"montserrat_{args.size}"
    
    os.makedirs(args.output_dir, exist_ok=True)
    
    # Generate MiniUI font (ASCII only)
    generate_miniui_font(glyphs, args.size, args.output_dir, font_name, metrics['line_height'], metrics['baseline_from_top'])
    
    # Generate icon bitmaps
    generate_icon_bitmaps(glyphs, args.size, args.output_dir)
    
    print("\nDone!")


if __name__ == '__main__':
    main()
