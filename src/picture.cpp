/*
 *    This file is part of Motion.
 *
 *    Motion is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    Motion is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Motion.  If not, see <https://www.gnu.org/licenses/>.
 *
 */
#include "motion.hpp"
#include "util.hpp"
#include "camera.hpp"
#include "conf.hpp"
#include "logger.hpp"
#include "picture.hpp"
#include "jpegutils.hpp"
#include "draw.hpp"
#include "dbse.hpp"
#include "picture.hpp"


void cls_picture::picname(char* fullname, std::string fmtstr
    , std::string basename, std::string extname)
{
    char filename[PATH_MAX];
    int  retcd;

    mystrftime(cam, filename, sizeof(filename)
        , basename.c_str(), NULL);
    retcd = snprintf(fullname, PATH_MAX, fmtstr.c_str()
        , cam->cfg->target_dir.c_str(), filename, extname.c_str());
    if ((retcd < 0) || (retcd >= PATH_MAX)) {
        MOTION_LOG(ERR, TYPE_EVENTS, NO_ERRNO
            ,_("Error creating picture file name"));
        return;
    }
    full_nm = fullname;
    file_dir =full_nm.substr(0,full_nm.find_last_of("/"));
    file_nm = full_nm.substr(file_dir.length()+1);

}

void cls_picture::on_picture_save_command(char *fname)
{
    MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO, _("File saved to: %s"), fname);

    if (cam->cfg->on_picture_save != "") {
        util_exec_command(cam, cam->cfg->on_picture_save.c_str(), fname);
    }
}

void cls_picture::process_norm()
{
    char filename[PATH_MAX];

    if (cam->cfg->picture_output == "on") {
        picname(filename,"%s/%s.%s"
            , cam->cfg->picture_filename
            , cam->cfg->picture_type);
        if ((cam->imgs.size_high > 0) && (cam->movie_passthrough == false)) {
            save_norm(filename, cam->current_image->image_high, cam->imgs.width_high,  cam->imgs.height_high);
        } else {
            save_norm(filename,cam->current_image->image_norm, cam->imgs.width,  cam->imgs.height);
        }
        on_picture_save_command(filename);
        cam->app->dbse->exec(cam, filename, "pic_save");
        cam->app->dbse->filelist_add(cam, &cam->current_image->imgts
            ,"pic", file_nm, full_nm, file_dir);
    }
}

void cls_picture::process_motion()
{
    char filename[PATH_MAX];

    if (cam->cfg->picture_output_motion == "on") {
        picname(filename,"%s/%sm.%s", cam->cfg->picture_filename, cam->cfg->picture_type);
        save_norm(filename, cam->imgs.image_motion.image_norm, cam->imgs.width,  cam->imgs.height);
        on_picture_save_command(filename);
        cam->app->dbse->exec(cam, filename, "pic_save");
        cam->app->dbse->filelist_add(cam, &cam->imgs.image_motion.imgts
            ,"pic", file_nm, full_nm, file_dir);


    } else if (cam->cfg->picture_output_motion == "roi") {
        picname(filename,"%s/%sr.%s", cam->cfg->picture_filename, cam->cfg->picture_type);
        save_roi(filename, cam->current_image->image_norm);
        on_picture_save_command(filename);
        cam->app->dbse->exec(cam, filename, "pic_save");
        cam->app->dbse->filelist_add(cam, &cam->current_image->imgts
            ,"pic", file_nm, full_nm, file_dir);

    }
}

void cls_picture::process_snapshot()
{
    char filename[PATH_MAX];
    char linkpath[PATH_MAX];
    int offset;

    offset = (int)cam->cfg->snapshot_filename.length() - 8;
    if (offset < 0) {
        offset = 1;
    }

    if (cam->cfg->snapshot_filename.compare((uint)offset, 8, "lastsnap") != 0) {
        picname(filename,"%s/%s.%s"
            , cam->cfg->snapshot_filename
            , cam->cfg->picture_type);
        if ((cam->imgs.size_high > 0) && (cam->movie_passthrough == false)) {
            save_norm(filename, cam->current_image->image_high, cam->imgs.width_high,  cam->imgs.height_high);
        } else {
            save_norm(filename, cam->current_image->image_norm, cam->imgs.width,  cam->imgs.height);
        }
        on_picture_save_command(filename);
        cam->app->dbse->exec(cam, filename, "pic_save");
        cam->app->dbse->filelist_add(cam, &cam->current_image->imgts
            ,"pic", file_nm, full_nm, file_dir);

        /* Update symbolic link */
        picname(linkpath,"%s/%s.%s"
            , "lastsnap", cam->cfg->picture_type);
        remove(linkpath);
        if (symlink(filename, linkpath)) {
            MOTION_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO
                ,_("Could not create symbolic link [%s]"), filename);
            return;
        }

    } else {
        picname(filename,"%s/%s.%s"
            , cam->cfg->snapshot_filename
            , cam->cfg->picture_type);
        remove(filename);
        if ((cam->imgs.size_high > 0) && (cam->movie_passthrough == false)) {
            save_norm(filename, cam->current_image->image_high, cam->imgs.width_high,  cam->imgs.height_high);
        } else {
            save_norm(filename, cam->current_image->image_norm, cam->imgs.width,  cam->imgs.height);
        }
        on_picture_save_command(filename);
        cam->app->dbse->exec(cam, filename, "pic_save");
        cam->app->dbse->filelist_add(cam, &cam->current_image->imgts
            ,"pic", file_nm, full_nm, file_dir);
    }

    cam->action_snapshot = false;
}

