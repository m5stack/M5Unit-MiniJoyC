#include <M5StickCPlus.h>
#include "UNIT_MiniJoyC.h"

extern const unsigned char gImage_statusLed[];

#define POS_X 0
#define POS_Y 1

#define SAMPLE_TIMES 100
#define XY_CAL_TIMES 10

#define X_DEFAULT_MIN 500
#define X_DEFAULT_MAX 3600
#define Y_DEFAULT_MIN 500
#define Y_DEFAULT_MAX 3600
#define MID_DEFAULT   2000

uint8_t flag_sd_card = 0;
uint8_t flag_tf_test = 0;
char tf_write_buf[20];
char tf_read_buf[20];

uint8_t cal_state = 0;
enum { CAL_CENTER = 0, CAL_CENTER_DONE, CAL_XY, CAL_DONE, CAL_VERIFY };

TFT_eSprite tftSprite = TFT_eSprite(&M5.Lcd);
UNIT_JOYC sensor;

hw_timer_t *timer              = NULL;
unsigned int flag_lcd_interval = 0;
volatile uint8_t flag_1s       = 0;

hw_timer_t *timer2          = NULL;
volatile uint8_t flag_200ms = 0;

void IRAM_ATTR onTimer() {
    flag_lcd_interval++;
    flag_200ms = 1;
}

void cal_center(void) {
    while (!(sensor.begin(&Wire, JoyC_ADDR, 0, 26, 100000UL))) {
        delay(100);
    }
    int16_t adc_mid_x, adc_mid_y;
    uint16_t adc_cal_data[6];

    int16_t adc_x, adc_y;
    int16_t adc_max_x = 0;
    int16_t adc_max_y = 0;
    int16_t adc_min_x = 4095;
    int16_t adc_min_y = 4095;
    int16_t adc_x_set[SAMPLE_TIMES];
    int16_t adc_y_set[SAMPLE_TIMES];
    int32_t sum_x = 0;
    int32_t sum_y = 0;

    tftSprite.fillSprite(BLACK);
    tftSprite.setTextSize(1);
    tftSprite.setCursor(0, 0, 4);
    tftSprite.setTextColor(YELLOW);
    tftSprite.printf("CenterCal");
    tftSprite.drawLine(5, 30, 130, 30, ORANGE);
    tftSprite.setCursor(0, 30, 2);
    tftSprite.setTextSize(1);
    tftSprite.setTextColor(WHITE);
    for (int i = 0; i < SAMPLE_TIMES; i++) {
        adc_x_set[i] = sensor.getADCValue(POS_X);
        adc_y_set[i] = sensor.getADCValue(POS_Y);
        tftSprite.printf(".");
        tftSprite.pushSprite(0, 0);
    }
    for (int i = 0; i < SAMPLE_TIMES; i++) {
        if (adc_x_set[i] < adc_min_x) {
            adc_min_x = adc_x_set[i];
        }
        if (adc_x_set[i] > adc_max_x) {
            adc_max_x = adc_x_set[i];
        }
        if (adc_y_set[i] < adc_min_y) {
            adc_min_y = adc_y_set[i];
        }
        if (adc_y_set[i] > adc_max_y) {
            adc_max_y = adc_y_set[i];
        }
    }
    for (int i = 0; i < SAMPLE_TIMES; i++) {
        sum_x += adc_x_set[i];
        sum_y += adc_y_set[i];
    }

    adc_mid_x = (sum_x - adc_min_x - adc_max_x) / (SAMPLE_TIMES - 2);
    adc_mid_y = (sum_y - adc_min_y - adc_max_y) / (SAMPLE_TIMES - 2);

    sensor.setOneCalValue(4, adc_mid_x);
    sensor.setOneCalValue(5, adc_mid_y);

    tftSprite.fillSprite(BLACK);
    tftSprite.setTextSize(1);
    tftSprite.setCursor(0, 0, 4);
    tftSprite.setTextColor(YELLOW);
    tftSprite.printf("CenterCal");
    tftSprite.drawLine(5, 30, 130, 30, ORANGE);
    tftSprite.setCursor(0, 30, 2);
    tftSprite.setTextSize(2);
    tftSprite.setTextColor(WHITE);

    tftSprite.printf("RawX:%d", sensor.getADCValue(POS_X));
    tftSprite.setCursor(0, 60, 2);
    tftSprite.setTextSize(2);
    tftSprite.printf("RawY:%d", sensor.getADCValue(POS_Y));
    tftSprite.setTextSize(2);
    tftSprite.drawLine(5, 92 + 10, 130, 92 + 10, ORANGE);
    tftSprite.setTextColor(GREEN);
    tftSprite.setCursor(0, 95 + 10, 2);
    tftSprite.printf("CalX=%d", sensor.getCalValue(4));
    // tftSprite.drawLine(5, 127+15, 130, 127+15, ORANGE);
    tftSprite.setTextColor(GREEN);
    tftSprite.setCursor(0, 130 + 15, 2);
    tftSprite.printf("CalY=%d", sensor.getCalValue(5));
    tftSprite.drawLine(5, 162 + 20, 130, 162 + 20, ORANGE);
    tftSprite.setCursor(0, 165 + 20, 2);
    tftSprite.setTextColor(WHITE);
    tftSprite.printf("btnA");
    tftSprite.setTextColor(YELLOW);
    tftSprite.printf("Cal XY");
    tftSprite.setCursor(0, 190 + 20, 2);
    tftSprite.setTextColor(WHITE);
    tftSprite.printf("btnB");
    tftSprite.setTextColor(YELLOW);
    tftSprite.printf("Recal");
    tftSprite.pushSprite(0, 0);
    cal_state = CAL_CENTER_DONE;
}

