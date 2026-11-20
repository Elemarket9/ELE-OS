# Test key

The update prototype's Ed25519 seed is 32 bytes of `0x11`, in `ota/test_key.h`. `public.hex` is the raw 32-byte public key. Anyone can produce a signature that this key accepts.

No PEM file is stored here. The build contract refuses PEM files and private-key blocks in the tree. `ele-ota-sign` signs with the compiled seed and refuses every other key.

OS release keys are a different set. The owner generates those offline when a release build is prepared, and they are not stored in this repository.
