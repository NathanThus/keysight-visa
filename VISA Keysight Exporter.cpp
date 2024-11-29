// VISA Keysight Exporter.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <chrono>
#include <thread>
#include <fstream>
#include "./VISA/visa.h"

#define VISA_ADDRESS "USB0::0x2A8D::0x179B::CN59152133::0::INSTR"
#define QUERY_DELAY std::chrono::milliseconds(200)
#define PROBE_CHANNEL "1"
#define IEEEBLOCK_SPACE 5000000 // 5 Million. This requires at least 5MB of RAM

// Known Indexes for :WAVeform:PREamble:
#define WAVEFORM_X_INCREMENT 4
#define WAVEFORM_X_ORIGIN 5
#define WAVEFORM_Y_INCREMENT 7
#define WAVEFORM_Y_ORIGIN 8
#define WAVEFORM_Y_REFERENCE 9

// Device Session ID
ViSession defaultRM, scope; 
// Communication Status of the scope
ViStatus visaStatus; 

/*Depending on Query type, either use queryResultText or queryResultNumber*/
// For Text Queries
char queryResultText[256] = { 0 };
// For Single-Number Queries
double queryResultNumber = 0;
// For Multi-Number Queries
double queryResultNumbers[10];
// One giant block of data for the actual waveform.
unsigned char IEEEEBlockData[IEEEBLOCK_SPACE];

// Prototypes
void CloseConnection();
int DoQueryString(const char query[]);
int DoCommand(const char command[]);
int DoQueryIEEEBlock(const char query[]);
int DoQueryNumbers(const char query[]);
void QuerySleep(std::chrono::milliseconds sleep);

int main()
{
	std::cout << "Welcome to the unofficial KEYSIGHT EDUX 1000X Test program" << std::endl;

	visaStatus = viOpenDefaultRM(&defaultRM);
	if (visaStatus != VI_SUCCESS)
	{
		std::cout << "Error: Could not open the default resource manager" << std::endl;
		CloseConnection();
		return 1;
	}

	visaStatus = viOpen(defaultRM, VISA_ADDRESS, VI_NULL, VI_NULL, &scope);
	if (visaStatus != VI_SUCCESS)
	{
		std::cout << "Error: Could not connect to the scope!" << std::endl;
		CloseConnection();
		return 1;
	}

	//Set the timeout to 15 seconds, to allow for sufficient processing. Realistically, nothing should exceed this.
	visaStatus = viSetAttribute(scope, VI_ATTR_TMO_VALUE, 15000);
	if (visaStatus != VI_SUCCESS)
	{
		std::cout << "Error: Could not set the timeout value!" << std::endl;
		CloseConnection();
		return 1;
	}

	// Reset Settings

	visaStatus = viClear(scope);
	if (visaStatus != VI_SUCCESS)
	{
		std::cout << "Could not tell scope to clear!" << std::endl;
		return 1;
	}

	if (DoQueryString("*IDN?") == -1)
	{
		CloseConnection();
		return -1;
	}
	std::cout << queryResultText; // Has EOL by default.

	if (DoCommand(":autoscale") == -1)
	{
		CloseConnection();
		return -1;
	}

	std::cout << "Peforming Setup." << std::endl;

	if (DoCommand(":waveform:points:mode raw") == -1)
	{
		CloseConnection();
		return -1;
	}

	if (DoCommand(":waveform:points 10240") == -1)
	{
		CloseConnection();
		return -1;
	}

	if (DoCommand(":waveform:source channel2") == -1)
	{
		CloseConnection();
		return -1;
	}

	if (DoCommand(":waveform:format byte") == -1)
	{
		CloseConnection();
		return -1;
	}

	if (DoQueryNumbers(":WAVeform:PREamble?") == -1)
	{
		CloseConnection();
		return -1;
	}

	std::cout << "Requesting data." << std::endl;

	int bytes = DoQueryIEEEBlock(":waveform:data?");
	std::cout << "Bytes Transferred: " << bytes << std::endl;

	FILE* writeFilePtr;
	errno_t err = fopen_s(&writeFilePtr, "data.csv", "w");

	if (err != 0)
	{
		std::cout << "Error: Could not open file for writing!" << std::endl;
		return -1;
	}

	double x_origin = queryResultNumbers[WAVEFORM_X_ORIGIN];
	double x_increment = queryResultNumbers[WAVEFORM_X_INCREMENT];
	double y_increment = queryResultNumbers[WAVEFORM_Y_INCREMENT];
	double y_origin = queryResultNumbers[WAVEFORM_Y_ORIGIN];
	double y_reference = queryResultNumbers[WAVEFORM_Y_REFERENCE];

	std::cout << "Writing data to disk." << std::endl;

	for (size_t i = 0; i < bytes; i++)
	{
		/* Write time value, voltage value with inverted order in the column. */
		size_t reversedIndex = bytes - 1 - i;
		fprintf(writeFilePtr, "%9f, %6f\n",
			x_origin + ((float)i * x_increment),
			((y_reference - (float)IEEEEBlockData[reversedIndex]) * y_increment) - y_origin); // Inverted order in each column
	}

	fclose(writeFilePtr);
	std::cout<< "File Written To: data.csv" << std::endl;

	CloseConnection();

	return 0;
}

