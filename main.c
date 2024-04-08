#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

#include <image_arrays.h>


//--------------------------- Globals ---------------------------//
// Video device registers
volatile int pixel_buffer_start;
volatile int *buffer_reg = (int *)0xFF203020;

// Buffers
short int Buffer1[240][512]; // 240 rows, 512 (320 + padding) columns
short int Buffer2[240][512];

// Global game variables
bool currently_moving = false;
int direction_store = 4;
int current_score = 0;
int best_score = 0;

// Audio device location
struct audio_t *const audiop = ((struct audio_t *)0xff203040);

// Keyboard controller location
struct key_PIT *const keyboard = ((struct key_PIT *) 0xFF200100);


//--------------------- Function Prototypes ---------------------//
// Graphics
void clear_grid();
void plot_pixel(int x, int y, short int color);
void wait_for_vsync();

// Tiles
void init_tiles(bool *active_tile, int *location_tile, int *value_tile, int *shift_offset_x, int *shift_offset_y, bool *tile_moving, bool *tile_merged, int *stop_moving_next_cycle);
void activate_tile(bool *active_tile, int *location_tile, int *value_tile, int *shift_offset_x, int *shift_offset_y, bool *tile_moving, bool *tile_merged, int *stop_moving_next_cycle, int tile_id);
bool check_location_taken(bool *active_tile, int *location_tile, int location);
void move_tiles(bool *active_tile, int *location_tile, int *value_tile, int *shift_offset_x, int *shift_offset_y, int *shift_offset_x_prev, int *shift_offset_y_prev, bool *tile_moving, bool *tile_merged, int *stop_moving_next_cycle, int direction);
bool tile_should_move(bool *active_tile, int *location_tile, int *value_tile, bool *tile_moving, bool *tile_merged, int *stop_moving_next_cycle, int location_inc, int tile_id, int location);
void merge_tiles(bool *active_tile, int *location_tile, int *value_tile, bool *tile_merged, int tile_id_1, int tile_id_2);
void erase_tiles(bool *active_tile, int *location_tile, int *shift_offset_x_prev, int *shift_offset_y_prev, bool *tile_moving, int direction);
void draw_tiles(const bool *active_tile, const int *value_tile, const int *location_tile, int *shift_offset_x, int *shift_offset_y, bool *tile_moving, bool *tile_merged, int *stop_moving_next_cycle);

// Keyboard
int chooseDirection(int dirCode);

// Checking Win and Game Over Screen
bool checkGameOver(bool *active_tile, int *location_tile, int *value_tile);
void gameIsOver(bool *active_tile, int *location_tile, int *value_tile, int *shift_offset_x, int *shift_offset_y, bool *tile_moving, bool *tile_merged, int *stop_moving_next_cycle);

// Audio function
void audio_player(const int *audio); 

// Score Drawing
void draw_score();
void draw_bestScore();

// Writing Characters to Char buffer
void write_char(int x, int y, char c);


//--------------------------- Classes ---------------------------//
// Keyboard PIT struct
struct key_PIT {
	volatile unsigned int RB;
	volatile unsigned int WB;
	volatile unsigned int STATUS;
	volatile unsigned int CMD;
};

