#include <ncurses.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "entry.h"
#include "group.h"
#include "read_cfg.h"
#define MAX_LEN 6
#define BUF_LEN 1024

void draw_title();
void draw_win(WINDOW *new, char *title);
void fill_groups(GROUP **group_arr, int count);
void fill_entries(ENTRY **entry_arr, int count);
char *trim_name(char *name, char *path, int max_len);
void update_groups(int hl_where); //0 = last, 1 = first
void update_entries(int hl_where); //0 = last, 1 = first
void switch_col();
void trav_col(int dir); //0 = down, 1 = up
void launch_entry();
char *compat_convert(char *path, int mode);

static int width;
static int height;
WINDOW *group_win;
WINDOW *entry_win;
WINDOW *info_win;
int g_hover = 0;
int e_hover = 0;
int true_hover = 0; //which column are we currently hovering on? (I think)
GROUP **g;
ENTRY **e;
int g_count;
int e_count;
int g_offset = 0;
int e_offset = 0;

//TODO consider figuring out where some refreshes are unecessary

int main(){
	bool tall = true; //is the window a certain height (tbd what the threshold should be TODO)
	bool wide = true; //is the window a certain width (tbd what the threshold should be TODO)
	int input;

	//Fill Groups
	cfg_interp(); //read the contents of the cfg file
	g = get_groups(); //retrieve results of previous function
	g_count = get_gcount(g); //retrieve number of groups in g

	initscr();
	cbreak();
	keypad(stdscr, true);
	start_color();

	width = getmaxx(stdscr);
	height = getmaxy(stdscr);
	init_pair(0, COLOR_WHITE, COLOR_BLACK);
	init_pair(1, COLOR_BLACK, COLOR_WHITE);
	init_pair(2, COLOR_BLACK, COLOR_YELLOW);
	attron(COLOR_PAIR(0));

	//title at the top (Terminal Media Launcher) (23 chars)
	draw_title();

	//Draw Search Bar 2 spaces (3 spaces if window is big enough) under the title
	move(3, width/4);
	printw("[ Search: ");
	move(3, (width*3)/4);
	printw("]");
	move(3, (width/4)+10);

	//Draw Columns 
	//TODO create conditionals based on size of window)
	group_win = newwin(height-4, width/3, 4, 0);
	entry_win = newwin(height-4, width/3, 4, width/3);
	info_win = newwin(height-4, width/3, 4, (2*width)/3);
	refresh();
	draw_win(group_win, "GROUP");
	draw_win(entry_win, "ENTRY");
	draw_win(info_win, "INFO");
	update_groups(1);

	//start with hover on the first group, draw the entries from the selected group, true_hover is over the groups (rather than the entries)
	update_entries(1);
	move(3, (width/4)+10);

	//drawing is done, now run a while loop to receive input
	while(1){
		input = getch();

		switch(input){
			case 9: //tab key
			case KEY_LEFT:
			case KEY_RIGHT:
				//switch true_hover to look at the other column (TODO this code could use some polish)
				switch_col();
				break;

			case KEY_DOWN:
				trav_col(0);
				break;

			case KEY_UP:
				trav_col(1);
				break;

			case 10: //enter key
				launch_entry();
				break;

			default:
				endwin();
				return 0;

		}

		move(3, (width/4)+10); //reset cursor location to the search bar after any action
	}
	
	endwin();
	return 0;
}

void draw_title(){
	WINDOW *title;

	title = newwin(2, width, 0, 0);
	refresh();
	attron(A_BOLD | A_UNDERLINE);
	move(0, (width-23)/2);
	printw("Terminal Media Launcher");
	attroff(A_BOLD | A_UNDERLINE);
	box(title, 0, 0);
	wrefresh(title);

	return;
}

void draw_win(WINDOW *new, char *title){
	int title_len = strlen(title);

	box(new, 0, 0);
	attron(A_UNDERLINE);
	wmove(new, 0, (new->_maxx - title_len)/2);
	wprintw(new, "%s", title);
	attroff(A_UNDERLINE);
	wrefresh(new);

	return;
}

