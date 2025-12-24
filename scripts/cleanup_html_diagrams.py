#!/usr/bin/env python3
"""
HTML Cleanup Script for Ai0Win-generated Diagram Documentation

Fixes common issues in HTML exports from legacy Ai0Win tool:
1. Modernizes to valid HTML5 structure
2. Fixes broken href links (URL-encoded characters)
3. Updates SVG embedding from deprecated <EMBED> to modern <object>
4. Removes outdated Adobe plugin references
5. Fixes file extension references (.htm → .html)
6. Preserves navigation structure while modernizing markup
"""

import os
import re
from pathlib import Path
from urllib.parse import unquote


def fix_html_links(content):
    """
    Fix URL-encoded characters in href attributes.
    
    Ai0Win encodes special characters: %5F=_, %28=(, %29=), %2F=/, etc.
    """
    # Decode URL-encoded characters in quoted href values
    def decode_quoted_href(match):
        href = match.group(1)
        decoded = unquote(href)
        return f'href="{decoded}"'
    
    # Decode URL-encoded characters in unquoted href values
    def decode_unquoted_href(match):
        href = match.group(1)
        decoded = unquote(href)
        return f'href="{decoded}"'
    
    # Match quoted href="..."
    content = re.sub(r'href="([^"]*%[0-9A-Fa-f]{2}[^"]*)"', decode_quoted_href, content)
    content = re.sub(r'HREF="([^"]*%[0-9A-Fa-f]{2}[^"]*)"', decode_quoted_href, content)
    
    # Match unquoted href=... (ends at space or > or newline)
    content = re.sub(r'href=([^"\s>]*%[0-9A-Fa-f]{2}[^"\s>]*)', decode_unquoted_href, content)
    content = re.sub(r'HREF=([^"\s>]*%[0-9A-Fa-f]{2}[^"\s>]*)', decode_unquoted_href, content)
    
    return content


def fix_svg_embedding(content):
    """
    Replace deprecated <EMBED> tags with modern <object> or <img>.
    Also handles mixed case tags.
    """
    # Pattern: <EMBED NAME="" SRC="dgm1305.svg" TYPE="image/svg+xml"...>
    # Replace with: <object data="dgm1305.svg" type="image/svg+xml" ...></object>
    
    def convert_embed_to_object(match):
        src_match = re.search(r'SRC="([^"]*)"', match.group(0), re.IGNORECASE)
        width_match = re.search(r'WIDTH="([^"]*)"', match.group(0), re.IGNORECASE)
        height_match = re.search(r'HEIGHT="([^"]*)"', match.group(0), re.IGNORECASE)
        
        src = src_match.group(1) if src_match else ""
        width = width_match.group(1) if width_match else "100%"
        height = height_match.group(1) if height_match else "600"
        
        return f'<object data="{src}" type="image/svg+xml" width="{width}" height="{height}"></object>'
    
    content = re.sub(r'<EMBED[^>]*>', convert_embed_to_object, content, flags=re.IGNORECASE)
    return content


