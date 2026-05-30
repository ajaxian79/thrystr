# Wave settings files

Wave settings files use the `.thryw` extension. Current writers emit portable
little-endian v5 files with an endian stamp.

Older v1, v2, v3, and v4 files still load. Older versions are upgraded in
memory to the current workspace representation.
