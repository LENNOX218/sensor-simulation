#include <Wire.h>
#include <MCP3424.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

/* OLED定義 */
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET    -1

/* アドレス定義 */
#define SCREEN_ADDR 0x3C
#define MCP4728_ADDR 0x60
#define MCP3424_ADDR 0x68

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
MCP3424 adc(PIN_FLOAT, PIN_FLOAT); /* ピンAdr1,Adr0 未接続 */

double targetVoltage[4] = {0, 0, 0, 0};
double currentVoltage[4] = {0, 0, 0, 0};
double VoltageOffset[4] = {0, 0, 0, 0}; 
int cnt = 0; /* counter of check Deviation */
bool reset = 0;

const char * errmsg[] = {"", "underflow", "overflow", "i2c", "in progress", "timeout"};

// ======================= DAC WRITE =======================
void readADC()
{
    for (int i = 0; i < 4; i++)
    {
        currentVoltage[i] = 0.0;
        ConvStatus err = adc.read((Channel)i, currentVoltage[i]);

        if (err != R_OK)
        {
            Serial.print("conversion error: ");
            Serial.println(errmsg[err]);
        }
        else
        {
            ;
        }
        asm volatile ("nop");
    }
}


// ======================= DAC WRITE =======================
//  0-5.0V（物理値） を 12bit（生値）に変換 (0-4095)
void writeDAC(uint8_t channel, float voltage)
{
    if (voltage < 0) voltage = 0;
    if (voltage > 5.0) voltage = 5.0;

    // 12-bit 分解能: 4095 ＝ 5V (仮 VREF = 5V)
    uint16_t value = (uint16_t)((voltage / 5.0) * 4095.0);

    Wire.beginTransmission(MCP4728_ADDR);

    // コマンド仕様: 0100 0[Channel(2bit)] 0
    // 0x40 は MCP4728 は命令アドレス【固定】
    uint8_t command = 0x40 | (channel << 1);
    Wire.write(command);
    Wire.write(value >> 8);   // HIGH 8 位
    Wire.write(value & 0xFF); // LOW 8 位

    if (Wire.endTransmission() != 0)
    {
        Serial.println("I2C Write Error to MCP4728");
    }
    
    /* reset counter of check Deviation*/
    cnt = 0;
}
// ======================= check DAC Deviation =======================
void checkDeviation()
{
    double setValue;

    for (int i = 0; i < 4; i++)
    {
        VoltageOffset[i] = targetVoltage[i] - currentVoltage[i];

        /* MCP3424最大対応2.048V */
        if ( currentVoltage[i] < 2.03 && (VoltageOffset[i] < 0.005 || VoltageOffset[i] > 0.005))
        {
            /* 注意：目標値更新しない */
            setValue = targetVoltage[i] + VoltageOffset[i];
            writeDAC(i, setValue);
        }
        else
        {
            ;
        }
    }
}

// ======================= SERIAL PARSE =======================
void handleSerial()
{
    if (Serial.available() > 0)
    {
        // 1. バファを取得して、初期化
        String input = Serial.readStringUntil('\n');
        input.trim();

        if (input.length() == 0) return;

        // equalsIgnoreCase は大小文字無視する　⇒ "init"、"INIT" 、 "Init"
        if (input.equalsIgnoreCase("init"))
        {
            Serial.println(F(">>> System Initializing: Resetting all channels to 0V..."));

            for (int i = 0; i < 4; i++) 
            {
                targetVoltage[i] = 0.0;
                writeDAC(i, 0.0);
            }
            Serial.println(F(">>> All channels are now 0.000V."));
            return; // この関数停止
        }

        if (input.equalsIgnoreCase("reset"))
        {
            checkDeviation();

            Serial.println(F(">>> reset complite."));
            return; // この関数停止
        }

        // 2. バファ解析：スペースを検索
        int spaceIndex = input.indexOf(' ');

        // バファチェック１：必ずスペースあり（数不問）
        if (spaceIndex <= 0 || spaceIndex == (int)input.length() - 1)
        {
            Serial.println(F("Error: Invalid Format. Use [ch] [voltage] or 'init'"));
            return;
        }

        // 3. データ分け（スペースを区切りとして、前半と後半）
        String part1 = input.substring(0, spaceIndex);
        String part2 = input.substring(spaceIndex + 1);

        /* 後半のデータまだスペースがあったら、エラー */
        if (part2.indexOf(' ') != -1)
        {
            Serial.println(F("Error: Too many arguments."));
            return;
        }

        // 4. 数値型変換
        int rawCh = part1.toInt();
        double val = part2.toDouble();

        // 5. 異常チェック
        /* ch1~4のみ有効 */
        if (rawCh < 1 || rawCh > 4)
        {
            Serial.println(F("Error: Channel must be 1-4."));
            return;
        }

        /* 電圧が負ではない、5V超えるのもできない */
        if (val < 0.0 || val > 5.0)
        {
            Serial.println(F("Error: Voltage must be 0-5V."));
            return;
        }

        // 6. データ大丈夫なら、実行
        int index = rawCh - 1;
        targetVoltage[index] = val;
        writeDAC(index, val);
        reset = 1;

        // 7. デバック出力
        Serial.print(F("Success: CH"));
        Serial.print(rawCh);
        Serial.print(F(" -> "));
        Serial.print(val, 3);
        Serial.println(F("V"));
    }
}

// ======================= OLED =======================
void updateDisplay()
{
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    //display.println(F("DAC Output Setting:"));
    //display.drawLine(0, 9, 128, 9, SSD1306_WHITE);
    //display.println();

    /* OLED電圧表示更新 */
    for (int i = 0; i < 4; i++)
    {
        display.print("C");
        display.print(i+1);
        display.print(":");
        display.print(currentVoltage[i], 3);
        display.print("V ");

        display.print("T");
        display.print(i+1);
        display.print(":");
        display.print(targetVoltage[i], 3);
        display.print("V  ");
        //if (i == 1) display.println(); // 改行
    }
  
    display.display();
}

// ======================= SETUP =======================
void setup()
{
    Wire.begin();
    Serial.begin(115200);

    adc.generalCall(GC_RESET);
    for (int i = 0; i < 4; i++)
    {
        adc.creg[i].bits = { GAINx1, R12B, CONTINUOUS, (Channel)i, 1 };
    }

    // OLED 初期化
    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDR))
    {
        for(;;);
    }

    for (int i = 0; i < 4; i++)
    {
        currentVoltage[i] = 0.0;
        writeDAC(i, 0.0);
    }

    display.clearDisplay();
    display.display();

    Serial.println("System Ready. Input format: [channel] [voltage]");
    Serial.println("Example: 1 2.5");
}

// ======================= LOOP =======================
void loop()
{
    /* １．受信チェック、DAC設定 */
    handleSerial();

    /* ２．ADC更新 */
    readADC();

    /* ３．DAC出力補正処理 */
    if ( reset == 1 && cnt == 10 )
    {
        checkDeviation();
        cnt = 0;
        reset = 0;
    }
    else if ( reset == 1 && cnt < 10 )
    {
        cnt++;
    }
    else
    {
        ;
    }

    /* ４．OLED更新 */
    updateDisplay();

    /* ５．100ms周期 */
    delay(100);
}