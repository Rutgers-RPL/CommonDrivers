#include "unity.h"
#include "cmock.h"

#include "mock_bmp5.h"

#include "bmp581.h"
#include "sensor.h"

bool bmp581_read(void *context, struct packet *packet);

void set_up() {}
void tear_down() {}

// Try using both calculators. First one seems to be for tropo only
// https://codingace.net/physics/barometric_pressure_to_altitude.html
// https://www.sensorsone.com/us-standard-atmosphere-altitude-pressure-calculator/
// Select lapse-rate model and enter your mock pressure. For tests, 
// we make the assumption of standard sea level conditions, ie
// pressure at 101325 Pa and temperature of around 15 celcius.

void test_bmp581_read_sea()
{
	struct bmp581_ctx ctx = {0};
	struct packet packet;

	float fake_pressure = 101325.0 - 3.0;
	float fake_temperature = 15.3;
	struct bmp5_sensor_data fake = {
		.pressure = fake_pressure,
		.temperature = fake_temperature,
	};

	bmp5_get_sensor_data_ExpectAnyArgsAndReturn(true);
	bmp5_get_sensor_data_ReturnMemThruPtr_sensor_data(&fake, sizeof(fake));

	bool result = bmp581_read(&ctx, &packet);

	float expected_alt = 0.249734;
	TEST_ASSERT_EQUAL_FLOAT(fake_temperature, packet.temperature_c);
	TEST_ASSERT_EQUAL_FLOAT(fake_pressure, packet.kf_position_m);
	TEST_ASSERT_FLOAT_WITHIN(0.001, expected_alt, packet.barometer_hMSL_m);
}

void test_bmp581_read_tropo()
{
	struct bmp581_ctx ctx = {0};
	struct packet packet;

	float fake_pressure = 22630.0f + 41.0;
	float fake_temperature = -51.23;
	struct bmp5_sensor_data fake = {
		.pressure = fake_pressure,
		.temperature = fake_temperature,
	};

	bmp5_get_sensor_data_ExpectAnyArgsAndReturn(true);
	bmp5_get_sensor_data_ReturnMemThruPtr_sensor_data(&fake, sizeof(fake));

	bool result = bmp581_read(&ctx, &packet);

	float expected_alt = 10989.260441;
	TEST_ASSERT_EQUAL_FLOAT(fake_temperature, packet.temperature_c);
	TEST_ASSERT_EQUAL_FLOAT(fake_pressure, packet.kf_position_m);
	// Formula gets less accurate higher up, so it is fine to be less strict 
	TEST_ASSERT_FLOAT_WITHIN(1, expected_alt, packet.barometer_hMSL_m);
}


// Lower stratosphere is isothermal as oppose to lapse (upper stratosphere) 
void test_bmp581_read_strato_lower()
{
	struct bmp581_ctx ctx = {0};
	struct packet packet;

	float fake_pressure = 5475.0f + 1030.0;
	float fake_temperature = -51.23;
	struct bmp5_sensor_data fake = {
		.pressure = fake_pressure,
		.temperature = fake_temperature,
	};

	bmp5_get_sensor_data_ExpectAnyArgsAndReturn(true);
	bmp5_get_sensor_data_ReturnMemThruPtr_sensor_data(&fake, sizeof(fake));

	bool result = bmp581_read(&ctx, &packet);

	float expected_alt = 18906.71;
	TEST_ASSERT_EQUAL_FLOAT(fake_temperature, packet.temperature_c);
	TEST_ASSERT_EQUAL_FLOAT(fake_pressure, packet.kf_position_m);
	// Formula gets less accurate higher up, so it is fine to be less strict 
	TEST_ASSERT_FLOAT_WITHIN(1, expected_alt, packet.barometer_hMSL_m);
}

void test_bmp581_read_strato_upper()
{
	struct bmp581_ctx ctx = {0};
	struct packet packet;

	float fake_pressure = 5475.0f - 43.0;
	float fake_temperature = -56.23;
	struct bmp5_sensor_data fake = {
		.pressure = fake_pressure,
		.temperature = fake_temperature,
	};

	bmp5_get_sensor_data_ExpectAnyArgsAndReturn(true);
	bmp5_get_sensor_data_ReturnMemThruPtr_sensor_data(&fake, sizeof(fake));

	bool result = bmp581_read(&ctx, &packet);

	float expected_alt = 20049.8797;
	TEST_ASSERT_EQUAL_FLOAT(fake_temperature, packet.temperature_c);
	TEST_ASSERT_EQUAL_FLOAT(fake_pressure, packet.kf_position_m);
	TEST_ASSERT_FLOAT_WITHIN(1, expected_alt, packet.barometer_hMSL_m);
}

