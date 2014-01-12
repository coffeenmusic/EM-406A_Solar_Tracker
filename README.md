solar_tracker.c

Revision 5: Azimuth and Elevation Working and Displaying on LCD Screen

Known Issues: 	Must input time difference manually between your local time and UTC standard time
					The only ways I know how to do this a different way are by wireless or crude approximations of time zone areas
					
This program receives GPS data from the EM-406A GPS Receiver 
through UART communication, picks out the relevant data
and sends that data out on the USART transmission line

The GPS Transmission line should connect to the ATMEGA168 Recieve line

Available Data: 
GLL Message ID - Example: $GPRMC
GGA[Global Positioning System Fixed Data]............................
UTC Time				|	hhmmss.sss
Latitude				|	ddmm.mmmm
N/S Indicator			|	N=north or S=south	
Longitude				|	dddmm.mmmm
E/W Indicator			|	E=east or W=west
Position Fix Indicator	|	0 = fix not available
						|	1 = GPS SPS Mode, fix valid
						|	2 = Differential GPS, SPS Mode, fix valid
						|	3 = GPS PPS Mode, fix valid
Satellites Used			|	Range 0-12
HDP						|	Horizontal Dilution of Precision
MSL Altitude			|	meters
Units					| 	meters
Geoid Seperation		|	meters
Units					|	meters
Age of Diff. Corr.		|	[seconds] Null fields when DGPS is not used
Diff. Ref. Station ID	|
Checksum				|	*??		Example: *3F

GLL[Geographic Position-Latitude/Longitude]..........................
Latitude	
N/S Indicator
Longitude
E/W Indicator
UTC Position
Status
Checksum

GSA[GNSS DOP and Active Satellites]..................................
GSV[GNSS Satellites in View].........................................
RMC[Recommended Minimum Specific GNSS Data]..........................
VTG[Course Over Ground and Ground Speed].............................

Fix:
Change UTC to update with GGA_message instead of GPS_message