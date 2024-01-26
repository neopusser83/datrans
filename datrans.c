/*
    datrans: Convert any files to video.
    Copyright (C) 2023  neopusser83  <srmomichi@yahoo.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

/* TODO:
	* Improve the code structure.
	* Implement fork() for MinGW.
	* Implement image writing in threads for improved speed.
	* Make the program independent of ffmpeg.
	* Add a quiet mode.
	* Add a no-confirm mode (for scripting).
	* Add a video codec chooser.
	* Add a resolution chooser.
*/

#include <stdio.h>
#include <stdlib.h>
#include <png.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <libgen.h>

#ifndef __WIN32
#include <sys/wait.h>
#endif

#ifdef __WIN32
#include <windows.h>
#include <conio.h>
#endif

#define PNG_WIDTH  1280
#define PNG_HEIGHT 720

#define PIXEL_SIZE 4
#define WORD_SIZE 8 /* no needed anymore */

void img_index(char *ptr, int num,int pid){
	sprintf(ptr,"./%d_frames/frame_%06d.png",pid,num);
}

void get_bin(char c, int *num){
	int i = 0;
	for (int j = 7; j >= 0; j--){
		num[i] = ((c >> j) & 1);
		i++;
	}
}

char set_bin(int *num){

	unsigned char byte = 0;

	for(int i=0;i<8;i++){
		byte = (byte << 1) | num[i];
	}
	
	return byte;
}

void print_num(int *arr){
	for(int i=0;i<8;i++){
		printf("%d",arr[i]);
	}
	printf("\n");
}

FILE *read_file(const char *filename, long int *filesize){
	FILE *fd = fopen(filename,"rb");

	if(!fd){
		printf("Error opening file : %s\n",filename);
		exit(1);
	}

	fseek(fd,0L,SEEK_END);
	*filesize = ftell(fd);
	fseek(fd,0L,SEEK_SET);

	return fd;
}

void set_pixel(png_bytep row,int x, int color){
	for(int i=0;i<PIXEL_SIZE; i++){
		row[x * PIXEL_SIZE + i] = color;
	}
}

int get_pixel(png_bytep row, int x, int y){

	png_bytep px = &(row[x * 4]);

//TODO!!

/*
	if((px[0] == 0 || px[0] == 255) &&
		(px[1] == 0 || px[1] == 255) &&
		(px[2] == 0 || px[2] == 255)){
		
		if(px[0] == 255){
			return 0;
		}
		else{
			return 1;
		}
	}
	else{
		return 127;
	}

*/

	if(px[0] > 200){
		return 0;
	}
	if(px[0] > 100 && px[0] < 200){
		return 127;
	}
	if(px[0] < 100){
		return 1;
	}
	
	else{
		return 127;
	}

}

int dig(int num){
	int quan = 0;

	if (num == 0) {
		return 1;
	}

	while (num != 0) {
		num = num / 10;
		quan++;
	}

	return quan;
}

#ifdef __WIN32

#define B_GREEN	10
#define B_WHITE	15
#define	B_RED	12
#define RED		4
#define WHITE	7
#define CYAN	3

#else

#define B_GREEN	10
#define B_WHITE	15
#define	B_RED	1
#define RED		9
#define WHITE	7
#define CYAN	6

#endif

void change_color(int color){
	#ifndef __WIN32
	printf("\033[38;5;%dm",color);
	#else
	HANDLE hCon = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(hCon,color);
	#endif
}

