# Roadmap

## 현재 기준점

이 로드맵은 현재 저장소가 M21까지의 아키텍처 closure를 완료한 상태를 기준으로 한다.

- M00-M09: 부트스트랩, UI 쉘, DB 초기화, 디렉토리 스캔, 라이브러리 연결, 재생, 상세 편집, 파일 관리, 스냅샷/썸네일, 품질 보강 완료
- M10-M12: 설정/진단 UX, 성능/보안 회귀, 유틸리티 모듈화 완료
- M13-M21: 클린 아키텍처 레이어 분리, use case/ports/infrastructure/presentation 정리, `AppController` facade closure 완료
- 다음 제품 확장 작업은 M22부터 진행한다.

---

## 원칙

- 항상 빌드 가능한 상태를 유지한다.
- 한 번에 하나의 작은 마일스톤만 구현한다.
- 각 단계는 명확한 완료 기준을 가진다.
- QML은 UI만 담당하고, 파일 시스템/DB/스캔/해시/메타데이터/이름 변경 로직은 C++에 둔다.
- 리스트 상태는 `QAbstractListModel`과 presentation/backend 상태로 관리하고, delegate 내부에 영구 상태를 저장하지 않는다.
- DB 스키마 변경이 필요한 경우 migration을 추가한다.
- 실제 구현은 Codex가 맡더라도, 각 단계의 범위와 완료 여부는 사람이 통제한다.

---

## 완료된 기반 마일스톤

### M00. 저장소 부트스트랩
- Qt Quick/CMake 기반 프로젝트 골격, 기본 디렉토리, 초기 문서와 실행 가능한 창을 준비했다.

### M01. UI 쉘
- 중앙 플레이어, 하단 재생바, 우측 라이브러리, 상세 패널의 기본 레이아웃을 구성했다.

### M02. DB 초기화
- SQLite DB, migration 테이블, 핵심 테이블을 앱 시작 시 준비하는 초기화 경로를 추가했다.

### M03. 디렉토리 스캔
- 사용자가 선택한 폴더를 재귀 스캔하고 동영상 후보를 DB에 upsert하는 흐름을 구현했다.

### M04. 라이브러리 리스트 연결
- `QAbstractListModel` 기반 라이브러리 목록, 검색, 정렬, 선택/상세 패널 연결을 구현했다.

### M05. 기본 재생 기능
- Qt Multimedia 기반 `MediaPlayer`/`VideoOutput`, 재생/일시정지, 시크, 볼륨, 시간 표시를 연결했다.

### M06. 상세 메타데이터 및 편집
- ffprobe 기반 메타데이터 추출 경로와 설명, 태그, 검토 상태, 평점 저장을 구현했다.

### M07. 파일 관리 기능
- F2 이름 변경, 즐겨찾기, 삭제후보, 마지막 재생 위치 저장을 추가했다.

### M08. 스냅샷 및 썸네일
- 특정 시점 스냅샷 생성, 대표 썸네일 저장, 리스트 썸네일 표시 경로를 구현했다.

### M09. 품질 보강
- 스캔 취소/진행률, 로그, 에러 상태, 성능 기초 회귀와 안정성 보강을 진행했다.

### M10. 설정 및 진단 UX
- ffmpeg/ffprobe 경로, 라이브러리/플레이어 설정, 시스템 정보, 진단 리포트, 로그/앱데이터 열기 기능을 추가했다.

### M11. 성능 및 보안 회귀
- worker DB connection, path hardening, process timeout/output cap, 50k synthetic 성능 검증을 추가했다.

### M12. 유틸리티 모듈화
- 경로 보안, 테스트 fixture, managed path 검증 등 반복 유틸리티를 정리했다.

### M13. 아키텍처 경계 기반
- domain/application/ports target 뼈대와 forbidden include 검사 기반을 추가했다.

### M14. 라이브러리/스캔 Use Case
- 라이브러리 로드와 스캔 commit 흐름을 application use case로 분리했다.