// Audio PIT struct
struct audio_t {
      volatile unsigned int control;
      volatile unsigned char rarc;
      volatile unsigned char ralc;
      volatile unsigned char wsrc;
      volatile unsigned char wslc;
      volatile unsigned int ldata;
      volatile unsigned int rdata;
};

 
//------------------------ Main Function ------------------------//
int main(void) {

	// Declare tile variables
	bool active_tile[16];
	int location_tile[16];
	int value_tile[16];
	int shift_offset_x[16];
	int shift_offset_y[16];
	int shift_offset_x_prev[16];
	int shift_offset_y_prev[16];
	bool tile_moving[16];
	bool tile_merged[16];
	int stop_moving_next_cycle[16];
	bool spawn_tile = false;

	// Declare keyboard variables
	int PS2_data;
	int command;
	int direction = 4;
	
	// Initialize tiles
	init_tiles(active_tile, location_tile, value_tile, shift_offset_x, shift_offset_y, tile_moving, tile_merged, stop_moving_next_cycle);
	
    // Set front pixel buffer to Buffer 1
    *(buffer_reg + 1) = (int) &Buffer1; // first store the address in the back buffer					
    wait_for_vsync(); // Now swap the front/back buffers, to set the front buffer location
	
    // Initialize a pointer to the pixel buffer, used by drawing functions
    pixel_buffer_start = *buffer_reg;
    
	clear_grid();

    // Set back pixel buffer to Buffer 2
    *(buffer_reg + 1) = (int) &Buffer2;
    pixel_buffer_start = *(buffer_reg + 1); // we draw on the back buffer
    
	clear_grid();
	
	// Loop infinitely
    while (1) {
		
		// Poll keyboard for input 
 		PS2_data = keyboard->RB;
		
		// If input detected, update direction
		if ((PS2_data & 0xFF00) != 0) {
			command = PS2_data & 0xFF;
			direction = chooseDirection(command);
			for (int tile_id = 0; tile_id < 16; tile_id++) {
				tile_merged[tile_id] = false;
			}
		}

		// Move tiles
		move_tiles(active_tile, location_tile, value_tile, shift_offset_x, shift_offset_y, shift_offset_x_prev, shift_offset_y_prev, tile_moving, tile_merged, stop_moving_next_cycle, direction);
		
		// Set currently_moving and reset stop_moving_next_cycle
		currently_moving = false;
		direction = 4;
		for (int tile_id = 0; tile_id < 16; tile_id++) {
			if (active_tile[tile_id] && tile_moving[tile_id]) {		
				currently_moving = true;
				spawn_tile = true;
				break;
			}
			
			if (stop_moving_next_cycle[tile_id] > 0) {
				stop_moving_next_cycle[tile_id]++;
			}

			if (stop_moving_next_cycle[tile_id] == 4) {
				stop_moving_next_cycle[tile_id] = 0;
			}
		}
		
		// If tiles done moving and tile to be spawned, activate it
		if (!currently_moving && spawn_tile) {
			int tile_id = 0;
			while (tile_id < 15 && active_tile[tile_id]) {
				tile_id++;
			}
			activate_tile(active_tile, location_tile, value_tile, shift_offset_x, shift_offset_y, tile_moving, tile_merged, stop_moving_next_cycle, tile_id);
			spawn_tile = false;
		}

		// Draw tiles
		draw_tiles(active_tile, value_tile, location_tile, shift_offset_x, shift_offset_y, tile_moving, tile_merged, stop_moving_next_cycle);
		
		// Erase old tiles
		erase_tiles(active_tile, location_tile, shift_offset_x_prev, shift_offset_y_prev, tile_moving, direction);
		
		// Draw score
		draw_score();
		draw_bestScore();

		// Checks if game is over and if it is then it goes into game over screen
		bool cheese = checkGameOver(active_tile, location_tile, value_tile);
		if (cheese) {
			gameIsOver(active_tile, location_tile, value_tile, shift_offset_x, shift_offset_y, tile_moving, tile_merged, stop_moving_next_cycle);
		}
	
		// Swap front and back buffers
        wait_for_vsync(); 
		
		// Set new backbuffer
        pixel_buffer_start = *(buffer_reg + 1);
    }
}


//--------------------- Clear Grid Function ---------------------//
void clear_grid() {
    
	short int color;

    // Iterate through all points and draw background
    for (int x = 0; x < 512; x++) {
        for (int y = 0; y < 240; y++) {

			// Assemble color from background image array
			color = 0x0000;
			color = color | background[(320 * 2 * y) + (2 * x) + 1]; // First 8 bits
			color = color << 8; // Bit shift
			color = color | background[(320 * 2 * y) + (2 * x)]; // Last 8 bits

            // Plot pixel
            plot_pixel(x, y, color);
        }
    }
}


//--------------------- Plot Pixel Function ---------------------//
void plot_pixel(int x, int y, short int color) {
	
	volatile short int *one_pixel_address;
    
	one_pixel_address = pixel_buffer_start + (y << 10) + (x << 1);
	*one_pixel_address = color;
}


//------------------------ Vsync Function -----------------------//
void wait_for_vsync() {
	
	// Store 1 to buffer register, wait until S bit turns off
	*buffer_reg = 1;
	
	int status = *(buffer_reg + 3);
	
	while ((status & 0x01) != 0) {
		status = *(buffer_reg + 3);
	}
}


