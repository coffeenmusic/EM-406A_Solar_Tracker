// solar_tracker.c
//
// Revision 5: Azimuth and Elevation Working and Displaying on LCD Screen
//
//
// Known Issues: 	Must input time difference manually between your local time and UTC standard time
// 					The only ways I know how to do this a different way are by wireless or crude approximations of time zone areas
//
// This program receives GPS data from the EM-406A GPS Receiver 
// through USART communication, picks out the relevant data
// and sends that data out on the USART transmission line
//
//
// 	The GPS Transmission line should connect to the ATMEGA168 Recieve line
//
//
// 	Available Data: 
//	GLL Message ID - Example: $GPRMC
//
//	GGA[Global Positioning System Fixed Data]............................
//
// 	UTC Time				|	hhmmss.sss
// 	Latitude				|	ddmm.mmmm
// 	N/S Indicator			|	N=north or S=south	
//	Longitude				|	dddmm.mmmm
// 	E/W Indicator			|	E=east or W=west
//	Position Fix Indicator	|	0 = fix not available
//							|	1 = GPS SPS Mode, fix valid
//							|	2 = Differential GPS, SPS Mode, fix valid
//							|	3 = GPS PPS Mode, fix valid
//	Satellites Used			|	Range 0-12
//	HDP						|	Horizontal Dilution of Precision
//	MSL Altitude			|	meters
//	Units					| 	meters
//	Geoid Seperation		|	meters
//	Units					|	meters
//	Age of Diff. Corr.		|	[seconds] Null fields when DGPS is not used
// 	Diff. Ref. Station ID	|
//	Checksum				|	*??		Example: *3F
//
//
//	GLL[Geographic Position-Latitude/Longitude]..........................
//
//	Latitude	
//	N/S Indicator
//	Longitude
//	E/W Indicator
//	UTC Position
//	Status
//	Checksum
//
//
//	GSA[GNSS DOP and Active Satellites]..................................
//	...
//
//	GSV[GNSS Satellites in View].........................................
//	...
//
//	RMC[Recommended Minimum Specific GNSS Data]..........................
//	...
//
//	VTG[Course Over Ground and Ground Speed].............................
//	...


// Fix:
// Change UTC to update with GGA_message instead of GPS_message


#define F_CPU 14745600
#define PI 3.14159265358979323846 	

#include <avr/io.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "../libnerdkits/delay.h"
#include "../libnerdkits/lcd.h"
#include "../libnerdkits/uart.h"

#include <avr/interrupt.h>
#include <avr/pgmspace.h>

char checksum, GPS_data;
char GPS_message[100],GGA_message[100];
char UTC_time[10], UTC_display[16];
char lat_ASCII[10], lon_ASCII[11];
unsigned int GPS_checksum, x, array_size;
int day_of_year, doy_count, time_diff_UTC_local, UTC_hour, UTC_minute, UTC_second, hour, minute, second;
float Azimuth, Elevation, Latitude, Longitude, hours_LST;



//Start up the Serial Port.........................................+++++++++++++++++
void serial_init(){
	// set baud rate to 4800bps
	UBRR0H = 0;	
	UBRR0L = 191;						// for 4800bps with 14.7456MHz clock
	// enable usart RX and TX
	UCSR0B = (1<<RXEN0)|(1<<TXEN0);
	// set 8N1 frame format
	UCSR0C = (1<<UCSZ01)|(1<<UCSZ00);

	// set up STDIO handlers so you can use printf, etc
	fdevopen(&uart_putchar, &uart_getchar);
  
	FILE uart_stream = FDEV_SETUP_STREAM(uart_putchar, uart_getchar, _FDEV_SETUP_RW);
	stdin = stdout = &uart_stream;
}
//.................................................................+++++++++++++++++




// This is necessary for verifying the checksum..<<<<
int ascii_to_hex(int GPS_data){
	if(GPS_data>='0' && GPS_data<='9'){
		GPS_checksum = GPS_data - '0';
	}
	if(GPS_data>='A' && GPS_data<='F'){
		GPS_checksum = GPS_data - 'A' + 10;
	}
		
	return GPS_checksum;
}
// ..............................................<<<<