### M15. 미디어 액션 Use Case
- 상세 편집, 플래그, 재생 위치, 이름 변경을 use case 경로로 정리했다.

### M16. Infrastructure Adapter
- scanner, rename, metadata, snapshot, thumbnail, path policy 구현을 ports 뒤에 유지하도록 정리했다.

### M17. Presentation Facade
- QML-facing `AppController` API와 모델 role compatibility를 보존하는 회귀 검사를 추가했다.

### M18. Settings/Diagnostics Controller
- 설정, 진단, external tool validation, work event log를 전용 controller로 분리했다.

### M19. Repository/Presentation Mapper
- raw repository entity 조회와 presentation 표시값 mapping을 분리하고 unsafe thumbnail path suppression을 추가했다.

### M20. Architecture Boundary Closure
- `ports -> core` 의존 제거, source folder ownership, target link graph guard를 정리했다.

### M21. AppController Facade Closure
- `AppController`를 500줄 이하 facade로 축소하고 QML API 안정성 검사를 유지했다.

---

## 다음 제품 기능 마일스톤

## M22. 스마트 뷰 및 고급 필터

### 목표
검토 작업에 자주 쓰는 조건을 빠르게 전환할 수 있도록 라이브러리 필터를 확장한다.

### 범위
- 미검토, 즐겨찾기, 삭제후보, 최근 본 파일 스마트 뷰 추가
- 태그 기반 필터 추가
- 기존 파일명/설명 검색, 정렬, 썸네일 토글과 조합 가능한 query 구조 확장
- 필터 상태를 C++ controller/model 상태로 관리하고 QML은 선택 UI만 담당
- 필요 시 app setting에 마지막 필터 상태 저장

### 완료 기준
- 라이브러리에서 스마트 뷰를 선택할 수 있다.
- 미검토/즐겨찾기/삭제후보/최근 본 파일/태그 필터가 DB 데이터 기준으로 동작한다.
- 검색어와 정렬 상태를 유지한 채 필터를 바꿀 수 있다.
- 선택 파일의 상세 패널과 재생 source가 필터 변경 후에도 일관되게 갱신된다.
- 관련 repository/use case/model/controller 테스트가 추가된다.

### 제외
- FTS5 검색 전환
- 복잡한 saved search builder
- 다중 선택 액션

---

## M23. 다중 선택 및 일괄 태그

### 목표
여러 파일을 한 번에 선택하고 공통 정리 작업을 수행할 수 있게 한다.

### 범위
- 라이브러리 리스트 다중 선택 상태 추가
- 선택 개수와 대표 선택 항목 표시
- 선택 항목 대상 일괄 태그 추가/제거
- 선택 항목 대상 일괄 검토 상태/평점 변경
- 다중 선택 상태는 delegate가 아니라 model/controller/presentation 상태에 둔다.

### 완료 기준
- Ctrl/Shift 또는 명시적 UI 액션으로 여러 항목을 선택할 수 있다.
- 선택된 항목 개수가 UI에 표시된다.
- 일괄 태그 추가/제거가 DB와 리스트에 반영된다.
- 일괄 상태/평점 변경이 DB와 상세 패널에 반영된다.
- 필터/검색/정렬 변경 후 선택 상태가 예측 가능한 방식으로 유지되거나 정리된다.
- 단일 선택 재생 흐름은 기존처럼 동작한다.

### 제외
- 일괄 이름 변경
- 실제 파일 삭제
- 드래그 선택 UI

---

## M24. 일괄 이름 변경

### 목표
다중 선택된 파일의 이름을 안전하게 미리보기 후 일괄 변경한다.

### 범위
- 선택 항목 대상 preview-first rename 흐름 추가
- prefix/suffix/numbering 기반의 단순 rename rule 지원
- 충돌, 빈 이름, Windows 금지 문자, 확장자 보존 실패 사전 검증
- 실제 rename은 C++ use case와 repository update 경로로 처리
- 실패 항목은 파일 시스템과 DB 상태가 불일치하지 않도록 결과를 명확히 보고

