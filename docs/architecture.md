# Pickle Architecture

## 1. 목표

Pickle은 로컬 동영상 라이브러리 관리용 Windows 데스크톱 앱이다.

핵심 목적:
- 디렉토리 스캔
- 동영상 메타데이터 수집
- SQLite 기반 라이브러리 관리
- 플레이어 중심 검토 UI
- 정리 기능: 태그, 설명, 파일명 변경, 스냅샷, 썸네일

이번 아키텍처 방향은 QML 공개 동작을 유지하면서 C++ 내부를 단계적으로 클린 아키텍처에 맞추는 것이다.

## 2. 현재 분석

현재 구조는 `core`, `db`, `media`, `app`, `qml`로 물리 분리는 되어 있었지만, 클린 아키텍처는 부분 적용 상태였다.

강점:
- QML은 대체로 UI 표현과 사용자 상호작용에 머문다.
- SQLite 접근은 `MediaRepository`, `AppSettingsRepository` 중심으로 모여 있다.
- `MediaLibraryModel`은 `QAbstractListModel` 기반이며 delegate 영구 상태 의존을 피한다.
- 최근 성능/보안 작업으로 worker DB connection, path hardening, process output cap, M11/M12 회귀 테스트가 추가되었다.

개선 필요점:
- `AppController`가 UI facade, use case orchestration, background job, settings, diagnostics, file action 상태를 동시에 갖고 있다.
- 기존 `MediaTypes.h`는 domain entity, UI DTO, infra result, settings DTO가 섞인 큰 공유 헤더였다.
- `db`와 `media`가 서로의 concrete 타입에 의존해 repository 방향이 내부 정책 쪽으로 잘 닫혀 있지 않았다.
- `ScanService`, `MetadataService`, `SnapshotService`, `ThumbnailService`는 application 정책이라기보다 filesystem/process/image adapter 책임을 함께 갖고 있었다.

## 3. 목표 의존 방향

의도하는 흐름:

```text
QML
  -> presentation
  -> application
  -> ports / domain
  <- infrastructure
```

규칙:
- `domain`은 안정적인 타입과 값 객체를 가진다.
- `application`은 use case orchestration만 가진다.
- `ports`는 application이 호출하는 interface를 가진다.
- `infrastructure`는 SQLite, filesystem, ffmpeg/ffprobe, image IO, path policy 구현을 가진다.
- `presentation`은 `AppController`, list model, QML DTO mapping, status string, Qt async watcher를 가진다.

## 4. CMake Target 구조

현재 내부 target:
- `pickle_domain`: domain-safe 타입. Qt Core value type까지만 허용한다.
- `pickle_ports`: application이 의존하는 interface.
- `pickle_application`: use case orchestration.
- `pickle_core`: logging, cancellation, process/path 등 기존 core compatibility.
- `pickle_db`: SQLite repository implementation.
- `pickle_media`: scanner, metadata, rename, snapshot, thumbnail, playback/model compatibility.
- `pickle_infrastructure`: ports 구현 adapter의 새 위치.
- `pickle_app`: QML-facing facade와 async orchestration.
- `pickle_presentation`: presentation compatibility target.

장기 target 의존 방향:
- `pickle_domain`은 `Qt6::Core` 외 production target에 의존하지 않는다.
- `pickle_application`은 `pickle_domain`, `pickle_ports`에 의존한다.
- `pickle_infrastructure`는 `pickle_ports`, `pickle_domain`, concrete adapter target에 의존한다.
- `pickle_app`/`pickle_presentation`은 application use case를 호출하고 QML API를 유지한다.

## 5. 레이어별 책임

### 5.1 UI Layer (QML)

책임:
- 메인 화면 렌더링
- 사용자 입력 처리
- 재생 컨트롤 표시
- 라이브러리 리스트 표시
- 상세 패널 표시

포함 요소:
- `qml/Main.qml`
- `qml/pages/PlayerPage.qml`
- `qml/components/LibraryPanel.qml`
- `qml/components/InfoPanel.qml`
- `qml/components/PlaybackBar.qml`

금지:
- DB 접근
- 파일 시스템 처리
- 보안 정책 판단
- 장시간 작업 실행

### 5.2 Presentation Layer (C++)

책임:
- QML property/signal/invokable 제공
- 사용자 command를 use case로 forwarding
- selection, status text, async watcher 관리
- domain/application result를 model role 또는 QML property로 변환

현재 구성:
- `AppController`
- `LibraryController`
- `ScanController`
- `LibraryReloadService`
- `MediaLibraryModel`
- `PlaybackController`