int encode(char *filename){
	
	long int sz;

	FILE *input = read_file(filename,&sz);

	if(sz == 0){
		fprintf(stderr,"Error. The file is empty.\n");
		fclose(input);
		return(1);
	}
		
	long unsigned int max_img_size;
	max_img_size = ((PNG_WIDTH*PNG_HEIGHT)/WORD_SIZE)/(PIXEL_SIZE*PIXEL_SIZE);
	
	unsigned int img_quant = sz/max_img_size;

	img_quant++;

	change_color(B_WHITE);	
	fprintf(stderr," * Max image size: %ld bytes\n",max_img_size);
	fprintf(stderr," * Input buffer size: %ld bytes\n",sz);	
	fprintf(stderr," * Frame quant: %d\n\n",img_quant);
	
	change_color(B_RED);
	fprintf(stderr," > Press any key to continue...");
	
	#ifndef __WIN32
	getchar();
	#else
	getch();
	#endif

	long unsigned int buf_ind = 0;
	
	FILE *png_output;

	png_infop info;
	png_structp png;

	png_byte *row = (png_byte *)malloc(4*PNG_WIDTH*sizeof(png_byte));

	int *num = (int *)malloc(sizeof(int) * 8);

	char *png_filename = (char *)malloc(sizeof(char)*128);

	unsigned int frame_index;

	int percent;

	int pid = getpid();
	char dir[32];

	sprintf(dir,"%d_frames",pid);

	#ifndef __WIN32
	mkdir(dir,0700);
	#else
	mkdir(dir);
	#endif

	char c;
		
	change_color(B_GREEN);
	printf("\n");
	for(frame_index = 0; frame_index < img_quant; frame_index++){

		img_index(png_filename,frame_index,pid);
		png_output = fopen(png_filename,"wb");

		percent = (100*frame_index)/(img_quant-1);
		fprintf(stderr," :: Writing data to: %s | %d%%",png_filename,percent);

		for(int i=0;i<50+dig(percent)+dig(pid)+1;i++){
			fprintf(stderr,"\b");
		}

		png = png_create_write_struct(PNG_LIBPNG_VER_STRING,NULL,NULL,NULL);

		if(!png){
			return 1;
		}

		info = png_create_info_struct(png);

		if(!info){
			return 1;
		}

		png_init_io(png,png_output);

		png_set_IHDR(
			png,
			info,
			PNG_WIDTH,
			PNG_HEIGHT,
			8,
			PNG_COLOR_TYPE_GRAY,
			PNG_INTERLACE_NONE,
			PNG_COMPRESSION_TYPE_DEFAULT,
			PNG_FILTER_TYPE_DEFAULT
		);

		png_write_info(png,info);

		for (int y = 0; y < PNG_HEIGHT / PIXEL_SIZE; ++y){
			for (int x = 0; x < (PNG_WIDTH / PIXEL_SIZE); ++x){

				if(buf_ind < (sz)){

					c = fgetc(input);
					get_bin(c, num);

					for(int i=0;i<8;i++){

						if(num[i] ){
							set_pixel(row, x+(i), 0);
						}
						else{
							set_pixel(row, x+(i), 255);
						}

					}
					x+=7;
					
					++buf_ind;
				}
				
				else{
					for(int i=0;i<8;i++){
						set_pixel(row, x, 127);
					}
				}
			}

			for (int i = 0; i < PIXEL_SIZE; ++i) {
				png_write_row(png, row);
			}
			
		}

		png_write_end(png,NULL);
		
		png_destroy_write_struct(&png, &info);
		png_free_data(png,info,PNG_FREE_ALL,-1);
		
		fclose(png_output);
		
		free(info);
	}
	
	fclose(input);
	free(num);
	free(png_filename);

	#ifndef __WIN32
	pid_t pidC = fork();
	char *ffm_input = (char *)malloc(sizeof(char) * 128);
	sprintf(ffm_input,"%d_frames/frame_%%06d.png",pid);
	
	char *output_vid_name = (char *)malloc(sizeof(char) * 512);
	sprintf(output_vid_name,"%s.mp4",basename(filename));
	
	int status;
	
	if(pidC > 0){
		printf("\n :: Encoding video with ffmpeg...\n");
	}

	else if(pidC == 0){
		//ffmpeg -framerate 30 -i frame_%06d.png -c:v libx265 -crf 0 -vf
		//fps=30 out.mp4
		execlp("/usr/bin/ffmpeg","ffmpeg",
			"-framerate","30",
			"-i",ffm_input,
			"-c:v","libx265","-crf","0",
			"-vf","fps=30",
			output_vid_name,
			"-hide_banner",
			"-loglevel","error","-stats",
			NULL
		);
	}

	else{
		change_color(RED);
		printf(" !! Error creating child process for ffmpeg");
		return 1;
	}
	
	pidC = wait(&status);

	free(ffm_input);
	free(output_vid_name);

#else /*TODO!!!!*/
/*
	I hate using the system() function, but I couldn't find
	an alternative for fork() function for MinGW. I'll work
	on it.
*/
	FILE *ffmpeg_exe_path = fopen("ffmpeg.txt","r");
	if(ffmpeg_exe_path == NULL){
		change_color(RED);
		perror("!! fmpeg.txt");
		return 1;
	}
	
	fseek(ffmpeg_exe_path,0L,SEEK_END);
	int fen_sz = ftell(ffmpeg_exe_path);
	fseek(ffmpeg_exe_path,0L,SEEK_SET);
	
	if(fen_sz > 256){
		change_color(RED);
		fprintf(stderr, " !! fmpeg.txt is very large. Please check it.\n");
		fclose(ffmpeg_exe_path);		
		change_color(WHITE);
		return 1;
	}
	rewind(ffmpeg_exe_path);
	fflush(ffmpeg_exe_path);
	
	char *ffm_bin = (char *)malloc(sizeof(char) * fen_sz);
	fread(ffm_bin,sizeof(char),fen_sz,ffmpeg_exe_path);
	ffm_bin[fen_sz] = '\0';

	fclose(ffmpeg_exe_path);
	
	char *ffm_input = (char *)malloc(sizeof(char) * 128);
	sprintf(ffm_input,"%d_frames/frame_%%06d.png",pid);
	
	char *output_vid_name = (char *)malloc(sizeof(char) * 512);
	sprintf(output_vid_name,"%s.mp4",basename(filename));

	char *ffm_args = (char *)malloc(sizeof(char) * 256);	
	sprintf(ffm_args,"\"%s\" -framerate 30 -i %s \
	-c:v libx264 -crf 0 -vf fps=30 %s -hide_banner -loglevel error \
	-stats",ffm_bin,ffm_input,output_vid_name);
		
	system(ffm_args);

	free(ffm_args);
	free(ffm_input);
	free(output_vid_name);
#endif
	change_color(B_WHITE);
	printf("\n :: All done!\n");
	change_color(WHITE);

	return 0;
}

