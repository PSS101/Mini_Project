# Store folder

## store.c
- createStore(): creates a new store instance.
- freeStore(): destroys the store and releases entries.
- setKey(), getKey(), deleteKey(): manage data in the store.
- storeGetEntry(): locates a specific entry.
- setExpire(), ttlKey(), persistKey(): manage TTL behavior.
- activeExpiryCycle(): removes expired items.
- type-specific helpers such as hsetKey(), zaddKey(), and lpushKey(): apply typed operations to the store.

## Workflow
The store receives requests from the engine and provides the direct memory-backed data access layer.
