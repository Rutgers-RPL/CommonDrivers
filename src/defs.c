#include "defs.h"
#include <assert.h>

#ifdef HAL_SPI_MODULE_ENABLED
static bool spi_read(struct handle* handle, struct op_params* params)
{
	HAL_StatusTypeDef res;
	struct handle_spi *spi = handle_as_spi(handle);

	HAL_GPIO_WritePin(spi->port, spi->pin, GPIO_PIN_RESET);
	HAL_SPI_Transmit(spi->handle, params->cmd, params->cmd_size, HAL_MAX_DELAY);
	res = HAL_SPI_Receive(spi->handle, params->buffer, params->buffer_size, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(spi->port, spi->pin, GPIO_PIN_SET);
	return res;
}

static bool spi_write(struct handle* handle, struct op_params* params)
{
	HAL_StatusTypeDef res;
	struct handle_spi *spi = handle_as_spi(handle);

	HAL_GPIO_WritePin(spi->port, spi->pin, GPIO_PIN_RESET);
	HAL_SPI_Transmit(spi->handle, params->cmd, params->cmd_size, HAL_MAX_DELAY);
	if (params->buffer_size > 0) {
		res = HAL_SPI_Transmit(spi->handle, params->buffer, params->buffer_size, HAL_MAX_DELAY);
	}
	HAL_GPIO_WritePin(spi->port, spi->pin, GPIO_PIN_SET);
	return res;
}

void serial_api_spi(struct serial_api *api, struct handle* handle)
{
	assert(handle->protocol == SPI);
	api->handle = handle;
	api->write = spi_write;
	api->read = spi_read;
}
#else
void serial_api_spi(struct serial_api *api, struct handle* handle)
{
	assert(0 && "SPI module is disabled, SPI is inaccessible!");
}
#endif

#ifdef HAL_I2C_MODULE_ENABLED
static bool i2c_read(struct handle* handle, struct op_params* params)
{
	HAL_StatusTypeDef res;
	struct handle_i2c *i2c = handle_as_i2c(handle);
	HAL_I2C_Mem_Read(i2c->handle, i2c->address << 1,
			 params->cmd[0], I2C_MEMADD_SIZE_8BIT,
			 params->buffer, params->buffer_size, HAL_MAX_DELAY);
	return res;
}

static bool i2c_write(struct handle* handle, struct op_params* params)
{
	HAL_StatusTypeDef res;
	struct handle_i2c *i2c = handle_as_i2c(handle);
	HAL_I2C_Mem_Write(i2c->handle, i2c->address << 1,
			 params->cmd[0], I2C_MEMADD_SIZE_8BIT,
			 params->buffer, params->buffer_size, HAL_MAX_DELAY);
	return res;
}

void serial_api_i2c(struct serial_api *api, struct handle* handle)
{
	assert(handle->protocol == I2C);
	api->handle = handle;
	api->write = i2c_write;
	api->read = i2c_read;
}
#else
void serial_api_i2c(struct serial_api *api, struct handle* handle)
{
	assert(0 && "I2C module is disabled, I2C api is inaccessible!");
}
#endif

#ifdef HAL_QSPI_MODULE_ENABLED
static bool qspi_read(struct handle* handle, struct op_params* params)
{
	QSPI_CommandTypeDef cmd;
	HAL_StatusTypeDef res;
	struct handle_qspi *qspi = handle_as_qspi(handle);

	// Common configuration
	cmd.Instruction = params->cmd[0];
	cmd.DummyCycles = params->dummy_cycles;
	cmd.InstructionMode = QSPI_INSTRUCTION_1_LINE;
	cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	cmd.AddressMode = QSPI_ADDRESS_1_LINE;
	cmd.AddressSize = QSPI_ADDRESS_24_BITS;
	cmd.Address = 0;
	cmd.DataMode = QSPI_DATA_4_LINES;
	cmd.NbData = params->buffer_size;
	HAL_QSPI_Command(&qspi->handle, &cmd, HAL_MAX_DELAY);
	res = HAL_QSPI_Receive(&qspi->handle, params->buffer, HAL_MAX_DELAY);
	return res;
}

static bool qspi_write(struct handle* handle, struct op_params* params)
{
	QSPI_CommandTypeDef cmd;
	HAL_StatusTypeDef res;
	struct handle_qspi *qspi = handle_as_qspi(handle);

	// Common configuration
	cmd.Instruction = params->cmd[0];
	cmd.DummyCycles = params->dummy_cycles;
	cmd.InstructionMode = QSPI_INSTRUCTION_1_LINE;
	cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	if (params->buffer_size > 0) {
		// Write mode
		cmd.Address = 0;
		cmd.AddressMode = QSPI_ADDRESS_1_LINE;
		cmd.AddressSize = QSPI_ADDRESS_24_BITS;
		cmd.DataMode = QSPI_DATA_4_LINES;
		cmd.NbData = params->buffer_size;
	} else {
		// Command mode
		sCommand.AddressMode = QSPI_ADDRESS_NONE;
		cmd.DataMode = QSPI_DATA_NONE;
	}

	HAL_QSPI_Command(&qspi->handle, &cmd, HAL_MAX_DELAY);
	if (params->buffer_size > 0) {
		res = HAL_QSPI_Transmit(&qspi->handle, params->buffer, HAL_MAX_DELAY);
	}
	return res;
}

void serial_api_qspi(struct serial_api *api, struct handle* handle)
{
	assert(handle->protocol == QSPI);
	api->handle = handle;
	api->write = qspi_write;
	api->read = qspi_read;
}
#else
void serial_api_qspi(struct serial_api *api, struct handle* handle)
{
	assert(0 && "QSPI module is disabled, QSPI api is inaccessible!");
}
#endif
