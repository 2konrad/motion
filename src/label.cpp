
#include "motion.hpp"
#include "util.hpp"
#include "conf.hpp"
#include "camera.hpp"
#include "draw.hpp"
#include "logger.hpp"
#include "alg.hpp"
#include "label.hpp"
#include <vector>




typedef struct {
    int y, xl, xr, dy;
} Segment;

// set motion_per_tile 
void cls_label::set_motion_per_tile(){
    int x,y; 

    //u_char *motion = cam->imgs.image_motion.image_norm;
    

    //reset motion_per_tile and label number
    for (x = 0; x < cam->imgs.width / (TILE_SIZE*2); x++) {
        for (y = 0; y < cam->imgs.height /(TILE_SIZE*2) ; y++){
            //motion_per_tile[x][y] = 0;
            tiles[x][y].label = 0;
        }
    }
    // int motionpixelcounter = 0;
    // //set motion_per_tile = sum of motion pixels in tile
    // for (py = 0; py < cam->imgs.height; py++){
    //     for (px = 0; px < cam->imgs.width; px++) {
    //     
    //         x= (int) (px/TILE_SIZE);
    //         y= (int) (py/TILE_SIZE);
    //         motion_per_tile[x][y] += (*motion > 0) ? 1 : 0;
    //         motionpixelcounter += (*motion > 0) ? 1 : 0;
    //         motion++;
    //     }
    // }
    // MOTION_LOG(INF, LOG_TYPE_ALL, NO_ERRNO, "sum motionplixel %d", motionpixelcounter);
}
//call labelling for all x,y tiles  
void cls_label::assign_labels(){
    int x,y;
    current_label.index = 1;
    current_label.motion = 0;
    current_label.tiles = 0;
    current_label.x = cam->imgs.width;
    current_label.xx = 0;
    current_label.y = cam->imgs.height;
    current_label.yy = 0;
    

    //clear labels vector
    labels.clear();

    //MOTION_LOG(INF, LOG_TYPE_ALL, NO_ERRNO, "start labels");
    //current_label_number = 1;
    //tiles_per_label[current_label_number] = 0; 
    //motion_per_label[current_label_number] = 0;  
    //largest_label = 0;

    //set_motion_per_tile();
    //MOTION_LOG(INF, LOG_TYPE_ALL, NO_ERRNO, "done set motion per tile");
    //set label number for all tiles
    min_motion =  TILE_SIZE * TILE_SIZE * MIN_MOTION_PERCENT / 100;  // 80 pixel 
    for (x = 0; x < cam->imgs.width / (TILE_SIZE*2); x++) {
        for (y = 0; y < cam->imgs.height / (TILE_SIZE*2); y++){
            if ((tiles[x][y].label != 0) ||
                (tiles[x][y].motion <= min_motion)) {
                // we have been here already, or no motion so lets leave
                 
            }else{
                // we call label_recursive,  this will label this and all neighbour tiles 
                // and will only return when there is no neighbouring motion tile left
                // if this returns we have found the complete motion area around this tile
                //MOTION_LOG(INF, LOG_TYPE_ALL, NO_ERRNO, "going recursive");
                label_recursive(x,y);
                labels.push_back(current_label);
                //MOTION_LOG(INF, LOG_TYPE_ALL, NO_ERRNO, "identified a label x%d y%d #t:%d #:%d m:%d t:%d", x,y, labels.size() , labels.back().index, labels.back().motion, labels.back().tiles);
                // since we labelled the complete motion area of this tile, we can 
                // increase the current_label_number for next area
                current_label.index++;
                //set size and motion to zero
                current_label.motion = 0;
                current_label.tiles = 0;
                current_label.x = cam->imgs.width;
                current_label.xx = 0;
                current_label.y = cam->imgs.height;
                current_label.yy = 0;
                //tiles_per_label[current_label_number] =0; 
                //motion_per_label[current_label_number] =0; 
            }
        }
    }

    
    std::sort(labels.begin(), labels.end());
    //largest_label = labels.at(0).index;
    //MOTION_LOG(INF, LOG_TYPE_ALL, NO_ERRNO, "total labels found %d", (int) labels.size());
    // fill matrix with label ranks

    for (auto it = begin (labels); it != end (labels); ++it) 
    {
        //MOTION_LOG(INF, LOG_TYPE_ALL, NO_ERRNO, "sorted labels  i%d m%d t%d x%d xx%d y%d yy%d", it->index, it->motion, it->tiles , it->x, it->xx, it->y, it->yy  );         
    }

    //set 2 largest labels in img
    
    ctx_coord *l1 = &cam->current_image->largest_location;
    ctx_coord *l2 = &cam->current_image->second_largest_location;
    int label_count = (int) cam->label->labels.size();
    if (label_count >0) {
        auto it = begin (labels);
        l1->minx = it->x;
        l1->maxx = it->xx;
        l1->miny = it->y;
        l1->maxy = it->yy;
        l1->x = (it->x + it->xx)/2; //center;
        l1->y = (it->y + it->yy)/2; //center;
        l1->width = l1->maxx-l1->minx;
        l1->height = l1->maxy-l1->miny;
        l1->stddev_xy = it->motion; //use stddev for motion
    }
    if (label_count >1) {
        auto it = begin (labels) +1;
        l2->minx= it->x;
        l2->maxx = it->xx;
        l2->miny = it->y;
        l2->maxy = it->yy;
        l2->x = (it->x + it->xx)/2; //center;
        l2->y = (it->y + it->yy)/2; //center;
        l2->width = l2->maxx-l2->minx;
        l2->height = l2->maxy-l2->miny;
        l2->stddev_xy = it->motion; //use stddev for motion
    }else{
        l2->maxx = 0; //this means that there is no 2nd label
    }

    // fill matrix with label ranks   
    // only needed to directly check if a certan pixel is in the largest / second largest / etc label
    // not needed any more, if ctx_ccord largest_label and second_largest_label is saved to img
    // needed for displaying tiles in motion picture
    for (x = 0; x < cam->imgs.width / (TILE_SIZE*2); x++) {
        for (y = 0; y < cam->imgs.height / (TILE_SIZE*2); y++){
            tiles[x][y].label_rank = 0;
            for (auto it = begin (labels); it != end (labels); ++it) {
                int rank = (int) distance(labels.begin(),it) + 1;
                if (tiles[x][y].label == it->index) {
                    tiles[x][y].label_rank = rank;
                }
            }
        }
    }
    
    cam->imgs.largest_label =1; //dummy needed to display labels
}


