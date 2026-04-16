# Codex Handoff Guide

이 문서는 **프로젝트 피클을 Codex로 단계적으로 구현하기 위한 운영 가이드**입니다.

목표는 단순합니다.
- Qt 프로젝트를 사람이 통제 가능한 작은 단위로 쪼갠다.
- Codex에는 항상 **하나의 명확한 작업**만 넘긴다.
- 매 작업 뒤에는 **로컬 빌드/실행 확인**을 한다.
- 문제가 생기면 한 단계 전 상태로 쉽게 돌아갈 수 있게 한다.

---

## 1. 추천 작업 방식

### 가장 권장하는 방식
- **메인 IDE**: Qt Creator
- **Codex 실행 방식**: Codex CLI 또는 Codex 앱
- **문서 기준점**: `AGENTS.md`, `docs/architecture.md`, `docs/roadmap.md`, `docs/tasks/*.md`

이 프로젝트는 Qt/QML/C++ 기반의 Windows 데스크톱 앱이므로, 실제 빌드/실행은 Qt Creator에서 확인하는 편이 가장 편합니다.
Codex는 **문서를 읽고 코드 수정/생성**을 맡기고, 최종 실행 확인은 사람이 직접 하는 흐름이 안정적입니다.

### 왜 이 방식이 좋은가
- Qt 프로젝트는 Qt Creator와 실제 Kit/MSVC 설정 확인이 중요하다.
- 작업 단위를 잘게 나누면 Codex가 불필요한 리팩터링을 덜 한다.
- 플레이어/DB/스캔 기능은 서로 영향이 크기 때문에, 작은 단계로 진행해야 디버깅이 쉽다.

---

## 2. 작업 전 준비

아래 문서가 저장소에 있어야 합니다.

- `README.md`
- `AGENTS.md`
- `docs/architecture.md`
- `docs/roadmap.md`
- `docs/tasks/M00_bootstrap.md`
- `docs/tasks/M01_ui_shell.md`
- `docs/tasks/M02_db_init.md`

권장 폴더 구조:

```text
pickle/
  CMakeLists.txt
  README.md
  AGENTS.md
  docs/
    architecture.md
    roadmap.md
    codex_handoff.md
    tasks/
      M00_bootstrap.md
      M01_ui_shell.md
      M02_db_init.md
    prompts/
      M00_bootstrap_prompt.txt
      M01_ui_shell_prompt.txt
      M02_db_init_prompt.txt
```

---

## 3. Codex에 일을 넘기는 기본 규칙

### 규칙 1. 한 번에 하나의 마일스톤만
좋은 예:
- "M00 부트스트랩만 구현해줘"
- "M01 UI 쉘만 구현해줘"

나쁜 예:
- "앱 전체를 다 만들어줘"
- "플레이어, DB, 스캔, 태그, 썸네일까지 한 번에 만들어줘"

### 규칙 2. 범위 밖 구현 금지
항상 아래를 명시하세요.
- 이번 단계의 목표
- 이번 단계에서 **하지 말아야 할 것**
- 완료 기준

### 규칙 3. 보고 형식 고정
항상 아래 형식으로 보고하게 하세요.
1. 변경한 파일 목록
2. 무엇을 왜 바꿨는지
3. 직접 확인한 항목
4. 남은 이슈
5. 다음 추천 작업

### 규칙 4. 빌드 가능한 상태 유지
Codex가 작업을 끝냈다고 해도, 반드시 로컬에서
- 프로젝트 열기
- Build
- Run
을 직접 확인하세요.

---

## 4. 실제 운영 루프

매 작업은 아래 순서대로 반복합니다.

1. `docs/tasks/Mxx_*.md`를 읽는다.
2. 해당 마일스톤용 prompt 파일을 Codex에 전달한다.
3. Codex가 수정한 파일과 보고 내용을 확인한다.
4. Qt Creator에서 빌드한다.
5. 실행해서 완료 기준을 체크한다.
6. 만족하면 커밋한다.
7. 다음 마일스톤으로 넘어간다.

---

## 5. 권장 커밋 단위

커밋은 마일스톤 또는 하위 작업 기준으로 작게 유지합니다.

좋은 예:
- `chore: bootstrap Qt Quick project skeleton`
- `feat: add mock library panel and info panel`
- `feat: initialize sqlite database and migrations`

나쁜 예:
- `update project`
- `many fixes`

---

## 6. 추천 진행 순서

### 1차
- M00 부트스트랩

### 2차
- M01 UI 쉘

### 3차
- M02 DB 초기화

이 3단계가 끝나면, 그 다음부터는 아래 순서로 진행합니다.
- M03 디렉토리 스캔
- M04 라이브러리 리스트 연결
- M05 기본 재생 기능

---

## 7. 사람이 직접 확인해야 하는 항목

Codex가 잘해도 아래는 반드시 직접 확인하세요.

### 부트스트랩 단계
- 프로젝트가 실제로 열리는가
- Kit 설정이 올바른가
- 기본 창이 뜨는가

### UI 단계
- 레이아웃이 무너지지 않는가
- 리스트 선택 시 정보 패널이 바뀌는가
- 창 크기를 바꿔도 UI가 너무 깨지지 않는가

### DB 단계
- DB 파일이 생성되는가
- 재실행해도 DB가 유지되는가
- 테이블 생성 실패 로그가 없는가

---

## 8. Codex에 넘길 때 붙이는 기본 문장

아래 문장을 매번 프롬프트 앞이나 뒤에 붙이면 좋습니다.

```text
반드시 AGENTS.md와 docs/architecture.md, docs/roadmap.md를 먼저 읽고 작업하세요.
이번 작업은 지정된 마일스톤 범위만 구현하세요.
범위를 벗어난 리팩터링은 하지 마세요.
항상 빌드 가능한 상태를 유지하세요.
작업 후에는 변경 파일 목록, 변경 이유, 확인 방법, 남은 이슈를 보고하세요.
```

---

## 9. 문제 발생 시 대응 원칙

### 빌드가 깨졌을 때
- 마지막 Codex 변경 범위만 확인한다.
- CMake 수정인지, include 누락인지, Qt module 누락인지 먼저 본다.
- 한 번에 여러 기능을 합치지 않는다.

### UI가 이상할 때
- QML에서 비즈니스 로직이 들어갔는지 본다.
- Layout과 anchors를 동시에 과도하게 섞지 않았는지 본다.
- mock 데이터부터 다시 확인한다.

### DB가 꼬였을 때
- migration이 idempotent한지 확인한다.
- 기존 컬럼/테이블 존재 여부를 확인한다.
- 개발 중에는 테스트 DB를 지우고 재생성할 수 있게 한다.

---

## 10. 이번 프로젝트에서 권장하는 Codex 사용 우선순위

1. **Codex CLI**: 저장소 루트에서 작은 작업을 순차 처리할 때
2. **Codex 앱**: 작업을 분기하거나 병렬로 비교할 때
3. **IDE 확장**: 보조 수단

Qt 프로젝트에서는 실제 실행과 디버깅이 Qt Creator 중심이므로,
**Codex는 코드 생성/수정 담당**, **실행 확인은 로컬 Qt Creator 담당**으로 나누는 것이 가장 현실적입니다.