//.................................................................................###############
// Example: $GPGGA,161229.487,3723.2475,N,12158.3416,W,1,07,1.0,9.0,M,,,,0000*18
void get_GPS_sentence(){
	
	while(GPS_data!='$'){
		GPS_data = uart_read();
	}
	
	if(GPS_data == '$'){											// Start at $ ASCII symbol
		
		int count = 0;
		do{		
			GPS_message[count] = GPS_data;
			GPS_data = uart_read();				
			while(GPS_data=='\0'){
				GPS_data = uart_read();
			}
			count++;			
		} while(GPS_data!='*');									// Stop at * ASCII symbol
		
		GPS_message[count] = GPS_data; 								// Record *
		count++;
		
		GPS_data = uart_read();
		GPS_message[count] = GPS_data;								// Record Hex Checksum 1
		count++;		
		GPS_checksum = ascii_to_hex(GPS_data);						// Checksum 1 - ASCII to Hex
		
		GPS_data = uart_read();
		GPS_message[count] = GPS_data;								// Record Hex Checksum 2
		count++;
		GPS_checksum = (GPS_checksum*16) + ascii_to_hex(GPS_data);	// Checksum 2 - ASCII to Hex
		
	}
}
//.................................................................................###############





// XOR all values between $ and * for checksum value....................................%%%
int calculate_checksum(){
	char checksum_temp = 0;
	for(x = 0; x<100; x++){
		if(GPS_message[x]!='$'){ 				//Start XOR after $
			if(GPS_message[x]=='*') {			// Save current value on *
				checksum = checksum_temp; 		// Don't XOR beyond the asterisk *
			} else {
				checksum_temp = GPS_message[x] ^ checksum_temp;
			}
		}		
	}
	
	return checksum;
}
//......................................................................................%%%






// Display the Universal Time Constant.................................^^^^^^^^^^
void get_UTC(){	

	// Make sure the UTC Time value gets filled before executing more code
	while(GPS_message[3]!='G' || GPS_message[4]!='G' || GPS_message[5]!='A'){
		get_GPS_sentence();
		checksum = calculate_checksum(GPS_message);
	}
	
	if(GPS_message[3]=='G' && GPS_message[4]=='G' && GPS_message[5]=='A'){
			
		// Convert the ASCII hour representation into an integer value
		UTC_hour = (GPS_message[7]-'0')*10 + (GPS_message[8]-'0');
		UTC_minute = ((GPS_message[9]-'0')*10) + (GPS_message[10]-'0');
		UTC_second = ((GPS_message[11]-'0')*10) + (GPS_message[12]-'0');
	}
}
//....................................................................^^^^^^^^^^








// Pull your current location from the GPS convert from ASCII to an integer and convert minutes to decimal...LATITUDE
void get_Latitude(){
	
	// Make sure the Latitude gets filled before executing more code
	while(GPS_message[3]!='G' || GPS_message[4]!='G' || GPS_message[5]!='A'){
		get_GPS_sentence();
		checksum = calculate_checksum(GPS_message);
	}
	
	// Convert ASCII latitude (only minutes) into integer latitude mm.mmmm
	Latitude =  ((GPS_message[20]-'0')*10.00) 	+
				((GPS_message[21]-'0')*1.000)	+
				((GPS_message[23]-'0')*0.100) 	+
				((GPS_message[24]-'0')*.0100)	+
				((GPS_message[25]-'0')*.0010)	+
				((GPS_message[26]-'0')*.0001);
				
	// Convert latitude minutes into decimal format
	Latitude = (Latitude/60) + ((GPS_message[18]-'0')*10) + (GPS_message[19]-'0');

}
//...........................................................................................................LATITUDE






// Pull your current longitude from the GPS convert from ASCII to an integer and convert minutes to decimal...LONGITUDE
void get_Longitude(){

	// Make sure the Longitude gets filled before executing more code
	while(GPS_message[3]!='G' || GPS_message[4]!='G' || GPS_message[5]!='A'){
		get_GPS_sentence();
		checksum = calculate_checksum(GPS_message);
	}
	
	// Convert ASCII longitude (only minutes) into integer longitude mm.mmmm
	Longitude = ((GPS_message[33]-'0')*10.00) 	+
				((GPS_message[34]-'0')*1.000)	+
				((GPS_message[36]-'0')*0.100) 	+
				((GPS_message[37]-'0')*.0100)	+
				((GPS_message[38]-'0')*.0010)	+
				((GPS_message[39]-'0')*.0001);
				
	// Convert longitude minutes into decimal format
	Longitude = (Longitude/60) + ((GPS_message[30]-'0')*100) + ((GPS_message[31]-'0')*10) + (GPS_message[32]-'0');
	
	if(GPS_message[41]=='W'){
		Longitude = -Longitude;
	}
}
//............................................................................................................LONGITUDE



