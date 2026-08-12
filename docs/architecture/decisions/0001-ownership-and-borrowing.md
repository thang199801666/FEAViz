# ADR 0001: Explicit ownership and borrowed getters

Status: accepted

FEAViz uses atomic intrusive reference counting behind opaque C objects.
Creation/copy/retain APIs transfer an owned reference; ordinary getters and
output-port proxies are borrowed. Setters retain before releasing the old child
so self-assignment is safe. Weak back-references are used where strong ownership
would form a cycle. This keeps the ABI small and makes lifetime behavior usable
from C without importing a garbage collector.
