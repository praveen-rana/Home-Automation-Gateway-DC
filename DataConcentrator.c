/*
 ============================================================================
 Name        : DataConcentrator.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Sensor Data concentrator Firmware
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <gpio.h>
#include <error.h>
#include <poll.h>

/***************************************** PROTOTYPES **********************************/
int main(int argc, char *argv[]);
void* networkThread (void * arg);
void Prepare_SensorDataPackets (char* networkBuffer, uint16_t *buffLen);
void ReadSensor_Digital (void);
void ReadSensor_Analog (void);
void InitSensors (void);
void InitSensorDataPackets (void);
void ReflectSensorDataUpdate (void);
void InitSensorDataIndications (void);
void InitNetworkStatusIndication (void);
void UpdateNetworkLed (char * pCmd);


/**************************** SYSTEM CALL RETURN TYPES ********************************/
typedef enum
{
	RETVAL_ERR_SOCKETS = -1,
	RETVAL_SUCCESS = 0
}eRETVAL;


/**************************** SENSOR DATABASE & DEFINITIONS ****************************/
typedef enum
{
	HUMIDITY_SENSOR_1 = 0,
	HUMIDITY_SENSOR_2,
	TOTAL_HUMIDITY_SENSORS
}eHUMIDITY_SENSOR;

/*
 * Sensor Data
 */
typedef struct {
	uint16_t        value_analog;
	uint8_t			value_binary;
	uint8_t			reserved;
}sHumidity_t;

sHumidity_t HumidityValue[TOTAL_HUMIDITY_SENSORS];


/**************************** SENSOR DATA PACKET ***************************************/
#define NETWORK_BUFFER_TOTAL_LEN 					1024
#define DATA_PACKET_TOTAL_HUMIDITY_SENSORS			2
#define DATA_PACKET_TOTAL_WATER_LEVEL_SENSORS		0
#define DATA_PACKET_TOTAL_WATER_SENSORS				0
#define DATA_PACKET_TOTAL_SENSORS					(DATA_PACKET_TOTAL_HUMIDITY_SENSORS+DATA_PACKET_TOTAL_WATER_LEVEL_SENSORS+DATA_PACKET_TOTAL_WATER_SENSORS)
#define DATALEN_PER_SENSOR							20

typedef enum {
	eSensorType_Humidity = 1,
	eSensorType_WaterLevel,
	eSensorType_WaterSensor,
}eSensorType;

typedef struct {
	uint16_t analogVal;
	uint8_t digitalVal;
	uint8_t sensorNumber;		//There can be 255 sensors of each type
	uint8_t validity;
	uint8_t reserved[3];
}eSensorData;

typedef struct {
	eSensorType sensorType;
	eSensorData sensorData;
}sSensorDataPacket_t;

sSensorDataPacket_t SensorDataPackets[DATA_PACKET_TOTAL_SENSORS];

/************************************* HW CONFIGURATION ********************************/
#define SENSOR_DATA_INDICATION_1 0
#define SENSOR_DATA_INDICATION_2 1

typedef enum
{
	HUMIDITY_SENSOR_1_DIGITAL = 60,   		//Enumerated as GPIO60 at P9_12 by driver
	HUMIDITY_SENSOR_2_DIGITAL,
	HUMIDITY_SENSOR_1_ADC = 0,				//Enumerated as ADC0 at P9_39 by driver
	HUMIDITY_SENSOR_2_ADC = 1
}eHumiditySensorGpio;

/************************************* NETWORK LED CONTROL *****************************/
#define NETWORK_STATUS_INDICATION_1				2
#define NETWORK_STATUS_INDICATION_2 			3

#define NETWORK_STATUS_UNINITIALIZED			"none"
#define NETWORK_STATUS_SOCKET_INITIALIZED		"default-on"
#define NETWORK_STATUS_SOCKET_FAILED			"none"
#define NETWORK_STATUS_CONNECTED				"heartbeat"
#define NETWORK_STATUS_CONNECTION_TIMEOUT		"default-on"
#define NETWORK_STATUS_CONNECTION_REFUSED		"default-on"
#define NETWORK_STATUS_CONNECTION_BREAK			"default-on"


/************************************* THREADS and APIs ********************************/

/*
 * Update Network LED status
 */
void UpdateNetworkLed (char * pCmd)
{
	write_trigger_values (NETWORK_STATUS_INDICATION_1, pCmd);
	write_trigger_values (NETWORK_STATUS_INDICATION_2, pCmd);
}

/*
 * Initialize Network Status indications
 */
