import ddf.minim.*;

Minim minim;
AudioSample[] samples;        //holds the audio samples by [row]
int minim_buf_size = 1024;


int[][][] colour_lookup;     //lookup table for the colour [seq#][type][rgb]
int[] active_channel;        //records the active channel to display for each seq [seq#]
int[] active_page;           //records the active page to display for each seq [seq#]
int active_audio_page;       //records the active page to play audio from
int active_seq;              //records the active seq (the one playing sound)
int[][][][][] state_ctrl;    //records the state machine values [seq#][channel#][page#][x][y]
int scroll_step = 0;         //pointer to the active playing step in scroll_seq
int[] num_steps;             //number of steps to play before looping back to start [seq#]
int[][] prob;                //probability of moving to the next seq [seq#][next_seq#]

//time/tempo
float timestep;         //value in ms, used to check if the tempo period has elapsed
int tempo_ms = 250;     //tempo period in milli seconds

//constants for draw
int cellsize = 35;      //button size in pixels
int cor = 100;          //greyscale unpressed colour
int offset_x = 40;      //main grid is offset by this many pixels to leave room for the menu
int offset_y = 40;

//constants for rgb array index
int r = 0;
int g = 1;
int b = 2;

//constants for colour lookup
int c_grid = 0;
int c_menu = 1;
int c_scroll = 2;
int c_gbl_on = 3;
int c_gbl_off = 4;
int c_gbl = 0;

//constants for array sizes
int max_seq = 3;            //num seq
int max_x = 8;              //num col in square grid
int max_menu = 2;           //num menus
int max_x_menu = max_x + max_menu;    //treat menus as extra cols
int max_y = 8;              //num rows in square grid
int max_rgb = 3;            //num rgb colour elements
int max_channel = 4;        //num audio channels
int max_page = 6;           //num pages per audio channel
int max_colour_type = 5;    //num colour types for the colour_lookup array

//constants for array indexes
int ch_tempo = 7;            //ver menu index for the tempo channel
int page_ver_menu = 0;       //page number to use on ver menu items, only one is necessary
int ch_steps = 6;            //ver menu index for the steps channel
int ch_prob = 5;             //ver menu index for the prob channel
int ch_clear = 4;            //ver menu index for the clear channel
int menu_hor = 8;            //array index for the horizontal menu
int menu_ver = 9;            //array index for the vertical menu
int menu_play = 7;           //hor menu index for the play pause button
int menu_follow = 6;         //hor menu index for the follow freeze button

//state control
boolean gbl_play = true;      //play pause flag - true = play the seq, false = pause the seq
boolean gbl_follow = true;    //follow freeze flag - true = follow the active audio page as the display, false = freeze to the current page as the display



 
void setup() {
 size(offset_x + (max_x * cellsize), offset_y + (max_y * cellsize));
 smooth();
 
 //init functions
 state_init();      //initialize the state_ctrl array
 colour_init();     //initialize the colour_lookup array
 active_init();     //initialize the active state pointers  
 sample_init();     //setup minim audio
 steps_init();      //initialize the number of steps per sequencer
 prob_init();       //initialize the prob for the next seq
  
 timestep = millis();    //get first time stamp
}

void draw(){
  background(0);
  draw_grid();
  draw_grid_menu();
 
  if (check_timestep() && gbl_play){    //one step time period has elapsed and we are in play mode
    move_scroll_step();        //check for move to next sequencer    
    trigger_samples();         //play sounds
  }
}


void stop()
{
  // always close Minim audio classes when you are done with them
  for (int i = 0; i < max_y; i++) {
    samples[i].close();
  }
  minim.stop();  
  super.stop();
}






//check if tempo_ms ms have elapsed (true/false), update the step pointer if true
boolean check_timestep(){
   if (millis() >= (timestep + tempo_ms)) {        //one step period has elapsed
     //print(millis() - (timestep + tempo_ms) +","); 
       
     timestep = timestep + tempo_ms;
     return true;
   } else {
     return false;
   }
}


