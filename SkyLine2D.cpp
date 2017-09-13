#include <stdio.h>
#include <stdint.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <string.h>
#include <malloc.h>

typedef uint8_t uint8;
typedef int8_t int8;


struct motor_packet
{
	uint8 AddressByte;
	uint8 CommandByte;
	uint8 DataByte;
	uint8 Checksum;
};


void DriveForeward(motor_packet *Packet, int MotorAddress, int Speed)
{
	Packet->AddressByte = MotorAddress;
	Packet->CommandByte = 0;
	Packet->DataByte = Speed;
	Packet->Checksum = ((MotorAddress + 0 + Speed) & 0b01111111);
}

void DriveBackward(motor_packet *Packet, int MotorAddress, int Speed)
{
	Packet->AddressByte = MotorAddress;
	Packet->CommandByte = 1;
	Packet->DataByte = Speed;
	Packet->Checksum = ((MotorAddress + 1 + Speed) & 0b01111111);
}

bool OpenPort(const char *PortName, int *FileDescriptor)
{
	*FileDescriptor = open(PortName, O_RDWR | O_NOCTTY | O_NDELAY);
	return (*FileDescriptor < 0) ? false : true;
}

bool WriteToSyRen(motor_packet *Packet, int FileDescriptor)
{
	int BytesWritten = write(FileDescriptor, Packet, 4);

	return (BytesWritten < 0) ? false : true;
}

int main()
{
	
	int Status;
	addrinfo Hints = {};
	addrinfo *ServerInfo;
	
	memset(&Hints, 0, sizeof(Hints));

	Hints.ai_family = AF_INET;
	Hints.ai_socktype = SOCK_STREAM;
	Hints.ai_flags = AI_PASSIVE;

	Status = getaddrinfo(NULL, "7000", &Hints, &ServerInfo);

	if (Status != 0)
	{
		printf("error with getaddrinfo\n");
	} 

	//create a socket
	int SocketFileDescriptor = socket(ServerInfo->ai_family, ServerInfo->ai_socktype, ServerInfo->ai_protocol);
	
	if(SocketFileDescriptor < 0)
	{
		printf("Error opening socket\n");
	}

	int BindSuccessful = bind(SocketFileDescriptor, ServerInfo->ai_addr, ServerInfo->ai_addrlen);

	if(BindSuccessful < 0)
	{
		printf("Bind failed\n");
	}

	int ListenSuccessful = listen(SocketFileDescriptor, 5);

	if(ListenSuccessful < 0)
	{
		printf("Cannot Listen\n");
	}

	sockaddr_storage ClientAddress;
	socklen_t ClientAddressSize = sizeof(ClientAddress);

	int NewSocketFileDescriptor = accept(SocketFileDescriptor, (sockaddr *)&ClientAddress, &ClientAddressSize);

	if(NewSocketFileDescriptor < 0)
	{
		printf("Error on accept\n");
	}
	else
	{
		printf("Success on accept!\n");
	}


	motor_packet Packet = {};

	int FileDescriptor;

	if(OpenPort("/dev/ttyS0", &FileDescriptor))
	{
		int BytesWritten;

		char KeyPressed;

		bool Done = false;

		//						Main run loop
		while(!Done)
		{
			int8 *RecievedCommand =(int8 *) malloc(sizeof(int8));

			int RecieveStatus = recv(NewSocketFileDescriptor, RecievedCommand, sizeof(int8), 0);

			if(RecieveStatus < 0)
			{
				printf("error recieving data\n");
			}
			else
			{
				if(RecieveStatus == 0)
				{
					printf("client disconnected\n");
				}
				else
				{
					switch(*RecievedCommand)
					{
						case 0:
						DriveForeward(&Packet, 128, 0);
						break;
					
						case 1:
						DriveForeward(&Packet, 128, 64);
						break;

						case 2:
						DriveBackward(&Packet, 128, 64);
						break;
					}	
				}
			}				

			if(!WriteToSyRen(&Packet, FileDescriptor))
			{
				printf("error writing to SyRen\n");
			}

			Packet = {};
			RecievedCommand = NULL;
		}
	}
	else
	{
		printf("Error opening port\n");
	}
}