void cls_picture::process_preview()
{
    char filename[PATH_MAX];
    ctx_image_data *saved_current_image;

    if (cam->imgs.image_preview.diffs) {
        saved_current_image = cam->current_image;
        saved_current_image->imgts= cam->current_image->imgts;

        cam->current_image = &cam->imgs.image_preview;
        cam->current_image->imgts = cam->imgs.image_preview.imgts;

        picname(filename,"%s/%s.%s"
            , cam->cfg->picture_filename
            , cam->cfg->picture_type);

        ctx_coord cropped_area = crop_preview_img();


        if ((cam->imgs.size_high > 0) && (cam->movie_passthrough == false)) {
            save_norm(filename, cam->imgs.image_preview.image_high, cropped_area.width , cropped_area.height);
        } else {
            save_norm(filename, cam->imgs.image_preview.image_norm, cropped_area.width , cropped_area.height);
        }
        on_picture_save_command(filename);
        cam->app->dbse->exec(cam, filename, "pic_save");
        cam->app->dbse->filelist_add(cam, &cam->imgs.image_preview.imgts
            ,"pic", file_nm, full_nm, file_dir);

        /* Restore global context values. */
        cam->current_image = saved_current_image;
        cam->current_image->imgts = saved_current_image->imgts;
    }
}

ctx_coord cls_picture::get_box_size (ctx_coord location, int max_width, int max_height, int min_boxsize ){
        //MOTION_LOG(ERR, TYPE_EVENTS, NO_ERRNO, "Processing previw from sec%d shot%d diff%d", timestamp_tm.tm_sec, sh, diff);
    // now crop it to location of motion detected
    // determine w/h of cubic box around motion, min 25% of full image height
    int boxsize;
    ctx_coord area;

    boxsize = MAX( MIN( MIN ( (MAX (location.height, location.height) ) , max_height ) , max_width ), min_boxsize);
    boxsize = MAX(boxsize ,160);
    boxsize = (boxsize /8 ) *8;
    //minimum size
    //find new x of box not lower that 0 and not higher than w-boxsize
    area.width = boxsize;
    area.height = boxsize;
    area.minx = MIN( MAX(location.x - boxsize/2, 0), max_width - boxsize - 1);
    area.minx = (area.minx /2) *2;
    area.maxx = area.minx + boxsize;
    area.miny = MIN( MAX(location.y - boxsize/2, 0), max_height - boxsize - 1);
    area.miny = (area.miny /2) *2;
    area.maxy = area.miny + boxsize;

    return area;
}

