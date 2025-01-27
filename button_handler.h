#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include <Arduino.h>
#include "driver/touch_pad.h"  // 添加触摸传感器驱动头文件
//#include "soc/rtc_periph.h"    // 添加RTC外设头文件
//#include "soc/sens_periph.h"   // 添加传感器外设头文件
#include "config.h"

class ButtonHandler {
public:
    enum ButtonAction {
        NONE,
        VOLUME_UP,
        VOLUME_DOWN,
        UP_AND_DOWN,
        NEXT,
        ALL_BUTTONS
    };

    // 定义回调函数类型
    typedef void (*ButtonCallback)();
    
private:
    const unsigned long REPEAT_DELAY = 100;      // 连按间隔(ms)
    const unsigned long FIRST_REPEAT_DELAY = 500;// 首次连击前的延迟
    const unsigned long RELEASE_PROTECT = 200;   // 释放保护时间(ms)
    const unsigned long COMBO_WINDOW = 800;      // 组合按键时间窗口(ms)

    volatile bool touchVolUpdetected = false;
    volatile bool touchVolDowndetected = false;
    volatile bool touchNextdetected = false;
    bool lastTouchUp = false;
    bool lastTouchDown = false;
    bool lastTouchNext = false;
    bool buttonReleased = true;
    bool canChangeAction = false;
    bool isFirstRepeat = true;
    volatile bool buttonActive = false;

    unsigned long lastButtonTime = 0;
    unsigned long lastRepeatTime = 0;
    unsigned long firstButtonTime = 0;
    unsigned long lastVolumeTime = 0;

    ButtonAction currentAction = NONE;

    // 回调函数指针
    ButtonCallback onVolumeUp = nullptr;
    ButtonCallback onVolumeDown = nullptr;
    ButtonCallback onUpAndDown = nullptr;
    ButtonCallback onNext = nullptr;
    ButtonCallback onAllButtons = nullptr;

    // 添加互斥锁
    portMUX_TYPE buttonMux = portMUX_INITIALIZER_UNLOCKED;
    volatile uint8_t buttonStates = 0;  // 使用位来存储按钮状态

public:
    ButtonHandler() {
        buttonStates = 0;
    }

    // 新增初始化函数
    void init() {
        // 初始化触摸传感器
        touch_pad_init();
        touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);
        
        // 设置触摸传感器滤波器
        touch_pad_filter_start(TOUCH_FILTER);
        
        // 等待滤波器初始化完成
        delay(100);

        static ButtonHandler* instance = this;
        
        // 初始化触摸中断
        touchAttachInterrupt(ButtonUp, [](){ 
            instance->handleTouchInterrupt(0);
        }, TOUCH_THRESHOLD);
        
        touchAttachInterrupt(ButtonDown, [](){ 
            instance->handleTouchInterrupt(1);
        }, TOUCH_THRESHOLD);
        
        touchAttachInterrupt(ButtonNext, [](){ 
            instance->handleTouchInterrupt(2);
        }, TOUCH_THRESHOLD);
    }

    void setCallbacks(ButtonCallback volumeUp, ButtonCallback volumeDown,
                     ButtonCallback upAndDown, ButtonCallback next,
                     ButtonCallback allButtons) {
        onVolumeUp = volumeUp;
        onVolumeDown = volumeDown;
        onUpAndDown = upAndDown;
        onNext = next;
        onAllButtons = allButtons;
    }

    void update() {
        if (buttonActive) {  // 只在有按钮活动时处理
            updateButtonStates();
            handleButtons();
        }
    }

    // 新增：获取按钮活动状态的方法
    bool isActive() const {
        return buttonActive;
    }