### 완료 기준
- 일괄 변경 전 기존 이름과 새 이름 preview를 볼 수 있다.
- 검증 실패 항목은 실행 전에 표시된다.
- 통과한 항목만 rename 실행이 가능하다.
- rename 결과가 DB, 라이브러리 리스트, 선택 상태에 반영된다.
- 부분 실패 시 성공/실패 항목과 사유가 UI/상태에 드러난다.

### 제외
- 정규식 기반 고급 rename rule
- 파일 내용 기반 자동 이름 추천
- 실제 삭제 또는 휴지통 이동

## M27. 키보드 중심 검토 모드

### 목표
마우스 사용을 줄이고 키보드만으로 빠르게 재생, 평가, 분류, 이동할 수 있는 검토 모드를 제공한다.

### 범위
- session-only 빠른 검토 모드와 헤더 상태 표시 추가
- 다음/이전 항목 이동 단축키 추가. 이동은 선택만 변경하고 자동 재생하지 않는다.
- 평점, 검토 상태, 즐겨찾기, 삭제후보 단축키
- 재생 토글, 앞/뒤 시크, 스냅샷 단축키 정리
- 빠른 검토 모드 진입/해제 UI 상태 표시
- 텍스트 입력, rename dialog, 태그 입력과 충돌하지 않는 focus 규칙 정리

### 단축키

| 동작 | 키 |
| --- | --- |
| 검토 모드 토글 | Ctrl+R |
| 이전 항목 | Up, K |
| 다음 항목 | Down, J |
| 재생/일시정지 | Space |
| 5초 뒤로/앞으로 | Left/H, Right/L |
| 평점 설정 | 1-5 |
| 평점 초기화 | 0 |
| 검토 상태 | U=unreviewed, R=reviewed, N=needs_followup |
| 즐겨찾기 토글 | F |
| 삭제후보 토글 | D |
| 스냅샷 | S |

### 완료 기준
- 사용자가 키보드로 항목 이동, 재생, 평점, 상태 변경을 수행할 수 있다.
- 단축키가 입력 필드 편집 중에는 오작동하지 않는다.
- 변경 사항이 DB, 리스트, 상세 패널에 즉시 반영된다.
- 검토 모드 상태가 UI에 명확히 표시된다.
- 주요 단축키 흐름에 대한 수동 검증 체크리스트가 문서화된다.

### 수동 검증 체크리스트
- 헤더 버튼과 Ctrl+R로 검토 모드를 켜고 끌 수 있다.
- Up/Down/K/J로 선택만 이동하고 재생은 자동 시작되지 않는다.
- Space, Left/Right, H/L이 플레이어 제어와 충돌하지 않는다.
- 0-5, U/R/N, F, D, S가 현재 primary 항목에만 적용된다.
- 검색, 태그, rename, batch rename, settings 입력 중에는 검토 단축키가 실행되지 않는다.

### 제외
- 사용자 정의 단축키 편집기
- 게임패드/리모컨 입력
- 전체 UI 재설계

---

## M28. UI As-Is 캡처 및 가시성/사용성 개선

### 목표
현재 UI를 Figma에 As-Is 기준점으로 보존하고, 실제 화면 기준으로 가시성/사용성 개선 범위를 정리한다.

### 범위
- Figma 새 파일 `Pickle UI As-Is Audit - 2026-04-22`에 `As-Is` 페이지 생성
- synthetic/demo 데이터 기반으로 현재 QML UI 주요 상태 20개 캡처
- 캡처 그룹: Main, Library & Detail, Playback, Dialogs, Stress & Responsive
- 가시성 기준 검토: 대비, 글자 크기, truncation/overflow, 선택/상태 구분, disabled/active 상태, 진행/상태 메시지
- 사용성 기준 검토: 스캔/필터/검토/일괄 편집/이름 변경 흐름, keyboard focus, dialog 탈출성, destructive action 명확성, 빠른 검토 모드 발견성
- Figma `Findings` 섹션과 roadmap 후속 마일스톤으로 개선 범위 연결

