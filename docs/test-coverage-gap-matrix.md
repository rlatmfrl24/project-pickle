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
| Settings/diagnostics/tools | M10, M11 | Settings persistence, external tool validation, diagnostic path redaction, process timeout/output cap |
| Utility/module boundaries | M12, M13 | Shared path security helpers, test DB fixture lifecycle, managed path validation, CMake target and forbidden include guardrails |
| Application use cases | M14, M15 | Library load orchestration, scan commit flow, progress forwarding, details/flags/playback/rename command behavior |
| Infrastructure adapters | M16 | Existing scanner, rename, metadata, snapshot, thumbnail, and path policy implementations remain behind ports |
| Presentation facade | M17 | QML-facing AppController properties/invokables and library model roles remain stable after layering; library/scan controllers preserve facade compatibility |
| Performance regression | M09, M11 | Scan throttle behavior, 50k fetch budgets, unchanged rescan budget |

## Known Gaps

| Gap | Risk | Recommended next test |
| --- | --- | --- |
| Real Qt Multimedia playback behavior | Medium | Add a UI/manual smoke checklist once playback is wired to stable sample media |
| Real ffmpeg/ffprobe success paths | Medium | Add optional integration tests gated by explicit configured tool paths |
| Actual user library performance | Medium | Run `pickle_perf_probe` against `E:\Documents\Temp` and compare JSON baseline over time |
| Full AppController decomposition | Medium | Split metadata, snapshot, thumbnail, settings, and diagnostics coordinators after library/scan controllers |
| QML visual regressions | Low | Add a future screenshot smoke test after UI layout stabilizes |
| FTS/search redesign | Low | Defer until current LIKE + debounce path misses the 50k target in probe data |

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
