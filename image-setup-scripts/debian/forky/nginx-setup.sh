#!/usr/bin/env bash

# Exit immediately if a command exits with a non-zero status
set -e

# Ensure the script is run as root
if [ "$EUID" -ne 0 ]; then
  echo "Error: Please run this script as root or with sudo." >&2
  exit 1
fi

echo "=== 1. Updating System Repositories ==="
apt update && apt upgrade -y

echo "=== 2. Installing Nginx and PHP 8.4 ==="
apt install -y nginx php8.4-fpm php8.4-cli php8.4-common

echo "=== 3. Adjusting PHP-FPM Timeouts ==="
PHP_INI="/etc/php/8.4/fpm/php.ini"
if [ -f "$PHP_INI" ]; then
    sed -i 's/^max_execution_time =.*/max_execution_time = 60/' "$PHP_INI"
    echo "Set max_execution_time = 60 in $PHP_INI"
else
    echo "Warning: $PHP_INI not found, skipping inline adjustment."
fi

echo "=== 4. Configuring Nginx Server Block ==="
cat << 'EOF' > /etc/nginx/sites-available/default
server {
    listen 80 default_server;
    listen [::]:80 default_server;

    root /var/www/html;
    index index.php index.html index.htm;

    server_name _;

    location / {
        try_files $uri $uri/ =404;
    }

    location ~ \.php$ {
        include snippets/fastcgi-php.conf;
        fastcgi_pass unix:/run/php/php8.4-fpm.sock;
        
        # Prevent Nginx from cutting off the connection prematurely
        fastcgi_read_timeout 60s;
    }

    location ~ /\.ht {
        deny all;
    }
}
EOF

echo "=== 5. Creating the 30-Second Sleep Script ==="
cat << 'EOF' > /var/www/html/sleep.php
<?php
ob_end_clean();
header('Content-Type: text/plain');
header('Cache-Control: no-cache');

echo "Test started at: " . date('H:i:s') . "\n";
echo "Sleeping for 30 seconds...\n";
flush();

sleep(30);

echo "Test finished at: " . date('H:i:s') . "\n";
?>
EOF

# Fixed typo: Added missing comment symbol here
chown -R www-data:www-data /var/www/html/sleep.php

echo "=== 6. Validating Configuration ==="
nginx -t

echo "=== 7. Starting Services via /etc/init.d/ ==="
# Stop any existing processes safely before refreshing
/etc/init.d/nginx stop || true
/etc/init.d/php8.4-fpm stop || true

# Start PHP-FPM first to ensure the Unix domain socket is generated
/etc/init.d/php8.4-fpm start
/etc/init.d/nginx start

echo "====================================================="
echo " Installation Complete!"
echo " Test the setup via CLI using:"
echo " time curl -i http://localhost/sleep.php"
echo "====================================================="

