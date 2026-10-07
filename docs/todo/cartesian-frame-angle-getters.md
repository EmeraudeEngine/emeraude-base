---
id: cartesian-frame-angle-getters
title: Math — CartesianFrame::getPitchAngle/getYawAngle/getRollAngle are not Euler angles
status: open
priority: high
scope: Math / CartesianFrame
opened: 2026-09-29
tags: [math, naming, transforms]
---

# Math — CartesianFrame::getPitchAngle/getYawAngle/getRollAngle are not Euler angles

## Why

The three getters return the angle between the frame's BACKWARD axis and a fixed axis — `-Z`, `+X` and `+Y`
respectively — not a pitch, a yaw or a roll. An untouched frame (backward = +Z) reads 180° / 90° / 90°. Found on
2026-09-29 by the engine's editor panel, which first displayed them as the active entity's rotation.

The engine panel now reads `toQuaternion().eulerAngles()` (ZYX Tait-Bryan) instead.

## What remains

- Decide with the owner: fix them into real Tait-Bryan angles (a documented order), rename them to what they
  measure, or remove them. Find every caller first (emeraude-engine, projet-alpha) and check which meaning each one
  relies on.
- A unit test pinning the chosen meaning on the identity frame and on a known rotation.

## References

- `src/Math/CartesianFrame.hpp` (`getPitchAngle()`, `getYawAngle()`, `getRollAngle()`, `toQuaternion()`).
- `src/Math/Quaternion.hpp` `eulerAngles()`.
