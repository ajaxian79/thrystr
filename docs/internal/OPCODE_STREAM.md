# Documentation resource format

The in-house documentation generator emits two outputs:

- a static HTML site under `build/docs/site`
- generated C++ resources under `build/generated/docs`

The generated C++ resource is intentionally simple for the first in-app panel:
each page has a slug, title, Markdown body, and rendered HTML body. A future
opcode stream can be added beside those fields without changing the authored
Markdown source.
