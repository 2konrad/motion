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
#include "conf.hpp"
#include "camera.hpp"
#include "draw.hpp"
#include "logger.hpp"
#include "alg.hpp"

#define MAX2(x, y) ((x) > (y) ? (x) : (y))
#define MAX3(x, y, z) ((x) > (y) ? ((x) > (z) ? (x) : (z)) : ((y) > (z) ? (y) : (z)))
#define NORM               100
#define ABS(x)             ((x) < 0 ? -(x) : (x))
#define DIFF(x, y)         (ABS((x)-(y)))
#define NDIFF(x, y)        (ABS(x) * NORM / (ABS(x) + 2 * DIFF(x, y)))
#define MAXS 10000               /* max depth of stack */
#define EXCLUDE_LEVEL_PERCENT 20
/* Increment for *smartmask_buffer in alg_diff_standard. */
#define SMARTMASK_SENSITIVITY_INCR 5
#define PUSH(Y, XL, XR, DY)     /* push new segment on stack */  \
        if (sp<stack+MAXS && Y+(DY) >= 0 && Y+(DY) < height)     \
        {sp->y = Y; sp->xl = XL; sp->xr = XR; sp->dy = DY; sp++;}

#define POP(Y, XL, XR, DY)      /* pop segment off stack */      \
        {sp--; Y = sp->y+(DY = sp->dy); XL = sp->xl; XR = sp->xr;}

typedef struct {
    int y, xl, xr, dy;
} Segment;


void cls_alg::noise_tune()
{
    ctx_images *imgs = &cam->imgs;
    int i;
    u_char *ref = imgs->ref;
    int diff, sum = 0, count = 0;
    u_char *mask = imgs->mask;
    u_char *mask_final = smartmask_final;
    u_char *new_img = cam->imgs.image_vprvcy;


    i = imgs->motionsize;

    for (; i > 0; i--) {
        diff = ABS(*ref - *new_img);

        if (mask) {
            diff = ((diff * *mask++) / 255);
        }

        if (*mask_final) {
            sum += diff + 1;
            count++;
        }

        ref++;
        new_img++;
        mask_final++;
    }

    if (count > 3)  {
        /* Avoid divide by zero. */
        sum /= count / 3;
    }

    /* 5: safe, 4: regular, 3: more sensitive */
    cam->noise = 4 + (cam->noise + sum) / 2;
}

void cls_alg::threshold_tune()
{
    int i, top;
    int sum = 0;
    int diffs = cam->current_image->diffs;
    int motion = cam->detecting_motion;

    if (!diffs) {
        return;
    }

    top = diffs;

    if (motion) {
        diffs = cam->threshold / 4;
    }

    for (i = 0; i < THRESHOLD_TUNE_LENGTH - 1; i++) {
        sum += diffs_last[i];

        if (diffs_last[i + 1] && !motion) {
            diffs_last[i] = diffs_last[i + 1];
        } else {
            diffs_last[i] = cam->threshold / 4;
        }

        if (diffs_last[i] > top) {
            top = diffs_last[i];
        }
    }

    sum += diffs_last[i];
    diffs_last[i] = diffs;

    sum /= THRESHOLD_TUNE_LENGTH / 4;

    if (sum < top * 2) {
        sum = top * 2;
    }

    if (sum < cam->cfg->threshold) {
        cam->threshold = (cam->threshold + sum) / 2;
    }
}

/*
 * Labeling by Joerg Weber. Based on an idea from Hubert Mara.
 * Floodfill enhanced by Ian McConnel based on code from
 * http://www.acm.org/pubs/tog/GraphicsGems/
 * http://www.codeproject.com/gdi/QuickFill.asp

 * Filled horizontal segment of scanline y for xl <= x <= xr.
 * Parent segment was on line y - dy.  dy = 1 or -1
 */
int cls_alg::iflood(int x, int y, int width, int height,
        u_char *out, int *labels, int newvalue, int oldvalue)
{
    int l, x1, x2, dy;
    Segment stack[MAXS], *sp = stack; /* Stack of filled segments. */
    int count = 0;

    if (x < 0 || x >= width || y < 0 || y >= height) {
        return 0;
    }

    PUSH(y, x, x, 1);      /* Needed in some cases. */
    PUSH(y + 1, x, x, -1); /* Seed segment (popped 1st). */

    while (sp > stack) {
        /* Pop segment off stack and fill a neighboring scan line. */
        POP(y, x1, x2, dy);
        /*
         * Segment of scan line y-dy for x1<=x<=x2 was previously filled,
         * now explore adjacent pixels in scan line y
         */
        for (x = x1; x >= 0 && out[y * width + x] != 0 && labels[y * width + x] == oldvalue; x--) {
            labels[y * width + x] = newvalue;
            count++;
        }

        if (x >= x1) {
            goto skip;
        }

        l = x + 1;

        if (l < x1) {
            PUSH(y, l, x1 - 1, -dy); /* Leak on left? */
        }

        x = x1 + 1;

        do {
            for (; x < width && out[y * width + x] != 0 && labels[y * width + x] == oldvalue; x++) {
                labels[y * width + x] = newvalue;
                count++;
            }

            PUSH(y, l, x - 1, dy);

            if (x > x2 + 1) {
                PUSH(y, x2 + 1, x - 1, -dy); /* Leak on right? */
            }

        skip:
            for (x++; x <= x2 && !(out[y * width + x] != 0 && labels[y * width + x] == oldvalue); x++);
            l = x;
        } while (x <= x2);
    }
    return count;
}

int cls_alg::labeling()
{
    ctx_images *imgs = &cam->imgs;
    u_char *out = imgs->image_motion.image_norm;
    int *labels = imgs->labels;
    int ix, iy, pixelpos;
    int width = imgs->width;
    int height = imgs->height;
    int labelsize = 0;
    int current_label = 2;
    /* Keep track of the area just under the threshold.  */
    int max_under = 0;

    cam->current_image->total_labels = 0;
    imgs->labelsize_max = 0;
    /* ALL labels above threshold are counted as labelgroup. */
    imgs->labelgroup_max = 0;
    imgs->labels_above = 0;

    /* Init: 0 means no label set / not checked. */
    memset(labels, 0,(uint)(width * height) * sizeof(*labels));
    pixelpos = 0;

    for (iy = 0; iy < height - 1; iy++) {
        for (ix = 0; ix < width - 1; ix++, pixelpos++) {
            /* No motion - no label */
            if (out[pixelpos] == 0) {
                labels[pixelpos] = 1;
                continue;
            }

            /* Already visited by alg_iflood */
            if (labels[pixelpos] > 0) {
                continue;
            }

            labelsize = iflood(ix, iy, width, height, out, labels, current_label, 0);

            if (labelsize > 0) {
                /* Label above threshold? Mark it again (add 32768 to labelnumber). */
                if (labelsize > cam->threshold) {
                    labelsize = iflood(ix, iy, width, height, out, labels, current_label + 32768, current_label);
                    imgs->labelgroup_max += labelsize;
                    imgs->labels_above++;
                } else if(max_under < labelsize) {
                    max_under = labelsize;
                }

                if (imgs->labelsize_max < labelsize) {
                    imgs->labelsize_max = labelsize;
                    imgs->largest_label = current_label;
                }

                cam->current_image->total_labels++;
                current_label++;
            }
        }
        pixelpos++; /* Compensate for ix < width - 1 */
    }

    /* Return group of significant labels or if that's none, the next largest
     * group (which is under the threshold, but especially for setup gives an
     * idea how close it was).
     */
    return imgs->labelgroup_max ? imgs->labelgroup_max : max_under;
}

