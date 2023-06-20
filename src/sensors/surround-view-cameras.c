#include "../../include/service-functions.h"


#define PARK_TIME 30


int main(int argc, char const *argv[])
{
	int surround_view_cameras_pipe_fd,
		read_bytes,
		log_cameras_fd,
		counter = 0,
		ret = EXIT_FAILURE;
		
	char *input = malloc(SURR_CAM_LEN);
	
	FILE* file_descriptor;
		
	file_descriptor = fopen("/dev/urandom","r");
	
	log_cameras_fd = open("../../log/cameras.log", O_WRONLY | O_APPEND | O_CREAT, 0644);
	
	do
	{
		surround_view_cameras_pipe_fd = open("assist-surround.pipe",O_WRONLY);
		
		if(surround_view_cameras_pipe_fd == -1)
		sleep(0.1);
		
	} while (surround_view_cameras_pipe_fd == -1);
	
	/** 
	 * Ciclo che legge da /dev/urandom e scrive su cameras.log,
	 * e contemporaneamente scrive su assist-surround.pipe
	 */
	while(ret)
	{
		read_bytes = fread(input,1,SURR_CAM_LEN,file_descriptor);
		
		if(read_bytes != SURR_CAM_LEN){
			perror("fread");
			ret = EXIT_FAILURE;
		}
		else{
			broad_log(surround_view_cameras_pipe_fd,log_cameras_fd,input,SURR_CAM_LEN);
			
				//Aspettare un secondo prima di aggiungere uno al contatore
			wait(1);
			
			if(counter++ == PARK_TIME)
				ret = EXIT_SUCCESS;
		}
	}
	
	
	//Chiusura di tutti i file descriptor
	fclose(file_descriptor);
	close(log_cameras_fd);
	close(surround_view_cameras_pipe_fd);
	
	return ret;
}