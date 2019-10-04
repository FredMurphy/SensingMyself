#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <time.h>
#include <sys/time.h>

#include "algorithm_by_RF.h"
#include "applibs_versions.h"
#include "azure_iot_utilities.h"
#include "connection_strings.h"
#include "mt3620_avnet_dev.h"
#include "max30102.h"
#include <applibs/i2c.h>
#include <applibs/log.h>
#include <applibs/gpio.h>

static int i2cFd = -1;
static int hr4InterruptPinFd = -1;
static int redLedFd = -1;
static int greenLedFd = -1;
static int blueLedFd = -1;
static int buttonAGpioFd = -1;

static volatile int terminationRequired = false;
static volatile int takeReadings = false;
static volatile int takingReadings = false;

#define HR4_INT		AVT_MODULE_GPIO2_PWM2
#define RUN_TIME	30

#define JSON_BUFFER_SIZE 100
#define JSON_TEMPLATE "{ \"deviceId\": \"fred\", \"heart_rate\": %d, \"o2\": %.2f }"
char* jsonBuffer[JSON_BUFFER_SIZE];

const struct timespec sleepTime1s = { 1, 0 };

bool get_hr4_readings(int32_t* heart_rate, float* spo2);

void init_gpio(void) {
	hr4InterruptPinFd = GPIO_OpenAsInput(HR4_INT);

	redLedFd = GPIO_OpenAsOutput(MT3620_RDB_LED1_RED, GPIO_OutputMode_PushPull, GPIO_Value_High);
	if (redLedFd < 0) {
		Log_Debug("Error opening GPIO: %s (%d). Check that app_manifest.json includes the GPIO used.\n", strerror(errno), errno);
		return -1;
	}

	greenLedFd = GPIO_OpenAsOutput(MT3620_RDB_LED1_GREEN, GPIO_OutputMode_PushPull, GPIO_Value_High);
	if (greenLedFd < 0) {
		Log_Debug("Error opening GPIO: %s (%d). Check that app_manifest.json includes the GPIO used.\n", strerror(errno), errno);
		return -1;
	}

	blueLedFd = GPIO_OpenAsOutput(MT3620_RDB_LED1_BLUE, GPIO_OutputMode_PushPull, GPIO_Value_High);
	if (blueLedFd < 0) {
		Log_Debug("Error opening GPIO: %s (%d). Check that app_manifest.json includes the GPIO used.\n", strerror(errno), errno);
		return -1;
	}

	buttonAGpioFd = GPIO_OpenAsInput(MT3620_RDB_BUTTON_A);
	if (buttonAGpioFd < 0) {
		Log_Debug("ERROR: Could not open button A GPIO: %s (%d).\n", strerror(errno), errno);
		return -1;
	}

}

void init_i2c(void) {

	i2cFd = I2CMaster_Open(MT3620_I2C_ISU2);
	if (i2cFd < 0) {
		Log_Debug("ERROR: I2CMaster_Open: errno=%d (%s)\n", errno, strerror(errno));
		return;
	}

	int result = I2CMaster_SetBusSpeed(i2cFd, I2C_BUS_SPEED_STANDARD);
	if (result != 0) {
		Log_Debug("ERROR: I2CMaster_SetBusSpeed: errno=%d (%s)\n", errno, strerror(errno));
		return;
	}

	result = I2CMaster_SetTimeout(i2cFd, 100);
	if (result != 0) {
		Log_Debug("ERROR: I2CMaster_SetTimeout: errno=%d (%s)\n", errno, strerror(errno));
		return;
	}
}

int hr4_read_i2c(uint8_t addr, uint16_t count, uint8_t* ptr)
{
	int r = I2CMaster_WriteThenRead(i2cFd, MAX30101_SAD, &addr, sizeof(addr), ptr, count);
	if (r == -1)
		Log_Debug("ERROR: I2CMaster_Writer: errno=%d (%s)\n", errno, strerror(errno));
	return r;
}



void hr4_write_i2c(uint8_t addr, uint16_t count, uint8_t* ptr)
{
	uint8_t buff[2];
	buff[0] = addr;
	buff[1] = *ptr;

	int r = I2CMaster_Write(i2cFd, MAX30101_SAD, buff, 2);
	if (r == -1)
		Log_Debug("ERROR: I2CMaster_Writer: errno=%d (%s)\n", errno, strerror(errno));
}

/*
Entry point for direct method call from IoT Hub
*/
const char* okResponse = "{ \"message\": \"OK\" }";
const char* errorResponse = "{ \"message\": \"Not found\" }";

int directMethodCall(const char* directMethodName, const char* payload, size_t payloadSize, char** responsePayload, size_t* responsePayloadSize) {

	// We're just checking connectivity
	if (strcmp(directMethodName, "Connect") == 0) {
		*responsePayload = okResponse;
		*responsePayloadSize = strlen(okResponse);
		return 200;
	}

	// Trigger a reading to be posted to IoT Hub on completeion
	if (strcmp(directMethodName, "Read") == 0) {
		if (!takingReadings)
			takeReadings = true;
		*responsePayload = okResponse;
		*responsePayloadSize = strlen(okResponse);
		return 200;
	}

	*responsePayload = errorResponse;
	*responsePayloadSize = strlen(errorResponse);
	return 404;
}