ctx_coord cls_picture::crop_preview_img()
{
    ctx_coord fullpic;
    ctx_coord motionpic;
    int  croppedsize;
    ctx_coord location = cam->imgs.image_preview.location;
    ctx_coord label1_location = cam->imgs.image_preview.largest_location;
    //timespec ts = cam->imgs.image_preview.imgts;

    struct tm timestamp_tm;
    localtime_r(&cam->imgs.image_preview.imgts.tv_sec , &timestamp_tm);
    //int minute = timestamp_tm.tm_min;


    //int sh = cam->imgs.image_preview.shot;
    //int diff = cam->imgs.image_preview.diffs;
    ctx_coord area ; // new cubic area min 25% of height 
    ctx_coord area1; // cubic area of label 1
    u_char *img , *img_u_cropped , *img_v_cropped , *img_u , *img_v;
    int imgsz;

    if ((cam->imgs.size_high > 0) && (cam->movie_passthrough == false)) {
        fullpic.width = cam->imgs.width_high;
        fullpic.height = cam->imgs.height_high;
        img = cam->imgs.image_preview.image_high;
        imgsz = cam->imgs.width_high * cam->imgs.height_high;
    } else {
        fullpic.width = cam->imgs.width;
        fullpic.height = cam->imgs.height;
        img = cam->imgs.image_preview.image_norm;
        imgsz = cam->imgs.width * cam->imgs.height;
    }
    motionpic.width = cam->imgs.width;
    motionpic.height = cam->imgs.height;

    // Process original location dirived by std dev

    //MOTION_LOG(ERR, TYPE_EVENTS, NO_ERRNO, "Processing previw from sec%d shot%d diff%d", timestamp_tm.tm_sec, sh, diff);
    // now crop it to location of motion detected
    // determine w/h of cubic box around motion, min 25% of full image height
    area = get_box_size(location, motionpic.width, motionpic.height, (int)(motionpic.height / 4));
    
    area1 = get_box_size(label1_location, motionpic.width, motionpic.height, (int)(motionpic.height / 4));
     
    
    MOTION_LOG(ERR, TYPE_EVENTS, NO_ERRNO, "Label x%d mx%d y%d my%d --- area1 x%d mx%d y%d my%d",
    area.minx, area.maxx, area.miny, area.maxy, area1.minx, area1.maxx, area1.miny, area1.maxy);
    
    // transform area from norm to high
    area.minx = area.minx * 2;
    area.maxx = area.maxx * 2;
    area.miny = area.miny * 2;
    area.maxy = area.maxy * 2;
    area.height = area.height *2;
    area.width = area.width *2;
    

    //MOTION_LOG(ERR, TYPE_EVENTS, NO_ERRNO, "Adjusted preview norm->high box from x%d mx%d y%d my%d to x%d mx%d y%d my%d",
    //location.minx, location.maxx, location.miny, location.maxy, area.minx, area.maxx, area.miny, area.maxy);

    // crop cam->imgs.image_preview.image_high and change w / h
    int indxh,line;
    int cropped_pixel_counter = 0;
    // move y
    for (indxh=area.miny , line=0; indxh< area.miny + area.height; indxh++,line++){
        memmove(img+(line * area.width)
            , img+(indxh*fullpic.width) + area.minx
            , (uint)area.width);
        cropped_pixel_counter += area.width;    
    }
    croppedsize = area.width*area.height;
    //MOTION_LOG(ERR, TYPE_EVENTS, NO_ERRNO, "copied imgsz planned%d moved%d", croppedsize, cropped_pixel_counter);
    img_u_cropped = img + croppedsize;
    img_u = img + imgsz ;
    //move u
    for (indxh=area.miny/2, line =0; indxh< area.miny/2 + area.height/2; indxh++,line++){
        memmove(img_u_cropped+(line * area.width/2)
            , img_u+(indxh*fullpic.width/2) + area.minx/ 2
            , (uint)area.width / 2);
            cropped_pixel_counter += (area.width /2);
    }
    //MOTION_LOG(ERR, TYPE_EVENTS, NO_ERRNO, "copied U planned %d moved%d", croppedsize + croppedsize/4,cropped_pixel_counter );
    //move v
    img_v_cropped = img + croppedsize + croppedsize / 4;
    img_v = img + imgsz + imgsz / 4;
    //croppedsize_v = croppedsize + croppedsize/4;
    for (indxh=area.miny/2, line =0; indxh< area.miny/2 + area.height/2; indxh++,line++){
        memmove(img_v_cropped+(line * area.width/2)
            , img_v+(indxh*fullpic.width/2) + area.minx/ 2
            , (uint)area.width / 2);
        cropped_pixel_counter += (area.width /2);
    }
    //MOTION_LOG(ERR, TYPE_EVENTS, NO_ERRNO, "copied v planned %d moved%d", croppedsize + croppedsize/2,cropped_pixel_counter );

    return area; //: area fullpic
}

#ifdef HAVE_WEBP
void cls_picture::webp_exif(WebPMux* webp_mux
        , timespec *ts1, ctx_coord *box)
{
    u_char *exif = NULL;
        uint exif_len = jpgutl_exif(&exif, cam, ts1, box);

    if(exif_len > 0) {
        WebPData webp_exif;
        /* EXIF in WEBP does not need the EXIF marker signature (6 bytes) that are needed by jpeg */
        webp_exif.bytes = exif + 6;
        webp_exif.size = exif_len - 6;

        WebPMuxError err = WebPMuxSetChunk(webp_mux, "EXIF", &webp_exif, 1);
        if (err != WEBP_MUX_OK) {
            MOTION_LOG(ERR, TYPE_CORE, NO_ERRNO
                , _("Unable to set set EXIF to webp chunk"));
        }
        free(exif);
    }
}
#endif /* HAVE_WEBP */