private:
    void handleTouchInterrupt(int button) {
        int touchValue = touchRead(button == 0 ? ButtonUp : 
                                 button == 1 ? ButtonDown : ButtonNext);
        if (touchValue < TOUCH_THRESHOLD) {
            portENTER_CRITICAL_ISR(&buttonMux);
            buttonStates |= (1 << button);
            buttonActive = true;
            portEXIT_CRITICAL_ISR(&buttonMux);
        }
    }

    void updateButtonStates() {
        unsigned long currentTime = millis();
        
        portENTER_CRITICAL(&buttonMux);
        bool upPressed = buttonStates & 0x01;
        bool downPressed = buttonStates & 0x02;
        bool nextPressed = buttonStates & 0x04;
        portEXIT_CRITICAL(&buttonMux);

        int touchUpVal = touchRead(ButtonUp);
        int touchDownVal = touchRead(ButtonDown);
        int touchNextVal = touchRead(ButtonNext);
        
        portENTER_CRITICAL(&buttonMux);
        if (touchUpVal >= TOUCH_THRESHOLD && (buttonStates & 0x01)) {
            buttonStates &= ~0x01;
            touchVolUpdetected = false;
        } else if (upPressed) {
            touchVolUpdetected = true;
        }
        
        if (touchDownVal >= TOUCH_THRESHOLD && (buttonStates & 0x02)) {
            buttonStates &= ~0x02;
            touchVolDowndetected = false;
        } else if (downPressed) {
            touchVolDowndetected = true;
        }
        
        if (touchNextVal >= TOUCH_THRESHOLD && (buttonStates & 0x04)) {
            buttonStates &= ~0x04;
            touchNextdetected = false;
        } else if (nextPressed) {
            touchNextdetected = true;
        }

        if (buttonStates == 0) {
            buttonActive = false;
        }
        portEXIT_CRITICAL(&buttonMux);

        // 使用当前状态检测边沿
        bool touchUpEdge = touchVolUpdetected && !lastTouchUp;
        bool touchDownEdge = touchVolDowndetected && !lastTouchDown;
        bool touchNextEdge = touchNextdetected && !lastTouchNext;

        lastTouchUp = touchVolUpdetected;
        lastTouchDown = touchVolDowndetected;
        lastTouchNext = touchNextdetected;

        // 检测所有按钮是否释放
        if (!touchVolUpdetected && !touchVolDowndetected && !touchNextdetected) {
            buttonReleased = true;
            currentAction = NONE;
            canChangeAction = false;
            isFirstRepeat = true;
            buttonActive = false;  // 所有按钮释放时关闭活动状态
        }
        // 检测新的按钮按下
        else if (touchUpEdge || touchDownEdge || touchNextEdge) {
            if (buttonReleased) {
                firstButtonTime = currentTime;
                lastButtonTime = currentTime;
                lastVolumeTime = currentTime;
                buttonReleased = false;
                canChangeAction = true;
                isFirstRepeat = true;
                currentAction = NONE;
            }
        }
        // 检测连续按住
        else if (!buttonReleased && (currentAction == VOLUME_UP || currentAction == VOLUME_DOWN)) {
            unsigned long requiredDelay = isFirstRepeat ? FIRST_REPEAT_DELAY : REPEAT_DELAY;
            if (currentTime - lastVolumeTime >= requiredDelay) {
                lastVolumeTime = currentTime;
                canChangeAction = true;
                isFirstRepeat = false;
            } else {
                canChangeAction = false;
            }
        }
    }

    void handleButtons() {
        if (touchVolUpdetected && touchVolDowndetected && touchNextdetected) {
            handleAllButtonsPressed();
        }
        else if (touchVolUpdetected && touchVolDowndetected) {
            handleUpAndDownButtons();
        }
        else if (touchVolUpdetected) {
            handleVolumeUp();
        }
        else if (touchVolDowndetected) {
            handleVolumeDown();
        }
        else if (touchNextdetected) {
            handleNextButton();
        }
    }

    void handleAllButtonsPressed() {
        // 检查是否在组合键时间窗口内
        if (millis() - firstButtonTime <= COMBO_WINDOW) {
            if ((buttonReleased && currentAction == NONE) || 
                (canChangeAction && (currentAction == VOLUME_UP || 
                                   currentAction == VOLUME_DOWN || 
                                   currentAction == NEXT || 
                                   currentAction == UP_AND_DOWN))) {
                currentAction = ALL_BUTTONS;
                if (onAllButtons) onAllButtons();
            }
        }
    }

    void handleUpAndDownButtons() {
        // 检查是否在组合键时间窗口内
        if (millis() - firstButtonTime <= COMBO_WINDOW) {
            // 允许从音量连击状态切换到静音
            if (buttonReleased && currentAction == NONE || 
                currentAction == VOLUME_UP || 
                currentAction == VOLUME_DOWN) {
                currentAction = UP_AND_DOWN;
                canChangeAction = false;  // 防止继续触发音量连击
                if (onUpAndDown) onUpAndDown();
            }
        }
    }

    void handleVolumeUp() {
        // 如果检测到另一个音量键被按下，不触发音量增加
        if (touchVolDowndetected && millis() - firstButtonTime <= COMBO_WINDOW) {
            return;
        }

        if (currentAction != UP_AND_DOWN && 
            currentAction != ALL_BUTTONS &&
            canTriggerVolume()) {
            currentAction = VOLUME_UP;
            if (onVolumeUp) onVolumeUp();
        }
    }

    void handleVolumeDown() {
        // 如果检测到另一个音量键被按下，不触发音量减小
        if (touchVolUpdetected && millis() - firstButtonTime <= COMBO_WINDOW) {
            return;
        }

        if (currentAction != UP_AND_DOWN && 
            currentAction != ALL_BUTTONS &&
            canTriggerVolume()) {
            currentAction = VOLUME_DOWN;
            if (onVolumeDown) onVolumeDown();
        }
    }

    void handleNextButton() {
        if (currentAction != UP_AND_DOWN && 
            currentAction != ALL_BUTTONS && 
            !buttonReleased && 
            currentAction == NONE) {
            currentAction = NEXT;
            if (onNext) onNext();
        }
    }

    bool canTriggerVolume() {
        if (buttonReleased) {
            return false;  // 按钮释放时不触发
        }
        
        if (currentAction == NONE) {
            return canChangeAction;  // 首次按下时触发
        }
        
        if (currentAction == VOLUME_UP || currentAction == VOLUME_DOWN) {
            return canChangeAction;  // 连击时只在允许时触发
        }
        
        return false;
    }
};

#endif 