/**  Dilates a 3x3 box. */
int cls_alg::dilate9(u_char *img, int width, int height, void *buffer)
{
    /*
     * - row1, row2 and row3 represent lines in the temporary buffer.
     * - Window is a sliding window containing max values of the columns
     *   in the 3x3 matrix.
     * - width is an index into the sliding window (this is faster than
     *   doing modulo 3 on i).
     * - blob keeps the current max value.
     */
    int y, i, sum = 0, widx;
    u_char *row1, *row2, *row3, *rowTemp, *yp;
    u_char window[3], blob, latest;

    /* Set up row pointers in the temporary buffer. */
    row1 = (u_char *)buffer;
    row2 = row1 + width;
    row3 = row2 + width;

    /* Init rows 2 and 3. */
    memset(row2, 0, (uint)width);
    memcpy(row3, img, (uint)width);

    /* Pointer to the current row in img. */
    yp = img;

    for (y = 0; y < height; y++) {
        /* Move down one step; row 1 becomes the previous row 2 and so on. */
        rowTemp = row1;
        row1 = row2;
        row2 = row3;
        row3 = rowTemp;

        /* If we're at the last row, fill with zeros, otherwise copy from img. */
        if (y == height - 1) {
            memset(row3, 0, (uint)width);
        } else {
            memcpy(row3, yp + width, (uint)width);
        }

        /* Init slots 0 and 1 in the moving window. */
        window[0] = MAX3(row1[0], row2[0], row3[0]);
        window[1] = MAX3(row1[1], row2[1], row3[1]);

        /* Init blob to the current max, and set window index. */
        blob = MAX2(window[0], window[1]);
        widx = 2;

        /*
         * Iterate over the current row; index i is off by one to eliminate
         * a lot of +1es in the loop.
         */
        for (i = 2; i <= width - 1; i++) {
            /* Get the max value of the next column in the 3x3 matrix. */
            latest = window[widx] = MAX3(row1[i], row2[i], row3[i]);

            /*
             * If the value is larger than the current max, use it. Otherwise,
             * calculate a new max (because the new value may not be the max.
             */
            if (latest >= blob) {
                blob = latest;
            } else {
                blob = MAX3(window[0], window[1], window[2]);
            }

            /* Write the max value (blob) to the image. */
            if (blob != 0) {
                *(yp + i - 1) = blob;
                sum++;
            }

            /* Wrap around the window index if necessary. */
            if (++widx == 3) {
                widx = 0;
            }
        }

        /* Store zeros in the vertical sides. */
        *yp = *(yp + width - 1) = 0;
        yp += width;
    }

    return sum;
}

/**  Dilates a + shape. */
int cls_alg::dilate5(u_char *img, int width, int height, void *buffer)
{
    /*
     * - row1, row2 and row3 represent lines in the temporary buffer.
     * - mem holds the max value of the overlapping part of two + shapes.
     */
    int y, i, sum = 0;
    u_char *row1, *row2, *row3, *rowTemp, *yp;
    u_char blob, mem, latest;

    /* Set up row pointers in the temporary buffer. */
    row1 = (u_char *)buffer;
    row2 = row1 + width;
    row3 = row2 + width;

    /* Init rows 2 and 3. */
    memset(row2, 0, (uint)width);
    memcpy(row3, img, (uint)width);

    /* Pointer to the current row in img. */
    yp = img;

    for (y = 0; y < height; y++) {
        /* Move down one step; row 1 becomes the previous row 2 and so on. */
        rowTemp = row1;
        row1 = row2;
        row2 = row3;
        row3 = rowTemp;

        /* If we're at the last row, fill with zeros, otherwise copy from img. */
        if (y == height - 1) {
            memset(row3, 0, (uint)width);
        } else {
            memcpy(row3, yp + width, (uint)width);
        }

        /* Init mem and set blob to force an evaluation of the entire + shape. */
        mem = MAX2(row2[0], row2[1]);
        blob = 1; /* dummy value, must be > 0 */

        for (i = 1; i < width - 1; i++) {
            /* Get the max value of the "right edge" of the + shape. */
            latest = MAX3(row1[i], row2[i + 1], row3[i]);

            if (blob == 0) {
                /* In case the last blob is zero, only latest matters. */
                blob = latest;
                mem = row2[i + 1];
            } else {
                /* Otherwise, we have to check both latest and mem. */
                blob = MAX2(mem, latest);
                mem = MAX2(row2[i], row2[i + 1]);
            }

            /* Write the max value (blob) to the image. */
            if (blob != 0) {
                *(yp + i) = blob;
                sum++;
            }
        }

        /* Store zeros in the vertical sides. */
        *yp = *(yp + width - 1) = 0;
        yp += width;
    }
    return sum;
}

/**  Erodes a 3x3 box. */
int cls_alg::erode9(u_char *img, int width, int height, void *buffer, u_char flag)
{
    int y, i, sum = 0;
    char *Row1, *Row2, *Row3;

    Row1 = (char *)buffer;
    Row2 = Row1 + width;
    Row3 = Row1 + 2 * width;
    memset(Row2, flag, (uint)width);
    memcpy(Row3, img, (uint)width);

    for (y = 0; y < height; y++) {
        memcpy(Row1, Row2, (uint)width);
        memcpy(Row2, Row3, (uint)width);

        if (y == height - 1) {
            memset(Row3, flag, (uint)width);
        } else {
            memcpy(Row3, img + (y + 1) * width, (uint)width);
        }

        for (i = width - 2; i >= 1; i--) {
            if (Row1[i - 1] == 0 ||
                Row1[i]     == 0 ||
                Row1[i + 1] == 0 ||
                Row2[i - 1] == 0 ||
                Row2[i]     == 0 ||
                Row2[i + 1] == 0 ||
                Row3[i - 1] == 0 ||
                Row3[i]     == 0 ||
                Row3[i + 1] == 0) {
                img[y * width + i] = 0;
            } else {
                sum++;
            }
        }

        img[y * width] = img[y * width + width - 1] = flag;
    }
    return sum;
}

