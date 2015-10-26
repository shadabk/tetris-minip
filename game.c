#include <stdio.h>
#include <ncurses.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#define WELL_HEIGHT	22
#define WELL_WIDTH	32
#define NO_LEVELS	10
#define NEXT_HEIGHT	10
#define NEXT_WIDTH	20

/* Points are stored in three parts : points, lines, levels
 *
 * DOT is the structure defining the blocks
 */
typedef struct points {
	unsigned int points;
	unsigned int lines;
	unsigned int level;
} POINTS;


typedef struct dot {
	unsigned char y;
	unsigned char x;
} DOT;


/* well_data defines the occupancy of well  in terms of a character array.
 * delay[] is the time delay in microseconds depending on the level no.
 */
char *well_data;
int delay[NO_LEVELS] = {1000000, 770000, 593000, 457000, 352000, 271000, 208000, 160000, 124000, 95000};
WINDOW *gamew, *wellw, *statw, *nextw, *instw;


/* block_data[types][orientations][dots]
 * defining the blocks
 * (y, x)
 */
const DOT block_data[7][4][4] =
{
	{
		{{2,0},{2,1},{2,2},{2,3}},	/*      */
		{{0,1},{1,1},{2,1},{3,1}},	/*      */
		{{2,0},{2,1},{2,2},{2,3}},	/* XXXX */
		{{0,1},{1,1},{2,1},{3,1}}	/*      */
	},
	{	
		{{1,1},{2,1},{1,2},{2,2}},	/*      */
		{{1,1},{2,1},{1,2},{2,2}},	/*  XX  */
		{{1,1},{2,1},{1,2},{2,2}},	/*  XX  */
		{{1,1},{2,1},{1,2},{2,2}}	/*      */
	},
	{
		{{1,0},{1,1},{1,2},{2,2}},	/*      */
		{{2,0},{0,1},{1,1},{2,1}},	/* XXX  */
		{{0,0},{1,0},{1,1},{1,2}},	/*   X  */
		{{0,1},{1,1},{2,1},{0,2}}	/*      */
	},
	{
		{{1,0},{2,0},{1,1},{1,2}},	/*      */
		{{0,0},{0,1},{1,1},{2,1}},	/* XXX  */
		{{1,0},{1,1},{0,2},{1,2}},	/* X    */
		{{0,1},{1,1},{2,1},{2,2}}	/*      */
	},
	{
		{{1,0},{1,1},{2,1},{2,2}},	/*      */
		{{1,0},{2,0},{0,1},{1,1}},	/* XX   */
		{{1,0},{1,1},{2,1},{2,2}},	/*  XX  */
		{{1,0},{2,0},{0,1},{1,1}}	/*      */
	},
	{	
		{{2,0},{1,1},{2,1},{1,2}},	/*      */
		{{0,0},{1,0},{1,1},{2,1}},	/*  XX  */
		{{2,0},{1,1},{2,1},{1,2}},	/* XX   */
		{{0,0},{1,0},{1,1},{2,1}}	/*      */
	},
	{
		{{1,0},{1,1},{2,1},{1,2}},	/*  X   */
		{{1,0},{0,1},{1,1},{2,1}},	/* XXX  */
		{{1,0},{0,1},{1,1},{1,2}},	/*      */
		{{0,1},{1,1},{2,1},{1,2}}	/*      */
	}
};		


// Converts (y, x) cordinates into pointer to particular location in well_data
char *yx2pointer(int y, int x) {
	return well_data + (y * WELL_WIDTH) + x;
}


//Redraws stat window
void update_stat(POINTS points) {
	box(statw, ACS_VLINE, ACS_HLINE);
	mvwprintw(statw, 2, 5, "Score : %d", points.points);
	mvwprintw(statw, 4, 5, "Lines : %d", points.lines);
	mvwprintw(statw, 6, 5, "Level : %d", points.level);
	mvwprintw(statw, 0, 6 , "# STATS #");
	wrefresh(statw);
}


//updates the instructions window
void update_inst() {
	mvwprintw(instw, 1, 2, "Controls :");
	mvwprintw(instw, 3, 2, "Move Left -> j");
	mvwprintw(instw, 4, 2, "Move Right -> l");
	mvwprintw(instw, 5, 2, "Move down -> k");
	mvwprintw(instw, 6, 2, "Rotate -> i");
	mvwprintw(instw, 7, 2, "Pause Game -> p");
	mvwprintw(instw, 8, 2, "Quit game -> v");
	box(instw, ACS_VLINE, ACS_HLINE);	
	wrefresh(instw);
}