//--------------------- Init Tiles Function ---------------------//
void init_tiles(bool *active_tile, int *location_tile, int *value_tile, int *shift_offset_x, int *shift_offset_y, bool *tile_moving, bool *tile_merged, int *stop_moving_next_cycle) {
	
	// Activate first tile, deactivate the rest
	for (int tile_id = 0; tile_id < 16; tile_id++) {
		
		if (tile_id == 0) {
			activate_tile(active_tile, location_tile, value_tile, shift_offset_x, shift_offset_y, tile_moving, tile_merged, stop_moving_next_cycle, tile_id);
		}
		
		else {
			active_tile[tile_id] = false;
		}
	}
}


//------------------- Activate Tile Function --------------------//
void activate_tile(bool *active_tile, int *location_tile, int *value_tile, int *shift_offset_x, int *shift_offset_y, bool *tile_moving, bool *tile_merged, int *stop_moving_next_cycle, int tile_id) { 
	
	int rand_location;
	int rand_value;
	
	// Set random seed, pick random location and value for tile
	srand(time(NULL));
	
	rand_location = rand() % 16;
	rand_value = rand() % 2 == 0 ? 2 : 4;
	
	// Ensure random location is not already taken
	while (check_location_taken(active_tile, location_tile, rand_location)) {
		rand_location++;
		if (rand_location == 16) {
			rand_location = 0;
		}
	}
	
	// Turn on tile, set location, value, etc.
	active_tile[tile_id] = true;
	location_tile[tile_id] = rand_location;
	value_tile[tile_id] = rand_value;
	shift_offset_x[tile_id] = 0;
	shift_offset_y[tile_id] = 0;
	tile_moving[tile_id] = false;
	tile_merged[tile_id] = false;
	stop_moving_next_cycle[tile_id] = 0;
}


//----------------- Check Location Taken Function ---------------//
bool check_location_taken(bool *active_tile, int *location_tile, int location) {
	
	// If an active tile already occupies location, return true
	for (int tile_id = 0; tile_id < 16; tile_id++) {
		if (active_tile[tile_id] && location == location_tile[tile_id]) {
				return true;
		}
	}
	
	// Else return false
	return false;
}


//---------------------- Move Tile Function ---------------------//
void move_tiles(bool *active_tile, int *location_tile, int *value_tile, int *shift_offset_x, int *shift_offset_y, int *shift_offset_x_prev, int *shift_offset_y_prev, bool *tile_moving, bool *tile_merged, int *stop_moving_next_cycle, int direction) {
	
	// Directions: imagine going counter-clockwise starting at 0 degrees //
	// 0 = right
	// 1 = up
	// 2 = left
	// 3 = down
	
	// If direction == 4, no movement, return
	if (!currently_moving && direction == 4) {
		return;
	}
	
	// If not currently moving but key has been pressed, store direction in direction_store and start moving
	else if (!currently_moving && direction != 4) {
		direction_store = direction;
		currently_moving = true;
	}
	
	// If movement...
	int x_offset_inc;
	int	y_offset_inc;
	int	location_inc;
	
	// Right
	if (direction_store == 0) {
		x_offset_inc = 4;
		y_offset_inc = 0;
		location_inc = 1;
	}
	
	// Up
	if (direction_store == 1) {
		x_offset_inc = 0;
		y_offset_inc = -4;
		location_inc = -4;
	}
	
	// Left
	if (direction_store == 2) {
		x_offset_inc = -4;
		y_offset_inc = 0;
		location_inc = -1;
	}
	
	// Down
	if (direction_store == 3) {
		x_offset_inc = 0;
		y_offset_inc = 4;
		location_inc = 4;
	}
	
	// Move tile accordingly
	for (int tile_id = 0; tile_id < 16; tile_id++) {

		// If active tile isn't moving, check if it should
		if (active_tile[tile_id] && !tile_moving[tile_id]) {
			tile_moving[tile_id] = tile_should_move(active_tile, location_tile, value_tile, tile_moving, tile_merged, stop_moving_next_cycle, location_inc, tile_id, location_tile[tile_id]);
		}

		// If active tile already moving, keep moving to location
		if (active_tile[tile_id] && tile_moving[tile_id]) {
			shift_offset_x_prev[tile_id] = shift_offset_x[tile_id];
			shift_offset_y_prev[tile_id] = shift_offset_y[tile_id];
			shift_offset_x[tile_id] += x_offset_inc;
			shift_offset_y[tile_id] += y_offset_inc;
		}

		// If done moving, check for merge, update location
		if (shift_offset_x[tile_id] >= 53 || shift_offset_x[tile_id] <= -53 ||
		    shift_offset_y[tile_id] >= 53 || shift_offset_y[tile_id] <= -53) {
			
			location_tile[tile_id] += location_inc;
			tile_moving[tile_id] = tile_should_move(active_tile, location_tile, value_tile, tile_moving, tile_merged, stop_moving_next_cycle, location_inc, tile_id, location_tile[tile_id]);
			stop_moving_next_cycle[tile_id] = 1;
			shift_offset_x[tile_id] = 0;
			shift_offset_y[tile_id] = 0;
			shift_offset_x_prev[tile_id] = 0;
			shift_offset_y_prev[tile_id] = 0;
			
			// If tile in the way, check whether it merges or not
			for (int tile_id_check = 0; tile_id_check < 16; tile_id_check++) {
				
				// If location matches, merge tiles (must have same value)
				if (active_tile[tile_id_check] && location_tile[tile_id_check] == location_tile[tile_id] && tile_id != tile_id_check) {
					merge_tiles(active_tile, location_tile, value_tile, tile_merged, tile_id_check, tile_id);
					break;
				}
			}
		}
	}
}


