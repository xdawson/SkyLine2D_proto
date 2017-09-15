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

/* Open a new socket for the server to listen on for incomming connections 

   Takes as an argument the port number you want the server to listen on 
   (this must be the same number as the port you ask the phone to connect to)

   Returns -1 if an error occured or the File Descriptor if the socket was successfully created 
*/
int OpenServerSocket(const char *Port)
{	
	int Status;
	addrinfo Hints = {};
	addrinfo *ServerInfo;
	
	memset(&Hints, 0, sizeof(Hints));

	Hints.ai_family = AF_INET;
	Hints.ai_socktype = SOCK_STREAM;
	Hints.ai_flags = AI_PASSIVE;

	Status = getaddrinfo(NULL, Port, &Hints, &ServerInfo);

	if (Status == 0)
	{
		int SocketFileDescriptor = socket(ServerInfo->ai_family, ServerInfo->ai_socktype, ServerInfo->ai_protocol);
		
		if(SocketFileDescriptor >= 0)
		{
			int BindSuccessful = bind(SocketFileDescriptor, ServerInfo->ai_addr, ServerInfo->ai_addrlen);

			if(BindSuccessful >= 0)
			{
				int ListenSuccessful = listen(SocketFileDescriptor, 5);

				if(ListenSuccessful >= 0)
				{
					sockaddr_storage ClientAddress;
					socklen_t ClientAddressSize = sizeof(ClientAddress);

					int NewSocketFileDescriptor = accept(SocketFileDescriptor, (sockaddr *)&ClientAddress, &ClientAddressSize);

					if(NewSocketFileDescriptor >= 0)
					{
						printf("Success on accept!\n");
						freeaddrinfo(ServerInfo);
						return NewSocketFileDescriptor;
					}
					else
					{
						printf("Error on accept\n");
						freeaddrinfo(ServerInfo);
						return -1;
					}
				}
				else
				{
					printf("Cannot Listen\n");
					freeaddrinfo(ServerInfo);
					return -1;
				}
			}
			else
			{
				printf("Bind failed\n");
				freeaddrinfo(ServerInfo);
				return -1;
			}
		}
		else
		{
			printf("Error opening socket\n");
			freeaddrinfo(ServerInfo);
			return -1;
		}
	} 
	else
	{
		printf("error with getaddrinfo\n");
		freeaddrinfo(ServerInfo);
		return -1;
	}
}

int main()
{
	
	int ServerSocketFileDescriptor = OpenServerSocket("7000");

	motor_packet Packet = {};

	int FileDescriptor;

	if(OpenPort("/dev/ttyS0", &FileDescriptor))
	{
		bool Done = false;

		// Main run loop
		while(!Done)
		{
			int8 *RecievedCommand =(int8 *) malloc(sizeof(int8));

			int RecieveStatus = recv(ServerSocketFileDescriptor, RecievedCommand, sizeof(int8), 0);

			if(RecieveStatus >= 0)
			{
				if(RecieveStatus != 0)
				{
					switch(*RecievedCommand)
					{
						//Stop motor
						case 0:
						DriveForeward(&Packet, 128, 0);
						break;
						
						//Drive Forward
						case 1:
						DriveForeward(&Packet, 128, 64);
						break;

						//Drive Backward
						case 2:
						DriveBackward(&Packet, 128, 64);
						break;
					}	
				}
				else
				{
					printf("client disconnected\n");
				}
			}
			else
			{
				printf("error recieving data\n");
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