//Redraws well
void update_well(int row) {
	int y = 0, x = 0, i;
	for(x = 0; x < WELL_WIDTH; x++) {
		for(y = 0; y < WELL_HEIGHT; y++) {
			if(*yx2pointer(y, x) == 1) {
				wattrset(wellw, COLOR_PAIR(1));
				mvwprintw(wellw, y, 2 * x, "  ");
			}
			else {
				wattrset(wellw, COLOR_PAIR(9));
				mvwprintw(wellw, y, 2 * x, "  ");
			}
		}
	}
	wattroff(wellw, COLOR_PAIR(1));
	box(wellw, ACS_VLINE, ACS_HLINE);
	wrefresh(wellw);
}


// Removes the completely filled row from well
void remove_row(int row) {

	int i, j, k, x = 0, y = row;	
	for(x = 0; x < WELL_WIDTH; x++)
		*yx2pointer(y, x) = 0;
	
	for(i = row - 1; i > 0; i--) {
		for(x = 0; x < WELL_WIDTH/2; x++)
			*yx2pointer(i + 1, x) = *yx2pointer(i, x);
	}
	
	update_well(row);
}


// Checks if a particular row is filled
int check_row(int row) {
	int i, j, x = 0, y = row;
	for(x = 1; x < WELL_WIDTH/2; x++) {
		if(*yx2pointer(y, x) == 0)
			return 0;
	}
	return 1;
}


/* After fixing each block, checks if any lines are complete
 * If lines are complete, calls remove_row
 * Increases score according to number of lines removed in one turn.
 */ 
POINTS *check_lines(int start) {
	
	int y, count_row = 0;
	POINTS *temp = (POINTS *)malloc(sizeof(POINTS));
	temp->points = 0;
	temp->lines = 0;

	for (y = start; y < start + 4; y++)
		if(check_row(y)) {
			remove_row(y);
			count_row++;
		}
	switch(count_row) {

		case 1 : temp->points = 10;
			 temp->lines = 1;
			 break;

		case 2 : temp->points = 25;
			 temp->lines = 2;
			 break;

		case 3 : temp->points = 40;
			 temp->lines = 3;
			 break;

		case 4 : temp->points = 60;
			 temp->lines = 4;
			 break;

		default : break;
	}
	return temp;
}


// Fixes the block in a particular location in well
void fix_block(int y, int x, int type,int orient) {
	int i;
	DOT dot;
	for (i = 0; i < 4; i++) {
		dot = block_data[type][orient][i];
		*yx2pointer(y + dot.y, x + dot.x) = 1;
	}
}


//checks if the block can be moved to the next location
int check_pos(int y, int x, int type, int orient) {
	int i;
	DOT dot;
	for(i = 0; i < 4; i++) {
		dot = block_data[type][orient][i];
		if ((y + dot.y > WELL_HEIGHT - 1)		||
		    (x + dot.x < 1) 				||
		    (x + dot.x > WELL_WIDTH/2 - 1)		||
		    ((*yx2pointer(y + dot.y, x + dot.x) > 0))
		   )
		        	return 0;
	}
	return 1;	
}


// Draws or erases the block depending on the delete value
void draw_block(WINDOW *win, int y, int x, int type, int orient, char delete) {	
	int i;
	DOT dot; 
		
	for (i = 0; i < 4; i++) {
		dot = block_data[type][orient][i];
		wattron(win, COLOR_PAIR(delete ? 9 : 1));
		mvwprintw(win, y + dot.y, 2*(x + dot.x), "  ");
	}
	if (delete == 0)
		wrefresh(win);
	wattroff(win, COLOR_PAIR(delete ? 9 : 1));
	box(win, ACS_VLINE, ACS_HLINE);
	wrefresh(win);
}

//Redraws the window showing next block
void update_next(int next, int del) {
	mvwprintw(nextw, 0, 4, "NEXT BLOCK");
	if(del == 1)
		draw_block(nextw, 3, 3, next, 0, 1);
	if(del == 0)
		draw_block(nextw, 3, 3, next, 0, 0);
	box(nextw, ACS_VLINE, ACS_HLINE);	
	wrefresh(nextw);

}


/* Drops block till it reaches either well floor or another line
 * Uses time variables
 * Controls the block while it falls
 */
