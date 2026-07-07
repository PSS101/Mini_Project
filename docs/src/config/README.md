# Config folder

## config.c
- config_defaults(): fills in default settings.
- parse_bool(): converts text booleans into runtime values.
- trim(): removes whitespace from configuration values.
- config_load(): reads the config file and populates the runtime settings.
- config_print(): prints the effective configuration for debugging.

## Workflow
Configuration is loaded once during startup and then used by server and persistence behavior.