//increment to the next step, change page or seq if necessary
void move_scroll_step(){
 scroll_step++;
 //println("seq: "+active_seq+", num: " + num_steps[active_seq]+", step: "+scroll_step+", page: "+active_audio_page);
 
 if (((scroll_step) + (active_audio_page)*max_y) >= num_steps[active_seq]) {
   //we've reached the maximum number of steps for this seq, go to the next seq
   scroll_step = 0;
   active_audio_page = 0;
   active_seq = pick_next_seq(active_seq);    //pick next seq based on assigned prob
   
   if (gbl_follow) {
     active_page[active_seq] = active_audio_page;   //only move the display page if follow is enabled
   }
 } else if (scroll_step >= max_y) {
   //we've reached the edge of the grid, go to the start of the next page
   scroll_step = 0;
   active_audio_page = (active_audio_page + 1) % max_page;    //next page
   if (gbl_follow) {
     active_page[active_seq] = active_audio_page;             //only move the display page if follow is enabled
   }   
 }
 
 
 if (active_channel[active_seq] >= max_channel){    //we're on a ver menu, so use page 0
   active_page[active_seq] = page_ver_menu;
 }
 
 //println("step: " + scroll_step + ", Apage: " + active_audio_page + ", Dpage: " + active_page[active_seq]);
}


//draws the colours for the menu portion
void draw_grid_menu(){
  int seq = active_seq;    //for multiple displays, we should loop over all seq
  
  //horizontal menu
  for (int j = 0; j < max_y; j ++ ) {
       stroke(0);
       strokeWeight(2);
       switch(state_ctrl[seq][active_channel[seq]][active_page[seq]][menu_hor][j]) {
         case 0:
           fill(cor,cor,cor);
           break;
         case 1:
           fill(colour_lookup[seq][c_menu][r],colour_lookup[seq][c_menu][g],colour_lookup[seq][c_menu][b]);
           break;
       }
       if (j == menu_play) {      //play/pause button
         if (gbl_play == true) {
           fill(colour_lookup[c_gbl][c_gbl_on][r],colour_lookup[c_gbl][c_gbl_on][g],colour_lookup[c_gbl][c_gbl_on][b]);
         } else {
           fill(colour_lookup[c_gbl][c_gbl_off][r],colour_lookup[c_gbl][c_gbl_off][g],colour_lookup[c_gbl][c_gbl_off][b]);
         }
       } else if (j == menu_follow) {  //follow/freeze button
         if (gbl_follow == true) {
           fill(colour_lookup[c_gbl][c_gbl_on][r],colour_lookup[c_gbl][c_gbl_on][g],colour_lookup[c_gbl][c_gbl_on][b]);
         } else {
           fill(colour_lookup[c_gbl][c_gbl_off][r],colour_lookup[c_gbl][c_gbl_off][g],colour_lookup[c_gbl][c_gbl_off][b]);
         }
       } else if (j == active_audio_page) {  //active audio page
         fill(colour_lookup[seq][c_scroll][r],colour_lookup[seq][c_scroll][g],colour_lookup[seq][c_scroll][b]);
       }
       rect(j*cellsize + offset_x, 0,cellsize,cellsize); 
   }
   
   //vertical menu
   for (int j = 0; j < max_y; j ++ ) {
       stroke(0);
       strokeWeight(2);
       
       switch(state_ctrl[seq][active_channel[seq]][page_ver_menu][menu_ver][j]) {
         case 0:
           fill(cor,cor,cor);
           break;
         case 1:
           fill(colour_lookup[seq][c_menu][r],colour_lookup[seq][c_menu][g],colour_lookup[seq][c_menu][b]);
           break;
         case 2:    //next seq colour
           fill(colour_lookup[j][c_menu][r],colour_lookup[j][c_menu][g],colour_lookup[j][c_menu][b]);
           break;
       }
       rect(0, j*cellsize + offset_y,cellsize,cellsize); 
   }
}


