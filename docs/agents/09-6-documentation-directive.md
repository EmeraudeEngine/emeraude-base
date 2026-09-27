## 6. Documentation directive (doc-first)

> [!IMPORTANT]
> AI documentation maintenance is **as important as the code**. Every change ships
> with its doc update in the same session. After modifying a module, update its
> `AGENTS.md` and any affected `docs/` file, and tell the user what changed.

- Each module gets an `src/<Module>/AGENTS.md` as it is migrated/created.
- Cross-cutting topics live in `docs/`.
- The doc index is [`docs/ai_documentation_map.md`](../ai_documentation_map.md).

> **Since 2026-09-27 every `AGENTS.md` is a ROUTER**: "update the AGENTS.md" means update the `docs/` file the
> router points to (`docs/agents/`, `docs/subsystems/<area>/`, or a topic file) and add a router row only when a
> docs file is created. Knowledge never goes back into an `AGENTS.md`.