/* Erodes in a + shape. */
int cls_alg::erode5(u_char *img, int width, int height, void *buffer, u_char flag)
{
    int y, i, sum = 0;
    char *Row1, *Row2, *Row3;

    Row1 = (char *)buffer;
    Row2 = Row1 + width;
    Row3 = Row1 + 2 * width;
    memset(Row2, flag, (uint)width);
    memcpy(Row3, img, (uint)width);

    for (y = 0; y < height; y++) {
        memcpy(Row1, Row2, (uint)width);
        memcpy(Row2, Row3, (uint)width);

        if (y == height - 1) {
            memset(Row3, flag, (uint)width);
        } else {
            memcpy(Row3, img + (y + 1) * width, (uint)width);
        }

        for (i = width - 2; i >= 1; i--) {
            if (Row1[i]     == 0 ||
                Row2[i - 1] == 0 ||
                Row2[i]     == 0 ||
                Row2[i + 1] == 0 ||
                Row3[i]     == 0) {
                img[y * width + i] = 0;
            } else {
                sum++;
            }
        }

        img[y * width] = img[y * width + width - 1] = flag;
    }
    return sum;
}

void cls_alg::despeckle()
{
    int diffs, width, height, done;
    uint i, len;
    u_char *out, *common_buffer;

    if ((cam->cfg->despeckle_filter == "") || cam->current_image->diffs <= 0) {
        if (cam->imgs.labelsize_max) {
            cam->imgs.labelsize_max = 0;
        }
        return;
    }

    diffs = 0;
    out = cam->imgs.image_motion.image_norm;
    width = cam->imgs.width;
    height = cam->imgs.height;
    done = 0;
    len = (uint)cam->cfg->despeckle_filter.length();
    common_buffer = cam->imgs.common_buffer;
    cam->current_image->total_labels = 0;
    cam->imgs.largest_label = 0;

    for (i = 0; i < len; i++) {
        switch (cam->cfg->despeckle_filter[i]) {
        case 'E':
            diffs = erode9(out, width, height, common_buffer, 0);
            if (diffs == 0) {
                i = len;
            }
            done = 1;
            break;
        case 'e':
            diffs = erode5(out, width, height, common_buffer, 0);
            if (diffs == 0) {
                i = len;
            }
            done = 1;
            break;
        case 'D':
            diffs = dilate9(out, width, height, common_buffer);
            done = 1;
            break;
        case 'd':
            diffs = dilate5(out, width, height, common_buffer);
            done = 1;
            break;
        /* No further despeckle after labeling! */
        case 'l':
            diffs = labeling();
            i = len;
            done = 2;
            break;
        }
    }

    /* If conf.despeckle_filter contains any valid action EeDdl */
    if (done) {
        if (done != 2) {
            cam->imgs.labelsize_max = 0; // Disable Labeling
        }
        cam->current_image->diffs = diffs;
        return;
    } else {
        cam->imgs.labelsize_max = 0; // Disable Labeling
    }

    return;
}

void cls_alg::tune_smartmask()
{
    int i;
    u_char diff;
    int motionsize = cam->imgs.motionsize;
    int sensitivity = cam->lastrate * (11 - cam->cfg->smart_mask_speed);

    if ((cam->cfg->smart_mask_speed == 0) ||
        (cam->event_curr_nbr == cam->event_prev_nbr) ||
        (--smartmask_count)) {
        return;
    }

    for (i = 0; i < motionsize; i++) {
        /* Decrease smart_mask sensitivity every 5*speed seconds only. */
        if (smartmask[i] > 0) {
            smartmask[i]--;
        }
        /* Increase smart_mask sensitivity based on the buffered values. */
        diff = (u_char)(smartmask_buffer[i] / sensitivity);

        if (diff) {
            if (smartmask[i] <= diff + 80) {
                smartmask[i] += diff;
            } else {
                smartmask[i] = 80;
            }
            smartmask_buffer[i] %= sensitivity;
        }
        /* Transfer raw mask to the final stage when above trigger value. */
        if (smartmask[i] > 20) {
            smartmask_final[i] = 0;
        } else {
            smartmask_final[i] = 255;
        }
    }
    /* Further expansion (here:erode due to inverted logic!) of the mask. */
    erode9(smartmask_final, cam->imgs.width, cam->imgs.height,
                      cam->imgs.common_buffer, 255);
    erode5(smartmask_final, cam->imgs.width, cam->imgs.height,
                      cam->imgs.common_buffer, 255);
    smartmask_count = 5 * cam->lastrate * (11 - cam->cfg->smart_mask_speed);
}

void cls_alg::diff_nomask()
{
    u_char *ref = cam->imgs.ref;
    u_char *out = cam->imgs.image_motion.image_norm;
    u_char *new_img = cam->imgs.image_vprvcy;
    long sum_currdiff = 0;   // to see sum of difference in brightness for lightswitch

    int i, curdiff;
    int imgsz = cam->imgs.motionsize;
    int diffs = 0, diffs_net = 0;
    int noise = cam->noise;
    int lrgchg = cam->cfg->threshold_ratio_change;

    memset(out + imgsz, 128, (uint)(imgsz / 2));
    memset(out, 0, (uint)imgsz);

    for (i = 0; i < imgsz; i++) {
        curdiff = (*ref - *new_img);
        if (abs(curdiff) > noise) {
            *out = *new_img;
            diffs++;
            sum_currdiff -= curdiff;
            if (curdiff > lrgchg) {
                diffs_net++;
            } else if (curdiff < -lrgchg) {
                diffs_net--;
            }
        }
        out++;
        ref++;
        new_img++;
    }
    cam->current_image->diffs_raw = diffs;
    cam->current_image->diffs_raw = (int) (sum_currdiff / diffs);
    cam->current_image->diffs = diffs;
    cam->imgs.image_motion.imgts = cam->current_image->imgts;

    if (diffs > 0 ) {
        cam->current_image->diffs_ratio = (abs(diffs_net) * 100) / diffs;
    } else {
        cam->current_image->diffs_ratio = 100;
    }

}