//draws the colours for the main grid portion
void draw_grid(){
  int seq = active_seq;    //for multiple displays, we should loop over all seq
  
   for (int i = 0; i < max_x; i ++ ) {        //x
     for (int j = 0; j < max_y; j ++ ) {      //y
       //noStroke();
       stroke(0);
       strokeWeight(2);
                   
       //check for scroll fill.  only draw on the first and last rows for the audio channels, tempo or steps
       if (i == scroll_step && (j==0 || j==max_y-1) && (active_channel[seq] < max_channel || active_channel[seq] == ch_tempo || active_channel[seq] == ch_steps)){
         //use the scroll colour
         fill(colour_lookup[seq][c_scroll][r],colour_lookup[seq][c_scroll][g],colour_lookup[seq][c_scroll][b]);
       } else {
         //regular colour
         switch(state_ctrl[seq][active_channel[seq]][active_page[seq]][i][j]) {
           case 0:
             fill(cor,cor,cor);
             break;
           case 1:
             fill(colour_lookup[seq][c_grid][r],colour_lookup[seq][c_grid][g],colour_lookup[seq][c_grid][b]);
             break;
           case 2:
             fill(0,255,255);
             if (active_channel[seq] == ch_clear) {
               //use gbl_off for clear content
               fill(colour_lookup[c_gbl][c_gbl_off][r],colour_lookup[c_gbl][c_gbl_off][g],colour_lookup[c_gbl][c_gbl_off][b]);
             }
             break;
         }
       }            
       rect(i*cellsize + offset_x, j*cellsize + offset_y,cellsize,cellsize); 
     }
  }
}





// ***** button controls ***************************************************************
void mousePressed(){
   update_mouse_xy();
}

void mouseReleased(){
   //updategrid();
}

//find out where the mouse was pressed, and update the state of the button
void update_mouse_xy(){
  int x;
  int y;
  int seq = active_seq;    //for multiple displays, we should loop over all seq
  
  //mouse on grid
  if (mouseX > offset_x && mouseY > offset_y) {
    x = floor((mouseX-offset_x)/cellsize);
    y = floor((mouseY-offset_y)/cellsize);
    if (active_channel[seq] < max_channel) {
      switch(state_ctrl[seq][active_channel[seq]][active_page[seq]][x][y]){
        case 0:
          state_ctrl[seq][active_channel[seq]][active_page[seq]][x][y] = 1;
          break;
        case 1:
          state_ctrl[seq][active_channel[seq]][active_page[seq]][x][y] = 0;
          break;
      }
    } else if (active_channel[seq] == ch_tempo) {
      menu_set_tempo(x,y);
    } else if (active_channel[seq] == ch_steps) {
      menu_set_steps(seq,x,y);
    } else if (active_channel[seq] == ch_prob) {
      menu_set_prob(seq,x,y);
    } else if (active_channel[seq] == ch_clear) {
      menu_set_clear(seq,x,y);
    }
  }
    
  //mouse on horizontal menu
  else if (mouseX > offset_x && mouseY < cellsize){
    y = floor((mouseX-offset_x)/cellsize);
    if (y == menu_play) {                //play pause
      menu_set_play();        
    } else if (y == menu_follow) {       //follow freeze
      menu_set_follow();
    } else if (y < max_page) {           //audio channel pages
      active_page[seq] = y;
    } 
  }
  
  //mouse on vertical menu
  else if (mouseX < cellsize && mouseY > offset_y){
    y = floor((mouseY-offset_y)/cellsize);
    menu_set_channel(seq,y);    
    if (y >= max_channel) {  
      active_page[seq] = page_ver_menu;    //set to the menu page
    }
  }
}


// ******* initialize arrays *************************************************************

