#!/bin/bash

# Script to fix EMFILE: too many open files error on macOS

echo "🔧 Fixing file descriptor limits for React Native development..."

# Check current limits
echo ""
echo "Current file descriptor limits:"
echo "Soft limit: $(ulimit -n)"
echo "Hard limit: $(ulimit -Hn)"

# Increase the limit for the current session
ulimit -n 10000

echo ""
echo "✅ Increased file limit to: $(ulimit -n)"

# Create or update launchd configuration for permanent fix
if [ ! -f ~/Library/LaunchAgents/limit.maxfiles.plist ]; then
    echo ""
    echo "📝 Creating permanent configuration..."
    
    mkdir -p ~/Library/LaunchAgents
    
    cat > ~/Library/LaunchAgents/limit.maxfiles.plist << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>limit.maxfiles</string>
    <key>ProgramArguments</key>
    <array>
        <string>launchctl</string>
        <string>limit</string>
        <string>maxfiles</string>
        <string>65536</string>
        <string>524288</string>
    </array>
    <key>RunAtLoad</key>
    <true/>
</dict>
</plist>
EOF
    
    echo "✅ Created permanent configuration at ~/Library/LaunchAgents/limit.maxfiles.plist"
    echo ""
    echo "To apply permanently, run:"
    echo "  launchctl load -w ~/Library/LaunchAgents/limit.maxfiles.plist"
    echo ""
    echo "Then restart your terminal or reboot your system."
else
    echo ""
    echo "ℹ️  Permanent configuration already exists at ~/Library/LaunchAgents/limit.maxfiles.plist"
fi

echo ""
echo "🎉 Done! You can now run Metro bundler with:"
echo "  npm start -- --reset-cache"
echo ""
echo "If you still encounter issues:"
echo "  1. Clear Metro cache: npx react-native start --reset-cache"
echo "  2. Clear watchman: watchman watch-del-all"
echo "  3. Restart your terminal"