void InitNetworkStatusIndication (void)
{
	write_trigger_values (NETWORK_STATUS_INDICATION_1, "none");
	write_trigger_values (NETWORK_STATUS_INDICATION_2, "none");
}

/*
 * Network Thread, Shall connect with Gateway and send data collected by the sensors every 5 seconds.
 */
void* networkThread (void * arg)
{
	int clientSocket_fd;
	uint16_t OutBuffLen = 0;
	struct sockaddr_in name;
	char bufferOut[NETWORK_BUFFER_TOTAL_LEN];
    int retVal = 0;
    int interimConnectionBreak = 0;

    int nfds;
    struct pollfd pollStruct;
    struct pollfd *pfds = &pollStruct;

    InitNetworkStatusIndication ();
	//sleep(10);

	for (;;)
	{
		UpdateNetworkLed (NETWORK_STATUS_UNINITIALIZED);
		// Create socket
		clientSocket_fd = socket (PF_INET, SOCK_STREAM, 0);
		if (clientSocket_fd == RETVAL_ERR_SOCKETS)
		{
			printf("\nNetwork Thread failed to create Socket ! Entering dead loop...");
			UpdateNetworkLed (NETWORK_STATUS_SOCKET_FAILED);
			for (;;){sleep(5);}
		}
		printf ("\n\rCreated socket");
		UpdateNetworkLed (NETWORK_STATUS_SOCKET_INITIALIZED);

		// Store server address in Socket
		memset(&name, 0, sizeof(name));
		name.sin_family = AF_INET;
		name.sin_addr.s_addr = inet_addr("192.168.1.103");
		name.sin_port = 2017;
		printf ("\n\rConnecting to server...");

	//for (;;)
	//{
		// Connect to the Server
		if (RETVAL_ERR_SOCKETS == connect (clientSocket_fd, (struct sockaddr*) &name, sizeof(name)))
		{
			printf("\n\rNetwork Thread failed to connect to server ! Checking failure...");
			perror("\n\rError: ");
			close (clientSocket_fd);

			switch (errno)
			{

			case ETIMEDOUT:
				UpdateNetworkLed (NETWORK_STATUS_CONNECTION_TIMEOUT);
				break;

			default:
				UpdateNetworkLed (NETWORK_STATUS_CONNECTION_REFUSED);
				break;
			}

			printf ("\n\rRetrying Connection to server in 10 seconds...");
			sleep (10);
			continue;
		}
		//}

		printf ("\n\rConnection to server successful ");
		UpdateNetworkLed (NETWORK_STATUS_CONNECTED);
		interimConnectionBreak = 0;


		pfds->fd = clientSocket_fd;
		pfds->events = (POLLIN | POLLOUT | POLLHUP | POLLERR);
		pfds->revents = 0;
		nfds = 1;

		// Once connected to the Sensor, Fetch data every 5 seconds.
		for (;;)
		{
			poll(pfds, nfds, -1); // Wait for events on Socket

			if ((pfds->revents & POLLHUP) || (pfds->revents & POLLERR))
			{
				printf ("\n\r POLLHUP");
				pfds->revents = 0;
				printf ("\n\rDetected Connection break !");
				interimConnectionBreak = 1;
				break;
			}
			else if (pfds->revents & POLLOUT)
			{
				printf ("\n\r POLLOUT");

				// Prepare Sensor Data packets every 10 seconds
				Prepare_SensorDataPackets (bufferOut, &OutBuffLen);
				bufferOut[NETWORK_BUFFER_TOTAL_LEN-1] = '\0';
				write (clientSocket_fd, bufferOut, strlen(bufferOut));
				pfds->revents = 0;
				sleep (10);
			}
			else
			{
				printf ("\n\r Other.. %d", pfds->revents);
				pfds->revents = 0;
			}
		}

		if (interimConnectionBreak)
		{
			close (clientSocket_fd);
			UpdateNetworkLed (NETWORK_STATUS_CONNECTION_BREAK);
			printf ("\n\rRetrying Connection to server in 10 seconds...");
			sleep (10);
		}
	}
}


/*
 * Read Digital values from the sensors
 */
void ReadSensor_Digital (void)
{
	uint8_t digitalValue = gpio_read_value (HUMIDITY_SENSOR_1_DIGITAL);
	printf ("\n\rDigital Value of Sensor = %d", digitalValue);

	if (digitalValue != HumidityValue[HUMIDITY_SENSOR_1].value_binary)
	{
		HumidityValue[HUMIDITY_SENSOR_1].value_binary = digitalValue;
		SensorDataPackets[0].sensorData.digitalVal = HumidityValue[HUMIDITY_SENSOR_1].value_binary;

		ReflectSensorDataUpdate ();
	}
	//#TODO: Signal Network thread to send data if same is changed..
	//#TODO: Add logic to read for second humidity sensor as well..
}