void colour_init() {
 //initalize the colour_lookup array
 colour_lookup = new int[max_seq][max_colour_type][max_rgb];
 
 colour_lookup[0][c_grid][r] = 200;      //grid 0      purple theme
 colour_lookup[0][c_grid][g] = 0;
 colour_lookup[0][c_grid][b] = 255;
 
 colour_lookup[0][c_menu][r] = 240;    //menu 0
 colour_lookup[0][c_menu][g] = 0;
 colour_lookup[0][c_menu][b] = 255;
 
 colour_lookup[0][c_scroll][r] = 128;    //scroll 0
 colour_lookup[0][c_scroll][g] = 0;
 colour_lookup[0][c_scroll][b] = 128;


 colour_lookup[1][c_menu][r] = 50;    //menu 1      blue theme
 colour_lookup[1][c_menu][g] = 50;
 colour_lookup[1][c_menu][b] = 200;
 
 colour_lookup[1][c_grid][r] = 150;      //grid 1
 colour_lookup[1][c_grid][g] = 150;
 colour_lookup[1][c_grid][b] = 250;
 
 colour_lookup[1][c_scroll][r] = 0;    //scroll 1
 colour_lookup[1][c_scroll][g] = 0;
 colour_lookup[1][c_scroll][b] = 128;
 

 colour_lookup[2][c_menu][r] = 255;    //menu 2      orange theme
 colour_lookup[2][c_menu][g] = 50;
 colour_lookup[2][c_menu][b] = 0;
 
 colour_lookup[2][c_grid][r] = 255;      //grid 2
 colour_lookup[2][c_grid][g] = 150;
 colour_lookup[2][c_grid][b] = 0;
 
 colour_lookup[2][c_scroll][r] = 150;    //scroll 2
 colour_lookup[2][c_scroll][g] = 50;
 colour_lookup[2][c_scroll][b] = 0;

 
 
 colour_lookup[0][c_gbl_on][r] = 0;    //global on
 colour_lookup[0][c_gbl_on][g] = 255;
 colour_lookup[0][c_gbl_on][b] = 0;

 colour_lookup[0][c_gbl_off][r] = 255;    //global off
 colour_lookup[0][c_gbl_off][g] = 0;
 colour_lookup[0][c_gbl_off][b] = 0; 
 
}


// initialize the active pointers
void active_init() {
  active_seq = 0;
  active_audio_page = 0;
  active_channel = new int[max_seq];
  active_page = new int[max_seq];
  for (int i = 0; i < max_seq; i++) {
    active_channel[i] = 0;
    active_page[i] = 0;
  }
}


void state_init() {
 //initalize the state_ctrl array to 0s
 state_ctrl = new int[max_seq][max_y][max_page][max_x_menu][max_y];
 for (int i = 0; i < max_seq; i ++ ) {             //seq
   for (int j = 0; j < max_y; j ++ ) {             //channel
     for (int k = 0; k < max_page; k++) {          //page
       for (int l = 0; l < max_x_menu; l++) {      //x
         for (int m = 0; m < max_y; m++) {         //y
           state_ctrl[i][j][k][l][m] = 0;
           //add the page menu buttons
           if (k == m && l == menu_hor && j < max_channel) {
             state_ctrl[i][j][k][l][m] = 1;
           }
           //add the channel menu buttons
           else if (j == m && l == menu_ver) {
             state_ctrl[i][j][k][l][m] = 1;
           }
         }
       } 
     }    
   }
 }
 
 menu_set_tempo(6,1);              //load init into the display grid
}

void steps_init() {
   num_steps = new int[max_seq];
   for (int i = 0; i < max_seq; i++) {    //seq
     menu_set_steps(i,7,2);        //load into the display grid, y*8 + x + 1 for menu_set_steps
   }
}

void prob_init(){
  prob = new int[max_seq][max_seq];
  for (int i = 0; i < max_seq; i++) {      //seq#
    for (int j = 0; j < max_seq; j++) {    //next_seq#
      prob[i][j] = 4;
      menu_set_prob(i,4,j);
      state_ctrl[i][ch_prob][page_ver_menu][menu_ver][j] = 2;    //menu colours for next seq
    }
  }
}