/** Save image as webp to file */
void cls_picture::save_webp(FILE *fp, u_char *image, int width, int height
        , timespec *ts1, ctx_coord *box)
{
    #ifdef HAVE_WEBP
        /* Create a config present and check for compatible library version */
        WebPConfig webp_config;
        if (!WebPConfigPreset(&webp_config, WEBP_PRESET_DEFAULT
            , (float) cam->cfg->picture_quality)) {
            MOTION_LOG(ERR, TYPE_CORE, NO_ERRNO, _("libwebp version error"));
            return;
        }

        /* Create the input data structure and check for compatible library version */
        WebPPicture webp_image;
        if (!WebPPictureInit(&webp_image)) {
            MOTION_LOG(ERR, TYPE_CORE, NO_ERRNO,_("libwebp version error"));
            return;
        }

        /* Allocate the image buffer based on image width and height */
        webp_image.width = width;
        webp_image.height = height;
        if (!WebPPictureAlloc(&webp_image)) {
            MOTION_LOG(ERR, TYPE_CORE, NO_ERRNO,_("libwebp image buffer allocation error"));
            return;
        }

        /* Map the input YUV420P buffer as individual Y, U and V pointers */
        webp_image.y = image;
        webp_image.u = image + width * height;
        webp_image.v = webp_image.u + (width * height) / 4;

        /* Setup the memory writting method */
        WebPMemoryWriter webp_writer;
        WebPMemoryWriterInit(&webp_writer);
        webp_image.writer = WebPMemoryWrite;
        webp_image.custom_ptr = (void*) &webp_writer;

        /* Encode the YUV image as webp */
        if (!WebPEncode(&webp_config, &webp_image)) {
            MOTION_LOG(WRN, TYPE_CORE, NO_ERRNO,_("libwebp image compression error"));
        }
        /* A bitstream object is needed for the muxing proces */
        WebPData webp_bitstream;
        webp_bitstream.bytes = webp_writer.mem;
        webp_bitstream.size = webp_writer.size;

        /* Create a mux from the prepared image data */
        WebPMux* webp_mux = WebPMuxCreate(&webp_bitstream, 1);
        webp_exif(webp_mux, ts1, box);

        /* Add Exif data to the webp image data */
        WebPData webp_output;
        WebPMuxError err = WebPMuxAssemble(webp_mux, &webp_output);
        if (err != WEBP_MUX_OK) {
            MOTION_LOG(ERR, TYPE_CORE, NO_ERRNO,_("unable to assemble webp image"));
        }

        /* Write the webp final bitstream to the file */
        if (fwrite(webp_output.bytes, sizeof(uint8_t), webp_output.size, fp) != webp_output.size) {
            MOTION_LOG(ERR, TYPE_CORE, NO_ERRNO,_("unable to save webp image to file"));
        }

        #if WEBP_ENCODER_ABI_VERSION > 0x0202
            /* writer.mem must be freed by calling WebPMemoryWriterClear */
            WebPMemoryWriterClear(&webp_writer);
        #else
            /* writer.mem must be freed by calling 'free(writer.mem)' */
            free(webp_writer.mem);
        #endif /* WEBP_ENCODER_ABI_VERSION */

        /* free the memory used by webp for image data */
        WebPPictureFree(&webp_image);
        /* free the memory used by webp mux object */
        WebPMuxDelete(webp_mux);
        /* free the memory used by webp for output data */
        WebPDataClear(&webp_output);
    #else
        (void)fp;
        (void)image;
        (void)width;
        (void)height;
        (void)ts1;
        (void)box;
    #endif /* HAVE_WEBP */
}

/** Save image as yuv420p jpeg to file */
void cls_picture::save_yuv420p(FILE *fp, u_char *image, int width, int height
        , timespec *ts1, ctx_coord *box)
{

    int sz, image_size;

    image_size = (width * height * 3)/2;
    u_char *buf =(u_char*) mymalloc((uint)image_size);

    sz = jpgutl_put_yuv420p(buf, image_size, image, width, height
        , cam->cfg->picture_quality, cam ,ts1, box);


    MOTION_LOG(ERR, TYPE_EVENTS, NO_ERRNO, "jpgutil x%d y%d ", width , height);
    
    fwrite(buf, (uint)sz, 1, fp);

    free(buf);

}

/** Save image as grey jpeg to file */
void cls_picture::save_grey(FILE *picture, u_char *image, int width, int height
        , timespec *ts1, ctx_coord *box)
{
    int sz, image_size;

    image_size = (width * height * 3)/2;

    u_char *buf =(u_char*) mymalloc((uint)image_size);

    sz = jpgutl_put_grey(buf, image_size, image, width, height
        , cam->cfg->picture_quality, cam ,ts1, box);
    fwrite(buf, (uint)sz, 1, picture);

    free(buf);
}

