#ifndef _INCLUDE_LABEL_HPP_
#define _INCLUDE_LABEL_HPP_

#define MAX_LABELS 100
#define MIN_MOTION_PERCENT 5       // percent of motion pixels per tile needed to trigger tile motion   
#define TILE_SIZE 16


    struct label_t{
        int index ;
        int motion = 0;
        int tiles = 0; 
        int x,xx,y,yy;
        inline bool operator < (const label_t& o) const {
            return (motion > o.motion); //descending
        }
    } ;

    struct tile_t {
        int motion = 0;               // sum of motion in tile
        int label = 0;                 //  label number of tile
        int label_rank;            // rank of label per tile, rank 1 = largest label
    };
    

    class cls_label {
        public:
            cls_label(cls_camera *p_cam);
            ~cls_label();
            void diff();
            int second_largest_label = 0;
            int largest_label = 0;
            int total_labels = 0;
            tile_t tiles[99][99];                       // if  tile is 10x10 px of sub sampled img (half norm). so max morm : 2000 2000 
            std::vector<label_t> labels;
            int get_label_rank_normal_coord(int x, int y);
            void assign_labels();                           //call labelling for all x,y tiles  
            
        private:
            cls_camera *cam;
            label_t current_label;
            int current_label_number;                   // label number assigned to tiles starting with 1 
            //int motion_per_tile [16][ 9];               //tiles with sum of motion in tile
            //int label_per_tile[16][9 ];                 // tiles with label number of tile
            //int label_rank_per_tile[16][9 ];            // rank of label per tile, rank 0 = largest label
            int min_motion;                             // minimum motion pixel per tile
            //int motion_per_label[MAX_LABELS];         // max 100 labels
            //int tiles_per_label[100];                       // pixel size per label
            int tile_size_x;
            int tile_size_y;

            void label_recursive(int x, int y);             //check if curent tile is already labeled, if not do it and call label functio for all neighbours
            void set_motion_per_tile();  //set all tiles of motion aggregate array with the sum of pixel with motion per tile

    };

#endif /* _INCLUDE_LABEL_HPP_ */