개선 방향:
- `AppController`는 facade로 축소한다.
- 기능별 coordinator는 `LibraryController`, `ScanController`, `MetadataController`, `SnapshotController`, `ThumbnailController`, `SettingsController`, `DiagnosticsController`로 분리한다.
- QML-facing 이름은 유지한다.

### 5.3 Application Layer

책임:
- use case orchestration
- repository/scanner/probe/generator port 호출
- 성공/실패 결과 조립
- UI 문구를 만들지 않고 `OperationResult<T>`로 상태를 반환

현재 use case:
- `LoadLibraryUseCase`
- `ScanLibraryUseCase`
- `UpdateMediaDetailsUseCase`
- `UpdateMediaFlagsUseCase`
- `SavePlaybackPositionUseCase`
- `RenameMediaUseCase`
- `ExtractMetadataUseCase`
- `CaptureSnapshotUseCase`
- `RebuildThumbnailsUseCase`
- `SettingsUseCase`

금지:
- `QSql*`
- `QProcess`
- `QImage`
- `QAbstractListModel`
- `QVariantMap`
- QML headers

### 5.4 Ports Layer

책임:
- application이 의존하는 interface 선언
- infrastructure 구현 세부사항을 application에서 숨김

현재 ports:
- `IMediaRepository`
- `ISettingsRepository`
- `IFileScanner`
- `IFileRenamer`
- `IMetadataProbe`
- `ISnapshotGenerator`
- `IThumbnailGenerator`
- `IPathPolicy`
- `IExternalToolResolver`
- `IProcessRunner`

### 5.5 Domain Layer

책임:
- persistence/UI/process/image와 무관한 데이터 구조
- query/result/value 타입

현재 domain headers:
- `MediaEntities.h`
- `LibraryQuery.h`
- `OperationResult.h`

허용:
- `QString`, `QStringList`, `QVector`, `qint64` 같은 Qt Core value type

금지:
- SQLite, process, image, QML, model 타입

### 5.6 Infrastructure Layer

책임:
- SQLite 구현
- filesystem scan/rename 구현
- ffprobe/ffmpeg/process 실행 구현
- snapshot/thumbnail 저장 구현
- path security policy 구현

현재 adapter:
- `MediaRepository : IMediaRepository`
- `AppSettingsRepository : ISettingsRepository`
- `ScanService : IFileScanner`
- `RenameService : IFileRenamer`
- `MetadataService : IMetadataProbe`
- `SnapshotService : ISnapshotGenerator`
- `ThumbnailService : IThumbnailGenerator`
- `PathSecurityPolicy : IPathPolicy`

## 6. 주요 데이터 흐름

### 6.1 스캔 흐름

1. QML이 `AppController` command를 호출한다.
2. Presentation이 worker job을 시작한다.
3. `ScanLibraryUseCase`가 `IFileScanner`로 디렉토리를 순회한다.
4. scan result는 worker DB connection의 `IMediaRepository`로 upsert 된다.
5. UI thread는 완료 result와 async library reload만 반영한다.
6. `MediaLibraryModel`은 가능한 경우 전체 reset 대신 row 단위 갱신을 사용한다.

### 6.2 라이브러리 조회 흐름

1. 검색/sort 변경이 `MediaLibraryQuery`로 변환된다.
2. `LoadLibraryUseCase`가 `IMediaRepository::fetchLibraryItems(query)`를 호출한다.
3. generation id로 오래된 async 결과가 최신 model을 덮지 못하게 한다.
4. Presentation이 role/QML 표시용 값으로 노출한다.

### 6.3 편집 흐름

1. 사용자가 상세 정보, 태그, 상태, 평점을 수정한다.
2. Presentation이 `UpdateMediaDetailsUseCase` 또는 flag/playback use case를 호출한다.
3. Repository가 SQLite를 갱신한다.
4. 단일 row refresh는 `fetchMediaById(mediaId)`를 통해 model에 반영한다.

### 6.4 파일 액션 흐름

1. Presentation이 rename command를 받는다.
2. `RenameMediaUseCase`가 `IFileRenamer`로 파일명을 바꾼다.
3. DB update 실패 시 가능한 범위에서 filesystem rollback을 시도한다.
4. 실패 원인은 `OperationResult`로 presentation에 반환한다.

## 7. DB 설계

영속 데이터는 SQLite에 저장한다. 스키마 변경은 migration을 통해 추가한다.

핵심 테이블:
- `media_files`
- `tags`
- `media_tags`
- `snapshots`
- `scan_roots`
- `app_settings`

성능 원칙:
- worker thread는 main thread SQLite connection을 공유하지 않는다.
- 공통 pragma는 `DatabaseService`에서 적용한다.
- scan upsert는 prepared query 재사용과 transaction을 유지한다.
- tag lookup은 selected media id 범위로 제한하고 필요 시 chunk 처리한다.

