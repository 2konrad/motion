/*
<<<<<<< HEAD
 *    This file is part of Motion.
 *
 *    Motion is free software: you can redistribute it and/or modify
=======
 *    This file is part of MotionPlus.
 *
 *    MotionPlus is free software: you can redistribute it and/or modify
>>>>>>> plus/upstream__May25
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
<<<<<<< HEAD
 *    Motion is distributed in the hope that it will be useful,
=======
 *    MotionPlus is distributed in the hope that it will be useful,
>>>>>>> plus/upstream__May25
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
<<<<<<< HEAD
 *    along with Motion.  If not, see <https://www.gnu.org/licenses/>.
 *
 */
#include "motion.hpp"
=======
 *    along with MotionPlus.  If not, see <https://www.gnu.org/licenses/>.
 *
 */
#include "motionplus.hpp"
>>>>>>> plus/upstream__May25
#include "util.hpp"
#include "camera.hpp"
#include "conf.hpp"
#include "logger.hpp"
#include "video_loopback.hpp"

#if (defined(HAVE_V4L2)) && (!defined(BSD))

#include <dirent.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

typedef struct capent {const char *cap; unsigned int code;} capentT;
    capentT cap_list[] ={
        {"V4L2_CAP_VIDEO_CAPTURE"        ,0x00000001 },
        {"V4L2_CAP_VIDEO_CAPTURE_MPLANE" ,0x00001000 },
        {"V4L2_CAP_VIDEO_OUTPUT"         ,0x00000002 },
        {"V4L2_CAP_VIDEO_OUTPUT_MPLANE"  ,0x00002000 },
        {"V4L2_CAP_VIDEO_M2M"            ,0x00004000 },
        {"V4L2_CAP_VIDEO_M2M_MPLANE"     ,0x00008000 },
        {"V4L2_CAP_VIDEO_OVERLAY"        ,0x00000004 },
        {"V4L2_CAP_VBI_CAPTURE"          ,0x00000010 },
        {"V4L2_CAP_VBI_OUTPUT"           ,0x00000020 },
        {"V4L2_CAP_SLICED_VBI_CAPTURE"   ,0x00000040 },
        {"V4L2_CAP_SLICED_VBI_OUTPUT"    ,0x00000080 },
        {"V4L2_CAP_RDS_CAPTURE"          ,0x00000100 },
        {"V4L2_CAP_VIDEO_OUTPUT_OVERLAY" ,0x00000200 },
        {"V4L2_CAP_HW_FREQ_SEEK"         ,0x00000400 },
        {"V4L2_CAP_RDS_OUTPUT"           ,0x00000800 },
        {"V4L2_CAP_TUNER"                ,0x00010000 },
        {"V4L2_CAP_AUDIO"                ,0x00020000 },
        {"V4L2_CAP_RADIO"                ,0x00040000 },
        {"V4L2_CAP_MODULATOR"            ,0x00080000 },
        {"V4L2_CAP_SDR_CAPTURE"          ,0x00100000 },
        {"V4L2_CAP_EXT_PIX_FORMAT"       ,0x00200000 },
        {"V4L2_CAP_SDR_OUTPUT"           ,0x00400000 },
        {"V4L2_CAP_READWRITE"            ,0x01000000 },
        {"V4L2_CAP_ASYNCIO"              ,0x02000000 },
        {"V4L2_CAP_STREAMING"            ,0x04000000 },
        {"V4L2_CAP_DEVICE_CAPS"          ,0x80000000 },
        {"Last",0}
};

