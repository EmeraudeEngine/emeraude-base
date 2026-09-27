## 5. Conventions (project-wide)

- **Tabs** for indentation, never spaces.
- **Acronyms always uppercase** in identifiers: `TLAS`, `VBO`, `MD5`, `CRC32`.
- **No public data members** — getters/setters instead.
- **Booleans last** in class/struct member lists (avoids padding holes).
- **Always use braces**, even for single-line `if`/`for`/`while`.
- **Include layout** in `.hpp`: `/* Project config */ /* STL */ /* Third-party */ /* Local for inheritances */ /* Local for usages */`; in `.cpp` the self-header first. Empty sections are dropped (comment included).
- Use the library's own types everywhere (`EmEn::Base::Math::Vector`, `EmEn::Base::PixelFactory::Color`), not raw `float x,y,z`.