/*
 * Read Analog values from the sensors
 */
void ReadSensor_Analog (void)
{
	uint16_t analogValue = adc_read_value (HUMIDITY_SENSOR_1_ADC);
	printf ("\n\rAnalog Value of Sensor = %d", analogValue);

	if (analogValue != HumidityValue[HUMIDITY_SENSOR_1].value_analog)
	{
		HumidityValue[HUMIDITY_SENSOR_1].value_analog = analogValue;
		SensorDataPackets[0].sensorData.analogVal = HumidityValue[HUMIDITY_SENSOR_1].value_analog;
	}
	//#TODO: Signal Network thread to send data if same is changed..
	//#TODO: Add logic to read for second humidity sensor as well..
}


/*
 * Initialize Sensor database
 */
void InitSensors (void)
{
	memset (&HumidityValue, 0, sizeof(HumidityValue));
}


/*
 * Initialize Data packet
 */
void InitSensorDataPackets (void)
{
	memset (&SensorDataPackets, 0, sizeof(SensorDataPackets));

	SensorDataPackets[0].sensorType = eSensorType_Humidity;
	SensorDataPackets[0].sensorData.validity = 1;
	SensorDataPackets[0].sensorData.sensorNumber = 1;

	SensorDataPackets[1].sensorType = eSensorType_Humidity;
	SensorDataPackets[1].sensorData.validity = 0;
	SensorDataPackets[1].sensorData.sensorNumber = 2;
}


/*
 * Prepare Sensor data packets, each information-set is translated into 4 ASCII Characters
 */
void Prepare_SensorDataPackets (char* networkBuffer, uint16_t *buffLen)
{
	uint16_t index = 0;
	memset (networkBuffer, '\0', NETWORK_BUFFER_TOTAL_LEN);

	for (index = 0; index < DATA_PACKET_TOTAL_SENSORS; index++)
	{
		sprintf(networkBuffer, "%04d", SensorDataPackets[index].sensorData.validity);
		sprintf(networkBuffer+4, "%04d", SensorDataPackets[index].sensorType);
		sprintf(networkBuffer+8, "%04d", SensorDataPackets[index].sensorData.sensorNumber);
		sprintf(networkBuffer+12, "%04d", SensorDataPackets[index].sensorData.digitalVal);
		sprintf(networkBuffer+16, "%04d", SensorDataPackets[index].sensorData.analogVal);
		printf("\n\rSensor: Sending Data Packet: %s", networkBuffer);
		networkBuffer += DATALEN_PER_SENSOR;
	}
	*buffLen = (DATALEN_PER_SENSOR * DATA_PACKET_TOTAL_SENSORS);
}


/*
 * Perform IO Control based on Sensor reading
 */
void ReflectSensorDataUpdate (void)
{
	if (HumidityValue[HUMIDITY_SENSOR_1].value_binary)
	{
		write_trigger_values (SENSOR_DATA_INDICATION_1, "default-on");
		write_trigger_values (SENSOR_DATA_INDICATION_2, "default-on");
	}
	else
	{
		write_trigger_values (SENSOR_DATA_INDICATION_1, "none");
		write_trigger_values (SENSOR_DATA_INDICATION_2, "none");
	}
}

/*
 * Initialize IO Control
 */
void InitSensorDataIndications (void)
{
	write_trigger_values (SENSOR_DATA_INDICATION_1, "none");
	write_trigger_values (SENSOR_DATA_INDICATION_2, "none");
}


/*
 * Main Thread, Creates Network thread, performs Sensor reading every second
 * A change in Sensor's reading signals Network thread to send data to Server
 * Any new thread must be created from here.
 */
int main( int argc, char *argv[] )
{
	pthread_t network_thr_id;
	printf("\n\rData Concentrator: Starting Main Thread...");

	// Creating network Thread
	pthread_create (&network_thr_id, NULL, &networkThread, NULL);

	// Initialize Sensors; Read Digital Data and Analog Data from the Sensor.
	InitSensorDataIndications ();
	InitSensors();
	InitSensorDataPackets();

	// Initialize MQTT Stack

	// Print Sensor data on console.
    for (;;)
    {
    	printf("\n\n\r");
    	ReadSensor_Digital ();
    	ReadSensor_Analog ();
    	sleep(5);
    }
    return 0;
}

