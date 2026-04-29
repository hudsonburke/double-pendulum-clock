#if defined(ARDUINO_TEENSY41)
// https://www.amazon.com/dp/B0G2LPC1C7

constexpr int M1_EN = 2;
constexpr int M1_PWM3 = 3;
constexpr int M1_PWM2 = 4;
constexpr int M1_PWM1 = 5;

// AS5600
constexpr int I2C_SCL = 19;
constexpr int I2C_SDA = 18;

// https://www.amazon.com/dp/B0G2LR85GW

constexpr int M2_EN = 6;
constexpr int M2_PWM3 = 7;
constexpr int M2_PWM2 = 8;
constexpr int M2_PWM1 = 9;

// MT6701
constexpr int SPI_CS = 10;
constexpr int SPI_SCK = 13;
constexpr int SPI_MISO = 12;
#endif  // ARDUINO_TEENSY41