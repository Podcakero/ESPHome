esphome $1 `find devices/ -name *.yaml -not -path .esphome -not -name base.yaml -not -name secrets.yaml -not -name arduino-cli.yaml | tr '\n' ' '`