/** Save image as greyscale ppm image to file */
void cls_picture::save_ppm(FILE *picture, u_char *image, int width, int height)
{
    int x, y;
    u_char *l = image;
    u_char *u = image + width * height;
    u_char *v = u + (width * height) / 4;
    int r, g, b;
    u_char rgb[3];

    /*
     *  ppm header
     *  width height
     *  maxval
     */
    fprintf(picture, "P6\n");
    fprintf(picture, "%d %d\n", width, height);
    fprintf(picture, "%d\n", 255);
    for (y = 0; y < height; y++) {

        for (x = 0; x < width; x++) {
            r = 76283 * (((int)*l) - 16)+104595*(((int)*u) - 128);
            g = 76283 * (((int)*l) - 16)- 53281*(((int)*u) - 128) - 25625 * (((int)*v) - 128);
            b = 76283 * (((int)*l) - 16) + 132252 * (((int)*v) - 128);
            r = r >> 16;
            g = g >> 16;
            b = b >> 16;

            if (r < 0) {
                r = 0;
            } else if (r > 255) {
                r = 255;
            }

            if (g < 0) {
                g = 0;
            } else if (g > 255) {
                g = 255;
            }

            if (b < 0) {
                b = 0;
            } else if (b > 255) {
                b = 255;
            }

            rgb[0] = (u_char)b;
            rgb[1] = (u_char)g;
            rgb[2] = (u_char)r;

            l++;
            if (x%2 != 0) {
                u++;
                v++;
            }
            /* ppm is rgb not bgr */
            fwrite(rgb, 1, 3, picture);
        }
        if (y%2 == 0) {
            u -= width / 2;
            v -= width / 2;
        }
    }
}

/** Put picture into memory as jpg */
int cls_picture::put_memory(u_char *img_dst, int image_size
        , u_char *image, int quality, int width, int height)
{
    struct timespec ts1;
    int retcd;

    clock_gettime(CLOCK_REALTIME, &ts1);
    if (cam->cfg->stream_grey) {
        retcd = jpgutl_put_grey(img_dst, image_size, image
            , width, height, quality, cam, &ts1, NULL);
    } else {
        retcd = jpgutl_put_yuv420p(img_dst, image_size, image
            , width, height, quality, cam, &ts1, NULL);
    }

    return retcd;
}

/* Write the picture to a file */
void cls_picture::pic_write(FILE *picture, u_char *image, int width, int height)
{


    if (cam->cfg->picture_type == "ppm") {
        save_ppm(picture, image, width, height);
    } else if (cam->cfg->picture_type == "webp") {
        save_webp(picture, image, width, height
            , &(cam->current_image->imgts), &(cam->current_image->location));
    } else if (cam->cfg->picture_type == "grey") {
        save_grey(picture, image, width, height
            , &(cam->current_image->imgts), &(cam->current_image->location));
    } else {
        save_yuv420p(picture, image, width, height
            , &(cam->current_image->imgts), &(cam->current_image->location));
    }
}

/* Saves image to a file in format requested */
void cls_picture::save_norm(char *file, u_char *image, int width, int height)
{
    FILE *picture;

    picture = myfopen(file, "wbe");
    if (!picture) {
        MOTION_LOG(ERR, LOG_TYPE_ALL, SHOW_ERRNO
            ,_("Can't write picture to file %s"), file);
        return;
    }

    pic_write(picture, image, width, height);

    myfclose(picture);
}

/* Saves image to a file in format requested */
void cls_picture::save_roi(char *file, u_char *image)
{
    FILE *picture;
    int image_size, sz, indxh;
    ctx_coord *bx;
    u_char *buf, *img;

    bx = &cam->current_image->location;

    if ((bx->width <64) || (bx->height <64)) {
        return;
    }

    picture = myfopen(file, "wbe");
    if (!picture) {
        MOTION_LOG(ERR, LOG_TYPE_ALL, SHOW_ERRNO
            ,_("Can't write picture to file %s"), file);
        return;
    }

    image_size = bx->width * bx->height;

    buf =(u_char*) mymalloc((uint)image_size);
    img =(u_char*) mymalloc((uint)image_size);

    for (indxh=bx->miny; indxh< bx->miny + bx->height; indxh++){
        memcpy(img+((indxh - bx->miny)* bx->width)
            , image+(indxh*cam->imgs.width) + bx->minx
            , (uint)bx->width);
    }

    sz = jpgutl_put_grey(buf, image_size, img
        , bx->width, bx->height
        , cam->cfg->picture_quality, cam
        ,&(cam->current_image->imgts), bx);

    fwrite(buf, (uint)sz, 1, picture);

    free(buf);
    free(img);

    myfclose(picture);
}