//------------------ Tile Should Move Function ------------------//
bool tile_should_move(bool *active_tile, int *location_tile, int *value_tile, bool *tile_moving, bool *tile_merged, int *stop_moving_next_cycle, int location_inc, int tile_id, int location) {
	
	bool should_move = false;
	
	if (tile_merged[tile_id]) {
		return should_move;
	}
	
	// If tile not at edge
	if ((direction_store == 0 && (location + 1) % 4 != 0) ||
		(direction_store == 1 && location > 3) ||
		(direction_store == 2 && location % 4 != 0) ||
		(direction_store == 3 && location < 12)) {
		
		should_move = true;
		
		// If tile in the way, check whether it merges or not
		for (int tile_id_check = 0; tile_id_check < 16; tile_id_check++) {
			if (active_tile[tile_id_check] && location_tile[tile_id_check] == location_tile[tile_id] + location_inc && tile_id != tile_id_check) {

				// If tiles don't match, don't move (else move and merge)
				if (value_tile[tile_id_check] != value_tile[tile_id] && 
					((stop_moving_next_cycle[tile_id_check] > 0 && !tile_should_move(active_tile, location_tile, value_tile, tile_moving, tile_merged, stop_moving_next_cycle, location_inc, tile_id_check, location_tile[tile_id_check])) ||
					 (stop_moving_next_cycle[tile_id_check] < 1 && !tile_should_move(active_tile, location_tile, value_tile, tile_moving, tile_merged, stop_moving_next_cycle, location_inc, tile_id_check, location_tile[tile_id_check] + location_inc)) ||
					 (!tile_moving[tile_id_check] && !tile_should_move(active_tile, location_tile, value_tile, tile_moving, tile_merged, stop_moving_next_cycle, location_inc, tile_id_check, location_tile[tile_id_check])))) {
					should_move = false;
					break;
				}
			}
		}
	}
	
	return should_move;
}	
	

//--------------------- Merge Tiles Function --------------------//
void merge_tiles(bool *active_tile, int *location_tile, int *value_tile, bool *tile_merged, int tile_id_1, int tile_id_2) {
	
	// If tiles match, double value and deactivate second tile
	if (value_tile[tile_id_1] == value_tile[tile_id_2]) {
		
		value_tile[tile_id_1] *= 2;
		tile_merged[tile_id_1] = true;
		active_tile[tile_id_2] = false;
		audio_player(tile_collision_sound);
		current_score += value_tile[tile_id_1];
	}
}


