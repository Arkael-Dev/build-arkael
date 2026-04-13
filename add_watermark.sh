#!/bin/bash
echo "🛡️ Installing Watermark..."
FILES=$(find . -type f \( -name "*.sh" -o -name "*.c" -o -name "*.h" -o -name "*.py" \) | grep -v ".git/")

for file in $FILES; do
  if [ -f "$file" ]; then
    if ! grep -q "Original Author: Kingfinik98" "$file"; then
      sed -i '1i\/*!\n * © 2024-2026 Kingfinik98 (VorteX_E-Sport). All Rights Reserved.\n * Original Author: Kingfinik98\n * Original Repository: https://github.com/Kingfinik98/build-vortex\n * Modifying or claiming this as your own (e.g., Generic Zixine) is prohibited.\n */\n' "$file"
      echo "✅ $file"
    else
      echo "⏭️  $file"
    fi
  fi
done
echo "🎉 Done!"