void cls_alg::diff_mask()
{
    u_char *ref  = cam->imgs.ref;
    u_char *out  = cam->imgs.image_motion.image_norm;
    u_char *mask = cam->imgs.mask;
    u_char *new_img = cam->imgs.image_vprvcy;
    long sum_currdiff = 0;   // to see sum of difference in brightness for lightswitch


    int i, curdiff;
    int imgsz = cam->imgs.motionsize;
    int diffs = 0, diffs_net = 0;
    int noise = cam->noise;
    int lrgchg = cam->cfg->threshold_ratio_change;

    memset(out + imgsz, 128, (uint)(imgsz / 2));
    memset(out, 0, (uint)imgsz);

    for (i = 0; i < imgsz; i++) {
        curdiff = (*ref - *new_img);
        if (mask) {
            curdiff = ((curdiff * *mask) / 255);
        }

        if (abs(curdiff) > noise) {
            *out = *new_img;
            diffs++;
            sum_currdiff -= curdiff;
            if (curdiff > lrgchg) {
                diffs_net++;
            } else if (curdiff < -lrgchg) {
                diffs_net--;
            }
        }

        out++;
        ref++;
        new_img++;
        mask++;
    }
    cam->current_image->diffs_raw = diffs;
    cam->current_image->diffs_raw = (int) (sum_currdiff / diffs);
    cam->current_image->diffs = diffs;
    cam->imgs.image_motion.imgts = cam->current_image->imgts;

    if (diffs > 0 ) {
        cam->current_image->diffs_ratio = (abs(diffs_net) * 100) / diffs;
    } else {
        cam->current_image->diffs_ratio = 100;
    }

}

// diff calculated from half size subsamplead image from image_norm (sub_sampled_img), comparing Y, U V
// bg : background = refernce frame ; size: width * height * 3 / 4, non planar, each PIXEL hast 3 bytes Y, U, V 
// mot: motion image width * height * 1.5 // to be changed to subsampled resolution
// mask: mask width * height 
// img: current image
void cls_alg::diff_mask_YUV()
{
    float *bg  = cam->imgs.bg;
    u_char *mot  = cam->imgs.image_motion.image_norm;
    //u_char *motion = cam->imgs.motion;
    u_char *mask = cam->imgs.mask;
    u_char *img_vp = cam->imgs.image_vprvcy;
    u_char *img = cam->imgs.sub_sampled_img;
    long sum_currdiff = 0;   // to see sum of difference in brightness for lightswitch


    int Ydiff, Udiff, Vdiff, norm_x, norm_y;
    int imgsz = cam->imgs.motionsize;
    int width = cam->imgs.width;
    int height = cam->imgs.height;
    
    int diff = 0, diffs = 0, diffs_large = 0;
    int noise = cam->noise;
    int lrgchg = cam->cfg->threshold_ratio_change;

    memset(mot + imgsz, 128, (uint)(imgsz / 2));
    memset(mot, 0, (uint)imgsz);

    // Todo switch to precalculated subsampled pic sub_sampled_img
    for (int y = 0; y < height/2; y++) {
        for (int x = 0; x < width/2; x++) {
            norm_x = x * 2;
            norm_y = y * 2; 
            if (mask[norm_y * width + norm_x] == 0) {
                Ydiff = 0;
                Udiff = 0;
                Vdiff = 0;
            }else{
                // check Y, average 4 pixels 
                Ydiff = (int) (*bg - *img);
                // check U resolution is just right = half the resolution of norm
                Udiff = (int) (*bg - *img);
                // check V
                Vdiff = (int) (*bg - *img);
            }
            bg+=3;
            img+=3;
            diff = abs(Ydiff)+ abs(Udiff) + abs(Vdiff);
            if (diff > noise) {
                mot[norm_y * width + norm_x] = img_vp[norm_y * width + norm_x];    // 4 pixel !!!
                mot[norm_y * width + norm_x+1] = img_vp[norm_y * width + norm_x+1];
                mot[(norm_y+1) * width + norm_x] = img_vp[(norm_y+1) * width + norm_x];
                mot[(norm_y+1) * width + norm_x+1] = img_vp[(norm_y+1) * width + norm_x+1];

                cam->label->tiles[x/TILE_SIZE][y/TILE_SIZE].motion++;
                diffs++;
                sum_currdiff -= Ydiff; //measure light change 
            }
            if (diff > lrgchg) {
                diffs_large++;
            }

        }
    }

    cam->label->assign_labels();
    // Todo only take laregst label diff
    int diff_largest = cam->current_image->largest_location.stddev_xy;  // use std dev for maotion

    // int diff_tiles = 0;
    // int min_motion  =  TILE_SIZE * TILE_SIZE * MIN_MOTION_PERCENT / 100; 
    // // Todo : delete all motion of tiles < minmtion
    // for (int y = 0; y < height/(TILE_SIZE*2); y++) {
    //     for (int x = 0; x < width/(TILE_SIZE*2); x++) {
    //         if (cam->label->tiles[x][y].motion > min_motion) {
    //             diff_tiles += cam->label->tiles[x][y].motion;
    //         }
    //     }
    // }
    cam->current_image->diffs_raw = diffs;
    cam->current_image->diffs_raw = (int) (sum_currdiff / diffs);
    cam->current_image->diffs = diff_largest;
    cam->imgs.image_motion.imgts = cam->current_image->imgts;

    if (diffs > 0 ) {
        cam->current_image->diffs_ratio = (abs(diffs_large) * 100) / diffs;
    } else {
        cam->current_image->diffs_ratio = 100;
    }

}

void cls_alg::diff_smart()
{

    u_char *ref  = cam->imgs.ref;
    u_char *out  = cam->imgs.image_motion.image_norm;
    u_char *mask_final = smartmask_final;
    u_char *new_img = cam->imgs.image_vprvcy;

    int i, curdiff;
    int imgsz = cam->imgs.motionsize;
    int diffs = 0, diffs_net = 0;
    int noise = cam->noise;
    int *mask_buffer = smartmask_buffer;
    int lrgchg = cam->cfg->threshold_ratio_change;

    imgsz = cam->imgs.motionsize;
    memset(out + imgsz, 128, (uint)(imgsz / 2));
    memset(out, 0, (uint)imgsz);

    for (i = 0; i < imgsz; i++) {
        curdiff = (*ref - *new_img);
        if (cam->cfg->smart_mask_speed) {
            if (abs(curdiff) > noise) {
                if (cam->event_curr_nbr != cam->event_prev_nbr) {
                    (*mask_buffer) += SMARTMASK_SENSITIVITY_INCR;
                }
                if (!*mask_final) {
                    curdiff = 0;
                }
            }
            mask_final++;
            mask_buffer++;
        }
        /* Pixel still in motion after all the masks? */
        if (abs(curdiff) > noise) {
            *out = *new_img;
            diffs++;
            if (curdiff > lrgchg) {
                diffs_net++;
            } else if (curdiff < -lrgchg) {
                diffs_net--;
            }
        }
        out++;
        ref++;
        new_img++;
    }
    cam->current_image->diffs_raw = diffs;
    cam->current_image->diffs = diffs;
    cam->imgs.image_motion.imgts = cam->current_image->imgts;

    if (diffs > 0 ) {
        cam->current_image->diffs_ratio = (abs(diffs_net) * 100) / diffs;
    } else {
        cam->current_image->diffs_ratio = 100;
    }
}