//--------------------- Erase Tiles Function --------------------//
void erase_tiles(bool *active_tile, int *location_tile, int *shift_offset_x_prev, int *shift_offset_y_prev, bool *tile_moving, int direction) {
	
	int x_start;
	int y_start;
	int color = 0x0000;
		
	for (int tile_id = 0; tile_id < 16; tile_id++) {

		if (active_tile[tile_id] && tile_moving[tile_id]) {
			//(shift_offset_x_prev[tile_id] != 0 || shift_offset_y_prev[tile_id] != 0)

			// Moving right
			if (direction_store == 0) {

				// Set starting point for erase
				x_start = 12 + ((location_tile[tile_id] % 4) * 53 + shift_offset_x_prev[tile_id] - 4);
				y_start = 17 + ((location_tile[tile_id] / 4) * 53) + shift_offset_y_prev[tile_id];
				int extra = 8;
				
				// Erase extra pixels
				if (shift_offset_x_prev[tile_id] >= 44) {
					extra = 12;
				}

				for (int x = x_start; x <= x_start + extra; x++) {
					for (int y = y_start; y <= y_start + 46; y++) {

						// Assemble color from background image array
						color = 0x0000;
						color = color | background[(320 * 2 * y) + (2 * x) + 1]; // First 8 bits
						color = color << 8; // Bit shift
						color = color | background[(320 * 2 * y) + (2 * x)]; // Last 8 bits

						plot_pixel(x, y, color);				
					}
				}
			}

			// Moving up
			if (direction_store == 1) {

				// Set starting point for erase
				x_start = 12 + ((location_tile[tile_id] % 4) * 53 + shift_offset_x_prev[tile_id]);
				y_start = 17 + ((location_tile[tile_id] / 4) * 53) + shift_offset_y_prev[tile_id] + 49;
				int extra = 8;
				
				// Erase extra pixels
				if (shift_offset_y_prev[tile_id] <= -44) {
					extra = 12;
				}
				
				for (int x = x_start; x <= x_start + 46; x++) {
					for (int y = y_start; y >= y_start - extra; y--) {

						// Assemble color from background image array
						color = 0x0000;
						color = color | background[(320 * 2 * y) + (2 * x) + 1]; // First 8 bits
						color = color << 8; // Bit shift
						color = color | background[(320 * 2 * y) + (2 * x)]; // Last 8 bits

						plot_pixel(x, y, color);
					}
				}
			}

			// Moving left
			if (direction_store == 2) {

				// Set starting point for erase
				x_start = 12 + ((location_tile[tile_id] % 4) * 53 + shift_offset_x_prev[tile_id]) + 49;
				y_start = 17 + ((location_tile[tile_id] / 4) * 53) + shift_offset_y_prev[tile_id];
				int extra = 8;
				
				// Erase extra pixels
				if (shift_offset_x_prev[tile_id] <= -44) {
					extra = 12;
				}

				for (int x = x_start; x >= x_start - extra; x--) {
					for (int y = y_start; y <= y_start + 46; y++) {

						// Assemble color from background image array
						color = 0x0000;
						color = color | background[(320 * 2 * y) + (2 * x) + 1]; // First 8 bits
						color = color << 8; // Bit shift
						color = color | background[(320 * 2 * y) + (2 * x)]; // Last 8 bits

						plot_pixel(x, y, color);
					}
				}
			}

			// Moving down
			if (direction_store == 3) {

				// Set starting point for erase
				x_start = 12 + ((location_tile[tile_id] % 4) * 53 + shift_offset_x_prev[tile_id]);
				y_start = 17 + ((location_tile[tile_id] / 4) * 53) + shift_offset_y_prev[tile_id] - 4;
				int extra = 8;
				
				// Erase extra pixels
				if (shift_offset_y_prev[tile_id] >= 44) {
					extra = 12;
				}

				for (int x = x_start; x <= x_start + 46; x++) {
					for (int y = y_start; y <= y_start + extra; y++) {

						// Assemble color from background image array
						color = 0x0000;
						color = color | background[(320 * 2 * y) + (2 * x) + 1]; // First 8 bits
						color = color << 8; // Bit shift
						color = color | background[(320 * 2 * y) + (2 * x)]; // Last 8 bits

						plot_pixel(x, y, color);
					}
				}
			}
		}
	}
}


