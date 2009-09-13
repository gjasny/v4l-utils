/*
 *  Driver for the Conexant CX25821 PCIe bridge
 *
 *  Copyright (C) 2009 Conexant Systems Inc. 
 *  Authors  <hiep.huynh@conexant.com>, <shu.lin@conexant.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <linux/errno.h>
#include <linux/ioctl.h>

#define VID_UPSTREAM_SRAM_CHANNEL_I     9
#define VID_UPSTREAM_SRAM_CHANNEL_J     10
#define AUDIO_UPSTREAM_SRAM_CHANNEL_B   11
#define PIXEL_FRMT_422                  4
#define PIXEL_FRMT_411                  5
#define UPSTREAM_START_VIDEO            700
#define UPSTREAM_STOP_VIDEO             701
#define UPSTREAM_START_AUDIO            702
#define UPSTREAM_STOP_AUDIO             703
#define UPSTREAM_DUMP_REGISTERS         702


typedef struct {
    char *input_filename;
    char *vid_stdname;
    int pixel_format;
    int channel_select;
    int command;
} upstream_user_struct;

void print_video_usage()
{
    printf("\n************************************************************************ \n");
    printf("Video Sample Usage: ./upstream_app d 1 v /root/filename.yuv NTSC 422 1 start \n");
    printf("Argument 0: ./upstream_app \n");
    printf("Argument 1: Device ID (1 and above)\n");
    printf("Argument 2: v for video\n");
    printf("Argument 3: input file name \n");
    printf("Argument 4: Video Standard \n");
    printf("Argument 5: Pixel Format (Y422 or Y411) \n");
    printf("Argument 6: Upstream Channel Number\n");
    printf("Argument 7: start/stop command\n\n");
}
    
void print_audio_usage()
{    
    printf("\n************************************************************************ \n");
    printf("Audio Sample Usage: ./upstream_app d 1 a /root/audio.wav start\n");
    printf("Argument 0: ./upstream_app \n");
    printf("Argument 1: Device ID (1 and above)\n");
    printf("Argument 2: a for audio \n");
    printf("Argument 3: input file name \n");
    printf("Argument 4: start/stop command\n\n");
}

int main(int argc, char** argv)
{
    int fp;
    unsigned int cmd= 0 ;
    int i = 1;
    int mode = 0;
    int pixel_format = 422;
    int channel_select = 1;
    int device_id = 0, video_id = 11;
    char * temp2;
    char *video_device_str[4][2] = {{"/dev/video8", "/dev/video9"}, {"/dev/video20", "/dev/video21"},
                                    {"/dev/video32", "/dev/video33"}, {"/dev/video44", "/dev/video45"} };
    char *audio_device_str[4] = {"/dev/video10", "/dev/video22", "/dev/video34", "/dev/video46"};
    char mode_temp = 'v';

    if(argc < 2 || (tolower(*(argv[1])) != 'd') )
    { 
        print_video_usage();
        print_audio_usage();
        return -EINVAL;
    }
    else
    {
        sscanf(argv[2], "%d", &device_id ); 
        i += 2;
        
        if( device_id <= 0 || device_id > 4 )
        { 
            print_video_usage();
            print_audio_usage();
            return -EINVAL;
        }
    
        temp2 = argv[i];
        mode_temp = tolower(temp2[0]);
        switch(mode_temp)
        {
            case 'v':
                if( argc < 9 )
                {
                    print_video_usage();
                    return -EINVAL;
                }
                break;
                
            case 'a':
                if( argc < 6 )
                {
                    print_audio_usage();
                    return -EINVAL;
                }            
                break;
        }
    }
 
    if( mode_temp == 'v' || mode_temp == 'a' )
    {
        FILE* file_ptr = fopen(argv[4], "r");

        if( !file_ptr )
        {
            printf("\nERROR: %s file does NOT exist!!! \n\n", argv[4]);
            return -EINVAL;
        }
            
        fclose(file_ptr);
    }
    else
    {
        print_video_usage();
        print_audio_usage();
        return -EINVAL;
    }
        
    printf("\n*************************************************************** \n");

    switch(mode_temp)
    {
        case 'v':
            {
                char * temp = argv[5];
                upstream_user_struct arguments;
                arguments.input_filename = argv[4];
                arguments.vid_stdname    = (tolower(temp[0]) == 'p') ? "PAL" : "NTSC";
                sscanf(argv[6], "%d", &pixel_format);
                sscanf(argv[7], "%d", &channel_select);
                arguments.pixel_format   = (pixel_format == 422) ? PIXEL_FRMT_422 : PIXEL_FRMT_411;
                arguments.channel_select = (channel_select==1) ? VID_UPSTREAM_SRAM_CHANNEL_I : VID_UPSTREAM_SRAM_CHANNEL_J;                
                temp = argv[8];
                arguments.command        = (strcasecmp("STOP", temp) == 0) ? UPSTREAM_STOP_VIDEO : UPSTREAM_START_VIDEO;
                                
                
                if( channel_select >= 1 && channel_select <= 2 )
                {
                    if((fp = open(video_device_str[device_id-1][channel_select-1], O_RDWR)) == -1)  
                    { 
                        printf("Error: cannot open device file %s !\n", video_device_str[device_id-1][channel_select-1]);
                        return -EINVAL;
                    }
    
                    printf("Device %s open for IOCTL successfully!\n", video_device_str[device_id-1][channel_select-1]);                
                    printf("UPSTREAM parameters: filename = %s, channel_select(I=1) = %d \n", arguments.input_filename, channel_select);
                   
                   
                    if((ioctl(fp, arguments.command, (char *) &arguments)) == -1)
                    {
                        printf("Error: ioctl FAILED!\n");
                    }
                }
            }
            break;

        case 'a':
            {
                char * temp = argv[5];
                upstream_user_struct arguments;
                arguments.input_filename = argv[4];
                arguments.vid_stdname    = "NTSC";
                arguments.pixel_format   = PIXEL_FRMT_422;
                arguments.channel_select = AUDIO_UPSTREAM_SRAM_CHANNEL_B;
                arguments.command    = (strcasecmp("STOP", temp) == 0) ? UPSTREAM_STOP_AUDIO : UPSTREAM_START_AUDIO;
                
                
                if((fp = open(audio_device_str[device_id-1], O_RDWR)) == -1)                
                { 
                    printf("Error: cannot open device file %s !\n", audio_device_str[device_id-1]);
                    return -EINVAL;
                }
    
                printf("Device %s open for IOCTL successfully!\n", audio_device_str[device_id-1]);
                printf("UPSTREAM parameters: filename = %s, audio channel = %d \n", arguments.input_filename, arguments.channel_select);
                 
                 
                if((ioctl(fp, arguments.command, (char *) &arguments)) == -1)
                    printf("Error: ioctl FAILED!\n");                
            }
            break;
            
        default:
            printf("ERROR: INVALID ioctl command!\n");
            return -EINVAL;
    }
    
    printf("*************************************************************** \n\n");
    close(fp);
    return 0;
}
