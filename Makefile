SHELL := cmd.exe
.SHELLFLAGS := /D /C
.PHONY: all run migrated run-migrated test test-foundation test-migration test-application test-preview audit test-legacy clean
all:
	powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/run-library.ps1 -BuildOnly
run:
	powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/run-library.ps1
migrated: all
run-migrated: run
test:
	powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/test-all.ps1
test-foundation:
	powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/test-foundation.ps1
test-migration:
	powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/test-migration.ps1
test-application:
	powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/test-application.ps1
test-preview:
	powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/preview/run-preview.ps1 -Test
audit:
	powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/audit-source.ps1
.PHONY: test-concurrency
test-concurrency:
	powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/test-concurrency.ps1
test-legacy:
	powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/test-legacy.ps1
# build/ contains persistent user datasets. Never recursively delete it.
clean:
	@echo Persistent data retained. Remove individual unused executable files manually.

.PHONY: test-search
test-search:
	powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/test-search.ps1
