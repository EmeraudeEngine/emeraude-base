## Math: PiecewiseLinear — a curve given by points (the engine's wheeled vehicle, 2026-10-02)

`src/Math/PiecewiseLinear.hpp` (header-only, `PiecewiseLinear< T = float, max_points = 16 >`): a function of one
variable given by points, linear between two of them and constant beyond the first and the last. The engine's vehicle
uses it for the engine's torque over the RPM and the tyres' friction over the slip (engine
`docs/subsystems/physics/19-wheeled-vehicle.md`).

- **No allocation**: the points live in a `StaticVector< std::pair< T, T >, max_points >`.
- `addPoint(x, y)` appends after the last point. Refused (false, nothing added): a non-finite x or y, an x not
  STRICTLY above the last one, a full curve. Out-of-order points are therefore impossible: no sort, no search
  structure.
- `value(x)`: 0 without a point; the first value at or before the first x, and for a NaN (`!(x > first)`); the last
  value beyond the last x; otherwise linear between the two points around x. A linear walk: the curves hold a handful
  of points (Jolt's defaults: 3 each).
- `clear()`, `empty()`, `points()`.

Tests: `src/Testing/test_MathPiecewiseLinear.cpp` (4: empty answers 0, one point is constant, interpolation and both
clamps, invalid points refused), green in Release and under ASan / UBSan.