void cal_center_done(void) {
    tftSprite.fillSprite(BLACK);
    tftSprite.setTextSize(1);
    tftSprite.setCursor(0, 0, 4);
    tftSprite.setTextColor(YELLOW);
    tftSprite.printf("CenterCal");
    tftSprite.drawLine(5, 30, 130, 30, ORANGE);
    tftSprite.setCursor(0, 30, 2);
    tftSprite.setTextSize(2);
    tftSprite.setTextColor(WHITE);

    tftSprite.printf("RawX:%d", sensor.getADCValue(POS_X));
    tftSprite.setCursor(0, 60, 2);
    tftSprite.setTextSize(2);
    tftSprite.printf("RawY:%d", sensor.getADCValue(POS_Y));
    tftSprite.setTextSize(2);
    tftSprite.drawLine(5, 92 + 10, 130, 92 + 10, ORANGE);
    tftSprite.setTextColor(GREEN);
    tftSprite.setCursor(0, 95 + 10, 2);
    tftSprite.printf("CalX=%d", sensor.getCalValue(4));
    // tftSprite.drawLine(5, 127+15, 130, 127+15, ORANGE);
    tftSprite.setTextColor(GREEN);
    tftSprite.setCursor(0, 130 + 15, 2);
    tftSprite.printf("CalY=%d", sensor.getCalValue(5));
    tftSprite.drawLine(5, 162 + 20, 130, 162 + 20, ORANGE);
    tftSprite.setCursor(0, 165 + 20, 2);
    tftSprite.setTextColor(WHITE);
    tftSprite.printf("btnA:");
    tftSprite.setTextColor(YELLOW);
    tftSprite.printf("CalXY");
    tftSprite.setCursor(0, 190 + 20, 2);
    tftSprite.setTextColor(WHITE);
    tftSprite.printf("btnB:");
    tftSprite.setTextColor(YELLOW);
    tftSprite.printf("Recal");
    tftSprite.pushSprite(0, 0);
}

