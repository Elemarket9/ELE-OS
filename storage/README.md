# ELE-OS private record store

Development-image native CLI for bounded caller-owned records:

```sh
mkdir -m 700 /path/to/private-directory
printf 'example' | ele-store put /path/to/private-directory record
ele-store verify /path/to/private-directory record
ele-store get /path/to/private-directory record
```

A record is an eight-byte magic, 64-bit big-endian payload length, SHA-256
digest and at most 16 MiB of payload. No data is emitted before verification.
A successful write fsyncs a private temporary file, atomically renames it and
fsyncs the directory. Exit 3 means the rename happened but directory durability
is uncertain; do not blindly retry non-idempotent higher-level transactions.
Concurrent writers are last-rename-wins, not compare-and-swap. Abrupt death before
rename can leave an uncommitted temporary file; a production service needs
authorized orphan cleanup. No public or shared directory access, root privileges,
encryption, authenticated manifest, kernel filesystem replacement or power-cut
test is claimed. SHA-256 is corruption detection only, not authentication against
someone able to replace both data and digest. O(n) memory and I/O, bounded to
16 MiB per command. The debug CLI is not a backup or secrets vault.