void cls_alg::diff_masksmart()
{
    u_char *ref = cam->imgs.ref;
    u_char *out = cam->imgs.image_motion.image_norm;
    u_char *mask = cam->imgs.mask;
    u_char *mask_final = smartmask_final;
    u_char *new_img = cam->imgs.image_vprvcy;

    int i, curdiff;
    int imgsz = cam->imgs.motionsize;
    int diffs = 0, diffs_net = 0;
    int noise = cam->noise;
    int *mask_buffer = smartmask_buffer;
    int lrgchg = cam->cfg->threshold_ratio_change;

    imgsz= cam->imgs.motionsize;
    memset(out + imgsz, 128, ((uint)imgsz / 2));
    memset(out, 0, (uint)imgsz);

    for (i = 0; i < imgsz; i++) {
        curdiff = (*ref - *new_img);
        if (mask) {
            curdiff = ((curdiff * *mask) / 255);
        }

        if (cam->cfg->smart_mask_speed) {
            if (abs(curdiff) > noise) {
                if (cam->event_curr_nbr != cam->event_prev_nbr) {
                    (*mask_buffer) += SMARTMASK_SENSITIVITY_INCR;
                }
                if (!*mask_final) {
                    curdiff = 0;
                }
            }
            mask_final++;
            mask_buffer++;
        }

        /* Pixel still in motion after all the masks? */
        if (abs(curdiff) > noise) {
            *out = *new_img;
            diffs++;
            if (curdiff > lrgchg) {
                diffs_net++;
            } else if (curdiff < -lrgchg) {
                diffs_net--;
            }
        }

        out++;
        ref++;
        new_img++;
        mask++;
    }

    cam->current_image->diffs_raw = diffs;
    cam->current_image->diffs = diffs;
    cam->imgs.image_motion.imgts = cam->current_image->imgts;

    if (diffs > 0 ) {
        cam->current_image->diffs_ratio = (abs(diffs_net) * 100) / diffs;
    } else {
        cam->current_image->diffs_ratio = 100;
    }

}

bool cls_alg::diff_fast()
{
    ctx_images *imgs = &cam->imgs;
    int i, curdiff, diffs = 0;
    int step = cam->imgs.motionsize / 10000;
    int noise = cam->noise;
    int max_n_changes = cam->cfg->threshold / 2;
    u_char *ref = imgs->ref;
    u_char *new_img = cam->imgs.image_vprvcy;

    if (!step % 2) {
        step++;
    }

    max_n_changes /= step;

    i = imgs->motionsize;

    for (; i > 0; i -= step) {
        curdiff = abs(*ref - *new_img); /* Using a temp variable is 12% faster. */
        if (curdiff >  noise) {
            diffs++;
            if (diffs > max_n_changes) {
               return true;
            }
        }
        ref += step;
        new_img += step;
    }

    return false;
}

void cls_alg::diff_standard()
{
    if (cam->cfg->smart_mask_speed == 0) {
        if (cam->imgs.mask == NULL) {
            diff_nomask();
        } else {
            diff_mask_YUV(); // YUV444p version
        }
    } else {
        if (cam->imgs.mask == NULL) {
            diff_smart();
        } else {
            diff_masksmart();
        }
    }
}

void cls_alg::lightswitch_orig()
{
    if (cam->cfg->lightswitch_percent >= 1) {
        if (cam->current_image->diffs > (cam->imgs.motionsize * cam->cfg->lightswitch_percent / 100)) {
            MOTION_LOG(INF, LOG_TYPE_ALL, NO_ERRNO, _("Lightswitch detected"));
            if (cam->frame_skip < cam->cfg->lightswitch_frames) {
                cam->frame_skip = cam->cfg->lightswitch_frames;
            }
            util_exec_command(cam, cam->cfg->on_lightswitch.c_str(), NULL);
            cam->current_image->diffs = 0;
            ref_frame_update();
        }
    }
}

void cls_alg::lightswitch()
{

    if (cam->cfg->lightswitch_percent >= 1) {
        if (cam->current_image->diffs > (cam->imgs.motionsize * cam->cfg->lightswitch_percent / 100)) {
            
            //check if already a MOTION was detected - no lightswitch in current MOTION
            int motion = 0;
            int indx;
            for (indx = 0; indx < cam->cfg->minimum_motion_frames; indx++) {
                if (cam->imgs.image_ring[indx].motion == true) {
                    motion += 1;
                }
            }
            MOTION_LOG(INF, LOG_TYPE_ALL, NO_ERRNO, _("Possible lightswitch detected"));
            if (motion <= 2){
                MOTION_LOG(NTC, LOG_TYPE_ALL, NO_ERRNO, _("Lightswitch detected and <= 2 motion frames"));
            if (cam->frame_skip < (unsigned int)cam->cfg->lightswitch_frames) {
                cam->frame_skip = (unsigned int)cam->cfg->lightswitch_frames;
            }
            util_exec_command(cam, cam->cfg->on_lightswitch.c_str(), NULL); // before : event(cam, EVENT_LIGHTSWITCH);
            cam->current_image->diffs = 0;
            ref_frame_update();
            }
        }
    }
}