//check if curent tile is already labeled, if not do it and call label_recursive function for all neighbours
void cls_label::label_recursive(int x, int y){
    
    tiles[x][y].label = current_label.index;
    //set label borders
    current_label.x = MIN(current_label.x, x*TILE_SIZE);
    current_label.xx = MAX(current_label.xx, ((x+1)*TILE_SIZE)-1);
    current_label.y = MIN(current_label.y, y*TILE_SIZE);
    current_label.yy = MAX(current_label.y, ((y+1)*TILE_SIZE)-1);
    
    //MOTION_LOG(INF, LOG_TYPE_ALL, NO_ERRNO, "set label x%d y%d l%d ", x,y, current_label.index);
          
    current_label.motion += tiles[x][y].motion;
    //Set tiles per label += 1
    int x_, y_; // coordinates of neigbours for loop
    current_label.tiles += 1;
    int tiles_x = cam->imgs.width / (TILE_SIZE*2) ;
    int tiles_y = cam->imgs.height / (TILE_SIZE*2) ;
    for (x_ = MAX(0,x-1); x_ <= MIN(tiles_x-1, x+1); x_++) {
        for (y_ = MAX(0,y-1); y_ <= MIN(tiles_y-1, y+1); y_++){
            if( (tiles[x_][y_].label == 0) && 
                (tiles[x_][y_].motion > min_motion) )
            {
                label_recursive(x_,y_);
            } 
            else if ((tiles[x_][y_].label > 0) && 
                     (tiles[x_][y_].label < current_label.index) ) 
            {
                //this cannot happen
                MOTION_LOG(INF, LOG_TYPE_ALL, NO_ERRNO, "found neigbour with old label x%d y%d l%d l%d<<<<<<<<<<", x_,y_, tiles[x_][y_].label, current_label.index);
            }
        }
    }
}

//get label rank for normal resolution coordinates
int cls_label::get_label_rank_normal_coord(int px, int py){
    int x = (int) ((px/2)/TILE_SIZE);
    int y = (int) ((py/2)/TILE_SIZE);
    return tiles[x][y].label_rank  ;
}


cls_label::cls_label(cls_camera *p_cam)
{
    cam = p_cam;
    //tile_size_x = 20;
    //tile_size_y = 20;

}

cls_label::~cls_label()
{
    //myfree(nothing);

}

