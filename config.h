#ifndef CONFIG_H
#define CONFIG_H

#define HOSTNAME "esp32noise"
const char * defaultBtName = "";// "Tmall Genie BOOM";
const char * prefKey = "enoise";
int volumeStep = 4;
const int LED_PIN = 18;

#define ButtonUp T0    // IO4
#define ButtonDown T2  // IO2
#define ButtonNext T3  // IO15
#define TOUCH_THRESHOLD 40 // Touch threshold
#define TOUCH_FILTER 10 // 10ms debounce filter


#endif