#pragma once


#include "windows.h"


/**
 * gibt die Frequent des Winapi-Performancecounters in Herz zurueck
*/
inline size_t queryPerformanceFrequency() {
	LARGE_INTEGER perfFreq;
	QueryPerformanceFrequency(&perfFreq);
	return perfFreq.QuadPart;
};


/**
 * Gibt den aktuellen Wert des Winapi-Performancecounters zurueck. Sollte nur fuer Intervall-messungen verwendet werden
*/
inline size_t queryPerformanceCounter() {
	LARGE_INTEGER perfCount;
	QueryPerformanceCounter(&perfCount);
	return perfCount.QuadPart;
};