//---------------------- Draw Tile Function ---------------------//
void draw_tiles(const bool *active_tile, const int *value_tile, const int *location_tile, int *shift_offset_x, int *shift_offset_y, bool *tile_moving, bool *tile_merged, int *stop_moving_next_cycle) {
	
	int x_start;
	int y_start;
	short int color = 0x0000;
	
	// Iterate through tiles, draw active ones
	for (int tile_id = 0; tile_id < 16; tile_id++) {
		
		if (active_tile[tile_id] && (tile_moving[tile_id] || !currently_moving || tile_merged[tile_id] || stop_moving_next_cycle[tile_id] > 0)) {
			
			// Set top left corner of tile
			x_start = 12 + ((location_tile[tile_id] % 4) * 53);
			y_start = 17 + ((location_tile[tile_id] / 4) * 53);
			
			// Draw 46 x 46 tile
			for (int x = 0; x < 46; x++) {
				for (int y = 0; y < 46; y++) {
					
					// 2 Tile
					if (value_tile[tile_id] == 2) {
						color = 0x0000;
            			color = color | tile2[(46 * 2 * y) + (2 * x) + 1]; // First 8 bits
            			color = color << 8; // Bit shift
            			color = color | tile2[(46 * 2 * y) + (2 * x)]; // Last 8 bits            
					}
					
					// 4 Tile
					if (value_tile[tile_id] == 4) {
						color = 0x0000;
            			color = color | tile4[(46 * 2 * y) + (2 * x) + 1]; // First 8 bits
            			color = color << 8; // Bit shift
            			color = color | tile4[(46 * 2 * y) + (2 * x)]; // Last 8 bits            
					}
					
					// 8 Tile
					if (value_tile[tile_id] == 8) {
						color = 0x0000;
            			color = color | tile8[(46 * 2 * y) + (2 * x) + 1]; // First 8 bits
            			color = color << 8; // Bit shift
            			color = color | tile8[(46 * 2 * y) + (2 * x)]; // Last 8 bits            
					}
					
					// 16 Tile
					if (value_tile[tile_id] == 16) {
						color = 0x0000;
            			color = color | tile16[(46 * 2 * y) + (2 * x) + 1]; // First 8 bits
            			color = color << 8; // Bit shift
            			color = color | tile16[(46 * 2 * y) + (2 * x)]; // Last 8 bits            
					}
					
					// 32 Tile
					if (value_tile[tile_id] == 32) {
						color = 0x0000;
            			color = color | tile32[(46 * 2 * y) + (2 * x) + 1]; // First 8 bits
            			color = color << 8; // Bit shift
            			color = color | tile32[(46 * 2 * y) + (2 * x)]; // Last 8 bits            
					}
					
					// 64 Tile
					if (value_tile[tile_id] == 64) {
						color = 0x0000;
            			color = color | tile64[(46 * 2 * y) + (2 * x) + 1]; // First 8 bits
            			color = color << 8; // Bit shift
            			color = color | tile64[(46 * 2 * y) + (2 * x)]; // Last 8 bits            
					}
					
					// 128 Tile
					if (value_tile[tile_id] == 128) {
						color = 0x0000;
            			color = color | tile128[(46 * 2 * y) + (2 * x) + 1]; // First 8 bits
            			color = color << 8; // Bit shift
            			color = color | tile128[(46 * 2 * y) + (2 * x)]; // Last 8 bits            
					}
					
					// 256 Tile
					if (value_tile[tile_id] == 256) {
						color = 0x0000;
            			color = color | tile256[(46 * 2 * y) + (2 * x) + 1]; // First 8 bits
            			color = color << 8; // Bit shift
            			color = color | tile256[(46 * 2 * y) + (2 * x)]; // Last 8 bits            
					}
					
					// 512 Tile
					if (value_tile[tile_id] == 512) {
						color = 0x0000;
            			color = color | tile512[(46 * 2 * y) + (2 * x) + 1]; // First 8 bits
            			color = color << 8; // Bit shift
            			color = color | tile512[(46 * 2 * y) + (2 * x)]; // Last 8 bits            
					}
					
					// 1024 Tile
					if (value_tile[tile_id] == 1024) {
						color = 0x0000;
            			color = color | tile1024[(46 * 2 * y) + (2 * x) + 1]; // First 8 bits
            			color = color << 8; // Bit shift
            			color = color | tile1024[(46 * 2 * y) + (2 * x)]; // Last 8 bits            
					}
					
					// 2048 Tile
					if (value_tile[tile_id] == 2048) {
						color = 0x0000;
            			color = color | tile2048[(46 * 2 * y) + (2 * x) + 1]; // First 8 bits
            			color = color << 8; // Bit shift
            			color = color | tile2048[(46 * 2 * y) + (2 * x)]; // Last 8 bits            
					}
					
					// 4096 Tile
					if (value_tile[tile_id] == 4096) {
						color = 0x0000;
            			color = color | tile4096[(46 * 2 * y) + (2 * x) + 1]; // First 8 bits
            			color = color << 8; // Bit shift
            			color = color | tile4096[(46 * 2 * y) + (2 * x)]; // Last 8 bits            
					}
					
					// 8192 Tile
					if (value_tile[tile_id] == 8192) {
						color = 0x0000;
            			color = color | tile8192[(46 * 2 * y) + (2 * x) + 1]; // First 8 bits
            			color = color << 8; // Bit shift
            			color = color | tile8192[(46 * 2 * y) + (2 * x)]; // Last 8 bits            
					}
					
					// Background Tile
					if (value_tile[tile_id] == -1) {
						color = 0x0000;
            			color = color | tile8192[(46 * 2 * y) + (2 * x) + 1]; // First 8 bits
            			color = color << 8; // Bit shift
            			color = color | tile8192[(46 * 2 * y) + (2 * x)]; // Last 8 bits            
					}
					
					plot_pixel(x_start + x + shift_offset_x[tile_id], y_start + y + shift_offset_y[tile_id], color);
				}
			}
				
		}						
	}
}