/** Get the pgm file used as fixed mask */
u_char *cls_picture::load_pgm(FILE *picture, int width, int height)
{
    int x, y, mask_width, mask_height, maxval;
    char line[256];
    u_char *image, *resized_image;

    line[255] = 0;

    if (!fgets(line, 255, picture)) {
        MOTION_LOG(ERR, LOG_TYPE_ALL, SHOW_ERRNO,_("Could not read from pgm file"));
        return NULL;
    }

    if (strncmp(line, "P5", 2)) {
        MOTION_LOG(ERR, LOG_TYPE_ALL, SHOW_ERRNO
            ,_("This is not a pgm file, starts with '%s'"), line);
        return NULL;
    }

    /* Skip comment */
    line[0] = '#';
    while (line[0] == '#')
        if (!fgets(line, 255, picture)) {
            return NULL;
        }

    /* Read image size */
    if (sscanf(line, "%d %d", &mask_width, &mask_height) != 2) {
        MOTION_LOG(ERR, LOG_TYPE_ALL, SHOW_ERRNO
            ,_("Failed reading size in pgm file"));
        return NULL;
    }

    /* Maximum value */
    line[0] = '#';
    while (line[0] == '#')
        if (!fgets(line, 255, picture)) {
            return NULL;
        }

    if (sscanf(line, "%d", &maxval) != 1) {
        MOTION_LOG(ERR, LOG_TYPE_ALL, SHOW_ERRNO
            ,_("Failed reading maximum value in pgm file"));
        return NULL;
    }

    /* Read data */
    /* We allocate the size for a 420P since we will use
    ** this image for masking privacy which needs the space for
    ** the cr / cb components
    */
    image =(u_char*) mymalloc((uint)((mask_width * mask_height * 3) / 2));

    for (y = 0; y < mask_height; y++) {
        if ((int)fread(&image[y * mask_width], 1, (uint)mask_width, picture) != mask_width) {
            MOTION_LOG(ERR, LOG_TYPE_ALL, SHOW_ERRNO, "Failed reading image data from pgm file");
        }

        for (x = 0; x < mask_width; x++) {
            image[y * mask_width + x] = (u_char)((int)image[y * mask_width + x] * 255 / maxval);
        }

    }

    /* Resize mask if required */
    if (mask_width != width || mask_height != height) {
        MOTION_LOG(WRN, LOG_TYPE_ALL, NO_ERRNO
            ,_("The mask file specified is not the same size as image from camera."));
        MOTION_LOG(WRN, LOG_TYPE_ALL, NO_ERRNO
            ,_("Attempting to resize mask image from %dx%d to %dx%d")
            ,mask_width, mask_height, width, height);

        resized_image =(u_char*) mymalloc((uint)((width * height * 3) / 2));

        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x++) {
                resized_image[y * width + x] = image[
                        (mask_height - 1) * y / (height - 1) * mask_width +
                        (mask_width  - 1) * x / (width  - 1)];
            }
        }

        free(image);
        image = resized_image;
    }

    return image;
}

void cls_picture::write_mask(const char *file)
{
    FILE *picture;

    picture = myfopen(file, "wbe");
    if (!picture) {
        /* Report to syslog - suggest solution if the problem is access rights to target dir. */
        if (errno ==  EACCES) {
            MOTION_LOG(ERR, LOG_TYPE_ALL, SHOW_ERRNO
                ,_("can't write mask file %s - check access rights to target directory")
                ,file);
        } else {
            /* If target dir is temporarily unavailable we may survive. */
            MOTION_LOG(ERR, LOG_TYPE_ALL, SHOW_ERRNO
                ,_("can't write mask file %s"), file);
        }
        return;
    }
    memset(cam->imgs.image_motion.image_norm, 255, (uint)cam->imgs.motionsize); /* Initialize to unset */

    /* Write pgm-header. */
    fprintf(picture, "P5\n");
    fprintf(picture, "%d %d\n", cam->cfg->width, cam->cfg->height);
    fprintf(picture, "%d\n", 255);

    /* Write pgm image data at once. */
    if ((int)fwrite(cam->imgs.image_motion.image_norm, (uint)cam->cfg->width, (uint)cam->cfg->height, picture) != cam->cfg->height) {
        MOTION_LOG(ERR, LOG_TYPE_ALL, SHOW_ERRNO
            ,_("Failed writing default mask as pgm file"));
        return;
    }

    myfclose(picture);

    MOTION_LOG(ERR, LOG_TYPE_ALL, NO_ERRNO
        ,_("Creating empty mask %s\nPlease edit this file and "
        "re-run motion to enable mask feature"), cam->cfg->mask_file.c_str());
}

void cls_picture::scale_img(int width_src, int height_src, u_char *img_src, u_char *img_dst)
{

    int i = 0, x, y;
    for (y = 0; y < height_src; y+=2)
        for (x = 0; x < width_src; x+=2)
            img_dst[i++] = img_src[y * width_src + x];

    for (y = 0; y < height_src / 2; y+=2)
       for (x = 0; x < width_src; x += 4)
       {
          img_dst[i++] = img_src[(width_src * height_src) + (y * width_src) + x];       //This is probably wrong, not YUV420 logic
          img_dst[i++] = img_src[(width_src * height_src) + (y * width_src) + (x + 1)]; // dito
       }

    return;
}

