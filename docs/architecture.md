# Architecture

## 1. 목표

프로젝트 피클은 **로컬 동영상 라이브러리 관리용 Windows 데스크톱 앱**이다.

핵심 목적:
- 디렉토리 스캔
- 동영상 메타데이터 수집
- SQLite 기반 라이브러리 관리
- 플레이어 중심 검토 UI
- 정리 기능(태그, 설명, 파일명 변경, 스냅샷, 썸네일)

---

## 2. 전체 구조

앱은 크게 다음 레이어로 나눈다.

### 2.1 UI Layer (QML)
책임:
- 메인 화면 렌더링
- 사용자 입력 처리
- 재생 컨트롤 표시
- 리스트 표시
- 상세 패널 표시

포함 요소:
- `Main.qml`
- `pages/PlayerPage.qml`
- `components/LibraryPanel.qml`
- `components/InfoPanel.qml`
- `components/PlaybackBar.qml`

### 2.2 Application Layer (C++)
책임:
- 화면과 서비스 사이 조정
- 현재 선택 파일 관리
- 주요 액션 라우팅
- 상태 전파

예상 클래스:
- `AppController`
- `PlaybackController`

### 2.3 Domain / Service Layer (C++)
책임:
- 폴더 스캔
- 파일 정보 수집
- 해시 계산
- 메타데이터 추출
- 이름 변경
- 스냅샷 생성
- 태그/설명 저장

예상 클래스:
- `ScanService`
- `MetadataService`
- `FileHashService`
- `RenameService`
- `SnapshotService`
- `ProcessRunner`

### 2.4 Data Layer (SQLite)
책임:
- 영속 데이터 저장
- 검색/필터/정렬 기반 데이터 제공
- 앱 상태 복원

예상 클래스:
- `DatabaseService`
- `MigrationService`
- `MediaRepository`

---

## 3. UI 배치

### 중앙 영역
동영상 플레이어가 메인이다.

포함 기능:
- 재생
- 일시정지
- 시크
- 볼륨
- 배속
- 전체화면(후순위)

### 하단 영역
재생바와 주요 액션 버튼을 둔다.

포함 기능:
- 재생/일시정지
- 현재 시간 / 전체 시간
- 시크바
- 볼륨 슬라이더
- 배속 선택
- 스냅샷 버튼(후속 단계)

### 우측 상단 영역
라이브러리 리스트를 둔다.

포함 기능:
- 항목 목록
- 썸네일 토글
- 정렬
- 필터
- 검색
- F2 이름 바꾸기

### 우측 하단 영역
선택된 파일의 상세 정보를 둔다.

포함 기능:
- 파일 기본 정보
- 미디어 메타데이터
- 설명 편집
- 태그 편집
- 검토 상태/평점
- 대표 썸네일 정보

---

## 4. 주요 데이터 흐름

### 4.1 스캔 흐름
1. 사용자가 루트 디렉토리를 선택한다.
2. `ScanService`가 디렉토리를 재귀적으로 순회한다.
3. 동영상 파일 후보를 수집한다.
4. 기본 파일 정보를 추출한다.
5. 해시 계산이 필요한 경우 `FileHashService`가 처리한다.
6. 메타데이터 추출이 필요한 경우 `MetadataService`가 처리한다.
7. 결과를 `DatabaseService`를 통해 SQLite에 upsert 한다.
8. `MediaLibraryModel`이 갱신되어 UI가 업데이트된다.

### 4.2 선택 및 재생 흐름
1. 사용자가 우측 리스트에서 항목을 선택한다.
2. `AppController`가 현재 선택 파일을 갱신한다.
3. `PlaybackController`가 재생 소스를 갱신한다.
4. QML 플레이어 영역이 새 파일을 재생한다.
5. 상세 패널이 현재 선택 파일의 정보를 표시한다.

### 4.3 편집 흐름
1. 사용자가 태그, 설명, 상태, 평점을 수정한다.
2. 관련 컨트롤러가 C++ 서비스 호출
3. SQLite 갱신
4. 현재 선택 상태와 리스트 UI 반영

