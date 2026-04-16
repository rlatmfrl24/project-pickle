# M02 DB Initialization

## 목표
SQLite DB를 생성하고, 프로젝트 피클의 **기본 스키마와 초기화 루틴**을 만든다.

이 단계의 목적은 실제 스캔이 아니라,
- 앱 시작 시 DB가 준비되고
- 필요한 테이블이 자동 생성되며
- 다음 단계에서 스캔 결과를 저장할 수 있게 하는 것입니다.

---

## 범위

반드시 포함:
- SQLite DB 파일 생성
- DB 연결 서비스 작성
- migration 테이블 생성
- 기본 테이블 생성
- 앱 시작 시 DB 초기화 호출

초기 테이블:
- `media_files`
- `tags`
- `media_tags`
- `snapshots`
- `scan_roots`
- `app_settings`

---

## 이번 단계에서 하지 말 것

금지 범위:
- 실제 디렉토리 스캔
- 실제 ffprobe 메타데이터 적재
- 실제 플레이어 상태 저장
- 복잡한 검색/정렬 쿼리 최적화
- 실제 파일명 변경

즉, **DB가 준비되는 단계까지만** 구현합니다.

---

## 구현 메모

### 권장 클래스
- `DatabaseService`
- `MigrationService`

### 권장 책임
#### DatabaseService
- DB 연결 열기
- 연결 상태 확인
- DB 경로 제공

#### MigrationService
- migration 테이블 생성
- 초기 스키마 적용
- 중복 실행 시 안전하게 통과

### DB 파일 위치
개발 단계에서는 다음 중 하나를 택합니다.
- 앱 실행 디렉토리 아래
- 사용자 AppData 아래

처음에는 단순화를 위해 **개발 중 눈에 보이는 위치**를 선택해도 됩니다.

---

## 초기 스키마 예시

### media_files
- id
- file_name
- file_path
- file_extension
- file_size
- modified_at
- hash_value
- duration_ms
- bitrate
- frame_rate
- width
- height
- video_codec
- audio_codec
- description
- review_status
- rating
- thumbnail_path
- last_played_at
- last_position_ms
- created_at
- updated_at

### tags
- id
- name
- color
- created_at

### media_tags
- media_id
- tag_id

### snapshots
- id
- media_id
- image_path
- timestamp_ms
- created_at

### scan_roots
- id
- root_path
- is_enabled
- last_scanned_at
- created_at

### app_settings
- key
- value
- updated_at

---

## 완료 기준

1. 앱 실행 시 DB 파일이 생성된다.
2. DB 연결이 성공한다.
3. migration 테이블이 생성된다.
4. 기본 테이블이 자동 생성된다.
5. 앱 재실행 시 기존 DB를 재사용한다.
6. 테이블 생성이 중복 실행되어도 오류가 나지 않는다.

---

## 수동 검증 체크리스트

- [ ] 앱 실행 후 DB 파일 생성됨
- [ ] DB 브라우저로 테이블 존재 확인 가능
- [ ] 재실행 시 새로운 DB를 덮어쓰지 않음
- [ ] 로그에 DB 초기화 성공/실패가 보임
- [ ] 기본 실행 흐름이 깨지지 않음

---

## 작업 후 보고 형식

1. 변경한 파일 목록
2. 무엇을 왜 바꿨는지
3. DB 생성/초기화 확인 결과
4. 남은 이슈
5. 다음 추천 작업

---

## 추천 커밋 메시지

```text
feat: initialize sqlite database and baseline schema
```
