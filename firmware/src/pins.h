#if defined(ARDUINO_ARCH_ESP32) 
constexpr int M1_PWM1 = D10;
constexpr int M1_PWM2 = D9;
constexpr int M1_PWM3 = D8;

constexpr int M1_EN = D7;

constexpr int S1_SCL = D1;
constexpr int S1_SDA = D0;

#else if defined(ARDUINO_ARCH_TEENSY) // Teensy 4.1

// https://www.amazon.com/dp/B0G2LPC1C7 
constexpr int M1_PWM1 = 2;
constexpr int M1_PWM2 = 3;
constexpr int M1_PWM3 = 4;
constexpr int M1_EN = 8;

// AS5600
constexpr int I2C_SCL = 19;
constexpr int I2C_SDA = 18;

// https://www.amazon.com/dp/B0G2LR85GW
constexpr int M2_PWM1 = 5;
constexpr int M2_PWM2 = 6;
constexpr int M2_PWM3 = 7;
constexpr int M2_EN = 9;

// MT6701
constexpr int SPI_CS = 10;
constexpr int SPI_SCK = 13;
constexpr int SPI_MISO = 12;

#endif