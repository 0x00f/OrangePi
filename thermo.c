#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "dht22_driver.h"
#include "ds18b20_driver.h"

//#define DEBUG_MSG 
#define TIMEOUT 10 // seconds to wait for DHT reading before giving up

float humidity_ambient = 0, temperature_ambient = 0;
float *hum_pntr = &humidity_ambient;
float *temp_pntr = &temperature_ambient;

float temp_probe1 = 0;
float temp_probe2 = 0;

int main(void) {

	int i = 0; // counter var for timeout

	if(dht22_init()) {
		#ifdef DEBUG_MSG
		printf("WiringOP Library loaded\n");
		#endif
	}
	
	temp_probe1 = Read_Temperature();
	dht22_read_val(hum_pntr, temp_pntr);
	
	// if(dht22_read_val(hum_pntr, temp_pntr)) {
	// 	if(humidity_ambient == 0.0) {
	// 		printf("Read Error\n");
	// 	} 
	// 	//printf("DHT22 Read successful\n");
	// 	//printf("Humidity = %.2f%%, Ambient Temperature = %.2f°C\n", humidity_ambient, temperature_ambient);
	// 	//printf("%.2f,%.2f,%.2f\n", humidity_ambient, temperature_ambient, temp_probe1);
	// 	//while(humidity_ambient <= 10) {
	// 	//	delay(1000);
	// 	//	dht22_read_val(hum_pntr, temp_pntr);
	// 	//}
	// }
	// else {
	// 	//printf("DHT22 Read error\n");
	// 	//printf("-2\n"); // print error code to stdout
	// 	//return 1; // error
	// }
	

	
	//printf("Temperature Probe1: %.2f\n", temp_probe1);

	while(humidity_ambient == 0.00 || temp_probe1 == 0.00) {
		i += 1;
		if (i == TIMEOUT)
			break; // read timeout
		delay(1000);
		dht22_read_val(hum_pntr, temp_pntr);
		temp_probe1 = Read_Temperature();
	}
	
	printf("%.2f,%.2f,%.2f\n", humidity_ambient, temperature_ambient, temp_probe1);
	return 0;
}
