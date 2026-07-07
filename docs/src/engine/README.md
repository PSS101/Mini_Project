# Engine folder

## engine.c
- init_engine(): sets up the engine state.
- destroy_engine(): cleans it up.
- run_command(): executes a parsed command and returns a result.

## Workflow
The engine is the central dispatcher that applies commands to the store and persistence subsystem.