// ********* Sound Functions **********************************************************
void sample_init() {
  //load all of the audio samples into the array
 minim = new Minim(this);
 samples = new AudioSample[max_y]; 
 
 samples[0] = minim.loadSample("saw_c4.wav", minim_buf_size);
 samples[1] = minim.loadSample("saw_a3.wav", minim_buf_size);
 samples[2] = minim.loadSample("saw_g3.wav", minim_buf_size);
 samples[3] = minim.loadSample("saw_f3.wav", minim_buf_size);
 samples[4] = minim.loadSample("saw_d3.wav", minim_buf_size);
 
 samples[5] = minim.loadSample("CHH.wav", minim_buf_size);
 samples[6] = minim.loadSample("SD.wav", minim_buf_size); 
 samples[7] = minim.loadSample("BD.wav", minim_buf_size);
}


void trigger_samples() {
  //play the sounds - check all rows on all channels for the active seq/page/step
  for (int i = 0; i < max_y; i++) {              //y
    for (int j = 0; j < max_channel; j++) {      //channel
      if (state_ctrl[active_seq][j][active_audio_page][scroll_step][i] == 1) {
        samples[i].trigger();
        //should have different sounds per channel
      }
    }
  }  
}



//**** menu controls ******************************
void menu_set_channel(int seq, int ch) {
  active_channel[seq] = ch;
  
  if (ch == ch_clear) {
    find_content_for_clear(seq);    //update the clear channel
  }
}

void menu_set_tempo(int x, int y){
  //y = most sig digit, x = least sig digit
  tempo_ms = (y*max_x + x) * 6;               //set tempo, scaled by 6
  
  //update the grid display
  for (int k = 0; k < max_seq; k++){            //seq  - global to all seq
    for (int i = 0; i < max_x; i++){            //x
      for (int j = 0; j < max_y; j++) {         //y
        if (j < y || (j == y && i <= x)) {
          state_ctrl[k][ch_tempo][page_ver_menu][i][j] = 1;
        } else {
          state_ctrl[k][ch_tempo][page_ver_menu][i][j] = 0;
        }
      }
    }
  }
}

void menu_set_steps(int seq, int x, int y){
  //y = most sig digit, x = least sig digit
  if (y >= max_page) {        //catch an out of range row
    y = max_page - 1;
    x = max_x - 1;
  }  
  num_steps[seq] = (y*max_x + x) + 1;          //set the number of steps for the given seq
  
  //update the grid display
  for (int i = 0; i < max_x; i++){            //x
    for (int j = 0; j < max_y; j++) {         //y
      if (j < y || (j == y && i <= x)) {
        state_ctrl[seq][ch_steps][page_ver_menu][i][j] = 1;
      } else {
        state_ctrl[seq][ch_steps][page_ver_menu][i][j] = 0;
      }
    }
  }
}

void menu_set_play(){
  //toggle the play/pause button
  if (gbl_play){
    gbl_play = false;
  } else {
    gbl_play = true;
    timestep = millis() - (tempo_ms/2);  //update the time counter, 1/2 beat count in, then resume
  }
}

void menu_set_follow(){
  //toggle the follow/freeze button
  if (gbl_follow){
    gbl_follow = false;
  } else{
    gbl_follow = true;
    if (active_channel[active_seq] < max_channel){
      active_page[active_seq] = active_audio_page;    //unfreeze and jump to the active audio page
    }
  }
}

void menu_set_prob(int seq, int x, int y){
  //y = next_seq, x = prob value
  if (y >= max_seq) {return;}    //catch click on out of range row
  prob[seq][y] = x;              //set the probability for the specified sequencer
  
  //update the grid display
  for (int i = 0; i < max_page; i++){     //page
    for (int j = 0; j < max_x; j++){      //x
      if (j <= x) {
        state_ctrl[seq][ch_prob][i][j][y] = 1;
      } else {
        state_ctrl[seq][ch_prob][i][j][y] = 0;
      }
    }
  }
}