static int vlp_open_vidpipe(void)
{

    int pipe_fd = -1;
    char pipepath[255];
    char buffer[255];
    DIR *dir;
    struct dirent *dirp;
    const char prefix[] = "/sys/class/video4linux/";
    int fd,tfd;
    int len,min;
    int retcd;

    if ((dir = opendir(prefix)) == NULL) {
<<<<<<< HEAD
        MOTION_LOG(CRT, TYPE_VIDEO, SHOW_ERRNO,_("Failed to open '%s'"), prefix);
=======
        MOTPLS_LOG(CRT, TYPE_VIDEO, SHOW_ERRNO,_("Failed to open '%s'"), prefix);
>>>>>>> plus/upstream__May25
        return -1;
    }

    while ((dirp = readdir(dir)) != NULL) {
        if (!strncmp(dirp->d_name, "video", 5)) {

            retcd = snprintf(buffer, sizeof(buffer),"%s%s/name", prefix, dirp->d_name);
            if ((retcd<0) || (retcd >= (int)sizeof(buffer))) {
<<<<<<< HEAD
                MOTION_LOG(NTC, TYPE_VIDEO, SHOW_ERRNO
                    ,_("Error specifying buffer: %s"),buffer);
                continue;
            } else {
                MOTION_LOG(NTC, TYPE_VIDEO, SHOW_ERRNO,_("Opening buffer: %s"),buffer);
=======
                MOTPLS_LOG(NTC, TYPE_VIDEO, SHOW_ERRNO
                    ,_("Error specifying buffer: %s"),buffer);
                continue;
            } else {
                MOTPLS_LOG(NTC, TYPE_VIDEO, SHOW_ERRNO,_("Opening buffer: %s"),buffer);
>>>>>>> plus/upstream__May25
            }

            if ((fd = open(buffer, O_RDONLY|O_CLOEXEC)) >= 0) {
                if ((len = (int)read(fd, buffer, sizeof(buffer)-1)) < 0) {
                    close(fd);
                    continue;
                }
                buffer[len]=0;
<<<<<<< HEAD
                MOTION_LOG(NTC, TYPE_VIDEO, SHOW_ERRNO,_("Read buffer: %s"),buffer);
=======
                MOTPLS_LOG(NTC, TYPE_VIDEO, SHOW_ERRNO,_("Read buffer: %s"),buffer);
>>>>>>> plus/upstream__May25
                if (strncmp(buffer, "Loopback video device",21)) { /* weird stuff after minor */
                    close(fd);
                    continue;
                }
                min = atoi(&buffer[21]);

                retcd = snprintf(buffer,sizeof(buffer),"/dev/%s",dirp->d_name);
                if ((retcd < 0) || (retcd >= (int)sizeof(buffer))) {
<<<<<<< HEAD
                    MOTION_LOG(NTC, TYPE_VIDEO, SHOW_ERRNO
=======
                    MOTPLS_LOG(NTC, TYPE_VIDEO, SHOW_ERRNO
>>>>>>> plus/upstream__May25
                        ,_("Error specifying buffer: %s"),buffer);
                    close(fd);
                    continue;
                } else {
<<<<<<< HEAD
                    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("found video device '%s' %d"), buffer,min);
=======
                    MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("found video device '%s' %d"), buffer,min);
>>>>>>> plus/upstream__May25
                }

                if ((tfd = open(buffer, O_RDWR|O_CLOEXEC)) >= 0) {
                    strncpy(pipepath, buffer, sizeof(pipepath));
                    if (pipe_fd >= 0) {
                        close(pipe_fd);
                    }
                    pipe_fd = tfd;
                    break;
                }
            }
                close(fd);
        }
    }

    closedir(dir);

    if (pipe_fd >= 0) {
<<<<<<< HEAD
      MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("Opened %s as pipe output"), pipepath);
=======
      MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("Opened %s as pipe output"), pipepath);
>>>>>>> plus/upstream__May25
    }

    return pipe_fd;
}

static void vlp_show_vcap(struct v4l2_capability *cap)
{
    unsigned int vers = cap->version;
    unsigned int c    = cap->capabilities;
    int i;

<<<<<<< HEAD
    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, "Pipe Device");
    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, "cap.driver:   %s",cap->driver);
    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, "cap.card:     %s",cap->card);
    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, "cap.bus_info: %s",cap->bus_info);
    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, "cap.card:     %u.%u.%u",(vers >> 16) & 0xFF,(vers >> 8) & 0xFF,vers & 0xFF);
    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, "Device capabilities");
    for (i=0;cap_list[i].code;i++) {
        if (c & cap_list[i].code) {
            MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, "%s",cap_list[i].cap);
        }
    }
    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, "------------------------");
