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


#define REG_READ		    900
#define REG_WRITE		    901

typedef struct{
    char *vid_stdname;
    int pixel_format;
    int cif_resolution_enable;
    int cif_width;
    int decoder_select;
    int command;
    int reg_address;
    int reg_data;
}downstream_user_struct;


void print_usage()
{    
    printf("\n*********************************\n");
    printf("Sample Usage: ./reg d 1 r 0x01\n");
    printf("Sample Usage: ./reg d 1 w 0x0 0x2\n");
    printf("    d device_id   Device ID (1 and above) for MultiCard\n");
    printf("    r Read Athena register\n");
    printf("    w Write Athena register\n");
}

int main(int argc, char** argv)
{
    unsigned int cmd= 0 ;
    int fp;
    int i = 1, j = 0, k = 0, len = 0;
    int mode = 0;

    int register_addr = 0;
    int write_value = 0;

    int device_id = 0, video_id = 11;
    char * temp2;
    char *device_str[4] = {"/dev/video11", "/dev/video23", "/dev/video35", "/dev/video47"};

    char mode_temp = 's';
    char *param_temp;


    if(argc < 5 || (tolower(*(argv[1])) != 'd') )
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
            case 'r':
                {
                    downstream_user_struct arguments;

                    sscanf(param_temp, "%x", &register_addr );

                    arguments.reg_address  = register_addr;
                    arguments.command      = REG_READ;
                    
                     printf("Read parameters: read register = 0x%x, command = %d \n", arguments.reg_address, arguments.command);                  
                     
                    if((ioctl(fp, arguments.command, (char *) &arguments)) == -1)
                        printf("Error: ioctl FAILED!\n");  
                    printf("Reg 0x%x = 0x%x\n", arguments.reg_address, arguments.reg_data); 
                }
                break;
                                            
            case 'w':
                {
                    downstream_user_struct arguments;

                    sscanf(param_temp, "%x", &register_addr );
                    sscanf(argv[i+2], "%x", &write_value );
                    i++;
                    
                    arguments.command           = REG_WRITE;
                    arguments.reg_address       = register_addr;
                    arguments.reg_data          = write_value;
 
                    printf("Write parameters: write register = 0x%x, write_value = 0x%x, command = %d \n", arguments.reg_address, arguments.reg_data, arguments.command);

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