//scale down YUV420 image by 50% generating a YUV444-packed
void cls_picture::scale_img_YUV444p(int width_src, int height_src, u_char *img_src, u_char *img_dst)
{

    int i = 0, x, y, Y, U, V;
    int offset_U = width_src * height_src;
    int offset_V = offset_U + (width_src * height_src)/4;
    for (y = 0; y < height_src; y+=2)
        for (x = 0; x < width_src; x+=2)
        {
            img_dst[i++] = (u_char) ((img_src[y * width_src + x     ] +
                                     img_src[y * width_src + x+1    ] + 
                                     img_src[(y+1) * width_src + x  ] +
                                     img_src[(y+1) * width_src + x+1] ) / 4);
             
            img_dst[i++] = img_src[offset_U + (y/2 * width_src/2) + x/2];
            img_dst[i++] = img_src[offset_V + (y/2 * width_src/2) + x/2];

//             Y = (u_char) ((img_src[y * width_src + x     ] +
//                                      img_src[y * width_src + x+1    ] + 
//                                      img_src[(y+1) * width_src + x  ] +
//                                      img_src[(y+1) * width_src + x+1] ) / 4);
//              
//             U = img_src[offset_U + (y/2 * width_src/2) + x/2];
//             V = img_src[offset_V + (y/2 * width_src/2) + x/2];
//             // transform to RGB
//             //R = Y + 1.140V, G = Y - 0.395U - 0.581V, and B = Y + 2.032U. 
// 
//             Y -= 16;
//             U -= 128;
//             V -= 128;
//             img_dst[i++] = (u_char) MAX(MIN(1.164 * Y             + 1.596 * V , 255) , 0);
//             img_dst[i++] = (u_char) MAX(MIN(1.164 * Y - 0.392 * U - 0.813 * V , 255) , 0);
//             img_dst[i++] = (u_char) MAX(MIN(1.164 * Y + 2.017 * U             , 255) , 0);
            
        }
    return;
}

void cls_picture::save_preview()
{
    u_char *image_norm, *image_high;

    /* Save our pointers to our memory locations for images*/
    image_norm = cam->imgs.image_preview.image_norm;
    image_high = cam->imgs.image_preview.image_high;

    /* Copy over the meta data from the img into preview */
    memcpy(&cam->imgs.image_preview, cam->current_image, sizeof(ctx_image_data));

    /* Restore the pointers to the memory locations for images*/
    cam->imgs.image_preview.image_norm = image_norm;
    cam->imgs.image_preview.image_high = image_high;

    /* Copy the actual images for norm and high */
    memcpy(cam->imgs.image_preview.image_norm
        , cam->current_image->image_norm, (uint)cam->imgs.size_norm);
    if (cam->imgs.size_high > 0) {
        memcpy(cam->imgs.image_preview.image_high
            , cam->current_image->image_high, (uint)cam->imgs.size_high);
    }

    /*
     * If we set output_all to yes and during the event
     * there is no image with motion, diffs is 0, we are not going to save the preview event
     */
    if (cam->imgs.image_preview.diffs == 0) {
        cam->imgs.image_preview.diffs = 1;
    }

    cam->draw->locate();

}

