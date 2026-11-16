# Changelog

## 2.2
- Extracted `PasswordHasher` (PBKDF2, salts, constant-time compare) out of `DataStore`, with its own test.
- Drives page: sort by package or company name.
- Search results now open the correct tab for recruiters and students.
- Demo-password prompt gained "Later" and "Don't ask again".
- Tooltips and accessible names on navigation, notifications and account buttons.
- Added `docs/PROJECT_REPORT.md`.

## 2.1
- Permissions: single `Permissions::Role` enum; `User::accountRole()` + per-object capability checks.
  Fixes admin pages being denied because the display role "Placement Officer" was not recognised.
- Reports: "Placed students by branch" chart now uses real data (`PlacementStats`) instead of sample numbers.
- Added one-time password reset codes (admin issues, user redeems on the login screen).
- Added demo-password nudge after sign-in.
- Added upcoming drive/event reminders (sent when an admin signs in; idempotent).
- Added Ctrl+K search dialog backed by permission-aware `GlobalSearch`.
- Tests: permissions, eligibility boundaries, analytics, reset codes, lockout, reminders, search.
- Build: `-Wall -Wextra` / `/W4` enabled by default (`ENABLE_WARNINGS`).
- Cleanup: removed build output, stray folders, empty translation placeholders and test accounts from sample data.
- Added MIT license.