//------------------- Choose Direction Function -----------------//
int chooseDirection(int dirCode) { 
	
	// Up
	if (dirCode == 0x75) {
		return 1;
	}
	
	// Left
	else if (dirCode == 0x6B) {   
		return 2;
	}
	
	// Down
	else if (dirCode == 0x72) {
		return 3;
	}
	
	// Right
	else if (dirCode == 0x74) {
		return 0;
	}
	
	// No tile movement
	else {
		return 4;
	}
}

//-------------- Game Over Screen And Restart Function --------------//
void gameIsOver(bool *active_tile, int *location_tile, int *value_tile, int *shift_offset_x, int *shift_offset_y, bool *tile_moving, bool *tile_merged, int *stop_moving_next_cycle) {
	
	// Draw Game Over screen and Score and Best Score
	short int color;

    // Iterate through all points and draw game over background
    for (int x = 0; x < 512; x++) {
        for (int y = 0; y < 240; y++) {

			// Assemble color from background image array
			color = 0x0000;
			color = color | gameOverBackground[(320 * 2 * y) + (2 * x) + 1]; // First 8 bits
			color = color << 8; // Bit shift
			color = color | gameOverBackground[(320 * 2 * y) + (2 * x)]; // Last 8 bits

            // Plot pixel
            plot_pixel(x, y, color);
        }
    }

	// used code in draw_score but reformated it to show best score when game ends
	char digits[5] = {'\0'};
	char bestdigits[5] = {'\0'};
	int curr = current_score;
	int bestcurr = best_score;

	int count = 0;
	int bestcount = 0;
	
	int y = 134;
	int x = 23;
	char* hw = "            ";
	while (*hw) {
		write_char(x, y, *hw);
		write_char(x + 30, y, *hw);
		x++;
		hw++;
	}
	
	while (curr > 0) {
		digits[count] = (char)(curr % 10 + '0');
		curr = curr / 10;
		count++; 
	}
	
	while (bestcurr > 0) {
		digits[bestcount] = (char)(bestcurr % 10 + '0');
		bestcurr = bestcurr / 10;
		bestcount++; 
	}

	x = 23;
	for (int i = count-1; i >= 0; i--) {
		write_char(x, y, digits[i]);
		x++;
	}

	x = 53;
	if (best_score == 0) {
		x = 55;
		write_char(x, y, '0');
	}
	else {
		for (int i = bestcount-1; i >= 0; i--) {
			write_char(x, y, digits[i]);
			x++;
		}
	}

	// Wait for input key being pressed
	while (1) {
		// Declare keyboard variables
		int PS2_data;

		// Poll keyboard for input 
		PS2_data = keyboard->RB;
		
		// If input detected, make necessary reset configurations and go back to main method
		if ((PS2_data & 0xFF00) != 0) {
			// Set best score to current score if greater
			if (current_score > best_score) {
				best_score = current_score;
			}
			// Initialize tiles again
			init_tiles(active_tile, location_tile, value_tile, shift_offset_x, shift_offset_y, tile_moving, tile_merged, stop_moving_next_cycle);
			return; // Return control back to while loop in main method
		}
	}
}


