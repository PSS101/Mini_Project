# Commands folder

## dispatcher.c
- execute(): routes a parsed command to the correct execution path.

## help.c
- getHelpGeneral(): returns the top-level help overview.
- getHelpCategory(): shows help for one command category.
- getHelpCommand(): shows usage details for a single command.

## parser.c
- parse(): tokenizes raw input and creates a structured command.

## result.c
- ok(): builds a successful response.
- error(): builds an error response.
- freeResult(): frees response memory.

## Workflow
A raw request moves from text input to a parsed command, then to the engine, and finally becomes a reply object.
