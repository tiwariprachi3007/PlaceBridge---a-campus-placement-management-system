# PlaceBridge - Campus Placement Management System

B.Tech (CSE) Object-Oriented Programming project - C++17 with the Qt 6 Widgets framework.

## 1. Abstract
Campus placements are usually tracked in spreadsheets and e-mail threads. PlaceBridge is a
desktop application that keeps students, recruiters, drives, applications and interviews in one
SQLite database and enforces the placement rules (eligibility, offer limits, interview clashes)
in code instead of by convention.

## 2. Objectives
1. Model the people and companies of a placement season with a clean class hierarchy.
2. Enforce role-based access in the data layer, not only in the interface.
3. Apply the institute's placement policy automatically (CGPA, backlogs, branch, offer limits, Dream / Super Dream tiers).
4. Give every role a focused workflow: Admin, Department Coordinator, Recruiter, Student.
5. Keep credentials safe (salted PBKDF2 hashes, lockout, one-time reset codes) and keep an audit trail.

## 3. Modules
| Module | Files | Purpose |
|---|---|---|
| Domain model and persistence | `DataManager.h/.cpp`, `student/`, `company/`, `placement_officer/` | Abstract `User`, file repository, SQLite access, placement analytics |
| Access control, policy and validation | `SecurityManager.h/.cpp` | Role capabilities, eligibility rules, input validation, PBKDF2-HMAC-SHA-256 |
| Application services | `SystemServices.h/.cpp`, `ui/search_dialog.h` | Logging, search, mail drafts, REST client and utilities |
| User interface | `ui/` and `mainwindow.h` | Qt Widgets pages, dialogs and theme |
| REST server (optional) | `server/rest_server.cpp` | Token-authenticated local API |
| Tests | `tests/test_placebridge.cpp` | QtTest suite |

## 4. Database design (SQLite)
Tables: `students`, `drives`, `applications`, `events`, `accounts`, `activity_log`, `notifications`,
`documents`, `interview_slots`, `feedback`, `announcements`, `company_contacts`.
Applications reference a student and a drive by name and role; accounts hold the salt, password hash,
lockout counters and the hashed one-time reset code.

## 5. Object-oriented concepts used
| Concept | Where |
|---|---|
| Abstraction | `User` is abstract: `role()` and `accountRole()` are pure virtual |
| Inheritance | `Student`, `Company`, `PlacementOfficer`, `DepartmentCoordinator` derive from `User` |
| Polymorphism | `user->canManageDrives()` is answered by whichever subclass the session holds; `welcome()` is overridable |
| Encapsulation | `User` data is protected; `DataStore` hides SQL and checks permissions internally |
| Composition | `MainWindow` owns the `DataStore`, the session and the pages |
| Single responsibility | `PasswordHasher`, `PlacementPolicy`, `PlacementStats`, `GlobalSearch` each do one job |
| Signals and slots | `DataStore::changed()` refreshes the interface after every write |

## 6. Security
- Passwords: 16-byte random salt, PBKDF2-HMAC-SHA-256, 120 000 rounds.
- Five failed sign-ins lock the account temporarily.
- Password recovery without e-mail: an admin issues an 8-character one-time code valid for 15 minutes; only its hash is stored.
- Users on a documented demo password are prompted to change it.
- Every write is permission-checked in `DataStore` and recorded in the audit log.

## 7. Testing
QtTest cases cover: domain classes, validators, eligibility boundaries, permissions (string API versus
polymorphic API), statistics, password hashing, reset codes and lockout, demo-password detection,
reminders, permission-aware search and the complete SQLite workflow.
Run: `ctest --test-dir build --output-on-failure`.

## 8. Limitations and future scope
- `DataStore` should be split into repository classes.
- No translations are shipped yet.
- The REST server is local only; a production deployment would need HTTPS and a remote data layer.
- Possible additions: resume parsing, e-mail delivery of reminders, charts for package distribution.