int compat(const void *a, const void *b){
	return strcmp(*(const char **)a,*(const char **)b);
}

int decode(char *input_filename, char *output_filename){
	int pid = getpid();

	int status;

	char *dir_ffm = (char *)malloc(sizeof(char) * 128);
	sprintf(dir_ffm,"%d_frames/frame_%%06d.png",pid);

	char *input_dir = (char *)malloc(sizeof(char) * 128);
	sprintf(input_dir,"%d_frames",pid);
	#ifndef __WIN32
	mkdir(input_dir,0700);
	#else
	mkdir(input_dir);
	#endif

	#ifndef __WIN32
	pid_t pidC;
	
	pidC = fork();

	if(pidC > 0){
		printf("Extracting frames of %s\n",input_filename);
	}
	else if(pidC == 0){
		
		execlp("/usr/bin/ffmpeg","ffmpeg",
		"-i",input_filename,dir_ffm,
		"-hide_banner","-loglevel","error","-stats",NULL
		);
	
	}
	else{
		printf("Error creating child process.\n");
		return 1;
	}

	pidC = wait(&status);
	free(dir_ffm);

#else /*TODO!!!!*/
/*
	I hate using the system() function, but I couldn't find
	an alternative for fork() function for MinGW. I'll work
	on it.
*/
	change_color(B_GREEN);
	printf(" :: Extracting frames of %s\n",input_filename);

	FILE *ffmpeg_exe_path = fopen("ffmpeg.txt","r");
	if(ffmpeg_exe_path == NULL){
		change_color(4);
		perror("!! fmpeg.txt");
		change_color(7);
		
		return 1;
	}
	
	fseek(ffmpeg_exe_path,0L,SEEK_END);
	int fen_sz = ftell(ffmpeg_exe_path);
	fseek(ffmpeg_exe_path,0L,SEEK_SET);
	
	if(fen_sz > 256){
		change_color(RED);
		fprintf(stderr, " !! fmpeg.txt is very large. Please check it.\n");
		fclose(ffmpeg_exe_path);		
		change_color(WHITE);
		return 1;
	}
	rewind(ffmpeg_exe_path);
	fflush(ffmpeg_exe_path);
	
	char *ffm_bin = (char *)malloc(sizeof(char) * fen_sz);
	fread(ffm_bin,sizeof(char),fen_sz,ffmpeg_exe_path);
	ffm_bin[fen_sz] = '\0';

	fclose(ffmpeg_exe_path);
	
	char *ffm_args = (char *)malloc(sizeof(char) * 256);	
	sprintf(ffm_args,"\"%s\" -i %s %s -hide_banner -loglevel error -stats",
	ffm_bin,input_filename,dir_ffm);
	
	system(ffm_args);	
	
	#endif

	FILE * out = fopen(output_filename,"wb");
	if(!out){
		change_color(RED);
		fprintf(stderr," !! Error creating file: %s\n",output_filename);
		change_color(WHITE);
		return 1;
	}
	
	DIR *dir = opendir(input_dir);
	if(dir == NULL){
		change_color(RED);
		fprintf(stderr," !! Error opening directory: %s\n",input_dir);
		change_color(WHITE);
		return 1;
	}
	char framename[7]; /*frame_*/

	char **filenames;
	/*
	 frame_000000176.png
	*/
	
	filenames = (char **)malloc(sizeof(char *));
	filenames[0] = (char *)malloc(sizeof(char)*64);
	
	struct dirent *input;
	int frame_count = 0;

	FILE *frame;

	while((input = readdir(dir)) != NULL){
		for(int i=0;i<5;i++){
			framename[i] = input->d_name[i];
		}

		framename[5] = '\0';
		
		if(strcmp(framename, "frame") == 0){
			
			sprintf(filenames[frame_count],"%s/%s",input_dir,input->d_name);
			
			frame_count++;

			filenames = realloc(filenames,(frame_count + 1) * sizeof(char *));
			filenames[frame_count] = (char *)malloc(sizeof(char) * 64);
		}
	}
		
	if(!frame_count){
		change_color(RED);
		fprintf(stderr," !! No frame files found.\n");
		change_color(WHITE);
		return 1;
	}
	
	qsort(filenames,frame_count,sizeof(char *),compat);

	int aprox_size = ((PNG_WIDTH*PNG_HEIGHT)/(PIXEL_SIZE*PIXEL_SIZE));
	aprox_size = aprox_size*((frame_count)/WORD_SIZE);
	
	change_color(B_WHITE);
	printf(" * Frames: %d\n",frame_count);
	printf(" * approximate size: %d bytes\n",aprox_size);
	change_color(B_RED);
	printf(" > Press any key to continue...\n");

	#ifndef __WIN32
	getchar();
	#else
	getch();
	#endif
	change_color(B_GREEN);

	png_structp png;
	png_infop info;
	
	int percent;

	for(int i=0;i<frame_count;i++){
		percent = (100*i)/(frame_count-1);
		fprintf(stderr," :: Decoding %s  | %d%%",filenames[i],percent);

		for(int i=0;i<51+dig(percent)+strlen(input_dir)+1;i++)
			fprintf(stderr,"\b");


		frame = fopen(filenames[i],"rb");
		if(!frame){
			change_color(RED);
			fprintf(stderr,"\n !! Error opening file: %s",filenames[i]);
			change_color(WHITE);
			return 1;
		}
		png = png_create_read_struct(PNG_LIBPNG_VER_STRING,NULL,NULL,NULL);

		if(!png){
			fclose(frame);
			change_color(RED);
			fprintf(stderr," !! Error creating PNG read struct\n");
			change_color(WHITE);
			return 1;
		}

		info = png_create_info_struct(png);

		if(!info){
			png_destroy_read_struct(&png,NULL,NULL);
			fclose(frame);
			change_color(RED);
			fprintf(stderr," !! Error creating PNG info struct\n");
			change_color(WHITE);
			return 1;
		}

		png_init_io(png,frame);
		png_read_info(png,info);


//		width = png_get_image_width(png,info);
		int height = png_get_image_height(png,info);

		png_byte color_type = png_get_color_type(png, info);
		png_byte bit_depth = png_get_bit_depth(png, info);


		if (color_type == PNG_COLOR_TYPE_PALETTE){
			png_set_expand_gray_1_2_4_to_8(png);
		}

		if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8){
			png_set_expand_gray_1_2_4_to_8(png);
		}
		
		if (png_get_valid(png, info, PNG_INFO_tRNS)){
			png_set_tRNS_to_alpha(png);
		}

		if (color_type == PNG_COLOR_TYPE_RGB ||
			color_type == PNG_COLOR_TYPE_GRAY ||
			color_type == PNG_COLOR_TYPE_PALETTE){
			png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
		}

		if (color_type == PNG_COLOR_TYPE_GRAY ||
			color_type == PNG_COLOR_TYPE_GRAY_ALPHA){
			png_set_gray_to_rgb(png);
		}

		png_read_update_info(png, info);

		png_bytep *row_pointers;
		row_pointers = (png_bytep *)malloc(sizeof(png_bytep) * height);

		for(int i=0;i<PNG_HEIGHT;i++){
			row_pointers[i] = (png_byte *)malloc(png_get_rowbytes(png, info));
		}

		png_read_image(png, row_pointers);

		int k = 0;
		int res;

		int *num = (int *)malloc(sizeof(int) * 8);

		char lett;

		for (int y = 1; y <= PNG_HEIGHT; y+=PIXEL_SIZE){
			for (int x = 1; x <= PNG_WIDTH; x+=PIXEL_SIZE){

				res = get_pixel(row_pointers[y],x,y);

				if(res == 127){
					break;
				}

				num[k] = res;
				k++;

				if(k>7){
					lett = set_bin(num);
//					print_num(num);
//					fprintf(out,"%c",lett);
					fwrite(&lett,sizeof(char),1,out);
					k=0;
				}
			}
		}
		png_destroy_read_struct(&png, &info,NULL);
		png_free_data(png,info,PNG_FREE_ALL,-1);

		fclose(frame);

		for(int i=0;i<PNG_HEIGHT;i++){
			free(row_pointers[i]);
		}
		free(row_pointers);
	}

	for(int i=0;i<frame_count;i++){
		if(-1 == remove(filenames[i])){
			perror("remove:");
		}
	}
	rmdir(input_dir);
	
	change_color(B_WHITE);
	printf("\n :: All done!\n");
	change_color(WHITE);

	fclose(out);
	return 0;
}

