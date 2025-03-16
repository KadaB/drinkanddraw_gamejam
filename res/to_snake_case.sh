#!/bin/bash

# Ordner festlegen (aktueller Ordner)
DIR="." 

# Alle Dateien im Ordner durchgehen
for FILE in "$DIR"/*; do
    # Prüfen, ob es eine Datei ist
    if [ -f "$FILE" ]; then
        # Neuen Namen erstellen (Kleinbuchstaben + Ersetzen von Leerzeichen)
        NEW_NAME=$(basename "$FILE" | tr '[:upper:]' '[:lower:]' | tr ' ' '_')
        
        # Falls der Name sich geändert hat, umbenennen
        if [ "$NEW_NAME" != "$(basename "$FILE")" ]; then
            mv "$FILE" "$DIR/$NEW_NAME"
            echo "Renamed: $FILE -> $DIR/$NEW_NAME"
        fi
    fi
done
