# Tracks and parity

Tracks split ownership of source samples across multiple fitted wave sets. A
parity track routes each source index to the data track that owns it.

The v5 workspace format stores track metadata and optional owned masks after
the fitted section table. Single-track files omit the mask to avoid storing a
large all-owned bitset.