void clear(void){
	#ifndef __WIN32
	printf("\033[H\033[J"); // XD
	#else
    HANDLE hcon = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD coord = {0,0};
    DWORD count;

    CONSOLE_SCREEN_BUFFER_INFO cs;
    GetConsoleScreenBufferInfo(hcon,&cs);

    FillConsoleOutputCharacter(hcon,' ', cs.dwSize.X*cs.dwSize.Y,coord,&count);
    SetConsoleCursorPosition(hcon, coord);
	#endif
}

void print_logo(void){
	change_color(B_GREEN);
	printf("\n ________  ________  _________  ________  ________  ________   ________      \n");
	printf("|\\   ___ \\|\\   __  \\|\\___   ___\\\\   __  \\|\\   __  \\|\\   ___  \\|\\   ____\\     \n");
	printf("\\ \\  \\_|\\ \\ \\  \\|\\  \\|___ \\  \\_\\ \\  \\|\\  \\ \\  \\|\\  \\ \\  \\\\ \\  \\ \\  \\___|_    \n");
	printf(" \\ \\  \\ \\\\ \\ \\   __  \\   \\ \\  \\ \\ \\   _  _\\ \\   __  \\ \\  \\\\ \\  \\ \\_____  \\   \n");
	printf("  \\ \\  \\_\\\\ \\ \\  \\ \\  \\   \\ \\  \\ \\ \\  \\\\  \\\\ \\  \\ \\  \\ \\  \\\\ \\  \\|____|\\  \\  \n");
	printf("   \\ \\_______\\ \\__\\ \\__\\   \\ \\__\\ \\ \\__\\\\ _\\\\ \\__\\ \\__\\ \\__\\\\ \\__\\____\\_\\  \\ \n");
	printf("    \\|_______|\\|__|\\|__|    \\|__|  \\|__|\\|__|\\|__|\\|__|\\|__| \\|__|\\_________\\\n");
	printf("                                                                 \\|_________|\n\n");
	change_color(CYAN);
	printf("   --> By neopusser83 | get it at ");
	change_color(B_WHITE);
	printf("https://github.com/neopusser83/datrans");
	change_color(CYAN);
	printf(" <--   \n");
	change_color(B_WHITE);
	printf("\n");

}


