## 6c. Open work — `docs/todo/`, one file per idea

> [!IMPORTANT]
> **emeraude-base never had a root `TODO.md`. Open work lives in `docs/todo/`, one file per idea to do.**
> `docs/plans/` keeps its role: design and governance documents (the "Ave robustus!" plan, the
> TLS gating PoC) are KNOWLEDGE and stay there — only their open follow-ups become todo items. This is a project-wide rule (projet-alpha,
> emeraude-engine, emeraude-base, and every other project) — mirrored in the consumer's
> `.claude/rules/todo-items.md` (this repository carries no `.claude/` directory).
>
> - **One file = one idea.** Never a list of unrelated items in one file.
> - **Done = file deleted.** No `[x]`, no "DONE" section, no `done/` archive.
> - **The knowledge does not live there**: measurements, traps and owner decisions that must
>   survive the work go to `docs/caution-points.md`, the `docs/` topic files and the `AGENTS.md`
>   network — write them there **before** deleting the item file.
> - **Placement follows the dependency direction**: this repository is the bottom of the cascade,
>   so anything that must change here gets its item here, whoever noticed it.
> - Every file carries the YAML front-matter (`id`, `title`, `status`, `priority`, `scope`,
>   `opened`, optional `blocked-by`/`tags`) defined in
>   [`docs/todo/README.md`](../todo/README.md).
> - An inherited item that is ambiguous is **asked about**, never guessed: it may be done,
>   abandoned, or in need of an explanation that belongs in its file.