void fill_groups(GROUP **group_arr, int count){
	int i;
	int max_len = group_win->_maxx-1; //longest possible string length that can be displayed in the window
	int ycoord = 1;
	int max_y = height-5; //last possible slot to draw a group
	char *name;

	for(i = 0+g_offset; i < count; i++){
		if(ycoord >= max_y) break; //already at bottom of terminal window, stop drawing
		name = get_gname(group_arr[i]);

		//the name is too long, take the group to the trimming function
		if(strlen(name) > max_len) name = trim_name(name, get_gname(group_arr[i]), max_len);
		wmove(group_win, ycoord, 1);
		wprintw(group_win, "%s", name);
		ycoord++;
	}

	wrefresh(group_win);
	return;
}

//very similar to the previous function, perhaps they can be combined... (TODO)
void fill_entries(ENTRY **entry_arr, int count){
	int i;
	int max_len = entry_win->_maxx-1; //longest possible string length that can be displayed in the window
	int ycoord = 1;
	int max_y = height-5;
	char *name;

	for(i = 0+e_offset; i < count; i++){
		if(ycoord >= max_y) break; //reached the bottom of the terminal window, stop drawing
		name = get_ename(entry_arr[i]);

		//the name is too long, take the group to the trimming function
		if(strlen(name) > max_len) name = trim_name(name, get_epath(entry_arr[i]), max_len);
		wmove(entry_win, ycoord, 1);
		wprintw(entry_win, "%s", name);
		ycoord++;
	}

	wrefresh(entry_win);
	return;
}

char *trim_name(char *name, char *path, int max_len){
	char *relative;

	//group name and path are equivalent: special procedure
	if(!(strcmp(name, path))){
		//find relative path name
		relative = strrchr(name, '/');
		name = relative+1;
		if(strlen(name) <= max_len) return name;
	}

	name[max_len] = '\0';
	return name;
}

//TODO think about combining this and update_groups
void update_entries(int hl_where){
	int y_hl = (hl_where ? 1 : entry_win->_maxy-1);

	//reset the entry window (including reboxing and redrawing the title)
	wclear(entry_win);
	box(entry_win, 0, 0);
	wmove(entry_win, 0, (entry_win->_maxx - 5)/2);
	wprintw(entry_win, "ENTRY");
	wrefresh(entry_win);

	e_count = get_ecount(g[g_hover]);
	e = get_entries(get_ghead(g[g_hover]), e_count);
	fill_entries(e, e_count);
	mvwchgat(entry_win, y_hl, 1, entry_win->_maxx-1, A_DIM, 1, NULL);

	wrefresh(entry_win);
	return;
}

void update_groups(int hl_where){
	int y_hl = (hl_where ? 1 : group_win->_maxy-1);

	//reset the group window (including reboxing and redrawing the title)
	wclear(group_win);
	box(group_win, 0, 0);
	wmove(group_win, 0, (group_win->_maxx - 5)/2);
	wprintw(entry_win, "GROUP");
	wrefresh(group_win);

	fill_groups(g, g_count);
	mvwchgat(group_win, y_hl, 1, group_win->_maxx-1, A_DIM, 2, NULL);

	wrefresh(group_win);
	return;
}

void switch_col(){
	true_hover = (true_hover+1) % 2;
	if(true_hover){
		mvwchgat(group_win, 1+g_hover, 1, group_win->_maxx-1, A_DIM, 1, NULL); //adjust group light
		mvwchgat(entry_win, 1+e_hover, 1, entry_win->_maxx-1, A_DIM, 2, NULL); //adjust entry light
	}
	else{
		mvwchgat(group_win, 1+g_hover, 1, group_win->_maxx-1, A_DIM, 2, NULL); //adjust group light
		mvwchgat(entry_win, 1+e_hover, 1, entry_win->_maxx-1, A_DIM, 1, NULL); //adjust entry light
	}
	move(3, (width/4)+10);

	wrefresh(group_win);
	wrefresh(entry_win);
	return;
}

