esphome $1 `find ${2:-*/devices} -name "*.yaml" -not -path "*/generic/*" -not -path "*.esphome*" -not -name base.yaml -not -name secrets.yaml -not -name arduino-cli.yaml | tr '\n' ' '`
