# Loading source data

Use Load Source to choose any local file. thrystr reads the file lazily in
bounded blocks so large inputs can be inspected without loading every byte
into memory.

The initial view starts at the first loaded window. Phase fitting is deferred
for large files until you explicitly request it.
