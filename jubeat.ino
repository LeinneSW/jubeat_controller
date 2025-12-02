#include <Joystick.h>

// SERVICE, TEST 버튼
const uint8_t SW_PINS[] = {14, 15};

/*
┌──────┬──────┬──────┬──────┐
│ B1   │ A1   │ C1   │ D1   │
│ G0   │ G1   │ G28  │ G27  │
├──────┼──────┼──────┼──────┤
│ B2   │ A2   │ C2   │ D2   │
│ G2   │ G3   │ G26  │ G22  │
├──────┼──────┼──────┼──────┤
│ B3   │ A3   │ C3   │ D3   │
│ G4   │ G5   │ G21  │ G20  │
├──────┼──────┼──────┼──────┤
│ B4   │ A4   │ C4   │ D4   │
│ G6   │ G7   │ G19  │ G18  │
└──────┴──────┴──────┴──────┘
*/
const uint8_t BTN_PINS[] =    {
    0, 1, 28, 27,
    2, 3, 26, 22,
    4, 5, 21, 20,
    6, 7, 19, 18
};

/*
실제 회로 구성을 봐야지 판단 가능
┌──────┬──────┬──────┬──────┐
│ A1   │ B1   │ D1   │ C1   │
│ G1   │ G0   │ G27  │ G28  │
├──────┼──────┼──────┼──────┤
│ A2   │ B2   │ D2   │ C2   │
│ G3   │ G2   │ G22  │ G26  │
├──────┼──────┼──────┼──────┤
│ A3   │ B3   │ D3   │ C3   │
│ G5   │ G4   │ G20  │ G21  │
├──────┼──────┼──────┼──────┤
│ A4   │ B4   │ D4   │ C4   │
│ G7   │ G6   │ G18  │ G19  │
└──────┴──────┴──────┴──────┘
*/
const uint8_t BTN_PINS_NEW[] =    {
    1, 0, 27, 28,
    3, 2, 22, 26,
    5, 4, 20, 21,
    7, 6, 18, 19
};
const uint8_t BTN_COUNT = sizeof(BTN_PINS) / sizeof(BTN_PINS[0]);

// 디바운스 처리(채터링 방지)
constexpr uint64_t PRESS_US   = 500;
constexpr uint64_t RELEASE_US = 1600;

// 전송 스케줄
constexpr uint64_t REPORT_MIN_PERIOD_US = 500;  // 2kHz polling

struct TouchDebounce{
    bool state;       // 현재 상태(true: 눌림)
    int64_t remainUs; // 상태 유지 누적 시간(us)
    uint64_t lastUs;  // 마지막 업데이트 시각(us)
};
TouchDebounce buttons[BTN_COUNT];

bool needSync = false;
uint64_t nextReportAt = 0;

inline void updateDebounce(uint8_t btnIndex, uint64_t now){
    auto& btn = buttons[btnIndex];
    int64_t delta = (int64_t) (now - btn.lastUs); // 래핑-세이프 차이
    btn.lastUs = now;
    
    bool pressedNow = digitalRead(BTN_PINS[btnIndex]) == LOW; // INPUT_PULLUP 기준
    if(btn.state){ // 현재 눌려진 상태(떼는것만 확인)
        if(!pressedNow){
            btn.remainUs -= delta;
        }else{ // 일시적 눌림 체크(채터링)
            auto newValue = btn.remainUs + delta / 3;
            btn.remainUs = min(0, newValue);
            return;
        }

        if(-btn.remainUs >= RELEASE_US){
            btn.state = false;
            btn.remainUs = 0;
        }
    }else{ // 현재 떼진 상태(누르는것만 판단)
        if(pressedNow){
            btn.remainUs += delta;
        }else{ // 일시적 떼짐 체크(채터링)
            auto newValue = btn.remainUs - delta / 3;
            btn.remainUs = max(0, newValue);
            return;
        }

        if(btn.remainUs >= PRESS_US){
            btn.state = true;
            btn.remainUs = 0;
        }
    }
}

void setup(){
    Joystick.begin();
    Joystick.useManualSend(true);
    for(uint8_t i = 0; i < BTN_COUNT; ++i){
        buttons[i] = {false, 0, 0};
        pinMode(BTN_PINS[i], INPUT_PULLUP);
    }
}

void loop(){
    uint64_t now = micros();
    for(uint8_t i = 0; i < BTN_COUNT; ++i){
        bool before = buttons[i].state;
        updateDebounce(i, now);
        if(buttons[i].state != before){
            needSync = true;
            Joystick.setButton(i, buttons[i].state);
        }
    }

    if(needSync && (long) (now - nextReportAt) >= 0){ // overflow 방지를 위해 signed long으로 변환
        needSync = false;
        Joystick.send_now();
        nextReportAt = now + REPORT_MIN_PERIOD_US; // 다음 전송 최소 간격(500us)
    }
}