// Get the day of the year including an extra day if it is a leap year.........................................Day Of The Year
int get_doy(){

	// Make sure the day of year gets filled before executing more code
	while(GPS_message[3]!='R' && GPS_message[4]!='M' && GPS_message[5]!='C'){
		get_GPS_sentence();
		checksum = calculate_checksum(GPS_message); // Match XOR of all characters between $ and * with the value following *
	}

	if(GPS_message[3]=='R' && GPS_message[4]=='M' && GPS_message[5]=='C'){
		char date[6];
		int day,month,year,doy;
		int count = 0;
		int fill_count = 0;
		for(x=0;x<100;x++){			
			if(count==9){
				date[fill_count] = GPS_message[x];
				fill_count++;
			}
			if(GPS_message[x]==','){
				count++;
			}
		}
		day = ((date[0]-'0')*10)+(date[1]-'0');
		month = ((date[2]-'0')*10)+(date[3]-'0'); 
		year = ((date[4]-'0')*10)+(date[5]-'0');
		
		switch(month){
			case 1:		// January
				doy = day;
				break;
			case 2:		// February
				doy = 31 + day;
				break;
			case 3:		// March
				doy = 59 + day;
				break;
			case 4:		// April
				doy = 90 + day;
				break;
			case 5:		// May
				doy = 120 + day;
				break;
			case 6:		// June
				doy = 151 + day;
				break;
			case 7:		// July
				doy = 181 + day;
				break;
			case 8:		// August
				doy = 212 + day;
				break;
			case 9:		// September
				doy = 243 + day;
				break;
			case 10:	// October
				doy = 273 + day;
				break;
			case 11:	// November
				doy = 304 + day;
				break;
			case 12:	// December
				doy = 334 + day;
				break;
		}
		
		if((year/4==floor(year/4)) || (year/400==floor(year/400))){ // Leap Year
			doy++;
		}
		
		return doy;
	}
}
//...........................................................................................................Day Of The Year





// Use UTC Time to calculate local time......Local Time
void get_current_time(){
	hour = UTC_hour - time_diff_UTC_local;
	if(hour<0){
		hour = hour + 24;
	} else if(hour>24) {
		hour = hour - 24;
	}
}
//...........................................Local Time






// Perform Calculations to get Azimuth and Elevation of the Sun.........SOLAR_CALCULATIONS
void get_Az_El(){
	
	// I must manually get this information because I do not know a way to 
	// get this other than using wireless or very crude approximations
	time_diff_UTC_local = 4;	// Input the time difference between your local time and UTC standard time [in hours]
	
	// This calculates how many degrees you are away from the UTC standard
	// meridian (Prime Meridian), because the earth rotates 15 degrees every hour.
	// Meridian is the key word in local standard time meridian.
	// This gives your Meridian's longitude because the Prime Meridian is 0 deg
	int LSTM = (15*time_diff_UTC_local);  // Local Standard Time Meridian [Degrees]
	
	float B = ((360.0/365.0)*(day_of_year-81.0))*(PI/180.0); // [Radians]

	// Equation of time is the difference between mean solar time and solar time
	// Solar time is based off of Noon being orthogonal to your location
	// Solar mean time is the average noon over a 365.25 day period
	float equation_of_time = (9.87*sin(2.0*B))-(7.53*cos(B))-(1.5*sin(B));
	
	// The earth rotates 4mins every degree, so this calculates the number of
	// minutes to correct the time due to the difference
	float time_Correction = (4.0*(Longitude + LSTM)) + equation_of_time; // [minutes]
	float time_Correction_sec = round((time_Correction - floor(time_Correction))*60.0); // [seconds]
	
	get_current_time(); // Local Time
	
	// Get Local Solar Time and convert to decimal hour~~~~~~~~~~~~~~~~~~
	hours_LST = hour;
	int minutes_LST = UTC_minute + floor(time_Correction);
	if(minutes_LST>60){
		hours_LST++;
		minutes_LST = minutes_LST - 60;
	}
	int seconds_LST = UTC_second + time_Correction_sec;
	if(seconds_LST>60){
		minutes_LST++;
		seconds_LST = seconds_LST - 60;
		if(minutes_LST>60){
			hours_LST++;
			minutes_LST = minutes_LST - 60;
		}
	}
	hours_LST = hours_LST + (minutes_LST/60.0) + (seconds_LST/3600.0);
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	
	float hour_Angle = 15*(hours_LST-12);
	
	// This is the angle between the equatorial plane and the path of solar rays
	float declination = (180.0/PI)*asin(sin(23.45*(PI/180.0))*sin((360.0/365.0)*(day_of_year-81.0)*(PI/180.0)));  // Declination Angle [Degrees]
	
	// This is the angle between level ground at your position and the sun
	Elevation = asin((sin(declination*(PI/180.0))*sin(Latitude*(PI/180.0)))+(cos(declination*(PI/180.0))*cos(Latitude*(PI/180.0))*cos(hour_Angle*(PI/180.0))))*(180.0/PI);

	// This is the angle between north and the sun in a clockwise direction
	Azimuth = (acos(((sin(declination*(PI/180.0))*cos(Latitude*(PI/180.0)))-(cos(declination*(PI/180.0))*sin(Latitude*(PI/180.0))*cos(hour_Angle*(PI/180.0))))/cos(Elevation*(PI/180.0))))*(180.0/PI);
	if(hours_LST>12){
		Azimuth = 360.0 - Azimuth; 
	}

	//printf_P(PSTR("Latitude: %f\t\tLongitude: %f\n\r"),Latitude,Longitude);
	//printf_P(PSTR("Elevation: %f\t\tAzimuth: %f\n\r"),Elevation,Azimuth);
	
}
//......................................................................SOLAR_CALCULATIONS