int main(int argc, char *argv[]){
	
	clear();
	print_logo();
	int opt;

	if(argc == 1){
		fprintf(stderr," Usage:\n\n\t%s -e [file to encode]\n  or\n\t%s ",
		basename(argv[0]),basename(argv[0]));
		fprintf(stderr,"-d [video to decode] [output file]\n");		
		
		change_color(WHITE);
		fprintf(stderr,"\n > Press any key to exit ");
		#ifdef __WIN32
		getch();
		#else
		getchar();		
		#endif
		
		return 1;		
	}

	if(!strcmp(argv[1],"-e")){
		if(argc != 3){
			fprintf(stderr," Usage:\n\n\t%s -e [file to encode]\n  or\n\t%s ",
			basename(argv[0]),basename(argv[0]));
			fprintf(stderr,"-d [video to decode] [output file]\n");		
			
			change_color(WHITE);
			fprintf(stderr,"\n > Press any key to exit ");
			#ifdef __WIN32
			getch();
			#else
			getchar();		
			#endif
			
			return 1;
		}
		opt = 1;
	}

	else if(!strcmp(argv[1],"-d")){
		if(argc != 4){
			fprintf(stderr," Usage:\n\n\t%s -e [file to encode]\n  or\n\t%s ",
			basename(argv[0]),basename(argv[0]));
			fprintf(stderr,"-d [video to decode] [output file]\n");		
		
			change_color(WHITE);
			fprintf(stderr,"\n > Press any key to exit ");
			#ifdef __WIN32
			getch();
			#else
			getchar();		
			#endif
	
			return 1;
		}
		opt = 0;
	}

	else{
		fprintf(stderr," Usage:\n\n\t%s -e [file to encode]\n  or\n\t%s ",
		basename(argv[0]),basename(argv[0]));
		fprintf(stderr,"-d [video to decode] [output file]\n");		
		
		change_color(WHITE);
		fprintf(stderr,"\n > Press any key to exit ");
		#ifdef __WIN32
		getch();
		#else
		getchar();		
		#endif
					
		return 1;	
	}

	int ret;
	
	if(opt){
		ret = encode(argv[2]);
		return ret;
	}
	else{
		ret = decode(argv[2],argv[3]);
		return ret;
	}
}
