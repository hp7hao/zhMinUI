#!/bin/bash
# Build translation files for MinUI

LOCALE_DIR="$(dirname "$0")"
OUTPUT_BASE="${LOCALE_DIR}/../build/locale"

# Check if gettext tools are available
if ! command -v xgettext &> /dev/null || ! command -v msgfmt &> /dev/null; then
    echo "Warning: gettext tools not found, creating dummy translation files"
    echo "Install gettext to enable proper translations: apt-get install gettext"
    
    # Create dummy .mo files so the build doesn't fail
    for po_file in "${LOCALE_DIR}"/*.po; do
        if [ -f "$po_file" ]; then
            locale=$(basename "$po_file" .po)
            output_dir="${OUTPUT_BASE}/${locale}/LC_MESSAGES"
            
            echo "  Creating dummy $locale..."
            mkdir -p "$output_dir"
            touch "$output_dir/minui.mo"
        fi
    done
    exit 0
fi

echo "Extracting translatable strings..."

# Extract strings from all C files in the project
find "${LOCALE_DIR}/../.." -name "*.c" -type f | xargs xgettext --from-code=UTF-8 --keyword=_ --keyword=N_ -o "${LOCALE_DIR}/minui.pot" 2>/dev/null || true

echo "Updating translation files..."

# Update existing .po files with new strings from .pot
for po_file in "${LOCALE_DIR}"/*.po; do
    if [ -f "$po_file" ]; then
        locale=$(basename "$po_file" .po)
        echo "  Updating $locale..."
        msgmerge --update "$po_file" "${LOCALE_DIR}/minui.pot" 2>/dev/null || true
    fi
done

echo "Building translations..."

for po_file in "${LOCALE_DIR}"/*.po; do
    if [ -f "$po_file" ]; then
        locale=$(basename "$po_file" .po)
        output_dir="${OUTPUT_BASE}/${locale}/LC_MESSAGES"
        
        echo "  Building $locale..."
        mkdir -p "$output_dir"
        msgfmt "$po_file" -o "$output_dir/minui.mo" 2>/dev/null || true
    fi
done

echo "Done! Translation files created in ${OUTPUT_BASE}"

