# Pickle Test Coverage Gap Matrix

This project currently has no local line-coverage tool available on PATH, so coverage is tracked as a behavior matrix instead of a percentage.

## Current Automated Coverage

| Area | Tests | Covered behavior |
| --- | --- | --- |
| Scan and repository ingest | M03, M11 | Supported video discovery, rescan upsert, unchanged-row rewrite guard, 50k synthetic ingest |
| Library query and model | M04, M07, M11 | Sort/search behavior, tag loading, selection preservation, indexed row lookup, tight `dataChanged` ranges |
| Metadata/details | M06, M07 | Metadata parsing, details persistence, tags, review/rating state |
| File actions | M07 | Rename, favorite/delete candidate flags, playback position |
| Snapshots/thumbnails | M08, M09, M11, M12 | Snapshot DB records, thumbnail cache, managed root cleanup guards, unsafe thumbnail path suppression |
| Settings/diagnostics/tools | M10, M11, M18 | Settings persistence, controller load/save/reset, external tool validation/resolution, diagnostic path redaction, process timeout/output cap, work event cap |
| Utility/module boundaries | M12, M13, M20 | Shared path security helpers, test DB fixture lifecycle, managed path validation, CMake target/link graph and forbidden include guardrails |
| Application use cases | M14, M15 | Library load orchestration, scan commit flow, progress forwarding, details/flags/playback/rename command behavior |
| Infrastructure adapters | M16 | Existing scanner, rename, metadata, snapshot, thumbnail, and path policy implementations remain behind ports |
| Repository/presentation mapping | M19 | Raw media fetch, presentation-only formatting, unsafe thumbnail suppression, single-row model refresh |
| Presentation facade | M17, M21 | QML-facing AppController properties/invokables and library model roles remain stable after layering; library/scan/metadata/snapshot/thumbnail/settings/diagnostics/media-action controllers preserve facade compatibility; AppController stays within the 500-line closure budget |
| Performance regression | M09, M11 | Scan throttle behavior, 50k fetch budgets, unchanged rescan budget |

## Known Gaps

| Gap | Risk | Recommended next test |
| --- | --- | --- |
| Real Qt Multimedia playback behavior | Medium | Add a UI/manual smoke checklist once playback is wired to stable sample media |
| Real ffmpeg/ffprobe success paths | Medium | Add optional integration tests gated by explicit configured tool paths |
| Actual user library performance | Medium | Run `pickle_perf_probe` against `E:\Documents\Temp` and compare JSON baseline over time |
| QML visual regressions | Low | Add a future screenshot smoke test after UI layout stabilizes |
| FTS/search redesign | Low | Defer until current LIKE + debounce path misses the 50k target in probe data |

## Clean Architecture Closure

The previously tracked architecture gaps are now covered by M18-M21:
- `AppController.cpp` facade budget and forbidden dependency checks.
- settings, diagnostics, work event, and media action controller split.
- raw repository fetch plus presentation mapper boundary.
- `ports -> core` dependency removal via domain `CancellationToken`.

Remaining gaps are product or environment extensions rather than required clean-architecture TODOs.

## Manual Baseline Command

```powershell
build\Desktop_Qt_6_11_0_MSVC2022_64bit-Debug\pickle_perf_probe.exe --scan-root E:\Documents\Temp --database build\perf\pickle_perf_probe.sqlite3 --json build\perf\E_Documents_Temp.json --compare build\perf\E_Documents_Temp.json
```

The probe reads the previous JSON before writing the new one, then adds a compact comparison block with count and elapsed-time deltas. Output redacts local paths and records counts, elapsed times, sort fetches, search fetches, and extension distribution only.

## Optional Line Coverage

If OpenCppCoverage is installed and discoverable on PATH during CMake configure, this target becomes available:

```powershell
cmake --build build\Desktop_Qt_6_11_0_MSVC2022_64bit-Debug --target coverage_opencppcoverage
```

The target exports HTML coverage under `build\Desktop_Qt_6_11_0_MSVC2022_64bit-Debug\coverage\opencppcoverage`. When OpenCppCoverage is not installed, the behavior matrix above remains the authoritative local coverage check.
