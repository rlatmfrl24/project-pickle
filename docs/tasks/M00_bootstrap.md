# M00 Bootstrap

## 목표
Qt 6 기반의 **최소 실행 가능한 프로젝트 골격**을 만든다.

이 단계의 목적은 기능 구현이 아니라,
- 프로젝트가 열린다
- 빌드된다
- 실행된다
- 문서와 폴더 구조가 정리된다
를 만족하는 것입니다.

---

## 범위

반드시 포함:
- Qt Quick Application 기반 프로젝트 생성
- CMake 기반 프로젝트 구성
- `src`, `qml`, `docs`, `assets`, `tests` 디렉토리 생성
- `README.md`, `AGENTS.md`, `docs/architecture.md`, `docs/roadmap.md` 유지
- 최소한의 `main.cpp`와 `Main.qml` 구성
- 앱 실행 시 빈 메인 창 표시

선택적으로 포함 가능:
- 기본 앱 이름/아이콘 placeholder
- QML import 구조 정리
- `resources.qrc` 또는 Qt resource 등록 준비

---

## 이번 단계에서 하지 말 것

금지 범위:
- 실제 동영상 재생
- SQLite 연결
- 디렉토리 스캔
- 태그/설명 기능
- 썸네일/스냅샷 기능
- 복잡한 UI 구현

즉, **기능이 아니라 뼈대만** 만듭니다.

---

## 권장 폴더 구조

```text
pickle/
  CMakeLists.txt
  README.md
  AGENTS.md
  docs/
    architecture.md
    roadmap.md
  src/
    main.cpp
  qml/
    Main.qml
    components/
    pages/
  assets/
    icons/
  tests/
```

---

## 구현 메모

### CMake
아래 Qt 모듈을 사용할 준비를 합니다.
- Qt6::Quick
- Qt6::Multimedia
- Qt6::Sql

단, 이번 단계에서는 실제 사용하지 않아도 됩니다.
나중 단계에서 붙이기 쉽도록 CMake 구조만 정리해도 충분합니다.

### main.cpp
역할:
- `QGuiApplication` 생성
- `QQmlApplicationEngine` 생성
- `qml/Main.qml` 로드
- 로드 실패 시 종료

### Main.qml
역할:
- 최소한의 ApplicationWindow 생성
- 앱 제목 표시
- 추후 레이아웃이 들어갈 수 있도록 루트 영역 확보

---

## 완료 기준

아래를 모두 만족해야 완료입니다.

1. 프로젝트가 Qt Creator에서 열린다.
2. CMake configure가 성공한다.
3. Debug build가 성공한다.
4. 앱 실행 시 기본 창이 뜬다.
5. 저장소에 기본 폴더 구조가 존재한다.
6. 기존 문서 4개가 유지된다.

---

## 수동 검증 체크리스트

- [ ] Qt Creator에서 프로젝트 열기 성공
- [ ] 적절한 Desktop Kit 선택 가능
- [ ] Configure without errors
- [ ] Build without errors
- [ ] Run 시 빈 창 표시
- [ ] 종료 시 비정상 종료 없음

---

## 작업 후 보고 형식

아래 형식으로 보고해야 합니다.

1. 변경한 파일 목록
2. 무엇을 왜 바꿨는지
3. 빌드/실행 확인 결과
4. 남은 이슈
5. 다음 추천 작업

---

## 추천 커밋 메시지

```text
chore: bootstrap Qt Quick project skeleton
```