---

## 5. 권장 모듈 구조

```text
src/
  main.cpp
  app/
    AppController.h/.cpp
  core/
    FileHashService.h/.cpp
    ProcessRunner.h/.cpp
  db/
    DatabaseService.h/.cpp
    MigrationService.h/.cpp
    MediaRepository.h/.cpp
  media/
    MediaLibraryModel.h/.cpp
    ScanService.h/.cpp
    MetadataService.h/.cpp
    PlaybackController.h/.cpp
    RenameService.h/.cpp
    SnapshotService.h/.cpp
```

---

## 6. DB 설계(초기안)

### 6.1 `media_files`
주요 동영상 파일 레코드

권장 컬럼:
- `id`
- `file_name`
- `file_path`
- `file_extension`
- `file_size`
- `modified_at`
- `hash_value`
- `duration_ms`
- `bitrate`
- `frame_rate`
- `width`
- `height`
- `video_codec`
- `audio_codec`
- `description`
- `review_status`
- `rating`
- `thumbnail_path`
- `last_played_at`
- `last_position_ms`
- `created_at`
- `updated_at`

### 6.2 `tags`
태그 마스터

권장 컬럼:
- `id`
- `name`
- `color`
- `created_at`

### 6.3 `media_tags`
파일-태그 연결 테이블

권장 컬럼:
- `media_id`
- `tag_id`

### 6.4 `snapshots`
스냅샷 기록

권장 컬럼:
- `id`
- `media_id`
- `image_path`
- `timestamp_ms`
- `created_at`

### 6.5 `scan_roots`
사용자가 등록한 스캔 루트

권장 컬럼:
- `id`
- `root_path`
- `is_enabled`
- `last_scanned_at`
- `created_at`

### 6.6 `app_settings`
앱 설정 저장

예시 값:
- 썸네일 표시 여부
- 마지막 선택 정렬 조건
- 마지막 열었던 폴더
- 플레이어 볼륨

---

## 7. 리스트 설계

라이브러리 리스트는 `QAbstractListModel` 기반으로 구현한다.

필수 역할:
- 선택 가능한 파일 목록 제공
- 정렬 반영
- 필터 반영
- 검색 반영
- 썸네일 표시 여부 반영

대표 role 예시:
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
- 선택, 평점, 검토 상태는 모델/백엔드 데이터에서 온다.

---

## 8. 멀티스레드/성능 원칙

- UI 스레드를 블로킹하는 파일 스캔을 피한다.
- 해시 계산, 폴더 스캔, 메타데이터 분석은 백그라운드 작업으로 분리한다.
- 긴 작업은 진행률과 취소 가능성을 고려한다.
- 리스트는 필요 이상 복잡한 delegate를 피한다.
- 초기 버전에서는 실시간 파일 시스템 감시보다 수동 재스캔을 우선한다.

---

## 9. 외부 도구 전략

### Qt Multimedia
역할:
- 앱 내 기본 동영상 재생

### ffprobe
역할:
- 상세 메타데이터 추출

예시 정보:
- duration
- bitrate
- codec
- stream count
- frame rate

### ffmpeg
역할:
- 스냅샷 생성
- 대표 썸네일 생성

---

## 10. 예외 및 오류 처리 원칙

- 파일이 삭제되었거나 이동된 경우 상태를 DB에 반영한다.
- 메타데이터 추출 실패 시 앱 전체가 실패하지 않게 한다.
- 이름 변경 실패 시 원인을 사용자에게 노출한다.
- DB 초기화 실패 시 앱은 원인을 로그로 남기고 안전하게 종료하거나 read-only 상태로 전환한다.

---

## 11. 초기 MVP 범위

포함:
- 앱 실행
- 메인 UI 쉘
- SQLite 초기화
- 폴더 스캔
- 리스트 표시
- 파일 선택
- 재생
- 상세 정보 표시
- 설명/태그 저장

제외:
- 중복 탐지
- 실시간 폴더 감시
- 대규모 일괄 작업
- 고급 영상 편집 기능