int drop_block(int type, int level) {
	int mid = WELL_WIDTH / 2 - 2;
	int y = 0;
	int x = mid/2;
	int orient = 0;
	char ch;
	fd_set t1, t2;
	struct timeval timeout;
	int sel_ret;
	if (0 == check_pos(y, x, type, orient))			//check if game over
		return -1;

	timeout.tv_sec = 0;
	timeout.tv_usec = delay[level];

	FD_ZERO(&t1);			//initialise
	FD_SET(0, &t1);
	
	draw_block(wellw, y, x, type, orient, 0);
	
	while(1) {
		t2 = t1;
		sel_ret = select(FD_SETSIZE, &t2, (fd_set *) 0, (fd_set *) 0, &timeout);
	
		ch = getch();
		switch (ch) {

			case 'j':							//-------------------------------move left
				if(check_pos(y, x - 1, type, orient)) {
					draw_block(wellw, y, x, type, orient, 1);
					draw_block(wellw, y, --x, type, orient, 0);
				}
				break;

			case 'l':							//-------------------------------move right
				if(check_pos(y, x + 1, type, orient)) {
					draw_block(wellw, y, x, type, orient, 1);
					draw_block(wellw, y, ++x, type, orient, 0);
				}
				break;

			case 'i':							//-----------------------------------rotate
				if (check_pos(y, x, type,	orient + 1 == 4 ? 0 : orient + 1)) {
					draw_block(wellw, y, x, type, orient, 1);
					++orient == 4 ? orient = 0 : 0;
					draw_block(wellw, y, x, type, orient, 0);
				}
				break;

			case 'k':		//------------------------------------------------------------------------move down
				sel_ret = 0;
				break;

			case 'v':		//-----------------------------------------------------------------------------quit
				return -1;
		}
		
		if(sel_ret == 0) {
			if(check_pos(y + 1, x, type, orient)) {			//----------------------------------moves block down
				draw_block(wellw, y, x, type, orient, 1);
				draw_block(wellw, ++y, x, type, orient, 0);
			}
			else {							//--------------------------------fix block in place
				fix_block(y, x, type, orient);
				return y;
			}		
			timeout.tv_sec = 0;
			timeout.tv_usec = delay[level];
		}
	}
}


/* Actual loop of the game.
 * initialises well_data, calls drop_block, sets score
 *
 */
POINTS play_game(int level) {

	POINTS points, *temp;
	int i, curr, next, y;

	well_data = (unsigned char *)malloc(WELL_HEIGHT * WELL_WIDTH);
	for(i = 0; i < (WELL_HEIGHT * WELL_WIDTH); i++)
		well_data[i] = 0;

	points.points = 0;
	points.lines = 0;
	points.level = 0;
	
	temp = &points;

	curr = rand() % 7;

	update_stat(points);
	
	while(y != -1) {
		update_next(curr, 1);
		next = rand() % 7;
		update_next(next, 0);
		y = drop_block(curr, points.level);
		if(y > 0) 
			temp = check_lines(y);
		points.points = points.points + temp->points;
		points.lines = points.lines + temp->lines;
		update_stat(points);
		curr = next;
		update_inst();

		if(y == -1) {
			delwin(wellw);
			endwin();
		}	
	}
	return points;
}


//initiliases the windows for the first time
void init_windows() {
	POINTS points;
	points.points = 0;
	points.lines = 0;
	points.level = 0;
	int GAME_HEIGHT = WELL_HEIGHT + 4, GAME_WIDTH = WELL_WIDTH + NEXT_WIDTH + 4;
	int STAT_HEIGHT = WELL_HEIGHT / 2;
	gamew = newwin( GAME_HEIGHT, GAME_WIDTH, 0, 0);
	wellw = newwin( WELL_HEIGHT + 1, WELL_WIDTH + 2, 1, 1 );
	statw = newwin( STAT_HEIGHT, NEXT_WIDTH, NEXT_HEIGHT + 2, WELL_WIDTH + 3 );
	nextw = newwin( NEXT_HEIGHT, NEXT_WIDTH, 2, WELL_WIDTH + 3 );
	instw = newwin( WELL_HEIGHT + 1, 20, 1, WELL_WIDTH + NEXT_WIDTH + 4 );
	update_stat(points);
	update_next(1, 1);
	update_inst();
	box(statw, ACS_VLINE, ACS_HLINE);
	wrefresh( wellw );
	wrefresh( statw );
}

int main() {
	int ch, level;
	keypad(gamew, TRUE);
	initscr();
	while(1) {
		int play = 1;
		if(menu() == 0) {delwin( gamew );
			endwin();
			return 0;}
		init_windows();
		
		start_color();
		init_pair(9, COLOR_BLACK, COLOR_BLACK);
		init_pair(1, COLOR_BLACK, COLOR_RED);
	
		nodelay(stdscr, TRUE);
		while (play) {
			level = 0;
			play = (play_game(level)).points;
			//play = show_score(points, use_highscore);
		}
		clear();
		refresh();
	}
	delwin( gamew );
	endwin();
}