def fix_html_structure(content):
    """
    Modernize HTML to valid HTML5.
    - Add DOCTYPE
    - Fix case of tags
    - Remove outdated attributes
    - Quote unquoted attributes
    """
    
    # First, quote unquoted href attributes
    content = re.sub(r'href=([^\s>"]+)', r'href="\1"', content)
    
    # Convert uppercase tags to lowercase
    content = re.sub(r'<HTML>', '<html>', content, flags=re.IGNORECASE)
    content = re.sub(r'</HTML>', '</html>', content, flags=re.IGNORECASE)
    content = re.sub(r'<HEAD>', '<head>', content, flags=re.IGNORECASE)
    content = re.sub(r'</HEAD>', '</head>', content, flags=re.IGNORECASE)
    content = re.sub(r'<BODY', '<body', content, flags=re.IGNORECASE)
    content = re.sub(r'</BODY>', '</body>', content, flags=re.IGNORECASE)
    content = re.sub(r'<TITLE>', '<title>', content, flags=re.IGNORECASE)
    content = re.sub(r'</TITLE>', '</title>', content, flags=re.IGNORECASE)
    content = re.sub(r'<SCRIPT', '<script', content, flags=re.IGNORECASE)
    content = re.sub(r'</SCRIPT>', '</script>', content, flags=re.IGNORECASE)
    content = re.sub(r'<H2>', '<h2>', content, flags=re.IGNORECASE)
    content = re.sub(r'</H2>', '</h2>', content, flags=re.IGNORECASE)
    content = re.sub(r'<P ', '<p ', content, flags=re.IGNORECASE)
    content = re.sub(r'<P>', '<p>', content, flags=re.IGNORECASE)
    content = re.sub(r'</P>', '</p>', content, flags=re.IGNORECASE)
    content = re.sub(r'<BR>', '<br>', content, flags=re.IGNORECASE)
    content = re.sub(r'<A ', '<a ', content, flags=re.IGNORECASE)
    content = re.sub(r'</A>', '</a>', content, flags=re.IGNORECASE)
    content = re.sub(r'<STYLE', '<style', content, flags=re.IGNORECASE)
    content = re.sub(r'</STYLE>', '</style>', content, flags=re.IGNORECASE)
    
    # Fix HREF to href
    content = re.sub(r'HREF=', 'href=', content, flags=re.IGNORECASE)
    
    # Remove LANGUAGE attribute from script tags (deprecated)
    content = re.sub(r'<script\s+LANGUAGE\s*=\s*"[^"]*"', '<script', content, flags=re.IGNORECASE)
    content = re.sub(r'<script\s+TYPE\s*=\s*"text/javascript"', '<script', content, flags=re.IGNORECASE)
    
    # Fix onload attribute (should be lowercase)
    content = re.sub(r'onload\s*=\s*(\w+)\(\)', r'onload="\1()"', content, flags=re.IGNORECASE)
    
    # Fix mixed case attributes
    content = re.sub(r'NAME=', 'name=', content, flags=re.IGNORECASE)
    content = re.sub(r'SRC=', 'src=', content, flags=re.IGNORECASE)
    content = re.sub(r'TYPE=', 'type=', content, flags=re.IGNORECASE)
    content = re.sub(r'SCROLLING=', 'scrolling=', content, flags=re.IGNORECASE)
    content = re.sub(r'WIDTH=', 'width=', content, flags=re.IGNORECASE)
    content = re.sub(r'HEIGHT=', 'height=', content, flags=re.IGNORECASE)
    content = re.sub(r'CLASS=', 'class=', content, flags=re.IGNORECASE)
    content = re.sub(r'ID=', 'id=', content, flags=re.IGNORECASE)
    
    return content


def fix_file_extensions(content):
    """Fix references to .htm files (should be .html)."""
    content = content.replace('.htm"', '.html"')
    content = content.replace(".htm'", ".html'")
    return content


def add_doctype_and_meta(content):
    """Add HTML5 doctype and meta tags if missing."""
    
    # Add doctype if missing
    if not content.strip().lower().startswith('<!doctype'):
        content = '<!DOCTYPE html>\n<html>\n<head>\n<meta charset="UTF-8">\n<meta name="viewport" content="width=device-width, initial-scale=1.0">\n' + content
        # Need to close properly
        if not content.strip().endswith('</html>'):
            content += '\n</html>'
    else:
        # Already has doctype, just ensure meta charset
        if '<meta charset' not in content.lower():
            # Add after <head>
            content = re.sub(
                r'(<head[^>]*>)',
                r'\1\n<meta charset="UTF-8">',
                content,
                flags=re.IGNORECASE
            )
    
    return content


def process_file(filepath):
    """Process a single HTML file."""
    
    try:
        with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
            content = f.read()
    except UnicodeDecodeError:
        # Try iso-8859-1 as fallback
        with open(filepath, 'r', encoding='iso-8859-1') as f:
            content = f.read()
    
    # Apply fixes in order
    content = fix_html_links(content)
    content = fix_svg_embedding(content)
    content = fix_html_structure(content)
    content = fix_file_extensions(content)
    content = add_doctype_and_meta(content)
    
    # Write back
    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(content)
    
    return True


def process_directory(directory):
    """Process all HTML files in a directory."""
    
    html_dir = Path(directory)
    if not html_dir.exists():
        print(f"Directory not found: {directory}")
        return
    
    html_files = sorted(html_dir.glob('*.html')) + sorted(html_dir.glob('*.htm'))
    
    if not html_files:
        print(f"No HTML files found in {directory}")
        return
    
    print(f"Found {len(html_files)} HTML files to process\n")
    
    success = 0
    failed = 0
    
    for html_file in html_files:
        print(f"Processing: {html_file.name}")
        
        try:
            if process_file(html_file):
                print(f"  ✓ Fixed\n")
                success += 1
            else:
                print(f"  ✗ Failed\n")
                failed += 1
        except Exception as e:
            print(f"  ✗ Error: {e}\n")
            failed += 1
    
    print(f"\n✓ HTML cleanup complete!")
    print(f"  Successfully processed: {success} files")
    if failed > 0:
        print(f"  Failed: {failed} files")


if __name__ == '__main__':
    diagram_dir = Path(__file__).parent.parent / 'diagrams'
    if not diagram_dir.exists():
        diagram_dir = Path('diagrams')
    
    process_directory(diagram_dir)