/// <summary>
///
/// </summary>
/// <param name="query">The query to perform</param>
/// <returns>0 on success, -1 on failure.</returns>
int DoQueryString(const char query[])
{
	char message[80] = { 0 };
	strcpy_s(message, query);
	strcat_s(message, "\n");
	visaStatus = viPrintf(scope, message);

	QuerySleep(QUERY_DELAY);

	if (visaStatus != VI_SUCCESS)
	{
		std::cout << "Failed in sending phase!" << std::endl;
		return -1;
	}
	visaStatus = viScanf(scope, "%t", queryResultText);
	if (visaStatus != VI_SUCCESS)
	{
		std::cout << "Failed while parsing!" << std::endl;
		return -1;
	}
	return 0;
}

int DoCommand(const char command[])
{
	char message[80] = { 0 };
	strcpy_s(message, command);
	strcat_s(message, "\n");
	visaStatus = viPrintf(scope, message);

	QuerySleep(QUERY_DELAY);

	if (visaStatus != VI_SUCCESS)
	{
		std::cout << "Failed in sending phase!" << std::endl;
		return -1;
	}
	return 0;
}

int DoQueryIEEEBlock(const char query[])
{
	char message[80] = { 0 };
	strcpy_s(message, query);
	strcat_s(message, "\n");
	visaStatus = viPrintf(scope, message);
	QuerySleep(QUERY_DELAY);

	if (visaStatus != VI_SUCCESS)
	{
		std::cout << "Failed in sending phase!" << std::endl;
		return -1;
	}
	int dataLength = IEEEBLOCK_SPACE;

	visaStatus = viScanf(scope, "%#b\n", &dataLength, IEEEEBlockData);
	if (visaStatus != VI_SUCCESS)
	{
		std::cout << "Failed while parsing!" << std::endl;
		return -1;
	}

	// This gets changed in the library
	if (dataLength == IEEEBLOCK_SPACE)
	{
		std::cout << "Not all data may have been saved!" << std::endl;
	}
	return (int)dataLength;
}

int DoQueryNumbers(const char query[])
{
	char message[80];
	strcpy_s(message, query);
	strcat_s(message, "\n");
	visaStatus = viPrintf(scope, message);
	if (visaStatus != VI_SUCCESS)
	{
		std::cout << "Failed in sending phase!" << std::endl;
		return -1;
	}

	QuerySleep(QUERY_DELAY);

	visaStatus = viScanf(scope, "%,10lf\n", queryResultNumbers);
	if (visaStatus != VI_SUCCESS)
	{
		std::cout << "Failed in parsing phase!" << std::endl;
		return -1;
	}
	return 0;
}

void CloseConnection()
{
	// Properly close the connections.
	viClose(scope);
	viClose(defaultRM);
	std::cout << "Press enter to exit." << std::endl;
	char* nothing= new char[80];
	std::cin >> nothing;
}

void QuerySleep(std::chrono::milliseconds sleep)
{
	std::this_thread::sleep_for(sleep);
}