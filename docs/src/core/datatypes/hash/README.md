# Hash datatype folder

## hashobj.c
- createHash(): allocates a hash object.
- freeHash(): releases it.
- hset(): writes a field/value pair.
- hget(): reads a field value.

## Workflow
Hash commands work through the store and object layer to maintain field-based key/value data.
