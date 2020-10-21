#include "pch.h"
#include <iostream>
#include <string>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <bitset>
#include <string>
#include <windows.h>
#include <queue>
#include <thread>
#include <mutex>
using namespace std;

enum errorType {
	Success = 0,
	PORTISNOTAVALIBLE = 1,
	OTHERERROR = 2,
	CANTGETPARMS = 3,
	CANTSETPARMS = 4,
	READERROR = 5,
	TIMEOUT = 6,
	TOOBIGDATA = 7
};

class Arduino
{
private:
	const int WaitTime = 5;
	HANDLE handler;
	bool connected;
	COMSTAT status;
	errorType Errorer;
	DWORD errors;

	bool Connect(const char *PortName, unsigned int BoudRate) {
		handler = CreateFileA(static_cast<LPCSTR>(PortName),
			GENERIC_READ | GENERIC_WRITE,
			0,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL);
		if (handler == INVALID_HANDLE_VALUE) {
			if (GetLastError() == ERROR_FILE_NOT_FOUND) {
				Errorer = PORTISNOTAVALIBLE;
			}
			else {
				Errorer = OTHERERROR;
			}
			return false;
		}
		else {
			DCB SerialParam = { 0 };

			if (!GetCommState(handler, &SerialParam)) {
				Errorer = CANTGETPARMS;
				return false;
			}
			else {
				SerialParam.BaudRate = BoudRate;
				SerialParam.ByteSize = 8;
				SerialParam.StopBits = ONESTOPBIT;
				SerialParam.Parity = NOPARITY;
				SerialParam.fDtrControl = DTR_CONTROL_ENABLE;

				if (!SetCommState(handler, &SerialParam))
				{
					Errorer = CANTSETPARMS;
					return false;
				}
				else {
					connected = true;
					PurgeComm(this->handler, PURGE_RXCLEAR | PURGE_TXCLEAR);
					Errorer = Success;
					return true;
				}
			}
		}
	}
	bool ErrorTest() {
		if (connected && ClearCommError(handler, &errors, &status)) {
			Errorer = Success;
			return false;
		}
		return true;
	}
public:
	Arduino() {
		connected = false;
	}
	Arduino(string &PortName, unsigned int BoudRate) {
		Connect(PortName.c_str(), BoudRate);
	}
	Arduino(const char *PortName, unsigned int BoudRate) {
		Connect(PortName, BoudRate);
	}
	bool TryConnect(string &PortName, unsigned int BoudRate) {
		return Connect(PortName.c_str(), BoudRate);
	}
	bool TryConnect(const char *PortName, unsigned int BoudRate) {
		return Connect(PortName, BoudRate);
	}

	errorType Error() {
		return Errorer;
	}
	const char* StringError() {
		switch (Errorer)
		{
		case Success:
			return "Wykonano prawidlowo!";
			break;
		case PORTISNOTAVALIBLE:
			return "Pod podany port nie podlaczono zadnego urzadzenia!";
			break;
		case OTHERERROR:
			return "Port jest zajety przez inny program!";
			break;
		case CANTGETPARMS:
			return "Nie mozna wczytac ustawien portu";
			break;
		case CANTSETPARMS:
			return "Nie mozna zmienic ustawien portu";
			break;
		case READERROR:
			return "Podczas odczytu urzadzenie zostalo odlaczone!";
			break;
		case TIMEOUT:
			return "Urzadzenie nie odpowiadalo zbyt dlugi czas!";
		case TOOBIGDATA:
			return "Dane sa za duze!";
		default:
			break;
		}
	}
	~Arduino() {
		if (connected) {
			connected = false;
			CloseHandle(handler);
		}

	}
	bool isConnected()
	{
		if (!ClearCommError(handler, &errors, &status))
			connected = false;

		return connected;
	}
	bool ClearBuffor() {
		if (!connected) {
			return false;
		}
		ClearCommError(handler, &errors, &status);
		if (status.cbInQue) {
			ReadFile(handler, NULL, status.cbInQue, NULL, NULL);
		}
	}

	//Pobiera Dane z Arduino, wpisuje je do stringa out, jezeli napotka znak EndSing automatycznie przerywa
	bool ReadUntil(string &out, const char EndSing = '\n', const unsigned int timeout = 5000) {
		if (!connected) {
			return false;
		}
		ClearCommError(handler, &errors, &status);
		char buff = 0;
		out.clear();
		DWORD READ;
		int time = 0;

		while (buff != EndSing) {
			time = 0;
			while (status.cbInQue == 0) {
				Sleep(WaitTime);
				time += WaitTime;
				if (time > timeout || errors != 0) {
					Errorer = TIMEOUT;
					return false;
				}
				ClearCommError(this->handler, &errors, &status);
			}

			ReadFile(handler, &buff, 1, &READ, NULL);

			if (READ != 1) {
				Errorer = READERROR;
				return false;
			}
			if (buff != 0) {
				out.append(1, buff);
			}
		}
		return true;
	}

