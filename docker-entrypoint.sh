# If the config folder exists but is empty, then copy the sample config files into it.
#
# This allows users to use a Docker bind mount for /app/config without needing to provide initial configuration.
# https://docs.docker.com/engine/storage/bind-mounts/

if [ -z "$(ls -A '/app/config')" ]; then
    echo "/app/config is empty. Copying sample config from /app/config_sample..."
    cp -r /app/config_sample/. /app/config
fi

# Run akashi
./akashi
