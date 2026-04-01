#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define MCP4728_ADDR 0x60

float targetVoltage[4] = {0, 0, 0, 0};

// ======================= DAC WRITE =======================
// 将 0-5.0V 转换为 12位数字量 (0-4095)
void writeDAC(uint8_t channel, float voltage)
{
    if (voltage < 0) voltage = 0;
    if (voltage > 5.0) voltage = 5.0;

    // 12-bit 分辨率: 4095 对应 5V (假设 VREF = 5V)
    uint16_t value = (uint16_t)((voltage / 5.0) * 4095.0);

    Wire.beginTransmission(MCP4728_ADDR);
    
    // 多通道写命令格式: 0100 0[Channel(2bit)] 0
    // 这里的 0x40 是 MCP4728 的单通道写命令基址
    uint8_t command = 0x40 | (channel << 1); 
    
    Wire.write(command);
    Wire.write(value >> 8);   // 高 8 位
    Wire.write(value & 0xFF); // 低 8 位
    
    if (Wire.endTransmission() != 0) {
        Serial.println("I2C Write Error to MCP4728");
    }
}

// ======================= SERIAL PARSE =======================
void handleSerial()
{
    if (Serial.available() > 0)
    {
        // 1. 读取整行并清理空格
        String input = Serial.readStringUntil('\n');
        input.trim(); 

        if (input.length() == 0) return;

        // ---------------- 新增：初始化指令判断 ----------------
        // 使用 equalsIgnoreCase 可以兼容 "init"、"INIT" 或 "Init"
        if (input.equalsIgnoreCase("init")) {
            Serial.println(F(">>> System Initializing: Resetting all channels to 0V..."));
            for (int i = 0; i < 4; i++) {
                targetVoltage[i] = 0.0;
                writeDAC(i, 0.0);
            }
            Serial.println(F(">>> All channels are now 0.000V."));
            return; // 执行完初始化后直接退出函数
        }
        // ---------------------------------------------------

        // 2. 正常指令解析：查找空格位置
        int spaceIndex = input.indexOf(' ');
        
        // 逻辑检查：必须有空格
        if (spaceIndex <= 0 || spaceIndex == (int)input.length() - 1) {
            Serial.println(F("Error: Invalid Format. Use [ch] [voltage] or 'init'"));
            return;
        }

        // 3. 提取并检查参数数量
        String part1 = input.substring(0, spaceIndex);
        String part2 = input.substring(spaceIndex + 1);

        if (part2.indexOf(' ') != -1) {
            Serial.println(F("Error: Too many arguments."));
            return;
        }

        // 4. 数字转换
        int rawCh = part1.toInt();
        float val = part2.toFloat();

        // 5. 校验范围 (1-4 通道)
        if (rawCh < 1 || rawCh > 4) {
            Serial.println(F("Error: Channel must be 1-4."));
            return;
        }

        if (val < 0.0 || val > 5.0) {
            Serial.println(F("Error: Voltage must be 0-5V."));
            return;
        }

        // 6. 映射并执行
        int index = rawCh - 1;
        targetVoltage[index] = val;
        writeDAC(index, val);

        // 7. 反馈
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

    for (int i = 0; i < 4; i++)
    {
        display.print("CH");
        display.print(i+1);
        display.print(":");
        display.print(targetVoltage[i], 2);
        display.print("V  ");
        if (i == 1) display.println(); // 换行排版
    }

    display.display();
}

// ======================= SETUP =======================
void setup()
{
    Wire.begin();
    Serial.begin(115200);

    // OLED 初始化
    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        for(;;); 
    }
    for (int i = 0; i < 4; i++)
    {
        targetVoltage[i] = 0.0;
        writeDAC(i, 0.0);
    }

    display.clearDisplay();
    display.display();
    
    Serial.println("System Ready. Input format: [channel] [voltage]");
    Serial.println("Example: 0 2.5");
}

// ======================= LOOP =======================
void loop()
{
    handleSerial();
    updateDisplay();
    delay(100); 
}