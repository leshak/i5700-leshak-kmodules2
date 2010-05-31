/* rotator/s3c_rotator_common.h
 *
 * Copyright (c) 2008 Samsung Electronics
 *
 * Samsung S3C Rotator driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
 
#ifndef _S3C_ROTATOR_COMMON_H_
#define _S3C_ROTATOR_COMMON_H_

#define ROTATOR_IOCTL_MAGIC 'R'

#define ROTATOR_SFR_SIZE        0x1000
#define ROTATOR_MINOR  		    230

#define ROTATOR_90                      _IO(ROTATOR_IOCTL_MAGIC, 0)
#define ROTATOR_180                     _IO(ROTATOR_IOCTL_MAGIC, 1)
#define ROTATOR_270                     _IO(ROTATOR_IOCTL_MAGIC, 2)
#define HFLIP                           _IO(ROTATOR_IOCTL_MAGIC, 3)
#define VFLIP                           _IO(ROTATOR_IOCTL_MAGIC, 4)

#define ROTATOR_TIMEOUT	100	// normally 800 * 480 * 2 rotation takes about 20ms

typedef struct{
	unsigned int src_width;			    // Source Image Full Width
	unsigned int src_height;			// Source Image Full Height
	unsigned int src_addr_rgb_y; 		// Base Address of the Source Image (RGB or Y) : Physical Address
	unsigned int src_addr_cb; 			// Base Address of the Source Image (CB Component) : Physical Address
	unsigned int src_addr_cr; 			// Base Address of the Source Image (CR Component) : Physical Address	
	unsigned int src_format; 			// Color Space of the Source Image YUV420(non-interleave)/YUV422(interleave)/RGB565/RGB888

	unsigned int dst_addr_rgb_y; 		// Base Address of the Destination Image (RGB or Y) : Physical Address
	unsigned int dst_addr_cb; 			// Base Address of the Destination Image (CB Component) : Physical Address
	unsigned int dst_addr_cr; 			// Base Address of the Destination Image (CR Component) : Physical Address		
}ro_params;

#endif // _S3C_ROTATOR_COMMON_H_