void cal_xy(void) {
    while (!(sensor.begin(&Wire, JoyC_ADDR, 0, 26, 100000UL))) {
        delay(100);
    }
    int16_t adc_mid_x, adc_mid_y;
    uint16_t adc_cal_data[6];

    int16_t adc_x, adc_y;
    int16_t adc_max_x = 0;
    int16_t adc_max_y = 0;
    int16_t adc_min_x = 4095;
    int16_t adc_min_y = 4095;
    int16_t adc_x_set[SAMPLE_TIMES];
    int16_t adc_y_set[SAMPLE_TIMES];
    int32_t sum_x          = 0;
    int32_t sum_y          = 0;
    uint16_t cal_times     = 0;
    uint8_t cal_flag_x_min = 0;
    uint8_t cal_flag_x_max = 0;
    uint8_t cal_flag_y_min = 0;
    uint8_t cal_flag_y_max = 0;

    adc_min_x = sensor.getCalValue(0);
    adc_max_x = sensor.getCalValue(1);
    adc_min_y = sensor.getCalValue(2);
    adc_max_y = sensor.getCalValue(3);
    adc_mid_x = sensor.getCalValue(4);
    adc_mid_y = sensor.getCalValue(5);

    tftSprite.fillSprite(BLACK);
    tftSprite.setTextSize(1);
    tftSprite.setCursor(0, 0, 4);
    tftSprite.setTextColor(YELLOW);
    tftSprite.printf("XYCal");
    tftSprite.drawLine(5, 30, 130, 30, ORANGE);
    tftSprite.setCursor(0, 30, 2);
    tftSprite.setTextSize(1);
    tftSprite.setTextColor(WHITE);
    while (1) {
        adc_x = sensor.getADCValue(POS_X);
        adc_y = sensor.getADCValue(POS_Y);
        if (adc_x < adc_min_x) {
            adc_min_x = adc_x;
        }
        if (adc_x < X_DEFAULT_MIN) {
            cal_flag_x_min = 1;
        }
        if (adc_x > adc_max_x) {
            adc_max_x = adc_x;
        }
        if (adc_x > X_DEFAULT_MAX) {
            cal_flag_x_max = 1;
        }
        if (adc_y < adc_min_y) {
            adc_min_y = adc_y;
        }
        if (adc_y < Y_DEFAULT_MIN) {
            cal_flag_y_min = 1;
        }
        if (adc_y > adc_max_y) {
            adc_max_y = adc_y;
        }
        if (adc_y > Y_DEFAULT_MAX) {
            cal_flag_y_max = 1;
        }
        if (cal_flag_x_max && cal_flag_x_min && cal_flag_y_min &&
            cal_flag_y_max) {
            cal_times++;
            cal_flag_x_min = 0;
            cal_flag_x_max = 0;
            cal_flag_y_min = 0;
            cal_flag_y_max = 0;
        }
        if (cal_times > XY_CAL_TIMES) cal_times = XY_CAL_TIMES;
        if (flag_lcd_interval % 5 == 0) {
            M5.Lcd.drawBitmap(115, 5, 20, 20,
                              (uint16_t *)gImage_statusLed + (20 * 20 * 0));
        } else {
            tftSprite.fillSprite(BLACK);
            tftSprite.setTextSize(1);
            tftSprite.setCursor(0, 0, 4);
            tftSprite.setTextColor(YELLOW);
            tftSprite.printf("XYCal");
            tftSprite.drawLine(5, 30, 130, 30, ORANGE);
            tftSprite.setCursor(0, 30, 2);
            tftSprite.setTextSize(2);
            tftSprite.setTextColor(WHITE);
            tftSprite.printf("CalTimes:");
            tftSprite.setCursor(0, 60, 2);
            tftSprite.setTextSize(2);
            tftSprite.setTextColor(GREEN);
            tftSprite.printf("%d", XY_CAL_TIMES - cal_times);
            tftSprite.pushSprite(0, 0);
        }
        if (cal_times >= XY_CAL_TIMES) break;
    }
    adc_cal_data[0] = adc_min_x;
    adc_cal_data[1] = adc_max_x;
    adc_cal_data[2] = adc_min_y;
    adc_cal_data[3] = adc_max_y;
    adc_cal_data[4] = adc_mid_x;
    adc_cal_data[5] = adc_mid_y;
    cal_times       = 0;
    sensor.setAllCalValue(adc_cal_data);
    delay(1000);
    cal_state = CAL_DONE;
}

void cal_done(void) {
    tftSprite.fillSprite(BLACK);
    tftSprite.setTextSize(1);
    tftSprite.setCursor(0, 0, 4);
    tftSprite.setTextColor(YELLOW);
    tftSprite.printf("CalDone");
    tftSprite.drawLine(5, 30, 130, 30, ORANGE);
    tftSprite.setCursor(0, 30, 2);
    tftSprite.setTextSize(2);
    tftSprite.setTextColor(GREEN);

    tftSprite.printf("MinX:%d", sensor.getCalValue(0));
    tftSprite.setCursor(0, 60, 2);
    tftSprite.printf("MaxX:%d", sensor.getCalValue(1));
    tftSprite.setCursor(0, 90, 2);
    tftSprite.printf("MinY:%d", sensor.getCalValue(2));
    tftSprite.setCursor(0, 120, 2);
    tftSprite.printf("MaxY:%d", sensor.getCalValue(3));
    tftSprite.drawLine(5, 165, 130, 165, ORANGE);
    tftSprite.setCursor(0, 170, 2);
    tftSprite.setTextSize(2);
    tftSprite.setTextColor(WHITE);
    tftSprite.printf("btnA:");
    tftSprite.setTextColor(YELLOW);
    tftSprite.printf("Verify");
    tftSprite.setCursor(0, 200, 2);
    tftSprite.setTextSize(2);
    tftSprite.setTextColor(WHITE);
    tftSprite.printf("btnB:");
    tftSprite.setTextColor(YELLOW);
    tftSprite.printf("Recal");
    tftSprite.pushSprite(0, 0);
}

