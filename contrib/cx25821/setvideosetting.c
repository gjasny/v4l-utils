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


#define SET_VIDEO_STD               800
#define SET_PIXEL_FORMAT            1000
#define ENABLE_CIF_RESOLUTION       1001


#define PIXEL_FRMT_422    4
#define PIXEL_FRMT_411    5
#define PIXEL_FRMT_Y8     6
#define ALL_DECODERS      752

typedef struct {
    char *vid_stdname;
    int pixel_format;
    int cif_resolution_enable;
    int cif_width;
    int decoder_select;
    int command;
} downstream_user_struct;


void print_usage()
{    
    printf("\n*********************************\n");
    printf("Sample Usage: ./set d 1 standard NTSC format 422 7 resolution 320 7\n");
    printf("    device_id   Device ID (1 and above) for MultiCard\n");
    printf("    standard    Video Standard (PAL/NTSC)\n");
    printf("    format      Output Pixel Format (422 or 411) for specific decoder (e.g. 7)\n");
    printf("    resolution  352/320 for CIF and 720 for D1 resolution for specific decoder (e.g. 7)\n");
}

int main(int argc, char** argv)
{
    unsigned int cmd= 0 ;
    int fp;
    int i = 1, j = 0, k = 0, len = 0;
    int mode = 0;
    int pixel_format = 0;
    int width_input = 0;
    int decoder_input = 0;
    int device_id = 0, video_id = 11;
    char * temp2;
    char *device_str[4] = {"/dev/video11", "/dev/video23", "/dev/video35", "/dev/video47"};

    char mode_temp = 's';
    char *param_temp;


    if(argc < 3 || (tolower(*(argv[1])) != 'd') )
    { 
        print_usage();
        return -EINVAL;
    }
    
    sscanf(argv[2], "%d", &device_id ); 
    i += 2;
    
    if( device_id <= 0 || device_id > 4 )
    { 
        print_usage();
        return -EINVAL;
    }
    
    printf("\n********************************* \n");  
          
    if((fp = open(device_str[device_id-1], O_RDWR)) == -1)
    { 
        printf("Error: cannot open device file %s !\n", device_str[device_id-1]);
        return -EINVAL;
    }
             
    printf("Device %s open for IOCTL successfully!\n", device_str[device_id-1]);
    
    
    for( ; i < argc; i++) 
    {
        temp2 = argv[i];
        mode_temp = tolower(temp2[0]);        
        param_temp = argv[i+1];
        

        switch(mode_temp)
        {
            case 's':
                {
                    downstream_user_struct arguments;
                    char * temp = param_temp;
                    arguments.vid_stdname   = (tolower(temp[0]) == 'p') ? "PAL" : "NTSC";
                    arguments.command       = SET_VIDEO_STD;
                    
                    printf("User parameters: vid_stdname = %s, command = %d \n", arguments.vid_stdname, arguments.command);                     
                     
                    if((ioctl(fp, arguments.command, (char *) &arguments)) == -1)
                        printf("Error: ioctl FAILED!\n");   
                }
                break;
                                            
            case 'f':
                {
                    sscanf(param_temp, "%d", &pixel_format );
                    sscanf(argv[i+2], "%d", &decoder_input ); i++;
        
                    if( pixel_format < 411 || pixel_format > 422)
                        pixel_format = 422;
                        
                    if( decoder_input < 0 )
                        decoder_input = ALL_DECODERS;
                    
                    downstream_user_struct arguments;
                    arguments.pixel_format      = (pixel_format == 411) ? PIXEL_FRMT_411 : PIXEL_FRMT_422;
                    arguments.decoder_select    = decoder_input;
                    arguments.command           = SET_PIXEL_FORMAT;
                    
                    printf("User parameters: pixel_format = %d, decoder_input = %d, command = %d \n", arguments.pixel_format, arguments.decoder_select, arguments.command);
                     
                    if((ioctl(fp, arguments.command, (char *) &arguments)) == -1)
                        printf("Error: ioctl FAILED!\n"); 
                }
                break;
                
            case 'r':
                {
                    sscanf(param_temp, "%d", &width_input );
                    sscanf(argv[i+2], "%d", &decoder_input ); i++;
                    
                    if( width_input < 320 || width_input > 352 )
                        width_input = 720;
                        
                    if( decoder_input < 0 )
                        decoder_input = ALL_DECODERS;
                        
                    downstream_user_struct arguments;                    
                    arguments.cif_resolution_enable = (width_input == 320 || width_input == 352) ? 1 : 0;
                    arguments.cif_width             = width_input;
                    arguments.decoder_select        = decoder_input;
                    arguments.command               = ENABLE_CIF_RESOLUTION;
                    
                    printf("User parameters: cif_resolution_enable = %d, decoder_input = %d, command = %d \n", arguments.cif_resolution_enable, arguments.decoder_select, arguments.command);
                     
                     
                    if((ioctl(fp, arguments.command, (char *) &arguments)) == -1)
                        printf("Error: ioctl FAILED!\n");   
                }
                break;
                                
            default:
                printf("Please verify the options are correct!\n");
                break;
        }
        
        i++;
    }

    
    printf("********************************* \n\n");
    close(fp);
    return 0;
}
