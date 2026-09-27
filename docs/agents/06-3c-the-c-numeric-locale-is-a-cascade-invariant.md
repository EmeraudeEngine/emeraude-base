## 3c. The C numeric locale is a cascade invariant

`EmEn::Base::Locale::enforceNumericC()` (`src/Locale.hpp`) forces `LC_NUMERIC` to `"C"` and
**returns true when it had drifted**, so the caller can log a warning naming whatever ran in between.

> [!CAUTION]
> `LC_NUMERIC` decides the decimal separator of the entire C numeric family — `printf("%f")`,
> `strtod()`, `atof()`, `std::to_string()`, `std::stof()` — which is what the cascade uses to
> serialise floats (settings, scene files, cache keys, Saphir-generated shader code). A third-party
> library calling `setlocale(LC_ALL, "")` switches it to the user's locale, and in `fr_BE`, `fr_FR`
> or `de_DE` that separator is a **COMMA**: every float written changes format and `strtod("1.5")`
> stops at the dot. **No test on a `C` or `en_US` machine catches it.** GTK's `gtk_init()` performs
> exactly that call, and the CEF/GTK stack pulls it into the process.
>
> Only `LC_NUMERIC` is forced — `LC_CTYPE`, `LC_TIME` and `LC_MESSAGES` stay localised. C++ streams
> are unaffected either way: they carry a copy of the global `std::locale`, never the C locale.

**One call is not enough, by construction.** The engine calls it in `Core::initializeBaseLevel()`,
but an application initialises CEF *after* the engine and Chromium loads GTK lazily, so the
invariant must be **re-asserted after initialising any library known to touch the locale** —
projet-alpha does it right after `CefInitialize()` in both boot files. Measured 2026-08-22: at
steady state the process runs `LC_CTYPE=fr_BE.UTF-8` with `LC_NUMERIC=C`, because Chromium resets
that one category itself; the explicit call turns that luck into a contract. For new code the
structural answer is to not depend on the locale at all — `std::to_chars()` / `std::from_chars()`
are locale-independent by specification.