void cls_alg::ref_frame_update_orig()
{
    int accept_timer;
    int i, threshold_ref;
    int *ref_dyn = cam->imgs.ref_dyn;
    u_char *image_virgin = cam->imgs.image_vprvcy;
    u_char *ref = cam->imgs.ref;
    u_char *mask_final = smartmask_final;
    u_char *out = cam->imgs.image_motion.image_norm;

    accept_timer = cam->cfg->static_object_time * cam->cfg->framerate;
    threshold_ref = cam->noise * EXCLUDE_LEVEL_PERCENT / 100;

    for (i = cam->imgs.motionsize; i > 0; i--) {
        /* Exclude pixels from ref frame well below noise level. */
        if (((int)(abs(*ref - *image_virgin)) > threshold_ref) && (*mask_final)) {
            if (*ref_dyn == 0) { /* Always give new pixels a chance. */
                *ref_dyn = 1;
            } else if (*ref_dyn > accept_timer) { /* Include static Object after some time. */
                *ref_dyn = 0;
                *ref = *image_virgin;
            } else if (*out) {
                (*ref_dyn)++; /* Motionpixel? Keep excluding from ref frame. */
            } else {
                *ref_dyn = 0; /* Nothing special - release pixel. */
                *ref = (u_char)((*ref + *image_virgin) / 2);
            }
        } else {  /* No motion: copy to ref frame. */
            *ref_dyn = 0; /* Reset pixel */
            *ref = *image_virgin;
        }

        ref++;
        image_virgin++;
        mask_final++;
        ref_dyn++;
        out++;
    }

}

void  cls_alg::ref_frame_update_new()
{
    int accept_timer = 3 * cam->cfg->static_object_time;
    int i, accept_counter = 0, accept_counter_sum = 0, accept_counter_sum_all = 0, accept_counter_all = 0;
    int indx, motionframes = 0;
    int *ref_dyn = cam->imgs.ref_dyn; //diff_history
    bool onlytwomotionframes = false;
    //int noise = (cam->noise) ;
    u_char *image_virgin = cam->imgs.image_vprvcy;
    u_char *ref = cam->imgs.ref;
    u_char *mask_final = smartmask_final;
    u_char *mask = cam->imgs.mask;
    u_char *motion = cam->imgs.image_motion.image_norm;

    if (!cam->detecting_motion){
        for (indx = 0; indx < cam->cfg->minimum_motion_frames; indx++) {
            if (cam->imgs.image_ring[indx].motion) {
                motionframes += 1;
            }
        }
    }
    onlytwomotionframes = ( (!cam->detecting_motion) && (motionframes <= 1) );
    for (i = cam->imgs.motionsize; i > 0; i--) {
        if ((*mask) && (*mask_final) ){
            if (abs(*ref - *image_virgin) > (cam->noise * 0.6)){ //diff > noise
                (*ref_dyn)++;
                accept_counter++;
                accept_counter_sum += (*ref_dyn);
                accept_counter_sum_all += (*ref_dyn);
                accept_counter_all++;
                if (onlytwomotionframes) {
                    //high accept timer
                    (*ref_dyn) = accept_timer ; 
                }
                if (*ref_dyn > accept_timer) {
                    *ref = *image_virgin;
                    (*ref_dyn)--;
                }
                // if (cam->current_image->diffs == 0   ){
                //     MOTPLS_LOG(INF, TYPE_ALL, NO_ERRNO, "motion at diff 0");
                // }
            } else {
                //diff<noise
                // if ( ((*ref_dyn) > accept_timer/2) || 
                //     (((cam->current_image->diffs )>cam->threshold) && ((*ref_dyn) > 0)))  {
                //         (*ref_dyn)--;
                //     }
                // if ( (*ref_dyn) < accept_timer/2)   {
                //         (*ref_dyn)++;
                //     }
                if ( (*ref_dyn) > 0)   {
                    (*ref_dyn)--;
                }    
                if (cam->current_image->shot == 0){      //shots_mt                                                            
                    *ref = (unsigned char)( (((int) *ref) * 3 + (int) *image_virgin) / 4); 
                }
                accept_counter_sum_all += (*ref_dyn);
                accept_counter_all++;
            }
        }//mask
        ref++;
        image_virgin++;
        mask_final++;
        mask++;
        ref_dyn++;
        motion++;
    } /* end for i */
    if (accept_counter==0){accept_counter=1;}
    cam->current_image->accept_average = accept_counter_sum / accept_counter;
    //cam->current_image->accept_average = accept_counter_sum_all / accept_counter_all;

}

//Update referecnce frame BG with sub sampled resolution YUV444p
void  cls_alg::ref_frame_update()
{
    int accept_frames = 3 * cam->cfg->static_object_time; // number of frames with diff before it is accepten into BG

    //int i, accept_counter = 0, accept_counter_sum = 0, accept_counter_sum_all = 0, accept_counter_all = 0;
    //int indx, motionframes = 0;
    //int *ref_dyn = cam->imgs.diff_counter; //diff_history
    //bool onlytwomotionframes = false;
    //int noise = (cam->noise) ;
    //u_char *image_virgin = cam->imgs.image_vprvcy;
    //u_char *ref = cam->imgs.ref;
    //u_char *mask_final = smartmask_final;
    u_char *mask = cam->imgs.mask;
    u_char *motion = cam->imgs.image_motion.image_norm;
    float *bg = cam->imgs.bg;
    u_char *img = cam->imgs.sub_sampled_img;
    u_char *motion_counter = cam->imgs.motion_counter;

    // if (!cam->detecting_motion){
    //     for (indx = 0; indx < cam->cfg->minimum_motion_frames; indx++) {
    //         if (cam->imgs.image_ring[indx].motion) {
    //             motionframes += 1;
    //         }
    //     }
    // }
    // onlytwomotionframes = ( (!cam->detecting_motion) && (motionframes <= 1) );
    int width = cam->imgs.width;
    int height = cam->imgs.height;
    //int norm_x, norm_y;
    //u_char *pointer_norm;
    for (int y = 0; y < height/2; y++) {
        for (int x = 0; x < width/2; x++) {
            //norm_x = x * 2;
            //norm_y = y * 2; 
            if (*mask)  { 
                //if (abs(*bg++ - *img++) > (cam->noise)) //diff > noise*0.6
                //check if motion was detected
                if ((*motion))
                { 
                                            
                    if ((x==286) &&(y==53)) {
                        MOTION_LOG(INF, LOG_TYPE_ALL, NO_ERRNO, "x %d y %d bg-r %d bg-g %d bg-b %d img-r %d img-g %d img-b %d motion%d count%d", 
                            x,y, (int)*bg, (int)*(bg+1), (int)*(bg+2), *img, *(img+1), *(img+2) , *motion, *motion_counter);
                            }
                    (*motion_counter)++;
                    if (*motion_counter > accept_frames){
                        if ((x>286) &&(y==53)) {
                        MOTION_LOG(INF, LOG_TYPE_ALL, NO_ERRNO, "x %d y %d bg-r %d bg-g %d bg-b %d img-r %d img-g %d img-b %d", 
                            x,y, (int)*bg, (int)*(bg+1), (int)*(bg+2), *img, *(img+1), *(img+2));
                            }
                        *bg     = (float) (*bg     *.5 + *img     * .5) ;     //Y
                        *(bg+1) = (float) (*(bg+1) *.5 + *(img+1) * .5) ;     //U
                        *(bg+2) = (float) (*(bg+2) *.5 + *(img+2) * .5) ;     //V


                        
                        // *bg     =  (( *img)     );     //Y
                        // *(bg+1) =  (( *(img+1)) );     //U
                        // *(bg+2) =  (( *(img+2)) );     //V
                        
                        (*motion_counter)--;
                    }
                }else{
                    if ( (*motion_counter) > 0) (*motion_counter)--;
                    //but still minimally update ref every 1 second!!!!!!!!!!!!!!!!!!!!
                    if ((cam->current_image->shot == 1)) {//  && (cam->current_image->imgts.tv_sec)){
                        *bg     = (float) (*bg     *.98 + *img     * .02) ;     //Y
                        *(bg+1) = (float) (*(bg+1) *.98 + *(img+1) * .02) ;     //U
                        *(bg+2) = (float) (*(bg+2) *.98 + *(img+2) * .02) ;     //V                      
                    }
                }
            }
            mask   += 2; //norm resolution, skip every 2nd 
            motion += 2; //
            bg     += 3; //3 bytes per pixel
            img    += 3;
            motion_counter++;
        }//end for x
        //skip 1 line of mask and motion
        mask = mask + width;
        motion = motion + width;
    }
}

