# UXOXO — a subfiles monograph

`uxoxo-main.tex` is the umbrella. Each chapter lives in `chapters/` as its own
**compilable** document (via the `subfiles` package), so you can build the whole
book or any single chapter on its own.

## Build

```bash
# whole book (run 2–3× for the TOC and cross-references to settle)
pdflatex uxoxo-main
pdflatex uxoxo-main
pdflatex uxoxo-main

# a single chapter, standalone — it borrows uxoxo-main's preamble
pdflatex chapters/elements-and-commands
pdflatex chapters/idealized-ui
```

## Layout

```
uxoxo-main.tex            umbrella: the shared preamble + \subfile{...} includes
chapters/
  elements-and-commands.tex   Chapter 1 — the UI–DSL correspondence (free/cofree)
  idealized-ui.tex            Chapter 2 — the expressible and idealized UI
make_chapters.py          regenerates chapters/ from the two source documents
```

## Conventions baked into `uxoxo-main.tex`

- **One shared preamble.** All macros, the semantic colour palette, the TikZ
  styles, and the theorem definitions live in `uxoxo-main.tex` only. A chapter
  file contains no preamble; it starts with `\documentclass[../uxoxo-main]{subfiles}`.
- **Theorems** number per chapter (Definition 1.3, Theorem 2.5, …) with correct
  `cleveref` names, all sharing one counter so the sequence is continuous.
- **Every `\section` starts on a fresh page** (a redefinition of `\section`).
- **Each section opens with a “New in this section.” box** listing the symbols
  and terms first defined there (the `\sectionnotation{…}` macro).
- **Part dividers** (`\uxpart{…}`) begin a fresh page and carry their first
  section onto it.

## Adding a chapter

1. Drop `chapters/foo.tex` with this skeleton:
   ```latex
   \documentclass[../uxoxo-main]{subfiles}
   \begin{document}
   \chapter{Title}
   ... body, using \section, \uxpart, \sectionnotation ...
   \end{document}
   ```
2. Add `\subfile{chapters/foo}` to `uxoxo-main.tex`.
3. Keep labels unique across chapters (prefix per chapter, e.g. `foo:thing`).
