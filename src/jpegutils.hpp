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
=======
 *    along with MotionPlus.  If not, see <https://www.gnu.org/licenses/>.
>>>>>>> plus/upstream__May25
 *
 *
*/


#ifndef _INCLUDE_JPEGUTILS_HPP_
#define _INCLUDE_JPEGUTILS_HPP_

    int jpgutl_decode_jpeg (unsigned char *jpeg_data_in, int jpeg_data_len,
        unsigned int width, unsigned int height, unsigned char *volatile img_out);
    int jpgutl_put_yuv420p(unsigned char *dest_image, int image_size,
        unsigned char *input_image, int width, int height, int quality,
        cls_camera *cam, timespec *ts1, ctx_coord *box);
    int jpgutl_put_grey(unsigned char *dest_image, int image_size,
        unsigned char *input_image, int width, int height, int quality,
        cls_camera *cam, timespec *ts1, ctx_coord *box);
    uint jpgutl_exif(u_char **exif, cls_camera *cam
        , timespec *ts_in1, ctx_coord *box);

#endif /*  _INCLUDE_JPEGUTILS_HPP_ */