void cal_verify(void) {
    int8_t pos_x    = sensor.getPOSValue(POS_X, _8bit);
    int8_t pos_y    = sensor.getPOSValue(POS_Y, _8bit);
    bool btn_stauts = sensor.getButtonStatus();
    tftSprite.fillSprite(BLACK);
    tftSprite.setTextSize(1);
    tftSprite.setCursor(0, 0, 4);
    tftSprite.setTextColor(YELLOW);
    tftSprite.printf("Verify");
    tftSprite.drawLine(5, 30, 130, 30, ORANGE);
    tftSprite.setCursor(0, 30, 2);
    tftSprite.setTextSize(2);
    tftSprite.setTextColor(GREEN);

    tftSprite.printf("PosX:%d", pos_x);
    tftSprite.setCursor(0, 60, 2);
    tftSprite.printf("PosY:%d", pos_y);
    tftSprite.drawLine(5, 165, 130, 165, ORANGE);
    tftSprite.setCursor(0, 170, 2);
    tftSprite.setTextSize(2);
    tftSprite.setTextColor(WHITE);
    tftSprite.printf("btnA:");
    tftSprite.setTextColor(YELLOW);
    tftSprite.printf("Next");
    tftSprite.setCursor(0, 200, 2);
    tftSprite.setTextSize(2);
    tftSprite.setTextColor(WHITE);
    tftSprite.printf("btnB:");
    tftSprite.setTextColor(YELLOW);
    tftSprite.printf("Recal");
    tftSprite.pushSprite(0, 0);
}

void recal(void) {
    uint16_t adc_cal_data[6];
    adc_cal_data[0] = X_DEFAULT_MIN;
    adc_cal_data[1] = X_DEFAULT_MAX;
    adc_cal_data[2] = Y_DEFAULT_MIN;
    adc_cal_data[3] = Y_DEFAULT_MAX;
    adc_cal_data[4] = MID_DEFAULT;
    adc_cal_data[5] = MID_DEFAULT;
    sensor.setAllCalValue(adc_cal_data);
    delay(1000);
}

void setup() {
    M5.begin();

    while (!(sensor.begin(&Wire, JoyC_ADDR, 0, 26, 100000UL))) {
        delay(100);
        Serial.println("I2C Error!\r\n");
    }

    timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, &onTimer, true);
    timerAlarmWrite(timer, 1000000 / 5, true);
    timerAlarmEnable(timer);

    M5.lcd.setRotation(4);  // Rotate the screen. 将屏幕旋转
    tftSprite.createSprite(
        135, 240);  // Create a 240x135 canvas. 创建一块160x80的画布
    // cal_center();
}

void loop() {
    M5.update();
    if (M5.BtnA.wasPressed()) {
        if (cal_state == CAL_CENTER_DONE) {
            cal_state = CAL_XY;
        } else if (cal_state == CAL_DONE) {
            cal_state = CAL_VERIFY;
        } else if (cal_state == CAL_VERIFY) {
            cal_state = CAL_CENTER;
        }
    } else if (M5.BtnB.wasPressed()) {
        if (cal_state == CAL_CENTER_DONE) {
            cal_state = CAL_CENTER;
        } else if (cal_state == CAL_DONE) {
            cal_state = CAL_XY;
        } else if (cal_state == CAL_VERIFY) {
            recal();
            cal_state = CAL_CENTER;
        }
    }
    if (flag_lcd_interval % 5 == 0) {
        M5.Lcd.drawBitmap(115, 5, 20, 20,
                          (uint16_t *)gImage_statusLed + (20 * 20 * 0));
    } else {
        switch (cal_state) {
            case CAL_CENTER:
                cal_center();
                break;
            case CAL_CENTER_DONE:
                cal_center_done();
                break;
            case CAL_XY:
                cal_xy();
                break;
            case CAL_DONE:
                cal_done();
                break;
            case CAL_VERIFY:
                cal_verify();
                break;

            default:
                break;
        }
    }
}
