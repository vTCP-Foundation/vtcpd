#!/usr/bin/env python3
"""
Script to identify and help fix SQLite memory leaks in the vtcpd codebase.

This script finds files where multiple sqlite3_prepare_v2 calls are made
but statements are not properly finalized after each use.
"""

import os
import re
import sys
from pathlib import Path

def find_sqlite_files():
    """Find all C++ files that use sqlite3_prepare_v2"""
    sqlite_files = []
    
    for root, dirs, files in os.walk("src"):
        for file in files:
            if file.endswith(('.cpp', '.h')):
                filepath = os.path.join(root, file)
                try:
                    with open(filepath, 'r', encoding='utf-8') as f:
                        content = f.read()
                        if 'sqlite3_prepare_v2' in content:
                            sqlite_files.append(filepath)
                except Exception as e:
                    print(f"Error reading {filepath}: {e}")
    
    return sqlite_files

def analyze_file(filepath):
    """Analyze a file for potential SQLite memory leaks"""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
    except Exception as e:
        print(f"Error reading {filepath}: {e}")
        return None
    
    # Find all sqlite3_prepare_v2 calls
    prepare_calls = re.findall(r'sqlite3_prepare_v2\s*\([^)]+\)', content)
    
    # Find all sqlite3_finalize calls
    finalize_calls = re.findall(r'sqlite3_finalize\s*\([^)]+\)', content)
    
    # Look for constructors with multiple prepare calls
    constructor_pattern = r'(\w+::\w+\s*\([^{]*\)\s*:[^{]*\{[^}]*(?:\{[^}]*\}[^}]*)*\})'
    constructors = re.findall(constructor_pattern, content, re.DOTALL)
    
    issues = []
    
    for constructor in constructors:
        constructor_prepares = len(re.findall(r'sqlite3_prepare_v2', constructor))
        constructor_finalizes = len(re.findall(r'sqlite3_finalize', constructor))
        
        if constructor_prepares > 1 and constructor_finalizes < constructor_prepares:
            issues.append({
                'type': 'constructor_leak',
                'prepares': constructor_prepares,
                'finalizes': constructor_finalizes,
                'missing': constructor_prepares - constructor_finalizes
            })
    
    return {
        'filepath': filepath,
        'total_prepares': len(prepare_calls),
        'total_finalizes': len(finalize_calls),
        'issues': issues
    }

def main():
    print("SQLite Memory Leak Detector for vtcpd")
    print("=" * 50)
    
    sqlite_files = find_sqlite_files()
    print(f"Found {len(sqlite_files)} files using SQLite")
    
    problematic_files = []
    
    for filepath in sqlite_files:
        analysis = analyze_file(filepath)
        if analysis and analysis['issues']:
            problematic_files.append(analysis)
    
    if not problematic_files:
        print("\n✅ No obvious SQLite memory leaks found!")
        return
    
    print(f"\n⚠️  Found {len(problematic_files)} files with potential memory leaks:")
    print()
    
    for analysis in problematic_files:
        print(f"📁 {analysis['filepath']}")
        for issue in analysis['issues']:
            if issue['type'] == 'constructor_leak':
                print(f"   🔴 Constructor: {issue['prepares']} prepare calls, {issue['finalizes']} finalize calls")
                print(f"      Missing {issue['missing']} finalize call(s)")
        print()
    
    print("🔧 RECOMMENDED FIXES:")
    print("=" * 50)
    print("For each file above, ensure that:")
    print("1. Every sqlite3_prepare_v2() call is followed by sqlite3_finalize()")
    print("2. Add sqlite3_finalize(stmt) in error paths before throwing exceptions")
    print("3. Add sqlite3_finalize(stmt) after successful sqlite3_step() execution")
    print()
    print("Example fix pattern:")
    print("```cpp")
    print("rc = sqlite3_step(stmt);")
    print("if (rc == SQLITE_DONE) {")
    print("} else {")
    print("    sqlite3_finalize(stmt);  // Add this")
    print("    throw IOError(...);")
    print("}")
    print("sqlite3_finalize(stmt);      // Add this")
    print("```")

if __name__ == "__main__":
    main() 