=======
    MOTPLS_LOG(INF, TYPE_VIDEO, NO_ERRNO, "Pipe Device");
    MOTPLS_LOG(INF, TYPE_VIDEO, NO_ERRNO, "cap.driver:   %s",cap->driver);
    MOTPLS_LOG(INF, TYPE_VIDEO, NO_ERRNO, "cap.card:     %s",cap->card);
    MOTPLS_LOG(INF, TYPE_VIDEO, NO_ERRNO, "cap.bus_info: %s",cap->bus_info);
    MOTPLS_LOG(INF, TYPE_VIDEO, NO_ERRNO, "cap.card:     %u.%u.%u",(vers >> 16) & 0xFF,(vers >> 8) & 0xFF,vers & 0xFF);
    MOTPLS_LOG(INF, TYPE_VIDEO, NO_ERRNO, "Device capabilities");
    for (i=0;cap_list[i].code;i++) {
        if (c & cap_list[i].code) {
            MOTPLS_LOG(INF, TYPE_VIDEO, NO_ERRNO, "%s",cap_list[i].cap);
        }
    }
    MOTPLS_LOG(INF, TYPE_VIDEO, NO_ERRNO, "------------------------");
>>>>>>> plus/upstream__May25
}

static void vlp_show_vfmt(struct v4l2_format *v)
{
<<<<<<< HEAD
    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, "type: type:           %d",v->type);
    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, "fmt.pix.width:        %d",v->fmt.pix.width);
    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, "fmt.pix.height:       %d",v->fmt.pix.height);
    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, "fmt.pix.pixelformat:  %d",v->fmt.pix.pixelformat);
    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, "fmt.pix.sizeimage:    %d",v->fmt.pix.sizeimage);
    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, "fmt.pix.field:        %d",v->fmt.pix.field);
    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, "fmt.pix.bytesperline: %d",v->fmt.pix.bytesperline);
    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, "fmt.pix.colorspace:   %d",v->fmt.pix.colorspace);
    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, "------------------------");
=======
    MOTPLS_LOG(INF, TYPE_VIDEO, NO_ERRNO, "type: type:           %d",v->type);
    MOTPLS_LOG(INF, TYPE_VIDEO, NO_ERRNO, "fmt.pix.width:        %d",v->fmt.pix.width);
    MOTPLS_LOG(INF, TYPE_VIDEO, NO_ERRNO, "fmt.pix.height:       %d",v->fmt.pix.height);
    MOTPLS_LOG(INF, TYPE_VIDEO, NO_ERRNO, "fmt.pix.pixelformat:  %d",v->fmt.pix.pixelformat);
    MOTPLS_LOG(INF, TYPE_VIDEO, NO_ERRNO, "fmt.pix.sizeimage:    %d",v->fmt.pix.sizeimage);
    MOTPLS_LOG(INF, TYPE_VIDEO, NO_ERRNO, "fmt.pix.field:        %d",v->fmt.pix.field);
    MOTPLS_LOG(INF, TYPE_VIDEO, NO_ERRNO, "fmt.pix.bytesperline: %d",v->fmt.pix.bytesperline);
    MOTPLS_LOG(INF, TYPE_VIDEO, NO_ERRNO, "fmt.pix.colorspace:   %d",v->fmt.pix.colorspace);
    MOTPLS_LOG(INF, TYPE_VIDEO, NO_ERRNO, "------------------------");
>>>>>>> plus/upstream__May25
}

int vlp_startpipe(const char *dev_name, int width, int height)
{
    int dev;
    struct v4l2_format v;
    struct v4l2_capability vc;

    if (mystreq(dev_name, "-")) {
        dev = vlp_open_vidpipe();
    } else {
        dev = open(dev_name, O_RDWR|O_CLOEXEC);
<<<<<<< HEAD
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("Opened %s as pipe output"), dev_name);
    }

    if (dev < 0) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO,_("Opening %s as pipe output failed"), dev_name);
=======
        MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("Opened %s as pipe output"), dev_name);
    }

    if (dev < 0) {
        MOTPLS_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO,_("Opening %s as pipe output failed"), dev_name);
>>>>>>> plus/upstream__May25
        return -1;
    }


    if (ioctl(dev, VIDIOC_QUERYCAP, &vc) == -1) {
<<<<<<< HEAD
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "ioctl (VIDIOC_QUERYCAP)");
=======
        MOTPLS_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "ioctl (VIDIOC_QUERYCAP)");
>>>>>>> plus/upstream__May25
        return -1;
    }

    vlp_show_vcap(&vc);

    memset(&v, 0, sizeof(v));

    v.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

    if (ioctl(dev, VIDIOC_G_FMT, &v) == -1) {
<<<<<<< HEAD
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "ioctl (VIDIOC_G_FMT)");
        return -1;
    }
    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO,_("Original pipe specifications"));