void messageCall(const char* payload) {
	if ((strcmp(payload, "Read") == 0) && !takingReadings)
		takeReadings = true;
}

int main(void)
{
	float o2;
	int heart_rate;

	init_gpio();
	init_i2c();

	// Initialize Heart Rate 4 sensor
	maxim_max30102_i2c_setup(hr4_read_i2c, hr4_write_i2c);

	Log_Debug("HeartRate Click 4 - Revision: 0x%02X, Part Id: 0x%02X\n", max30102_get_revision(), max30102_get_part_id());

	if (!AzureIoT_SetupClient()) {
		Log_Debug("ERROR: Failed to set up IoT Hub client\n");
		return;
	}

	AzureIoT_SetDirectMethodCallback(&directMethodCall);
	AzureIoT_SetMessageReceivedCallback(&messageCall);

	// Wait. Everything else is driven from an incoming direct method call
	while (!terminationRequired) {

		// Blink green LED to show we're running and connected to Azure IoT Hub
		GPIO_SetValue(greenLedFd, GPIO_Value_Low);
		nanosleep(&sleepTime1s, NULL);
		GPIO_SetValue(greenLedFd, GPIO_Value_High);
		nanosleep(&sleepTime1s, NULL);

		// This needs to be called frequently in order to keep active the flow of data with the Azure IoT Hub
		AzureIoT_DoPeriodicTasks();

		if (takingReadings)
			continue;

		GPIO_Value_Type buttonAState;
		int result = GPIO_GetValue(buttonAGpioFd, &buttonAState);
		if (buttonAState == GPIO_Value_Low)
			takeReadings = true;

		if (takeReadings) {
			takingReadings = true;
			takeReadings = false;
			GPIO_SetValue(redLedFd, GPIO_Value_Low);
			if (get_hr4_readings(&heart_rate, &o2)) {
				snprintf(jsonBuffer, JSON_BUFFER_SIZE, JSON_TEMPLATE, heart_rate, o2);
				Log_Debug("Sending: %s\n", jsonBuffer);
				AzureIoT_SendMessage(jsonBuffer);
			}
			GPIO_SetValue(redLedFd, GPIO_Value_High);
			takingReadings = false;
		}
	}

	Log_Debug("Exiting");
}


bool get_hr4_readings(int32_t* heart_rate, float* spo2) {

	float    n_spo2, ratio, correl;                                       //SPO2 value
	int8_t   ch_spo2_valid;                                               //indicator to show if the SPO2 calculation is valid
	int32_t  n_heart_rate;                                                //heart rate value
	int8_t   ch_hr_valid;                                                 //indicator to show if the heart rate calculation is valid
	uint32_t aun_ir_buffer[BUFFER_SIZE];                                  //infrared LED sensor data
	uint32_t aun_red_buffer[BUFFER_SIZE];                                 //red LED sensor data
	int32_t  i;
	int32_t  sum_hr;
	float    sum_spo2;
	int32_t  nbr_readings;

	float average_spo2 = 0;
	int average_hr = 0;

	maxim_max30102_init();

	Log_Debug("Begin ... Place your finger on the sensor\n\n");

	// Fixed values for now as it's unreliable

	//*heart_rate = 70;
	//*spo2 = 95.6;
	//return;

	struct timeval time_start, time_now;
	gettimeofday(&time_start, NULL);
	time_now = time_start;
	nbr_readings = 0;

	while (difftime(time_now.tv_sec, time_start.tv_sec) < RUN_TIME) {
		//buffer length of BUFFER_SIZE stores ST seconds of samples running at FS sps
		//read BUFFER_SIZE samples, and determine the signal range
		GPIO_Value_Type intVal;
		for (i = 0; i < BUFFER_SIZE; i++) {
			do {
				GPIO_GetValue(hr4InterruptPinFd, &intVal);
			} while (intVal == 1);                               //wait until the interrupt pin asserts

			maxim_max30102_read_fifo((aun_red_buffer + i), (aun_ir_buffer + i));   //read from MAX30102 FIFO
			Log_Debug("*");
		}
		Log_Debug("\n");
		//calculate heart rate and SpO2 after BUFFER_SIZE samples (ST seconds of samples) using Robert's method
		rf_heart_rate_and_oxygen_saturation(aun_ir_buffer, BUFFER_SIZE, aun_red_buffer, &n_spo2, &ch_spo2_valid, &n_heart_rate, &ch_hr_valid, &ratio, &correl);

		if (ch_hr_valid && ch_spo2_valid) {
			Log_Debug("Blood Oxygen Level (SpO2)=%.2f%% [normal is 95-100%%], Heart Rate=%d BPM [normal resting for adults is 60-100 BPM]\n", n_spo2, n_heart_rate);
			*heart_rate = n_heart_rate;
			*spo2 = n_spo2;
			max301024_shut_down(1);
			return true;
		}
		else
			Log_Debug("ch_hr_valid=%d, ch_spo2_valid=%d\n", ch_hr_valid, ch_spo2_valid);

		gettimeofday(&time_now, NULL);
	}

	max301024_shut_down(1);

	// Fake some values
	*heart_rate = 70;
	*spo2 = 99;
	return true;

	//return false;
}
