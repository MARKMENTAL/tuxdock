#!/bin/bash
echo "Setting up application directory"
mkdir -p /app/bin
echo '#!/bin/bash' > /app/bin/run.sh
echo 'echo "Hello from Tux-Dock generated image"' >> /app/bin/run.sh
chmod +x /app/bin/run.sh
