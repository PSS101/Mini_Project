# Sorted-set datatype folder

## skiplist.c
- skiplistCreate(), skiplistFree(): manage the skip-list structure.
- skiplistInsert(), skiplistDelete(), skiplistFind(): mutate and query scores.
- skiplistGetRank(): reports the rank of a member.

## zset.c
- createZSet(): allocates a sorted set.
- zadd(): adds or updates a member score.
- zscore(), zrank(): read score and rank.
- zrange(), zrem(), zcard(): query or delete set contents.

## Workflow
Sorted-set commands provide ordered membership and range-based access.
