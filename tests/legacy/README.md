# Historical contract fixtures (not active tests)

The 23 files in this directory are unmodified historical source-level checks.
They referenced documents, compilation flags, files and NPC systems removed
before the audit stage began, so the original assertions cannot be run against
the current open.mp mode without producing false failures.

Each original has a same-named modernized regression in the parent `tests/`
directory. The active CI discovers **all** `tests/test_*.py`, and failing
active regressions block compilation output. The historical originals are
retained for future, assertion-by-assertion recovery; they are **not** evidence
that the old assertions still pass. Passing active contracts also does not
replace integration tests, a live MySQL failure-injection test or GTA UAT.

Do not add `__init__.py` here or interpret the archive as executable coverage.