void cls_picture::init_privacy()
{
    int indxrow, indxcol;
    int start_cr, offset_cb, start_cb;
    int y_index, uv_index;
    int indx_img, indx_max;         /* Counter and max for norm/high */
    int indx_width, indx_height;
    u_char *img_temp, *img_temp_uv;


    FILE *picture;

    /* Load the privacy file if any */
    cam->imgs.mask_privacy = NULL;
    cam->imgs.mask_privacy_uv = NULL;
    cam->imgs.mask_privacy_high = NULL;
    cam->imgs.mask_privacy_high_uv = NULL;

    if (cam->cfg->mask_privacy != "") {
        if ((picture = myfopen(cam->cfg->mask_privacy.c_str(), "rbe"))) {
            MOTION_LOG(INF, LOG_TYPE_ALL, NO_ERRNO, _("Opening privacy mask file"));
            /*
             * NOTE: The mask is expected to have the output dimensions. I.e., the mask
             * applies to the already rotated image, not the capture image. Thus, use
             * width and height from imgs.
             */
            cam->imgs.mask_privacy = load_pgm(picture, cam->imgs.width, cam->imgs.height);

            /* We only need the "or" mask for the U & V chrominance area.  */
            cam->imgs.mask_privacy_uv =(u_char*) mymalloc((uint)
                ((cam->imgs.height * cam->imgs.width) / 2));
            if (cam->imgs.size_high > 0) {
                MOTION_LOG(INF, LOG_TYPE_ALL, NO_ERRNO
                    ,_("Opening high resolution privacy mask file"));
                rewind(picture);
                cam->imgs.mask_privacy_high = load_pgm(picture, cam->imgs.width_high, cam->imgs.height_high);
                cam->imgs.mask_privacy_high_uv =(u_char*) mymalloc((uint)
                    ((cam->imgs.height_high * cam->imgs.width_high) / 2));
            }

            myfclose(picture);
        } else {
            MOTION_LOG(ERR, LOG_TYPE_ALL, SHOW_ERRNO
                ,_("Error opening mask file %s"), cam->cfg->mask_privacy.c_str());
            /* Try to write an empty mask file to make it easier for the user to edit it */
            write_mask(cam->cfg->mask_privacy.c_str() );
        }

        if (!cam->imgs.mask_privacy) {
            MOTION_LOG(ERR, LOG_TYPE_ALL, NO_ERRNO
                ,_("Failed to read mask privacy image. Mask privacy feature disabled."));
        } else {
            MOTION_LOG(INF, LOG_TYPE_ALL, NO_ERRNO
            ,_("Mask privacy file \"%s\" loaded."), cam->cfg->mask_privacy.c_str());

            indx_img = 1;
            if (cam->imgs.size_high > 0) {
                indx_max = 2;
            } else {
                indx_max = 1;
            }

            while (indx_img <= indx_max){
                if (indx_img == 1) {
                    start_cr = (cam->imgs.height * cam->imgs.width);
                    offset_cb = ((cam->imgs.height * cam->imgs.width)/4);
                    start_cb = start_cr + offset_cb;
                    indx_width = cam->imgs.width;
                    indx_height = cam->imgs.height;
                    img_temp = cam->imgs.mask_privacy;
                    img_temp_uv = cam->imgs.mask_privacy_uv;
                } else {
                    start_cr = (cam->imgs.height_high * cam->imgs.width_high);
                    offset_cb = ((cam->imgs.height_high * cam->imgs.width_high)/4);
                    start_cb = start_cr + offset_cb;
                    indx_width = cam->imgs.width_high;
                    indx_height = cam->imgs.height_high;
                    img_temp = cam->imgs.mask_privacy_high;
                    img_temp_uv = cam->imgs.mask_privacy_high_uv;
                }

                for (indxrow = 0; indxrow < indx_height; indxrow++) {
                    for (indxcol = 0; indxcol < indx_width; indxcol++) {
                        y_index = indxcol + (indxrow * indx_width);
                        if (img_temp[y_index] == 0xff) {
                            if ((indxcol % 2 == 0) && (indxrow % 2 == 0) ) {
                                uv_index = (indxcol/2) + ((indxrow * indx_width)/4);
                                img_temp[start_cr + uv_index] = 0xff;
                                img_temp[start_cb + uv_index] = 0xff;
                                img_temp_uv[uv_index] = 0x00;
                                img_temp_uv[offset_cb + uv_index] = 0x00;
                            }
                        } else {
                            img_temp[y_index] = 0x00;
                            if ((indxcol % 2 == 0) && (indxrow % 2 == 0) ) {
                                uv_index = (indxcol/2) + ((indxrow * indx_width)/4);
                                img_temp[start_cr + uv_index] = 0x00;
                                img_temp[start_cb + uv_index] = 0x00;
                                img_temp_uv[uv_index] = 0x80;
                                img_temp_uv[offset_cb + uv_index] = 0x80;
                            }
                        }
                    }
                }
                indx_img++;
            }
        }
    }

}

void cls_picture::init_mask()
{
    FILE *picture;

    /* Load the mask file if any */
    if (cam->cfg->mask_file != "") {
        if ((picture = myfopen(cam->cfg->mask_file.c_str(), "rbe"))) {
            /*
             * NOTE: The mask is expected to have the output dimensions. I.e., the mask
             * applies to the already rotated image, not the capture image. Thus, use
             * width and height from imgs.
             */
            cam->imgs.mask = load_pgm(picture, cam->imgs.width, cam->imgs.height);
            myfclose(picture);
        } else {
            MOTION_LOG(ERR, LOG_TYPE_ALL, SHOW_ERRNO
                ,_("Error opening mask file %s")
                ,cam->cfg->mask_file.c_str());
            /*
             * Try to write an empty mask file to make it easier
             * for the user to edit it
             */
            write_mask(cam->cfg->mask_file.c_str());
        }

        if (!cam->imgs.mask) {
            MOTION_LOG(ERR, LOG_TYPE_ALL, NO_ERRNO
                ,_("Failed to read mask image. Mask feature disabled."));
        } else {
            MOTION_LOG(INF, LOG_TYPE_ALL, NO_ERRNO
                ,_("Maskfile \"%s\" loaded.")
                ,cam->cfg->mask_file.c_str());
        }
    } else {
        cam->imgs.mask = NULL;
    }
}

cls_picture::cls_picture(cls_camera *p_cam)
{
    cam = p_cam;
    init_mask();
    init_privacy();
}

cls_picture::~cls_picture()
{

}

