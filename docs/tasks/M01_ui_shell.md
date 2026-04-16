# M01 UI Shell

## 목표
프로젝트 피클의 **메인 화면 뼈대**를 먼저 고정한다.

이 단계에서는 실제 동작보다 **UI 구조 확정**이 목적입니다.
중앙 플레이어, 하단 재생바, 우측 리스트, 우측 상세 패널의 배치를 완성합니다.

---

## 범위

반드시 포함:
- 중앙 플레이어 영역 placeholder
- 하단 재생바 placeholder
- 우측 상단 라이브러리 리스트 UI
- 우측 하단 상세 정보 패널 UI
- mock 데이터 10개 표시
- 리스트 선택 시 우측 하단 정보 변경

선택적으로 포함 가능:
- 상단 툴바 placeholder
- 검색창 placeholder
- 정렬 드롭다운 placeholder

---

## 이번 단계에서 하지 말 것

금지 범위:
- 실제 동영상 재생
- 실제 DB 연결
- 실제 폴더 스캔
- 실제 파일명 변경
- 실제 ffprobe/ffmpeg 호출

즉, **보이는 화면과 선택 흐름만** 만듭니다.

---

## 화면 구조

### 중앙
- 검은 배경 또는 placeholder 박스
- "Player Area" 같은 텍스트

### 하단
- 재생/일시정지 버튼 placeholder
- 시크바 placeholder
- 볼륨/배속 placeholder

### 우측 상단
- mock 라이브러리 리스트
- 항목명, 크기, 길이 같은 더미 텍스트
- 썸네일 토글 placeholder

### 우측 하단
- 선택 파일명
- 더미 해상도, 길이, 코덱, 설명
- 태그 영역 placeholder

---

## 구현 메모

### 데이터
- 실제 DB 대신 C++ mock model 또는 QML에서 소비 가능한 mock model 사용
- 가능하면 `QAbstractListModel` 형태를 미리 잡는 것을 권장

### QML 구조
권장 파일 분리:
- `qml/Main.qml`
- `qml/components/LibraryPanel.qml`
- `qml/components/InfoPanel.qml`
- `qml/components/PlaybackBar.qml`
- `qml/pages/PlayerPage.qml`

### 상호작용
- 리스트 클릭 시 현재 선택 항목이 바뀌어야 함
- 우측 하단 패널은 현재 선택 항목의 mock 정보 표시

---

## 완료 기준

1. 앱이 빌드된다.
2. 메인 화면 4영역 레이아웃이 보인다.
3. 리스트에 mock 데이터 10개가 보인다.
4. 리스트 선택 시 우측 하단 정보가 바뀐다.
5. 창 크기 변경 시 레이아웃이 심하게 깨지지 않는다.

---

## 수동 검증 체크리스트

- [ ] 중앙/하단/우측 상단/우측 하단 구조가 모두 보인다
- [ ] mock 리스트 10개 표시
- [ ] 항목 클릭 가능
- [ ] 선택된 항목의 정보가 패널에 반영됨
- [ ] 창 리사이즈 시 기본 사용성 유지

---

## 작업 후 보고 형식

1. 변경한 파일 목록
2. 무엇을 왜 바꿨는지
3. 실행 확인 결과
4. 남은 이슈
5. 다음 추천 작업

---

## 추천 커밋 메시지

```text
feat: add initial UI shell with mock library data
```