// This function writes the sentence to a different array depending on the ID
// It can be expanded in the future, but for now it only uses the GGA ID..............SENTENCE_ID
void set_sentence_ID(char id[3]){
	if(GPS_message[3]==id[0] && GPS_message[4]==id[1] && GPS_message[5]==id[2]){
		for(x=0;x<100;x++){
			GGA_message[x] = GPS_message[x]; 
		}
	} 
}
//....................................................................................SENTENCE_ID







// ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::MAIN
int main() { 	
	
	// Initialize the serial port
	uart_init();
	FILE uart_stream = FDEV_SETUP_STREAM(uart_putchar, uart_getchar, _FDEV_SETUP_RW);
	stdin = stdout = &uart_stream;
	
	// Start up the LCD
	lcd_init();
	FILE lcd_stream = FDEV_SETUP_STREAM(lcd_putchar, 0, _FDEV_SETUP_WRITE);
	lcd_home();
	
	// Make sure the GPS reciever has a satellite fix
	while(GPS_message[3]!='G' || GPS_message[4]!='G' || GPS_message[5]!='A'){
		get_GPS_sentence();
		checksum = calculate_checksum(GPS_message);
	}
	if(GPS_message[3]=='G' && GPS_message[4]=='G' && GPS_message[5]=='A'){
	printf_P(PSTR("%s\r\n"),GPS_message);
		int comma_count = 0;
		int sentence_position = 0;
		for(x=0;x<100;x++){
			if(GPS_message[x]==','){
				if(comma_count==6){
					sentence_position = x;
				}
				comma_count++;
			}
		}
		while(GPS_message[sentence_position]=='0'){
			lcd_home();
			lcd_write_string(PSTR("  No Satellite Fix  "));
			lcd_line_three();
			lcd_write_string(PSTR("    Please Wait    "));
			get_GPS_sentence();
		}
	}
  
  // loop keeps looking forever 
  while(1) {
	 
	get_GPS_sentence(); // Example: $GPGGA,161229.487,3723.2475,N,12158.3416,W,1,07,1.0,9.0,M,,,,0000*18
	
	checksum = calculate_checksum(GPS_message); // Match XOR of all characters between $ and * with the value following *
	
	char blah[3];
	blah[0] = 'G'; blah[1] = 'G'; blah[2] = 'A';
	set_sentence_ID(blah);
	
	get_Latitude();				// [Degrees]
	get_Longitude();			// [Degrees]
	day_of_year = get_doy();	// Get the Day of the Year
	get_UTC();					// Universal Time Constant [GMT]
	get_Az_El();				// Calculate the Solar Azimuth and Elevation	
	
	
	
	// write message to LCD
    lcd_home();
	fprintf_P(&lcd_stream, PSTR("Time: %02i:%02i:%02i"),hour,UTC_minute,UTC_second);
	lcd_line_two();
	fprintf_P(&lcd_stream, PSTR("Lat:%.02f  Lon:%.02f"),Latitude,Longitude);
	lcd_line_three();
	fprintf_P(&lcd_stream, PSTR("Elevation: %.2f"), Elevation);
    lcd_line_four();
	fprintf_P(&lcd_stream, PSTR("Azimuth: %.2f"), Azimuth);	
  }
  
  return 0;
}
// ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::MAIN
