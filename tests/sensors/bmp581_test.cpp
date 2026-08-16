#include "bmp5.h"
#include "bmp581.h"
#include "protocol_mock.h"

#include <functional>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

/* Configure FFF to use std::function, which enables capturing lambdas */
#define CUSTOM_FFF_FUNCTION_TEMPLATE(RETURN, FUNCNAME, ...)                    \
    std::function<RETURN(__VA_ARGS__)> FUNCNAME
#include "fff.h"

// Mock the bosch functions, since they are C, we can't touch them with gtest
// We assume bosch tests their functions already and so we only care about
// the logic in our code rather than in theirs.
//
// For some of our other drivers where we write more of the logic, we might
// want to properly mock the HAL as well, but we don't have to here.
DEFINE_FFF_GLOBALS;
extern "C" {
#include "bmp5.h"
#include "bmp5_defs.h"

FAKE_VALUE_FUNC(int8_t, bmp5_soft_reset, struct bmp5_dev*);
FAKE_VALUE_FUNC(int8_t, bmp5_init, struct bmp5_dev*);
FAKE_VALUE_FUNC(int8_t, bmp5_set_osr_odr_press_config,
                const struct bmp5_osr_odr_press_config*, struct bmp5_dev*);
FAKE_VALUE_FUNC(int8_t, bmp5_int_source_select,
                const struct bmp5_int_source_select*, struct bmp5_dev*);
FAKE_VALUE_FUNC(int8_t, bmp5_configure_interrupt, enum bmp5_intr_mode,
                enum bmp5_intr_polarity, enum bmp5_intr_drive,
                enum bmp5_intr_en_dis, struct bmp5_dev*);
FAKE_VALUE_FUNC(int8_t, bmp5_get_sensor_data, struct bmp5_sensor_data*,
                const struct bmp5_osr_odr_press_config*, struct bmp5_dev*);
FAKE_VALUE_FUNC(int8_t, bmp5_set_power_mode, enum bmp5_powermode,
                struct bmp5_dev*);
}

// This lets us return a value by reference using fff
auto mock_sensor_data(float pressure, float temp) {
    return [=](struct bmp5_sensor_data* out, auto, auto) {
        *out = {.pressure = pressure, .temperature = temp};
        return BMP5_OK;
    };
}

// Try using both calculators to get atmosphere data and verify if our
// algorithms are correct. First one seems to be for tropo only.
//
//   https://codingace.net/physics/barometric_pressure_to_altitude.html
//   https://www.sensorsone.com/us-standard-atmosphere-altitude-pressure-calculator/
//
// Select lapse-rate model and enter your mock pressure. For tests,
// we make the assumption of standard sea level conditions, ie
// pressure at 101325 Pa and temperature of around 15 celcius.
namespace Platform {
class BMP581Test : public testing::Test {
  protected:
    MockProtocol protocol = MockProtocol(ProtocolType::SPI);

    BMP581Test() {}
    void SetUp() override {
        FFF_RESET_HISTORY();
        RESET_FAKE(bmp5_get_sensor_data);
    }
};

TEST_F(BMP581Test, ShouldReadSeaLevelCorrectly) {
    float fake_pressure = 101325.0 - 3.0;
    float fake_temperature = 15.3;
    bmp5_get_sensor_data_fake.custom_fake =
        mock_sensor_data(fake_pressure, fake_temperature);

    auto packet = Packet();
    auto sensor = BMP581(protocol);
    sensor.read(packet);

    float expected_alt = 0.249734;
    EXPECT_FLOAT_EQ(fake_temperature, packet.temperature_c);
    EXPECT_FLOAT_EQ(fake_pressure, packet.kf_position_m);
    EXPECT_NEAR(expected_alt, packet.barometer_hMSL_m, 0.01);
}

// Formula gets less accurate higher up, so it is fine to be less strict
// with regards the the altitude check
TEST_F(BMP581Test, ShouldReadTroposphereCorrectly) {
    float fake_pressure = 22630.0f + 41.0;
    float fake_temperature = -51.23;
    bmp5_get_sensor_data_fake.custom_fake =
        mock_sensor_data(fake_pressure, fake_temperature);

    auto packet = Packet();
    auto sensor = BMP581(protocol);
    sensor.read(packet);

    float expected_alt = 10989.260441;
    EXPECT_FLOAT_EQ(fake_temperature, packet.temperature_c);
    EXPECT_FLOAT_EQ(fake_pressure, packet.kf_position_m);
    EXPECT_NEAR(expected_alt, packet.barometer_hMSL_m, 1);
}

// Lower stratosphere is isothermal as oppose to lapse (upper stratosphere)
TEST_F(BMP581Test, ShouldReadLowerStratosphereCorrectly) {
    float fake_pressure = 5475.0f + 1030.0;
    float fake_temperature = -51.23;
    bmp5_get_sensor_data_fake.custom_fake =
        mock_sensor_data(fake_pressure, fake_temperature);

    auto packet = Packet();
    auto sensor = BMP581(protocol);
    sensor.read(packet);

    float expected_alt = 18906.71;
    EXPECT_FLOAT_EQ(fake_temperature, packet.temperature_c);
    EXPECT_FLOAT_EQ(fake_pressure, packet.kf_position_m);
    EXPECT_NEAR(expected_alt, packet.barometer_hMSL_m, 1);
}

TEST_F(BMP581Test, ShouldReadUpperStratosphereCorrectly) {
    float fake_pressure = 5475.0f - 43.0;
    float fake_temperature = -56.23;
    bmp5_get_sensor_data_fake.custom_fake =
        mock_sensor_data(fake_pressure, fake_temperature);

    auto packet = Packet();
    auto sensor = BMP581(protocol);
    sensor.read(packet);

    float expected_alt = 20049.8797;
    EXPECT_FLOAT_EQ(fake_temperature, packet.temperature_c);
    EXPECT_FLOAT_EQ(fake_pressure, packet.kf_position_m);
    EXPECT_NEAR(expected_alt, packet.barometer_hMSL_m, 1);
}
} // namespace Platform