void cls_alg::ref_frame_reset_orig()
{
    /* Copy fresh image */
    memcpy(cam->imgs.ref, cam->imgs.image_vprvcy, (uint)cam->imgs.size_norm);
    /* Reset static objects */
    memset(cam->imgs.ref_dyn, 0
        ,(uint)cam->imgs.motionsize * sizeof(*cam->imgs.ref_dyn));

}

//reset new

void cls_alg::ref_frame_reset_new()
{
    /* Copy fresh image */
    memcpy(cam->imgs.ref, cam->imgs.image_vprvcy, cam->imgs.size_norm);
    /* Reset static objects */
    //memset(cam->imgs.ref_dyn, accept_timer * 0.5, cam->imgs.motionsize * sizeof(*cam->imgs.ref_dyn)); //accept_timer * 0.6
    int *ref_dyn = cam->imgs.ref_dyn;
    int i;
    for (i = cam->imgs.motionsize; i > 0; i--) {
        (*ref_dyn) = 0 ;
        ref_dyn++;
    }
}
void cls_alg::ref_frame_reset()
{
    /* Copy fresh image */
    int sub_size = cam->imgs.width * cam->imgs.height /4 ;
    float * bg = cam->imgs.bg;
    u_char *img = cam->imgs.sub_sampled_img;
    u_char *motion_counter = cam->imgs.motion_counter;
    int i;

    for (i = 0; i < sub_size; i++) {
        (*motion_counter) = 200 ; // allow quick update of bg for let the first 200 frames 
        motion_counter++;
        *bg = (float) *img;
        bg++;
        img++;
    }
}

/*Calculate the center location of changes*/
void cls_alg::location_center()
{
    int width = cam->imgs.width;
    int height = cam->imgs.height;
    ctx_coord *cent = &cam->current_image->location;
    u_char *out = cam->imgs.image_motion.image_norm;
    int x, y, centc = 0;

    cent->x = 0;
    cent->y = 0;

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            if (*(out++)) {
                cent->x += x;
                cent->y += y;
                centc++;
            }
        }
    }

    if (centc) {
        cent->x = cent->x / centc;
        cent->y = cent->y / centc;
    }

    /* This allows for the redcross and boxes to be drawn*/
    if (cent->x < 10) {
        cent->x = 15;
    }
    if (cent->y < 10) {
        cent->y = 15;
    }
    if ((cent->x + 10) > width) {
        cent->x = width - 15;
    }
    if ((cent->y + 10) > height) {
        cent->y = height - 15;
    }

}

/*Calculate distribution and variances of changes*/
void cls_alg::location_dist_stddev()
{
    ctx_images *imgs = &cam->imgs;
    int width = cam->imgs.width;
    int height = cam->imgs.height;
    ctx_coord *cent = &cam->current_image->location;
    u_char *out = imgs->image_motion.image_norm;
    int x, y, centc = 0, xdist = 0, ydist = 0;
    int64_t variance_x, variance_y, variance_xy, distance_mean;

    cent->maxx = 0;
    cent->maxy = 0;
    cent->minx = width;
    cent->miny = height;
    variance_x = 0;
    variance_y = 0;
    distance_mean = 0;

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            if (*(out++)) {
                variance_x += ((x - cent->x) * (x - cent->x));
                variance_y += ((y - cent->y) * (y - cent->y));
                distance_mean += (int64_t)sqrt(
                        ((x - cent->x) * (x - cent->x)) +
                        ((y - cent->y) * (y - cent->y)));

                if (x > cent->x) {
                    xdist += x - cent->x;
                } else if (x < cent->x) {
                    xdist += cent->x - x;
                }

                if (y > cent->y) {
                    ydist += y - cent->y;
                } else if (y < cent->y) {
                    ydist += cent->y - y;
                }

                centc++;
            }
        }
    }

    if (centc) {
        cent->minx = cent->x - xdist / centc * 3;
        cent->maxx = cent->x + xdist / centc * 3;
        cent->miny = cent->y - ydist / centc * 3;
        cent->maxy = cent->y + ydist / centc * 3;
        cent->stddev_x = (int)sqrt((variance_x / centc));
        cent->stddev_y = (int)sqrt((variance_y / centc));
        distance_mean = (int64_t)(distance_mean / centc);
    } else {
        cent->stddev_y = 0;
        cent->stddev_x = 0;
        distance_mean = 0;
    }

    variance_xy = 0;
    out = imgs->image_motion.image_norm;
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            if (*(out++)) {
                variance_xy += (
                    ((int64_t)sqrt(((x - cent->x) * (x - cent->x)) +
                          ((y - cent->y) * (y - cent->y))) - distance_mean) *
                    ((int64_t)sqrt(((x - cent->x) * (x - cent->x)) +
                          ((y - cent->y) * (y - cent->y))) - distance_mean));
            }
        }
    }
    /* Per statistics, divide by n-1 for calc of a standard deviation */
    if ((centc-1) > 0) {
        cent->stddev_xy = (int)sqrt((variance_xy / (centc-1)));
    }
}