	//Pobiera Dane z Arduino, wpisuje je do tablicy out o dlugosci BufSize, jezeli napotka znak EndSing automatycznie przerywa
	bool ReadUntil(char* out, const int BufSize, const char EndSing = '\n', const unsigned int timeout = 5000) {
		if (ErrorTest()) {
			return false;
		}
		char buff = 0;
		DWORD READ;
		int time = 0;

		for (int i = 0; i < BufSize; i++) {
			time = 0;
			while (status.cbInQue == 0) {
				Sleep(WaitTime);
				time += WaitTime;
				if (time > timeout) {
					Errorer = TIMEOUT;
					return false;
				}
				if (errors != 0) {
					Errorer = OTHERERROR;
					return false;
				}
				ClearCommError(this->handler, &errors, &status);
			}

			ReadFile(handler, &buff, 1, &READ, NULL);

			if (READ != 1) {
				Errorer = READERROR;
				return false;
			}
			out[i] = buff;
			if (buff != EndSing) {
				return true;
			}
		}
	}

	//Pobiera Dane z Arduino, wpisuje je do stringa out o dlugosci MaxSize, jezeli napotka znak EndSing automatycznie przerywa
	bool ReadMax(string &out, const char EndSing = '\n', unsigned int MaxSize = 1024, const unsigned int timeout = 5000) {
		if (ErrorTest()) {
			return false;
		}
		char buff = 0;
		out.clear();
		DWORD READ;
		int time = 0;

		while (buff != EndSing) {
			time = 0;
			while (status.cbInQue == 0) {
				Sleep(WaitTime);
				time += WaitTime;
				if (time > timeout) {
					Errorer = TIMEOUT;
					return false;
				}
				if (errors != 0) {
					Errorer = OTHERERROR;
					return false;
				}
				ClearCommError(handler, &errors, &status);
			}

			ReadFile(handler, &buff, 1, &READ, NULL);

			if (READ != 1) {
				Errorer = READERROR;
				return false;
			}
			out.append(1, buff);
			if (out.size() > 1024) {
				return false;
			}
		}
		return true;
	}

	template<typename T>
	bool Read(T* Data, const unsigned int Size) {
		if (ErrorTest()) {
			return false;
		}
		DWORD READ;
		if (!ReadFile(handler, Data, Size*sizeof(T), &READ, NULL)) {
			return false;
		}
		if (READ != Size*sizeof(T)) {
			Errorer = READERROR;
			return false;
		}
		return true;
	}

	//Komputer czeka az Arduino nada znak Sing
	void Sync(const char Sing = '\n', const unsigned int timeout = 20000) {
		if (ErrorTest()) {
			return;
		}
		char buff = 0;
		int time = 0;

		while (buff != Sing) {
			time = 0;
			while (status.cbInQue == 0) {
				Sleep(WaitTime);
				time += WaitTime;
				if (time > timeout) {
					Errorer = TIMEOUT;
				}
				if (errors != 0) {
					Errorer = OTHERERROR;
				}
				ClearCommError(handler, &errors, &this->status);
			}

			ReadFile(handler, &buff, 1, NULL, NULL);
		}
	}

	//Wpisuje Stringa do Arduino
	bool WriteData(string Data) {
		if (Data.back() != '\n') {
			Data += '\n';
		}
		return WriteDataC(Data.c_str(), Data.length() + 1);
	}

	//Wpisuje tablice charów do Arduino
	bool WriteDataC(const char* Data, const unsigned int DataSize) {
		if (ErrorTest()) {
			return false;
		}
		if (WriteFile(handler, (void*)Data, DataSize, NULL, 0)) {
			return true;
		}
		return false;
	}

	template<class T>
	bool Write(T* Data, const unsigned int DataSize) {
		if (ErrorTest()) {
			return false;
		}
		if (WriteFile(handler, (void*)Data, DataSize, NULL, 0)){
			return true;
		}

		return false;
	}

	template<class T>
	//Wpisuje Dane typu <T> o dlugosci DataSize, pierwsza wyslana wartosc zawiera dlugosc T
	bool WriteAndGiveDSize(T* Data, const unsigned int DataSize) {
		if (ErrorTest()) {
			return false;
		}
		if (sizeof(T) > 255) {
			Errorer = TOOBIGDATA;
			return false;
		}
		if (!WriteFile(handler, (unsigned char)sizeof(T), 1, NULL, 0)) {
			return false;
		}
		if (WriteFile(handler, (void*)Data, DataSize, NULL, 0)) {
			return true;
		}
		return false;
	}

	unsigned int InBuforSize() {
		if (!connected) {
			return false;
		}
		ClearCommError(this->handler, &errors, &this->status);
		return status.cbInQue;
	}
	unsigned int OutBuforSize() {
		if (!connected) {
			return false;
		}
		ClearCommError(this->handler, &errors, &this->status);
		return status.cbOutQue;
	}

	Arduino& operator= (const Arduino&) = delete;
	Arduino(const Arduino&) = delete;
};


//Example
int main()
{
	const int BlockSize = 128;
	int* Data = new int[BlockSize];
	for (int i = 0; i < BlockSize; i++) {
		Data[i] = 0;
	}
	cout << sizeof(int) << endl;
	Arduino arduino;

	while (true) {
		if (arduino.TryConnect("\\\\.\\COM4", 9600)) {
			
			cout << "Polaczono z Arduino" << endl;

			string out;
			arduino.Sync();
			while (arduino.isConnected()) {
				
				
			}
		}
		else {
			cout << arduino.StringError() << endl;
		}
		Sleep(100);
	}
}
