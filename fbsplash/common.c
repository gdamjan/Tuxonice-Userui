/*
 * common.c - miscellaneous functions used by both the kernel helper and
 *            user utilities.
 * 
 * TuxOnIce userui adaptations:
 *   Copyright (C) 2005 Bernard Blackham <bernard@blackham.com.au>
 *
 * Based on the original splashutils code:
 * Copyright (C) 2004-2005, Michal Januszewski <spock@gentoo.org>
 * 
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/vt.h>
#include <linux/kd.h>

#include "../userui.h"
#include "splash.h"
#include "../userui.h"

struct fb_var_screeninfo   fb_var;
struct fb_fix_screeninfo   fb_fix;

enum ENDIANESS endianess;
char *config_file = NULL;

enum TASK arg_task = none; 
int arg_fb = 0;
int arg_vc = 0;
char arg_mode = 'v';
char *arg_theme = NULL;
u16 arg_progress = 0;
char *progress_text = NULL;
u8 arg_kdmode = KD_TEXT;

#ifndef TARGET_KERNEL
char *arg_export = NULL;
#endif

int bytespp = 4;		/* bytes per pixel */
u8 fb_opt = 0;			/* can we use optimized 24/32bpp routines? */
u8 fb_ro, fb_go, fb_bo; 	/* red, green, blue offset */
u8 fb_rlen, fb_glen, fb_blen;	/* red, green, blue length */

struct fb_image pic;
char *pic_file = NULL;

void detect_endianess(void)
{
	u16 t = 0x1122;

	if (*(u8*)&t == 0x22) {
		endianess = little;
	} else {
		endianess = big;
	}

	DEBUG("This system is %s-endian.\n", (endianess == little) ? "little" : "big");
}

int get_fb_settings(int fb_num)
{
	char fn[32];
	int fb;
#ifdef TARGET_KERNEL
	char sys[128];
#endif

	sprintf(fn, PATH_DEV "/fb/%d", fb_num);	
	fb = open(fn, O_WRONLY, 0);

	if (fb == -1) {
		sprintf(fn, PATH_DEV "/fb%d", fb_num);
		fb = open(fn, O_WRONLY, 0);
	}
	
	if (fb == -1) {
#ifdef TARGET_KERNEL
		sprintf(sys, PATH_SYS "/class/graphics/fb%d/dev", fb_num);
		create_dev(fn, sys, 0x1);
		fb = open(fn, O_WRONLY, 0);
		if (fb == -1)
			remove_dev(fn, 0x1);
		if (fb == -1)
#endif
		{
			printf("Failed to open " PATH_DEV "/fb%d or " PATH_DEV 
				 "/fb%d for reading.\n", fb_num, fb_num);
			return 1;
		}
	}
		
	if (ioctl(fb,FBIOGET_VSCREENINFO,&fb_var) == -1) {
		printk("Failed to get fb_var info.\n");
		return 2;
	}

	if (ioctl(fb,FBIOGET_FSCREENINFO,&fb_fix) == -1) {
		printk("Failed to get fb_fix info.\n");
		return 3;
	}

	close(fb);

#ifdef TARGET_KERNEL
	remove_dev(fn, 0x1);
#endif
	bytespp = (fb_var.bits_per_pixel + 7) >> 3;

	/* Check if optimized code can be used. We use special optimizations for
	 * 24/32bpp modes in which all color components have a length of 8 bits. */
	if (bytespp < 3 || fb_var.blue.length != 8 || fb_var.green.length != 8 || fb_var.red.length != 8) {
		fb_opt = 0;

		if (fb_fix.visual == FB_VISUAL_DIRECTCOLOR) {
			fb_blen = fb_glen = fb_rlen = min(min(fb_var.red.length,fb_var.green.length),fb_var.blue.length);
		} else {
			fb_rlen = fb_var.red.length;
			fb_glen = fb_var.green.length;
			fb_blen = fb_var.blue.length;
		}
	} else {
		fb_opt = 1;

		/* Compute component offsets (ie. indexes in an array of u8's) */
		fb_ro = (fb_var.red.offset >> 3);
		fb_go = (fb_var.green.offset >> 3);
		fb_bo = (fb_var.blue.offset >> 3);
			
		if (endianess == big) {
			fb_ro = bytespp - 1 - fb_ro;
			fb_go = bytespp - 1 - fb_go;
			fb_bo = bytespp - 1 - fb_bo;
		}
	}

	return 0;
}

