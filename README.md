# XIAO ESP32-S3 실습 모음

Seeed Studio XIAO ESP32-S3 (Sense)로 만든 예제들입니다. Arduino CLI + esp32 코어 3.3.x 기준.

| 폴더 | 내용 |
|---|---|
| `XiaoBlink` | 내장 LED(GPIO21) 1초 깜빡이기 |
| `XiaoButtonLed` | BOOT 버튼 누르면 LED 켜기 |
| `XiaoTouchLed` | D0 터치센서 만지면 LED 켜기 |
| `XiaoWebDashboard` | 같은 와이파이에서 웹으로 LED 제어 + 터치 값 보기 |
| `XiaoSupabase` + `supabase/` | Supabase로 외부망에서 LED 제어 + 터치 값 보기 (+ 같은 망 카메라 스트리밍) |
| `XiaoAP` | 보드가 직접 와이파이(AP)를 만들어 인터넷 없이 폰으로 제어 (`http://192.168.4.1`) |
| `XiaoCam` | 카메라 스트리밍만 (같은 와이파이) |
| `XiaoCamDiag` | 카메라 I2C 진단 (센서 연결 확인용) |
| `XiaoBLE` | BLE로 폰에서 LED 제어 + 터치 값 보기 (`ble.html`, 안드로이드 크롬) |

웹 대시보드: https://gksdnd0519-commits.github.io/xiao-dashboard/

## 사용법

1. 코드의 `YOUR_WIFI_SSID` / `YOUR_WIFI_PASSWORD`를 내 2.4GHz 와이파이 정보로 바꿉니다.
2. 컴파일 & 업로드 (카메라 쓰는 예제는 `PSRAM=opi` 필요):

```
arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32S3:CDCOnBoot=cdc,PSRAM=opi XiaoAP
arduino-cli upload  -p COM4 --fqbn esp32:esp32:XIAO_ESP32S3:CDCOnBoot=cdc,PSRAM=opi XiaoAP
```

- 터치센서는 D0 핀(GPIO1)을 씁니다. 켤 때 1초 정도는 핀을 만지지 마세요(기준값 측정).
- 카메라 오류 `0x106`이 나오면 Sense 확장보드/카메라 모듈이 제대로 꽂혀 있는지 확인하세요 (`XiaoCamDiag`로 확인 가능).