### Figma 산출물
- 파일: [Pickle UI As-Is Audit - 2026-04-22](https://www.figma.com/design/uyNQTIi2CaSoi6GqEJhdTg?node-id=3-2)
- 페이지: `As-Is`
- 캡처 수: 20
- 사용자 실제 미디어/개인 라이브러리 경로는 사용하지 않는다.
- OS-native Folder/File dialog는 platform-owned UI로 기록하고 자동 캡처 범위에서는 제외한다.

### 1차 검토 결과
- P1 Visibility: Dialog와 일부 Qt Quick Controls가 dark shell 안에서 light/native styling으로 보이며 시각 체계가 섞인다.
- P1 Usability: 우측 rail에 필터, 리스트, 상태, 상세, batch action이 조밀하게 모여 긴 파일명/태그 처리 여유가 부족하다.
- P2 Visibility: 재생/반복 액션이 짧은 텍스트 버튼 위주라 빠른 스캔성이 낮고 disabled 상태 구분이 약하다.
- P2 Usability: Review Mode 상태 표시는 있으나 shortcut scope와 현재 적용 대상 피드백이 더 강해야 한다.

### 완료 기준
- Figma `As-Is` 페이지에 주요 화면/상태 캡처와 Findings 요약이 존재한다.
- 캡처 목록이 main workspace, library/detail, playback/review, dialogs, responsive stress를 포함한다.
- roadmap에 M29-M31 UI 개선 후보가 실행 가능한 단위로 정리된다.
- M28은 UI 구현 완료가 아니라 As-Is 캡처/감사/후속계획 마일스톤으로 명확히 남는다.

### 수동 검증 체크리스트
- Figma 파일을 열어 `As-Is / Screenshot Gallery + Findings` 프레임이 보이는지 확인한다.
- 20개 캡처가 모두 로드되고 깨진 이미지가 없는지 확인한다.
- 긴 파일명, 긴 태그, 좁은 viewport, batch rename dialog가 검토 가능한 크기로 보이는지 확인한다.
- Findings 요약이 가시성/사용성 기준으로 구분되어 있는지 확인한다.
- 후속 구현은 QML 전체 재설계가 아니라 M29-M31 단계별 polish로 진행한다.

### 제외
- 이번 단계에서 실제 QML UI 수정은 하지 않는다.
- 디자인 시스템 전면 도입이나 전체 레이아웃 재작성은 하지 않는다.
- 사용자 개인 미디어 라이브러리를 캡처하지 않는다.

---

## M29 이후 후속 후보

### UI 개선 후보
- M29 Visibility polish: 대비, spacing, typography, overflow, 상태 표시, light/native dialog styling 정리
- M30 Usability flow polish: 필터, 검토, batch, rename, focus/keyboard 흐름 정리
- M31 Visual system 정리: 재사용 UI 토큰/컴포넌트, 반응형 제약, screenshot smoke 기반 회귀 점검

### 기능/품질 후보

- FTS5 기반 검색 전환
- 실제 샘플 미디어 기반 Qt Multimedia playback smoke test
- ffmpeg/ffprobe optional integration test
- QML screenshot smoke test
- 대량 썸네일 일괄 생성 최적화
- 고급 saved search/smart collection builder
- 휴지통 통합 또는 실제 삭제 workflow
- 영상 유사도 기반 중복 탐지

---

## Codex 작업 순서 추천

1. M22 스마트 뷰 및 고급 필터
2. M23 다중 선택 및 일괄 태그
3. M24 일괄 이름 변경
4. M27 키보드 중심 검토 모드
5. M28 UI As-Is 캡처 및 가시성/사용성 개선
6. M29 Visibility polish
7. M30 Usability flow polish
8. M31 Visual system 정리

---

## 각 마일스톤의 공통 보고 형식

Codex는 각 마일스톤 종료 시 아래 형식으로 보고한다.

1. 변경한 파일 목록
2. 무엇을 왜 바꿨는지
3. 직접 확인한 항목
4. 남은 이슈
5. 다음 추천 작업