//FIXME some issues with cutting off too early
void trav_col(int dir){
	int *focus = (true_hover ? &e_hover : &g_hover); //make it easy to know which column we are looking at
	int *offset = (true_hover ? &e_offset : &g_offset); //this variable is broken
	int count = (true_hover ? e_count : g_count);
	int max_hl = height-2+*offset; //for some reason, this works
	int min_hl = 5+*offset;
	bool oob_flag = false;

	//check if the traversal is valid (i.e. not at top/bottom), exit if not
	//FIXME check that third parameter... might not be right (could cause issues)
	if((dir && !(*focus)) || (!dir && (*focus == count-1))) return;

	//reset previously highlighted entry and group, change focus
	mvwchgat(entry_win, 1+e_hover-e_offset, 1, entry_win->_maxx-1, A_NORMAL, 0, NULL);
	mvwchgat(group_win, 1+g_hover-g_offset, 1, group_win->_maxx-1, A_NORMAL, 0, NULL);
	(dir ? (*focus)-- : (*focus)++);

	//check offsets relating to new highlight, make sure nothing highlight did not go oob
	if(*focus+5 > max_hl){
		*focus--;
		(*offset)++;
		oob_flag = true;
	}
	else if(*focus+5 < min_hl){
		*focus++;
		(*offset)--;
		oob_flag = true;
	}

	//TODO rethink fill_groups (maybe make an update_groups function)
	if(oob_flag) (true_hover ? update_entries(dir) : update_groups(dir));

	//highlight newly hovered upon entry/group
	mvwchgat(entry_win, 1+e_hover-e_offset, 1, entry_win->_maxx-1, A_DIM, (true_hover ? 2 : 1), NULL);
	mvwchgat(group_win, 1+g_hover-g_offset, 1, group_win->_maxx-1, A_DIM, (true_hover ? 1 : 2), NULL);
	if(!true_hover){ //a little extra work regarding group hover
		e_offset = 0;
		update_entries(1);
		e_hover = 0;
	}

	wrefresh(group_win);
	wrefresh(entry_win);
	return;
}

//TODO clean up quotes
void launch_entry(){
	char *program = get_gprog(g[g_hover]);
	char *flags = get_gflags(g[g_hover]);
	char *path = get_epath(e[e_hover]);
	int mode = get_compmode();
	char full_command[BUF_LEN];
	bool quote_flag_f = (flags[0] == '"' ? false : true);
	bool quote_flag_e = (path[0] == '"' ? false : true);

	//if the entry is an executable file (doesn't have a launcher)
	if(!(strcmp(program, "./"))) system(path);

	else{
		if(mode != 0) path = compat_convert(path, mode);
		full_command[0] = '\0';
		strcat(full_command, program);
		if(flags[0] !='\0'){
			strcat(full_command, " ");
			if(quote_flag_f) strcat(full_command, "\"");
			strcat(full_command, flags);
			if(quote_flag_f) strcat(full_command, "\"");
		}
		strcat(full_command, " ");
		if(quote_flag_e) strcat(full_command, "\"");
		strcat(full_command, path);
		if(quote_flag_e) strcat(full_command, "\"");
		mvprintw(0, 0, "DEBUG: %s", full_command);
		system(full_command);
	}

	return;
}

char *compat_convert(char *path, int mode){
	char *new = malloc(sizeof(char) * BUF_LEN);
	char *trav = new;

	//1 -> WSL: letter is in /mnt/, convert slashes to backslashes
	if(mode == 1){
		path = path+5;
		*trav = *path - 32; //point at letter, make it uppercase
		trav++;
		*trav = ':';
		path++;
		trav++;

		//convert each character
		while(*path != '\0'){
			if(*path == '/') *trav = '\\';
			else *trav = *path;
			path++;
			trav++;
		}
		*trav = '\0';
	}

	else printf("Error: mode should not be %d\n", mode);

	return new;
}