char *get_filepath(char *path) 
{
	char buf[512];
	
	if (path[0] == '/')
		return strdup(path);

	snprintf(buf, 512, "%s/%s/%s", THEME_DIR, arg_theme, path);
	return strdup(buf);	
}

char *get_cfg_file(char *theme) 
{
	char buf[512];

	snprintf(buf, 512, "%s/%s/%dx%d.cfg", THEME_DIR, theme, fb_var.xres, fb_var.yres);
	return strdup(buf);	
}

int do_getpic(unsigned char origin, unsigned char do_cmds, char mode)
{	
	if (load_images(mode))
		return -1;

#if (defined(CONFIG_TTF) && !defined(TARGET_KERNEL)) || (defined(CONFIG_TTF_KERNEL) && defined(TARGET_KERNEL))
	load_fonts();
#endif
	
#ifdef CONFIG_FBSPLASH
	if (do_cmds) {
		cmd_setpic(&verbose_img, origin);
		free((u8*)verbose_img.data);
		if (verbose_img.cmap.red)
			free(verbose_img.cmap.red);
	}	
#endif
	return 0;
}

#ifdef CONFIG_FBSPLASH
int do_config(unsigned char origin)
{
	if (!config_file) {
		printk("No config file.\n");
		return -1;
	}
		
	/* If the user specified invalid values for the text field - correct it.
	 * Also setup default values (text field coverting the whole screen). */
	if (cf.tx > fb_var.xres)
		cf.tx = 0;

	if (cf.ty > fb_var.yres)
		cf.ty = 0;

	if (cf.tw > fb_var.xres || cf.tw == 0)
		cf.tw = fb_var.xres;

	if (cf.th > fb_var.yres || cf.th == 0)
		cf.th = fb_var.yres;

	cmd_setcfg(origin);
	return 0;
}
#endif

void vt_cursor_disable(int fd)
{
	if (write(fd, "\e[?25l\e[?1c",11)) do {} while(0);
}

void vt_cursor_enable(int fd)
{
	if (write(fd, "\e[?25h\e[?0c",11)) do {} while(0);
}

int open_fb()
{
	char dev[32];
	int c;
	
	sprintf(dev, PATH_DEV "/fb%d", arg_fb);
	if ((c = open(dev, O_RDWR)) == -1) {
		sprintf(dev, PATH_DEV "/fb/%d", arg_fb);
		if ((c = open(dev, O_RDWR)) == -1) {
			printk("Failed to open " PATH_DEV "/fb%d or " 
			         PATH_DEV "/fb/%d.\n", arg_fb, arg_fb);
			return -1;
		}
	}

	return c;
}

int open_tty(int tty)
{
	char dev[32];
	int c;

	sprintf(dev, PATH_DEV "/tty%d", tty);
	if ((c = open(dev, O_RDWR)) == -1) {
		sprintf(dev, PATH_DEV "/vc/%d", tty);
		if ((c = open(dev, O_RDWR)) == -1) {
			printk("Failed to open " PATH_DEV "/tty%d or " 
				 PATH_DEV "/vc/%d\n", tty, tty);
			return 0;
		}
	}

	return c;
}

int tty_set_silent(int tty, int fd)
{
	struct termios w;
	
	tcgetattr(fd, &w);
	w.c_lflag &= ~(ICANON|ECHO);
	w.c_cc[VTIME] = 0;
	w.c_cc[VMIN] = 1;
	tcsetattr(fd, TCSANOW, &w);
	vt_cursor_disable(fd);
	ioctl(fd, VT_ACTIVATE, tty);
	ioctl(fd, VT_WAITACTIVE, tty);

	return 0;
}

int tty_unset_silent(int fd)
{
	struct termios w;
	
	tcgetattr(fd, &w);
	w.c_lflag &= (ICANON|ECHO);
	w.c_cc[VTIME] = 0;
	w.c_cc[VMIN] = 1;
	tcsetattr(fd, TCSANOW, &w);
	vt_cursor_enable(fd);

	return 0;
}