void cls_alg::location_dist_basic()
{
    ctx_images *imgs = &cam->imgs;
    int width = cam->imgs.width;
    int height = cam->imgs.height;
    ctx_coord *cent = &cam->current_image->location;
    u_char *out = imgs->image_motion.image_norm;
    int x, y, centc = 0, xdist = 0, ydist = 0;

    cent->maxx = 0;
    cent->maxy = 0;
    cent->minx = width;
    cent->miny = height;

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            if (*(out++)) {
                if (x > cent->x) {
                    xdist += x - cent->x;
                } else if (x < cent->x) {
                    xdist += cent->x - x;
                }

                if (y > cent->y) {
                    ydist += y - cent->y;
                } else if (y < cent->y) {
                    ydist += cent->y - y;
                }

                centc++;
            }
        }
    }

    if (centc) {
        cent->minx = cent->x - xdist / centc * 3;
        cent->maxx = cent->x + xdist / centc * 3;
        cent->miny = cent->y - ydist / centc * 3;
        cent->maxy = cent->y + ydist / centc * 3;
    } else {
        cent->stddev_y = 0;
        cent->stddev_x = 0;
    }
}

/* Ensure min/max are within limits*/
void cls_alg::location_minmax()
{

    int width = cam->imgs.width;
    int height = cam->imgs.height;
    ctx_coord *cent = &cam->current_image->location;

    if (cent->maxx > width - 1) {
        cent->maxx = width - 1;
    } else if (cent->maxx < 0) {
        cent->maxx = 0;
    }

    if (cent->maxy > height - 1) {
        cent->maxy = height - 1;
    } else if (cent->maxy < 0) {
        cent->maxy = 0;
    }

    if (cent->minx > width - 1) {
        cent->minx = width - 1;
    } else if (cent->minx < 0) {
        cent->minx = 0;
    }

    if (cent->miny > height - 1) {
        cent->miny = height - 1;
    } else if (cent->miny < 0) {
        cent->miny = 0;
    }

    /* Align for better locate box handling */
    cent->minx += cent->minx % 2;
    cent->miny += cent->miny % 2;
    cent->maxx -= cent->maxx % 2;
    cent->maxy -= cent->maxy % 2;

    cent->width = cent->maxx - cent->minx;
    cent->height = cent->maxy - cent->miny;
    cent->y = (cent->miny + cent->maxy) / 2;
}

/* Determine the location and standard deviations of changes*/
void cls_alg::location()
{
    location_center();
    if (calc_stddev) {
        location_dist_stddev();
    } else {
        location_dist_basic();
    }
    location_minmax();
    //calculate new "location"
    //cam->label->assign_labels();

    // ctx_coord *cent = &cam->current_image->location;
    // struct tm timestamp_tm;
    // localtime_r(&cam->imgs.image_preview.imgts.tv_sec , &timestamp_tm);
    // int sec = timestamp_tm.tm_sec;
    // int diff = cam->imgs.image_preview.diffs;
    // int shot = cam->current_image->shot;

    //MOTION_LOG(INF, TYPE_EVENTS, NO_ERRNO, "sec %d shot%d prevdiff%d x%d xx%d y%d yy%d ", sec, shot, diff, cent->minx, cent->maxx, cent->miny, cent->maxy );
    //when is this preview image filled (now it still has the old setting)

    int label_count = (int) cam->label->labels.size();
    if (label_count >0) {
        //MOTION_LOG(INF, TYPE_EVENTS, NO_ERRNO, "labels motion%d tiles%d x%d xx%d y%d yy%d ", cam->label->labels.front().motion, cam->label->labels.front().tiles , 
        //cam->label->labels.front().x, cam->label->labels.front().xx, cam->label->labels.front().y, cam->label->labels.front().yy );
        // second largest label
        //MOTION_LOG(INF, TYPE_EVENTS, NO_ERRNO, "2nd label motion%d x%d xx%d y%d yy%d ", cam->current_image->second_largest_location.stddev_xy, 
        //    cam->current_image->second_largest_location.x, cam->current_image->second_largest_location.maxx, 
        //cam->current_image->second_largest_location.y, cam->current_image->second_largest_location.maxy );
        
    }
        
    
}

/* Apply user or default thresholds on standard deviations*/
void cls_alg::stddev()
{
    if (calc_stddev == false) {
        return;
    }
    if (cam->cfg->threshold_sdevx > 0) {
        if (cam->current_image->location.stddev_x > cam->cfg->threshold_sdevx) {
            cam->current_image->diffs = 0;
            return;
        }
    } else if (cam->cfg->threshold_sdevy > 0) {
        if (cam->current_image->location.stddev_y > cam->cfg->threshold_sdevy) {
            cam->current_image->diffs = 0;
            return;
        }
    } else if (cam->cfg->threshold_sdevxy > 0) {
        if (cam->current_image->location.stddev_xy > cam->cfg->threshold_sdevxy) {
            cam->current_image->diffs = 0;
            return;
        }
    }
}

void cls_alg::diff()
{
    if (cam->detecting_motion) {
        diff_standard();
        //cam->label->assign_labels();
    } else {
        if (true) { // (diff_fast()) { //diff fast needs to be adapted 
            diff_standard();
        } else {
            cam->current_image->diffs = 0;
            cam->current_image->diffs_raw = 0;
            cam->current_image->diffs_ratio = 100;
        }
    }
    lightswitch();
    despeckle();
    //MOTION_LOG(INF, LOG_TYPE_ALL, NO_ERRNO, "done labels");
    
}

cls_alg::cls_alg(cls_camera *p_cam)
{
    int i;

    cam = p_cam;

    if ((cam->cfg->threshold_sdevx == 0) &&
        (cam->cfg->threshold_sdevy == 0) &&
        (cam->cfg->threshold_sdevxy == 0)) {
        calc_stddev = false;
    } else {
        calc_stddev = true;
    }

    smartmask =(unsigned char*) mymalloc((uint)cam->imgs.motionsize);
    smartmask_final =(unsigned char*) mymalloc((uint)cam->imgs.motionsize);
    smartmask_buffer =(int*) mymalloc((uint)cam->imgs.motionsize * sizeof(*smartmask_buffer));

    memset(smartmask, 0, (uint)cam->imgs.motionsize);
    memset(smartmask_final, 255, (uint)cam->imgs.motionsize);
    memset(smartmask_buffer, 0, (uint)cam->imgs.motionsize * sizeof(*smartmask_buffer));

    for (i = 0; i < THRESHOLD_TUNE_LENGTH - 1; i++) {
        diffs_last[i] = 0;
    }

}

cls_alg::~cls_alg()
{
    myfree(smartmask);
    myfree(smartmask_final);
    myfree(smartmask_buffer);

}

