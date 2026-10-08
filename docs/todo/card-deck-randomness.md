---
id: card-deck-randomness
title: CardDeck randomness: std engines instead of PortableRandom, and Randomly never inserts last
status: open
priority: high
scope: src/GameTools/CardDeck.cpp, src/GameTools/CardHand.hpp
opened: 2026-10-08
tags: [ave-robustus-ii, gametools, defect, determinism]
---

# CardDeck randomness: std engines instead of PortableRandom, and Randomly never inserts last

## Why
`CardDeck` / `CardHand` use `std::mt19937` + `std::uniform_int_distribution` + `std::shuffle`: implementation-defined
results, against the cascade rule (seeded randomness goes through `Base::PortableRandom` / `Randomizer`). Seeded from
`std::random_device` today, so the effect is limited — until someone seeds it for a replay.
`insert()` with `Where::Randomly` draws in [0, size − 1]: the slot after the last card is never chosen. Intended?

## What remains
- **Owner decision (2026-10-08): `Where::Randomly` INCLUDES the end slot** (draw in [0, size]).
- Move to `PortableRandom`; test.

## References
- Found by the Ave Robustus II warning pass (2026-10-08, projet-alpha `docs/plans/ave-robustus-ii.md`); not raised by a warning, so left for its own fix.