//--------------------- Score Drawing Function ---------------------//
void draw_score() {
	char digits[5] = {'\0'};
	int curr = current_score;
	int count = 0;
	
	int x = 66;
	char* hw = "            ";
	while (*hw) {
		write_char(x, 28, *hw);
		x++;
		hw++;
	}
	
	if (current_score == 0) {
		x = 66;
		write_char(x, 28, '0');
		return;
	}
	
	while (curr > 0) {
		digits[count] = (char)(curr % 10 + '0');
		curr = curr / 10;
		count++; 
	}
	
	x = 66;
	for (int i = count-1; i >= 0; i--) {
		write_char(x, 28, digits[i]);
		x++;
	}
}

//--------------------- BestScore Drawing Function ---------------------//
void draw_bestScore() {
	char digits[5] = {'\0'};
	int curr = best_score;
	int count = 0;
	
	int x = 66;
	char* hw = "            ";
	while (*hw) {
		write_char(x, 38, *hw);
		x++;
		hw++;
	}
	
	if (best_score == 0) {
		x = 66;
		write_char(x, 38, '0');
		return;
	}
	
	while (curr > 0) {
		digits[count] = (char)(curr % 10 + '0');
		curr = curr / 10;
		count++; 
	}
	

	x = 66;
	for (int i = count-1; i >= 0; i--) {
		write_char(x, 38, digits[i]);
		x++;
	}
}


//------------------ Check Game Over Function ------------------//
bool checkGameOver(bool *active_tile, int *location_tile, int *value_tile) {
	// Check if there are any empty spaces first
	for (int i = 0; i < 16; i++) {
		// if a tile isnt active then board has empty spaces so return false
		if (active_tile[i] == false) {
			return false;
		}
	}

	// If not empty spaces, check if there are any adjacent tiles with same value
	int up_loc = 16;
	int down_loc = 16;
	int left_loc = 16;
	int right_loc = 16;
	int loc;
	for (int i = 0; i < 16; i++) {
		loc = location_tile[i];
		up_loc = location_tile[i] - 4;
		down_loc = location_tile[i] + 4; 
		left_loc = location_tile[i] - 1;
		right_loc = location_tile[i] + 1;
		int value_og = value_tile[i];
		for (int z = 0; z < 16; z++) {
			int loc_checking = location_tile[z];
			int value_checking = value_tile[z];
			//  If value of tile we are checking isnt equal to value of original (OG) tile, no point seeing if it is adjacent to OG tile
			if (value_checking != value_og) {
				continue;
			}
			// If location of tile we are checking is equal to the location above or below the OG tile
			if (loc_checking == up_loc || loc_checking == down_loc) {
				return false; 
			}
			// If location of OG tile is NOT 0, 4 , 8, or 12 and the location of tile we are checking is to the left of original
			// There must be a valid move
			if (loc_checking == left_loc && (loc != 0 || loc != 4 || loc != 8 || loc != 12)) {
				return false;
			} 
			// If location of OG tile is NOT  3, 7, 12, or 15 and the location of tile we are checking is to the right of original
			// There must be a valid move
			if (loc_checking == right_loc && (loc != 3 || loc != 7 || loc != 12 || loc != 15)) {
				return false;
			}
		}
	}
	// If no empty spaces and no adjacent tiles with same value return true as game is over
	return true;
}


//-------------------- Audio Player Function --------------------//
void audio_player(const int *audio) {
		
	audiop->control = 0x8; // clear the output FIFOs
	audiop->control = 0x0; // resume output conversion
	
	// While not at end of audio array
	for (int i = 0; i < 1875; i++) {
		
		// If space in output FIFOs, write out and get next sample
		while (audiop->wsrc == 0);
		
		audiop->ldata = audio[i];
		audiop->rdata = audio[i];
		//printf("Sample: %d\n", audio[i]);
	} 
}

//-------------------- Char Write Function --------------------//
void write_char(int x, int y, char c) {
  // VGA character buffer
  volatile char * character_buffer = (char *) (0x09000000 + (y<<7) + x);
  *character_buffer = c;
}	