=======
        MOTPLS_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "ioctl (VIDIOC_G_FMT)");
        return -1;
    }
    MOTPLS_LOG(INF, TYPE_VIDEO, NO_ERRNO,_("Original pipe specifications"));
>>>>>>> plus/upstream__May25
    vlp_show_vfmt(&v);

    v.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    v.fmt.pix.width = (uint)width;
    v.fmt.pix.height = (uint)height;
    v.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
    v.fmt.pix.sizeimage =(uint)(3 * width * height / 2);
    v.fmt.pix.bytesperline = (uint)width;
    v.fmt.pix.field = V4L2_FIELD_NONE;
    v.fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;
<<<<<<< HEAD
    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO,_("Proposed pipe specifications"));
    vlp_show_vfmt(&v);

    if (ioctl(dev,VIDIOC_S_FMT, &v) == -1) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "ioctl (VIDIOC_S_FMT)");
        return -1;
    }

    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO,_("Final pipe specifications"));
=======
    MOTPLS_LOG(INF, TYPE_VIDEO, NO_ERRNO,_("Proposed pipe specifications"));
    vlp_show_vfmt(&v);

    if (ioctl(dev,VIDIOC_S_FMT, &v) == -1) {
        MOTPLS_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "ioctl (VIDIOC_S_FMT)");
        return -1;
    }

    MOTPLS_LOG(INF, TYPE_VIDEO, NO_ERRNO,_("Final pipe specifications"));
>>>>>>> plus/upstream__May25
    vlp_show_vfmt(&v);

    return dev;
}

#endif /* HAVE_V4L2 && !BSD */

void vlp_putpipe(cls_camera *cam)
{
    #if (defined(HAVE_V4L2)) && (!defined(BSD))
        ssize_t retcd;

        if (cam->pipe >= 0) {
            retcd = write(cam->pipe
                , cam->current_image->image_norm
                , (uint)cam->imgs.size_norm);
            if (retcd < 0) {
<<<<<<< HEAD
                MOTION_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO
=======
                MOTPLS_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO
>>>>>>> plus/upstream__May25
                    ,_("Failed to put image into video pipe"));
            }
        }
        if (cam->mpipe >= 0) {
            retcd = write(cam->mpipe
                , cam->imgs.image_motion.image_norm
                , (uint)cam->imgs.size_norm);
            if (retcd < 0) {
<<<<<<< HEAD
                MOTION_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO
=======
                MOTPLS_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO
>>>>>>> plus/upstream__May25
                    ,_("Failed to put image into motion video pipe"));
            }
        }
    #else
        (void)cam;
    #endif
}

void vlp_init(cls_camera *cam)
{
    #if defined(HAVE_V4L2) && !defined(BSD)
        /* open video loopback devices if enabled */
        if (cam->cfg->video_pipe != "") {
<<<<<<< HEAD
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
=======
            MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
>>>>>>> plus/upstream__May25
                ,_("Opening video loopback device for normal pictures"));

            /* vid_startpipe should get the output dimensions */
            cam->pipe = vlp_startpipe(cam->cfg->video_pipe.c_str(), cam->imgs.width, cam->imgs.height);

            if (cam->pipe < 0) {
<<<<<<< HEAD
                MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
=======
                MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
>>>>>>> plus/upstream__May25
                    ,_("Failed to open video loopback for normal pictures"));
                return;
            }
        } else {
            cam->pipe = -1;
        }

        if (cam->cfg->video_pipe_motion != "") {
<<<<<<< HEAD
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
=======
            MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
>>>>>>> plus/upstream__May25
                ,_("Opening video loopback device for motion pictures"));

            /* vid_startpipe should get the output dimensions */
            cam->mpipe = vlp_startpipe(cam->cfg->video_pipe_motion.c_str(), cam->imgs.width, cam->imgs.height);

            if (cam->mpipe < 0) {
<<<<<<< HEAD
                MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
=======
                MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
>>>>>>> plus/upstream__May25
                    ,_("Failed to open video loopback for motion pictures"));
                return;
            }
        } else {
            cam->mpipe = -1;
        }
    #else
        cam->mpipe = -1;
        cam->pipe = -1;
    #endif /* HAVE_V4L2 && !BSD */
}