## 8. 리스트 설계

라이브러리 리스트는 `QAbstractListModel` 기반이다.

대표 role:
- `id`
- `fileName`
- `filePath`
- `fileSize`
- `durationMs`
- `resolution`
- `thumbnailPath`
- `rating`
- `reviewStatus`
- `isFavorite`

주의사항:
- delegate 내부에 영구 상태를 저장하지 않는다.
- 선택, 평점, 검토 상태는 model/backend 데이터에서 온다.
- 단일 항목 변경은 가능한 한 `dataChanged`로 처리한다.

## 9. 보안 및 경로 정책

원칙:
- shell을 거치지 않고 process를 실행한다.
- stdout/stderr output cap과 timeout/cancellation을 유지한다.
- external tool은 명시적으로 해석된 실행 파일만 허용한다.
- managed snapshot/thumbnail root 삭제는 앱 데이터 하위 canonical path일 때만 허용한다.
- DB에 임의 thumbnail path가 들어 있어도 QML이 직접 로컬 파일을 열지 않게 필터링한다.
- diagnostics 기본 출력은 사용자 홈/앱데이터 경로를 축약한다.

## 10. 테스트와 아키텍처 가드

현재 자동 테스트:
- M03-M12: 기존 scan/repository/query/actions/security/performance/modularization 회귀
- M13: target 의존 방향과 금지 include 패턴 검사
- M14: library/scan use case orchestration 검사
- M15: details/flags/playback/rename use case 검사
- M16: infrastructure adapter가 ports 뒤에 남아 있는지 검사
- M17: QML-facing facade와 model role compatibility 검사

성능 검증:
- M11 synthetic 50k 테스트를 유지한다.
- 실제 폴더 baseline은 `pickle_perf_probe`로 `E:\Documents\Temp`에 대해 별도로 측정한다.

## 11. 단계별 개선 계획

M13 완료 기준:
- target 뼈대 추가
- domain/application/ports 타입 분리
- architecture guard test 추가
- `MediaTypes.h` compatibility shim 유지

M14 완료 기준:
- library/scan use case 적용
- worker DB connection 생성 책임을 점진적으로 infrastructure 쪽으로 이동
- scan/reload orchestration을 `LibraryController`/`ScanController`로 분리

M15 완료 기준:
- details, flags, playback, rename use case 적용
- `fetchMediaById(mediaId)` 기반 단일 row refresh 통일

M16 완료 기준:
- metadata/snapshot/thumbnail/process/filesystem adapter를 ports 뒤로 이동
- cancellation, timeout, output cap, managed root guard 유지

M17 목표:
- settings/diagnostics 분리
- feature coordinator 도입
- `AppController.cpp`를 500줄 이하 facade로 축소

## 12. Compatibility Layer

`src/media/MediaTypes.h`는 migration 동안 기존 include를 깨지 않기 위한 shim이다. 새 코드는 필요한 좁은 header를 직접 include한다.

권장 include:
- domain entity: `domain/MediaEntities.h`
- library query: `domain/LibraryQuery.h`
- use case result: `application/LibraryLoadResult.h`, `application/ScanCommitResult.h`
- generic result: `domain/OperationResult.h`

## 13. 후속 반영 이슈

다음 항목은 현재 빌드 안정성을 유지하기 위해 단계적으로 처리한다.

- `AppController.cpp`는 `LibraryController`/`ScanController` 분리 후에도 아직 facade라고 보기에는 크다. `MetadataController`, `SnapshotController`, `ThumbnailController`, `SettingsController`, `DiagnosticsController`를 추가로 분리해 500줄 이하 facade를 목표로 한다.
- `ports` 일부는 cancellation 처리를 위해 `core/CancellationToken`에 의존한다. 장기적으로 application/ports 경계에 더 얇은 cancellation interface 또는 value type을 두어 `pickle_ports -> pickle_core` 의존을 줄인다.
- `MediaRepository`는 아직 persistence record와 UI-facing `MediaLibraryItem` 조립을 함께 수행한다. repository는 raw persistence record를 반환하고, presentation mapper가 QML/model 표시용 DTO를 만드는 방향으로 분리한다.
- metadata, snapshot, thumbnail rebuild의 QtConcurrent job orchestration은 아직 `AppController`에 남아 있다. 다음 feature controller로 모아 `AppController`가 상태 반영만 담당하게 한다.
- M13 architecture boundary test는 금지 include 중심의 최소 가드다. target link graph와 source folder ownership을 더 정밀하게 검사하는 방식으로 확장한다.
