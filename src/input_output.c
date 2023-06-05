#include <stdio.h>

int main(int argc, char const *argv[])
{
	short connected = 0;
	
	while (!connected)
	{
		//Search for server to connect
		if (connected)
		{
			printf("Succesfully connected to server \"Fill in\"");
			connected = 1;
		}
		else
		{
			printf("Be sure to have an existing servet to connect to\nTrying again in 2s");
			
		}
		
	}
	
	
	
	return 0;
}
