#!/usr/bin/env python3
"""
SVG Cleanup Script for IDEF0 Diagrams from Legacy Tools

Fixes common issues in SVG exports from older programs:
1. Adds proper XML/SVG declarations and namespaces
2. Removes duplicate IDs and makes them unique
3. Fixes nested anchor elements (invalid structure)
4. Removes invalid/broken xlink:href references
5. Fixes polygon elements inside removed anchors
6. Cleans up whitespace and formatting
"""

import os
import re
import sys
from pathlib import Path


def fix_svg(content, filename):
    """Fix SVG content."""
    
    # 1. Add proper XML and SVG declarations if missing
    if not content.strip().startswith('<?xml'):
        content = '<?xml version="1.0" encoding="UTF-8"?>\n' + content
    
    # 2. Ensure proper SVG namespace
    if 'xmlns=' not in content:
        content = re.sub(
            r'<svg\s+',
            '<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" ',
            content
        )
    
    # 3. Fix nested anchor elements (a inside a) - remove inner ones, keep outer
    # Pattern: <a ...>...<a ...>...<polygon .../></a>...</a>
    # We need to extract the polygons and keep them but remove the inner <a> wrapper
    lines = content.split('\n')
    fixed_lines = []
    in_anchor = False
    skip_anchor_close = False
    
    for line in lines:
        # Check if this is an opening anchor tag
        if re.match(r'\s*<a\s+xlink:href=', line):
            if in_anchor:
                # Skip this inner opening anchor tag
                skip_anchor_close = True
                continue
            else:
                in_anchor = True
                fixed_lines.append(line)
        # Check if this is a closing anchor tag
        elif line.strip() == '</a>':
            if skip_anchor_close:
                skip_anchor_close = False
                continue
            else:
                in_anchor = False
                fixed_lines.append(line)
        else:
            fixed_lines.append(line)
    
    content = '\n'.join(fixed_lines)
    
    # 4. Convert xlink:href to href - for HTML/SVG compatibility and navigation
    # xlink:href on <a> tags is not valid per SVG spec; use standard href instead
    # This preserves the navigation links while maintaining XML/SVG validity
    content = re.sub(
        r'<a\s+xlink:href="([^"]*)"([^>]*)>',
        r'<a href="\1"\2>',
        content
    )
    
    # 5. Fix duplicate IDs - make them unique by adding a counter suffix
    id_counts = {}
    def make_unique_id(match):
        old_id = match.group(1)
        if old_id not in id_counts:
            id_counts[old_id] = 0
        else:
            id_counts[old_id] += 1
        
        if id_counts[old_id] == 0:
            return match.group(0)  # First occurrence keeps original
        else:
            new_id = f"{old_id}_{id_counts[old_id]}"
            return match.group(0).replace(f'id="{old_id}"', f'id="{new_id}"')
    
    # Process line by line to track duplicates
    lines = content.split('\n')
    id_map = {}
    fixed_lines = []
    
    for line in lines:
        # Extract id attribute
        id_match = re.search(r'id="([^"]+)"', line)
        if id_match:
            old_id = id_match.group(1)
            if old_id not in id_map:
                id_map[old_id] = 0
                fixed_lines.append(line)
            else:
                id_map[old_id] += 1
                new_id = f"{old_id}_{id_map[old_id]}"
                new_line = line.replace(f'id="{old_id}"', f'id="{new_id}"')
                fixed_lines.append(new_line)
        else:
            fixed_lines.append(line)
    
    content = '\n'.join(fixed_lines)
    
    # 6. Format with proper line breaks for readability
    # Add newlines after main closing tags
    content = re.sub(r'(</g>|</a>|</svg>)', r'\1\n', content)
    content = re.sub(r'(<g\s+id=)', r'\n\1', content)
    
    # 7. Clean up extra whitespace inside tags
    content = re.sub(r'>\s+<', '><', content)
    content = re.sub(r'\s+text-anchor\s+=\s+"middle"', ' text-anchor="middle"', content)
    content = re.sub(r'\s+font-family="[^"]*"', ' font-family="Arial, sans-serif"', content)
    content = re.sub(r'\s+font-size="[^"]*"', ' font-size="11"', content)
    content = re.sub(r'\s+fill\s+="[^"]*"', ' fill="rgb(0, 0, 0)"', content)
    
    # 7. Remove trailing whitespace
    content = '\n'.join(line.rstrip() for line in content.split('\n'))
    
    # 8. Ensure final newline
    if not content.endswith('\n'):
        content += '\n'
    
    return content


def process_directory(directory):
    """Process all SVG files in a directory."""
    svg_dir = Path(directory)
    
    if not svg_dir.exists():
        print(f"Directory not found: {directory}")
        return
    
    svg_files = list(svg_dir.glob('*.svg'))
    
    if not svg_files:
        print(f"No SVG files found in {directory}")
        return
    
    print(f"Found {len(svg_files)} SVG files to process\n")
    
    for svg_file in sorted(svg_files):
        print(f"Processing: {svg_file.name}")
        
        try:
            # Read original
            with open(svg_file, 'r', encoding='iso-8859-1') as f:
                original_content = f.read()
            
            # Fix
            fixed_content = fix_svg(original_content, svg_file.name)
            
            # Write back
            with open(svg_file, 'w', encoding='utf-8') as f:
                f.write(fixed_content)
            
            # Report changes
            orig_lines = len(original_content.split('\n'))
            fixed_lines = len(fixed_content.split('\n'))
            
            print(f"  ✓ Fixed")
            if orig_lines != fixed_lines:
                print(f"    Lines: {orig_lines} → {fixed_lines}")
            print()
            
        except Exception as e:
            print(f"  ✗ Error: {e}\n")


if __name__ == '__main__':
    # Look for diagrams directory at repo root or scripts parent
    diagram_dir = Path(__file__).parent.parent / 'diagrams'
    if not diagram_dir.exists():
        diagram_dir = Path('diagrams')
    process_directory(diagram_dir)
    print("✓ SVG cleanup complete!")