void menu_set_clear(int seq, int x, int y) {
  if (x < max_page && y < max_channel) {
    //top left grid, clear page
    clear_page(seq,y,x);
  } else if (x == max_x - 1 && y < max_channel) {
    //top right column, clear channel
    clear_channel(seq,y);
  } else if (x == max_x - 1 && y == max_y - 1) {
    //bottom right button, clear seq
    clear_seq(seq);
  }  
  find_content_for_clear(seq);    //refresh the screen after the clear;
}





int pick_next_seq(int seq){
  //use the seq prob to determine the next seq to activate
  int p_tot = 0;
  int p_rand = 0;
  int p_rand_sum = 0;
  for (int i = 0; i < max_seq; i++){    //next_seq
    p_tot = p_tot + prob[seq][i];      //total prob weighting of all seq
  } 
  if (p_tot == 0){
    return seq;        //no probs were set, stay on this seq
  }
  
  // random pick algorithm: find where p_rand lies on the line, then pick that p#
  //
  //       p1     p2   pn      
  // 0 |--------|---|------| p_tot   = p1 + p2 + ... pn
  //   <--------------x----> p_rand is in (0,p_tot)
  
  p_rand = int(random(p_tot));                  //random int picked from the total prob weighting 
  for (int i = 0; i < max_seq - 1; i++){
    if (p_rand < prob[seq][i] + p_rand_sum){    //random number is less than the sum of the seq probs so far, this counts as a pick
      return i;
    } else {
      p_rand_sum = p_rand_sum + prob[seq][i];    //random number was bigger, add this seq to the sum and try on the next seq
    }
  }  
  return max_seq - 1;      //else must be the last seq
}



// ****** CLEAR functions ****************************************************

void clear_page(int seq, int ch, int pg){
 //clear the state_ctrl to 0's for the specified page
  for (int i = 0; i < max_x; i++) {
    for (int j = 0; j < max_y; j++) {
      state_ctrl[seq][ch][pg][i][j] = 0;
    }
  }
}

void clear_channel(int seq, int ch){
  //clear the state_ctrl to 0's for the specified channel (all pages)
  for (int i = 0; i < max_page; i++) {
    clear_page(seq,ch,i);
  }
}

void clear_seq(int seq) {
  //clear the state_ctrl to 0's for the specified seq (all channels)
  for (int i = 0; i < max_channel; i++) {
    clear_channel(seq, i);
  }
}

void find_content_for_clear(int seq) {
  //scan each page on the seq to see if there is programmed content that can be cleared/indicated on the clear channel
  int sum_pg = 0;
  int sum_ch = 0;
  int sum_seq = 0;
  for (int i = 0; i < max_channel; i++) {          //ch
    sum_ch = 0;
    for (int j = 0; j < max_page; j++) {           //pg
      sum_pg = 0;
      //add up all the state values for the page
      for (int k = 0; k < max_x; k++) {            //x
        for (int l = 0; l < max_y; l++) {          //y
          sum_pg = sum_pg + state_ctrl[seq][i][j][k][l]; 
        }
      }
      //update the clear channel state ctrl for each page
      if (sum_pg > 0){
        state_ctrl[seq][ch_clear][page_ver_menu][j][i] = 2;    //content was found on the page
        sum_ch++;
      } else {
        state_ctrl[seq][ch_clear][page_ver_menu][j][i] = 1;    //no content
      }    
    }
    //update the clear channel state ctrl for each channel
    if (sum_ch > 0){
      state_ctrl[seq][ch_clear][page_ver_menu][max_x-1][i] = 2;  //content was found on the channel
      sum_seq++;
    } else {
      state_ctrl[seq][ch_clear][page_ver_menu][max_x-1][i] = 1;  //no content
    }
  }
  
  //update the clear channel state ctrl for each channel
  if (sum_seq > 0){
    state_ctrl[seq][ch_clear][page_ver_menu][max_x-1][max_y-1] = 2;  //content was found on the seq
  } else {
    state_ctrl[seq][ch_clear][page_ver_menu][max_x-1][max_y-1] = 1;  //no content
  }
  
}
