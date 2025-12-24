#!/usr/bin/env python3
"""
Regenerate SVG diagrams with hex codes (dgm prefix) for intelligent hot-linking.

Maps human-readable filenames to their hex code equivalents and updates all
xlink:href references to use the hex code names. This enables diagrams to
hot-link to each other using Ai0Win's original naming convention.

Mapping:
  dgm24    = Project Concerns.svg (PRJ/A0 context)
  dgm333   = Decomposition of SSK Concerns.svg (PRJ/A0 decomposition)
  dgm824   = Implementation Concerns.svg (IMP/A0 context)
  dgm1305  = A0_ The SSK Extension.svg (IMP/A0 decomposition)
  dgm1320  = A1_ Value Decoder.svg (IMP/A1)
  dgm1382  = A2_ Function Processor.svg (IMP/A2)
  dgm1450  = A3_ Value Encoder.svg (IMP/A3)
"""

import os
import re
from pathlib import Path


# Mapping from hex codes to both human names and actual SVG filenames
DIAGRAM_MAPPING = {
    'dgm24': {
        'human_name': 'Project Concerns.svg',
        'id': '24'
    },
    'dgm333': {
        'human_name': 'Decomposition of SSK Concerns.svg',
        'id': '333'
    },
    'dgm824': {
        'human_name': 'Implementation Concerns.svg',
        'id': '824'
    },
    'dgm1305': {
        'human_name': 'A0_ The SSK Extension.svg',
        'id': '1305'
    },
    'dgm1320': {
        'human_name': 'A1_ Value Decoder.svg',
        'id': '1320'
    },
    'dgm1382': {
        'human_name': 'A2_ Function Processor.svg',
        'id': '1382'
    },
    'dgm1450': {
        'human_name': 'A3_ Value Encoder.svg',
        'id': '1450'
    }
}


def convert_href_to_hex(content):
    """Convert human-readable xlink:href values to hex code equivalents."""
    
    # Map human names to hex codes
    replacements = {
        'A0_ The SSK Extension.svg': 'dgm1305.svg',
        'A1_ Value Decoder.svg': 'dgm1320.svg',
        'A2_ Function Processor.svg': 'dgm1382.svg',
        'A3_ Value Encoder.svg': 'dgm1450.svg',
        'Project Concerns.svg': 'dgm24.svg',
        'Implementation Concerns.svg': 'dgm824.svg',
        'Decomposition of SSK Concerns.svg': 'dgm333.svg',
    }
    
    for human_name, hex_code in replacements.items():
        content = content.replace(f'xlink:href="{human_name}"', f'xlink:href="{hex_code}"')
    
    return content


def process_directory(diagram_dir):
    """Process all cleaned SVG files and create hex-code versions."""
    
    diagram_path = Path(diagram_dir)
    if not diagram_path.exists():
        print(f"Directory not found: {diagram_dir}")
        return
    
    print("Regenerating diagrams with hex code naming and hot-links...\n")
    
    for hex_code, info in DIAGRAM_MAPPING.items():
        human_name = info['human_name']
        source_file = diagram_path / human_name
        hex_file = diagram_path / f"{hex_code}.svg"
        
        if not source_file.exists():
            print(f"⚠ Missing source: {human_name}")
            continue
        
        print(f"Processing: {human_name}")
        print(f"  → {hex_code}.svg")
        
        try:
            # Read cleaned version
            with open(source_file, 'r', encoding='utf-8') as f:
                content = f.read()
            
            # Convert href references to hex codes
            content = convert_href_to_hex(content)
            
            # Ensure SVG id matches the hex code number
            # Update the SVG root id if needed
            svg_id = info['id']
            content = re.sub(
                r'<svg[^>]*id="[^"]*"',
                lambda m: m.group(0).replace(f'id="{m.group(0).split("id=\"")[1].split("\"")[0]}"', f'id="{svg_id}"'),
                content
            )
            
            # Write hex version
            with open(hex_file, 'w', encoding='utf-8') as f:
                f.write(content)
            
            print(f"  ✓ Created {hex_code}.svg")
            print()
            
        except Exception as e:
            print(f"  ✗ Error: {e}\n")
    
    print("✓ Hex diagram regeneration complete!")
    print("\nHot-link structure enabled:")
    print("  dgm24.svg (Project Concerns)")
    print("    → links to dgm333.svg (Decomposition)")
    print("  dgm824.svg (Implementation Concerns)")
    print("    → links to dgm1305.svg (A0_ The SSK Extension)")
    print("      → links to dgm1320, dgm1382, dgm1450 (A1, A2, A3)")


if __name__ == '__main__':
    diagram_dir = Path(__file__).parent.parent / 'diagrams'
    if not diagram_dir.exists():
        diagram_dir = Path('diagrams')
    
    process_directory(diagram_dir)
