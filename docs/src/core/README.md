# Core folder

## object.c
- createStringObj(), createListObj(), createSetObj(), createHashObj(), createZSetObj(): construct typed objects.
- freeObject(): clears an object and its owned memory.

## store.c
- createStore(): allocates the in-memory store.
- freeStore(): destroys it.
- setKey(), getKey(), deleteKey(): manage key/value entries.
- setExpire(), ttlKey(), persistKey(): manage expiration behavior.
- activeExpiryCycle(): removes expired keys.

## Workflow
The core layer owns the data model and the in-memory database that the engine mutates during command execution.
