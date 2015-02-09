/*
  ARDUINO STEP SEQUENCER
  2013-10-20
  Simon Nuk
*/


/*
--- Mapping for TLC library -------------------------------------------------

    -  +5V from Arduino -> TLC pin 21 and 19     (VCC and DCPRG)
    -  GND from Arduino -> TLC pin 22 and 27     (GND and VPRG)
    -  digital 9        -> TLC pin 18            (GSCLK)
    -  digital 11       -> TLC pin 24            (XLAT)
    -  digital 12       -> TLC pin 23            (BLANK)
    -  digital 51       -> TLC pin 26            (SIN)
    -  digital 52       -> TLC pin 25            (SCLK)
    -  The 2K resistor between TLC pin 20 and GND will let ~20mA through each
       LED.  To be precise, it's I = 39.06 / R (in ohms).  This doesn't depend
       on the LED driving voltage.
    - (Optional): put a pull-up resistor (~10k) between +5V and BLANK so that
                  all the LEDs will turn off when the Arduino is reset.

    Alex Leone <acleone ~AT~ gmail.com>, 2009-02-03 */

#include "Tlc5940.h"


// -------- DEFINE CONSTANTS & VARIABLES -----------------------------------------------------

const byte RUN_LED_TEST = 0;
const byte RUN_NORMAL = 1;
const byte RUN_MODE = RUN_NORMAL;    //set the run mode of the sequencer


const byte max_row = 8;
const byte max_col = 10;
const byte R = 0;    //Red or Row
const byte G = 1;    //Green
const byte B = 2;    //Blue
const byte C = 1;    //Column
const byte max_rgb = 3;
const byte max_seq = 3;
const byte max_state_val = 5;
const byte max_page = 4;  //4
const byte max_channel = 4;
const byte max_midi_channel = 3;
const byte max_random = 2;

const byte ch_menu = 3;
const byte pg_tempo = 0;
const byte pg_step = 1;
const byte pg_prob = 2;
const byte pg_clear = 3;

const byte pin_button_reg_clk = 6;    //74LS165 pin 2, clk (was 41)
const byte pin_button_reg_load = 5;   //74LS165 pin 1, shift/load (was 40)
const byte pin_button_reg_in[3] = {4, 3, 2};     //74LS165 pin 9, output Qh (was 42,43,44)


byte button_map[max_row * max_col][2];              //maps the serial stream of button states to their row/column index
byte column_map[max_col][max_rgb];                  //LED column to LED driver chip column for RGB. column_map[col][rgb] -> chip column
short colour_map[max_seq][max_state_val][max_rgb];  //state_val to rbg intensity for each seq.  colour_map[seq][state_val][rgb] -> rgb intensity (colour)
byte midi_map[max_row];                              //row to MIDI offset

boolean button_state[max_seq][max_row][max_col];  //current state of each button

byte state_ctrl[max_seq][max_channel][max_page][max_row][max_col];  //state of each grid element (visible and non). state_ctrl[seq][channel][page][row][col] -> state_val
byte active_channel[max_seq];           //active channel to be displayed on each seq
byte active_page[max_seq];              //active page to be displayed on each seq
byte active_audio_page[max_seq];        //active page to be played on each seq
byte seq_steps[max_seq];                //number of steps on each seq
byte seq_prob[max_seq][max_seq];         //probability to switch to the next sequencer
byte seq_rand[max_seq][max_midi_channel];  //randomization parameter for each channel
int active_seq = 0;                    //seq that is playing sound
boolean flag_pause = false;            //play/pause button
boolean flag_freeze[max_seq];          //freeze/follow button

float start_time;
float total_time;

float step_start;
float step_now;
float button_start;
float button_now;

byte step_num;
int step_period;
byte max_step = 8;
boolean state_change;


// -----------------------------------------------------------------------------------------------------------






void setup()
{
  Tlc.init();        //setup the LED driver chip
  column_map_init();
  colour_map_init();
  active_channel_init();
  active_page_init();
  state_ctrl_init();
  button_map_init();
  button_init();
  seq_steps_init();
  seq_prob_init();
  seq_rand_init();
  tempo_init();
  flag_freeze_init();
  midi_map_init();
  
  step_num = 0;
  state_change = true;
  //step_period = 250;
  step_start = millis();
  button_start = step_start;
  
  
  //Serial.begin(9600);    //println baud rate
  Serial.begin(31250);  //  Set MIDI baud rate:
}


void loop()
{ 
  
  if (RUN_MODE == RUN_NORMAL) {
    //normal sequencer
    if (flag_pause == false){      //not paused, lets play.  increment the step (if elapsed) and play midi
      
      step_now = millis();
      if (step_now - step_start >= step_period) {
        step_num = step_num + 1;
        if (step_num >= seq_steps[active_seq]){
          //select the next seq   < -------------------------------------------------
          pick_next_seq();
          step_num = 0;      //reset the step counter
        }
        active_audio_page[active_seq] = step_num / max_step; 
        if (flag_freeze[active_seq] == false && active_channel[active_seq] < max_midi_channel) {
          active_page[active_seq] = step_num / max_step;  //move to the appropriate page
        }
        
        add_random_note(step_num+1);        //apply the randomize function
        send_MIDI_out();
                
        step_start = step_start + step_period;
        state_change = true;
      }
    } else {
      step_start = millis();  //reset the counter while on pause
    }
    
    // <---------------------------------------------------------- catch for float overun on step/button_now/start???
    
    //check the buttons
    button_now = millis();
    if (button_now - button_start >= 250) {  //time for a button poll
      read_buttons();
      button_start = button_now;        //reset the timer
    }
    
    //if there is a state change, write the new LED states
    if (state_change == true) {
      write_LED();      //write the column values
      state_change = false;
    }
      
    
    
    
    
  }else if (RUN_MODE == RUN_LED_TEST) {
    //
    /*for (int i = 1; i < 4 ; i++) {
      for (int i_row = 0; i_row <= max_row; i_row++) {
        test_LED(i, i_row);
        delay(1000);
      }
    }*/    
    for (int i = 1; i < 4 ; i++) {
      test_LED(i, max_row);
      delay(5000);   
    }       
    //test_LED(7, max_row);
    //delay(2000);
  }

}




// --------------------------------------------------------------------------------------------------------
//
// --------------------------------------------------------------------------------------------------------



//write the LED (greyscale) column values for a given row (only one colour so far)
void write_LED()
{
  int state = 0;
  int step_colour = 3;
   
  for (int i_seq = 0; i_seq < max_seq; i_seq++) {
    for (int i_col = 0; i_col < max_col; i_col++) {  
      for (int i_row = 0; i_row < max_row; i_row++) {      
         
        Tlc.set_p(column_map[i_col][R] + i_row*32, colour_map[i_seq][state_ctrl[i_seq][active_channel[i_seq]][active_page[i_seq]][i_row][i_col]][R], i_seq);
        Tlc.set_p(column_map[i_col][G] + i_row*32, colour_map[i_seq][state_ctrl[i_seq][active_channel[i_seq]][active_page[i_seq]][i_row][i_col]][G], i_seq);
        Tlc.set_p(column_map[i_col][B] + i_row*32, colour_map[i_seq][state_ctrl[i_seq][active_channel[i_seq]][active_page[i_seq]][i_row][i_col]][B], i_seq); 
        
           
        if ((((i_col == (step_num % max_step)+1) && (step_num / max_step == active_page[i_seq]) && (active_channel[i_seq] < max_midi_channel)) || (i_col == 9 && i_row == active_audio_page[i_seq])) && i_seq == active_seq){    //highlight the active step column and page
          Tlc.set_p(column_map[i_col][R] + i_row*32, colour_map[i_seq][step_colour][R], i_seq);
          Tlc.set_p(column_map[i_col][G] + i_row*32, colour_map[i_seq][step_colour][G], i_seq);
          Tlc.set_p(column_map[i_col][B] + i_row*32, colour_map[i_seq][step_colour][B], i_seq);
        }
        
      }
    }
  }
                                                                                
  Tlc.update_p();    //write the col values to the LED driver 
}


//change all the LEDs to a single colour for visual inspection
void test_LED(byte led_colour, int led_row)
{
  int val_r = 0;
  int val_g = 0;
  int val_b = 0;
  
  switch (led_colour) {
    case 1:        //red
      val_r = 800;
      break;
    case 2:        //green
      val_g = 800;
      break;
    case 3:        //blue
      val_b = 800;
      break;
    case 4:        //r+g
      val_r = 100;
      val_g = 100;
      break;
    case 5:        //g+b
      val_b = 100;
      val_g = 100;
      break;
    case 6:        //r+b
      val_r = 100;
      val_b = 100;
      break;
    case 7:        //white
      val_r = 100;
      val_g = 100;
      val_b = 100;
  }
  
   
  for (int i_seq = 0; i_seq < max_seq; i_seq++) {
    for (int i_col = 0; i_col < max_col; i_col++) {  
      for (int i_row = 0; i_row < max_row; i_row++) {  
        if (i_row == led_row || led_row == max_row) {        
          Tlc.set_p(column_map[i_col][R] + i_row*32, val_r, i_seq);
          Tlc.set_p(column_map[i_col][G] + i_row*32, val_g, i_seq);
          Tlc.set_p(column_map[i_col][B] + i_row*32, val_b, i_seq); 
        } else {
          Tlc.set_p(column_map[i_col][R] + i_row*32, 0, i_seq);
          Tlc.set_p(column_map[i_col][G] + i_row*32, 0, i_seq);
          Tlc.set_p(column_map[i_col][B] + i_row*32, 0, i_seq);
        }     
      }
    }
  }                                                                             
  Tlc.update_p();    //write the col values to the LED driver 
} 




// ---------- INIT functions --------------------------------------------------

void flag_freeze_init() {
  for (int i_seq = 0; i_seq < max_seq; i_seq++){
    flag_freeze[i_seq] = false;
  }
}

void seq_steps_init() {
  for (int i_seq = 0; i_seq < max_seq; i_seq++){
    set_seq_steps(i_seq, 0, 8);
  }
}

void seq_prob_init() {
  for (int i_seq = 0; i_seq < max_seq; i_seq++){
    for (int i_seq_dest = 0; i_seq_dest < max_seq; i_seq_dest++){
      if (i_seq_dest == 0) {      
        set_seq_prob(i_seq, i_seq_dest, 8);      //all point to seq0
      } else {
        set_seq_prob(i_seq, i_seq_dest, 1);
      }
    }
  }
}

void seq_rand_init() {
  for (int i_seq = 0; i_seq < max_seq; i_seq++){
    for (int i_ch = 0; i_ch < max_midi_channel; i_ch++){    
      set_seq_rand(i_seq, i_ch, 1);      //all set to 0
    }
  }
}

void tempo_init() {
  for (int i_seq = 0; i_seq < max_seq; i_seq++){
    set_tempo(i_seq, 3, 5);
  }
}

void active_channel_init() {
  for (int i_seq = 0; i_seq < max_seq; i_seq++){
    active_channel[i_seq] = 0;
  }
}

void active_page_init() {
  for (int i_seq = 0; i_seq < max_seq; i_seq++){
    active_page[i_seq] = 0;
    active_audio_page[i_seq] = 0;
  }
}

void state_ctrl_init() {
  
  byte state_val = 0;
  
  for (int i_seq = 0; i_seq < max_seq; i_seq++){
    for (int i_channel = 0; i_channel < max_channel; i_channel++){
      for (int i_page = 0; i_page < max_page; i_page++){
        for (int i_row = 0; i_row < max_row; i_row++){
          for (int i_col = 0; i_col < max_col; i_col++){
            /*if (i_channel == 0 && i_col%3 == 1) {
              state_val = 1;
            } else if (i_channel == 0 && i_col%3 == 2) {
              state_val = 2;
            } else if (i_channel == 0 && i_col%3 == 0) {
              state_val = 3;
            } else {
               state_val = 0;
            }*/
            state_val = 0;
            
            //set the page buttons
            if (i_col == (max_col-1) && i_page == i_row) {  //&& i_page == i_row
              state_val = 4;
            }
            state_ctrl[i_seq][i_channel][i_page][i_row][i_col] = state_val;
            
          }
        }
      }
    }
  }
}


// --------------------------------------------------------------------------------


void send_MIDI_out() {
  for (int i_channel = 0; i_channel < max_midi_channel; i_channel++){
    for (int i_row = 0; i_row < max_row; i_row++){
      if (state_ctrl[active_seq][i_channel][active_audio_page[active_seq]][i_row][(step_num % max_step)+1] >0) {
        noteOn(0x90 + i_channel, 0x30 + midi_map[i_row], 0x45);
      } else {
        noteOn(0x90 + i_channel, 0x30 + midi_map[i_row], 0x00);
      }
    }
  }
 
 
 /*
 MIDI noteOn command:
 
 cmd:
 0x90  note ON channel 1
 0x91  note ON channel 2
 ...

 */
  
  //Note on channel 1 (0x90), some note value (note), middle velocity (0x45):
    //noteOn(0x90, 0x30, 0x45);
}


// ******* MIDI *********************************
void noteOn(int cmd, int pitch, int velocity) {
  Serial.write(cmd);
  Serial.write(pitch);
  Serial.write(velocity);
}


// ****** Buttons ******************************
void button_init() {
  //setup the pins
  pinMode(pin_button_reg_clk, OUTPUT);
  pinMode(pin_button_reg_load, OUTPUT);
  pinMode(pin_button_reg_in[0],INPUT);
  pinMode(pin_button_reg_in[1],INPUT);
  pinMode(pin_button_reg_in[2],INPUT);
  
  //starting state for the shift registers
  digitalWrite(pin_button_reg_clk,LOW);
  digitalWrite(pin_button_reg_load,HIGH);
  
  
  //set the button_state array to all false
  for (int i_seq; i_seq < max_seq; i_seq++){
    for (int i_row; i_row < max_row; i_row++){
      for (int i_col; i_col < max_col; i_col++){
        button_state[i_seq][i_row][i_col] = false;
      }
    }
  }
}

void read_buttons(){
  //check the current state of the buttons
  boolean button;
  
  //Serial.println("READ button");
  
  //shift/load the registers
  digitalWrite(pin_button_reg_load,LOW);
  toggle_button_clk();
  digitalWrite(pin_button_reg_load,HIGH);
  
  byte i_row = 0;
  byte i_col = 0;
  
  //read the inputs
  for (int i_button = 0; i_button < (max_row * max_col); i_button++){
      for (int i_seq = 0; i_seq < max_seq - max_seq + 1; i_seq++){
        button = (digitalRead(pin_button_reg_in[i_seq])==HIGH);
       // Serial.print(button);
        
        //lookup the button
        i_row = button_map[i_button][R];
        i_col = button_map[i_button][C];
        
        if (button != button_state[i_seq][i_row][i_col]){  //there was a change in button state
          button_state[i_seq][i_row][i_col] = button;      //record the new state
          if (button == true) {                            //toggle the state_ctrl only on press (not release)
            
            //Vertical Menu
            if (i_col == 0) {
              if (i_row < 5) {                  //new audio channel
                set_active_channel(i_seq, i_row);    
              }else if (i_row == 7) {              //toggle play/pause
                if (flag_pause == true) {
                  flag_pause = false;
                  state_ctrl[i_seq][active_channel[i_seq]][active_page[i_seq]][i_row][i_col] = 3;    //blue
                }else {
                  flag_pause = true;
                  state_ctrl[i_seq][active_channel[i_seq]][active_page[i_seq]][i_row][i_col] = 1;    //red
                }
              }else if (i_row == 6) {              //toggle freeze/follow
                if (flag_freeze[i_seq] == true) {
                  flag_freeze[i_seq] = false;
                  state_ctrl[i_seq][active_channel[i_seq]][active_page[i_seq]][i_row][i_col] = 3;    //blue
                }else {
                  flag_freeze[i_seq] = true;
                  state_ctrl[i_seq][active_channel[i_seq]][active_page[i_seq]][i_row][i_col] = 1;    //red
                }
              }
                
            } else if (i_col == 9) {  //Horizontal Menu
              active_page[i_seq] = i_row;
            
            } else {  //grid
              
              if (active_channel[i_seq] == ch_menu) {  //parameters
                if (active_page[i_seq] == 0) {  //tempo
                  set_tempo(i_seq, i_row, i_col);
                } else if (active_page[i_seq] == pg_step) {    //steps
                  set_seq_steps(i_seq, i_row, i_col);
                } else if (active_page[i_seq] == pg_prob) {    //prob/rand
                  if (i_row < max_channel) {
                    set_seq_prob(i_seq, i_row, i_col);  //prob
                  } else {
                    set_seq_rand(i_seq, i_row-5, i_col); //rand 
                  }
                } else if (active_page[i_seq] == 7) {          //clear
                  clear_page(i_seq, i_row, i_col);
                }
              
              } else {              
                //regular grid
                if (state_ctrl[i_seq][active_channel[i_seq]][active_page[i_seq]][i_row][i_col] > 0) {
                  state_ctrl[i_seq][active_channel[i_seq]][active_page[i_seq]][i_row][i_col] = 0;
                } else {
                  state_ctrl[i_seq][active_channel[i_seq]][active_page[i_seq]][i_row][i_col] = 4;
                }
              }
            }
            state_change = true;
          }
        }
      }
      toggle_button_clk();    //all sequencers were read, onto the next button
      //Serial.println("");
  }
        
}





// ---- Set Menu Parameters ----------------------------------------------------------------

//update the probability of switching to the next sequencer
void set_seq_prob(int i_seq, int i_row, int s_col) {
  boolean flag_found = false;

  if (i_row < max_seq) {
    for (int i_col = 1; i_col < max_col-1; i_col++) {
      if (flag_found == false) {        //turn on the LEDs until we reach the desired value
        state_ctrl[i_seq][ch_menu][pg_prob][i_row][i_col] = 1;
      } else {
        state_ctrl[i_seq][ch_menu][pg_prob][i_row][i_col] = 0;
      }
           
      if (i_col == s_col) {  //we have reached the desired value
        flag_found = true;
      }
    }
    
    seq_prob[i_seq][i_row] = s_col;   //update the global parameter
  }
}


//update the random note parameter
void set_seq_rand(int i_seq, int i_row, int s_col) {
  boolean flag_found = false;

  if (i_row < max_seq && i_row >= 0) {
    for (int i_col = 1; i_col < max_col-1; i_col++) {
      if (flag_found == false) {        //turn on the LEDs until we reach the desired value
        state_ctrl[i_seq][ch_menu][pg_prob][i_row+5][i_col] = 1;
      } else {
        state_ctrl[i_seq][ch_menu][pg_prob][i_row+5][i_col] = 0;
      }
           
      if (i_col == s_col) {  //we have reached the desired value
        flag_found = true;
      }
    }
    
    seq_rand[i_seq][i_row] = s_col;   //update the global parameter
  }
}


//update the number of steps on a given seq
void set_seq_steps(int i_seq, int s_row, int s_col) {
  boolean flag_found = false;
  for (int i_row = 0; i_row < max_row; i_row++) {
    for (int i_col = 1; i_col < max_col-1; i_col++) {
      if (i_row >= max_page) {
        flag_found = true;      //catch out-of-bounds steps
      }
      
      if (flag_found == false) {        //turn on the LEDs until we reach the desired step value
        state_ctrl[i_seq][ch_menu][pg_step][i_row][i_col] = 1;
      } else {
        state_ctrl[i_seq][ch_menu][pg_step][i_row][i_col] = 0;
      }
           
      if (i_row == s_row && i_col == s_col) {  //we have reached the desired step
        flag_found = true;
      }
    }
  }
  
  seq_steps[i_seq] = s_row*(max_col-2) + s_col;   //update the global parameter
}


//update the global tempo
void set_tempo(int i_seq, int s_row, int s_col) {
  boolean flag_found = false;
  for (int i_row = 0; i_row < max_row; i_row++) {
    for (int i_col = 1; i_col < max_col-1; i_col++) {

      
      if (flag_found == false) {        //turn on the LEDs until we reach the desired value
        state_ctrl[i_seq][ch_menu][pg_tempo][i_row][i_col] = 1;
      } else {
        state_ctrl[i_seq][ch_menu][pg_tempo][i_row][i_col] = 0;
      }
           
      if (i_row == s_row && i_col == s_col) {  //we have reached the desired value
        flag_found = true;
      }
    }
  }
  
  //map to BPM, then 1/8 notes, then divide for period
  step_period = 60000/(map((s_row*(max_col-2) + s_col),0,64,80,160)*2) ;   //update the global parameter
}


//update active_channel[] and state_ctrl[] when a new channel is selected
void set_active_channel(int ac_seq, int ac_row) {
  active_channel[ac_seq] = ac_row;    //new channel selected
  byte state_val = 0;
  for (int i_page = 0; i_page < max_page; i_page++){
    for (int i_row = 0; i_row < max_row; i_row++) {
      state_val = 0;          //disable all other channels
      if (i_row == ac_row) {  
        state_val = 4;        //activate the new channel
      }
      state_ctrl[ac_seq][active_channel[ac_seq]][i_page][i_row][0] = state_val;
    }
  }
  
}


void toggle_button_clk(){
  digitalWrite(pin_button_reg_clk,HIGH);
  digitalWrite(pin_button_reg_clk,LOW);
}


//picks the next sequencer based on the probability array
void pick_next_seq(){
  int i_seq = active_seq;
  int tot_prob = 0;
  float prob_val = 0;
  
  if (seq_prob[i_seq][0] <= 1 && seq_prob[i_seq][1] <= 1 && seq_prob[i_seq][2] <= 1){
    //check if the probability is programmed.  if not, continue with the same sequencer
    active_seq = active_seq;
  } else if (seq_prob[i_seq][0] <= 1 && seq_prob[i_seq][1] <= 1){
    active_seq = 2;
  } else if (seq_prob[i_seq][1] <= 1 && seq_prob[i_seq][2] <= 1){
    active_seq = 0;
  } else if (seq_prob[i_seq][0] <= 1 && seq_prob[i_seq][2] <= 1){
    active_seq = 1;
  } else {
    tot_prob = seq_prob[i_seq][0] + seq_prob[i_seq][2] + seq_prob[i_seq][3] - 3;
    prob_val = random(tot_prob);
    
    if (prob_val < seq_prob[i_seq][0]-1) {
      active_seq = 0;
    } else if (prob_val < seq_prob[i_seq][1]-1) {
      active_seq = 1;
    } else {
      active_seq = 2;
    }
  }
}


//add a random note to the sequence
void add_random_note(int step_num){
  float rand_val;
  byte rand_note;
  byte note_count;
  for (int i_ch = 0; i_ch < max_midi_channel; i_ch++) {

    rand_val = seq_rand[active_seq][i_ch];
    if (random(4*max_step) < rand_val && rand_val > 1){    //pick a random number between 0 and 8 and compare to the randomize param
      rand_note = random(max_row);
      if (rand_note == max_row){rand_note = max_row-1;}
      if (state_ctrl[active_seq][i_ch][active_audio_page[active_seq]][rand_note][step_num] == 0){    //swap the current state
        //check to see if we have too many states enabled
        note_count = 0;
        for (int i_row = 1; i_row <= max_row; i_row++){
          note_count = note_count + state_ctrl[active_seq][i_ch][active_audio_page[active_seq]][i_row][step_num];
        }
        if (note_count < max_random*4) { 
          state_ctrl[active_seq][i_ch][active_audio_page[active_seq]][rand_note][step_num] = 4;
        }
      } else {
        state_ctrl[active_seq][i_ch][active_audio_page[active_seq]][rand_note][step_num] = 0;
      }
    }
  }
}


//clears an entire page
void clear_page(int i_seq, int i_ch, int i_page){
  for (int i_row = 0; i_row < max_row; i_row++) {
    for (int i_col = 1; i_col < max_col-1; i_col++) {
      state_ctrl[i_seq][i_ch][i_page][i_row][i_col] = 0;
    }
  }
}



// ----------------------------- Mapping Arrays -----------------------------------------------------------------

void midi_map_init() {
  //major pentatonic
  midi_map[7] = -5;     //5th below
  midi_map[6] = -3;    //6th
  midi_map[5] = 0;    //root
  midi_map[4] = 2;    
  midi_map[3] = 4;    //maj 3rd
  midi_map[2] = 7;    //5th
  midi_map[1] = 9;
  midi_map[0] = 12;  //octave
}


void column_map_init() {
  //defines the LED column to the RGB column mapping on the LED driver
  column_map[0][R] = 0;
  column_map[0][G] = 1;
  column_map[0][B] = 2;
  
  column_map[1][R] = 3;
  column_map[1][G] = 4;
  column_map[1][B] = 5;
  column_map[2][R] = 6;
  column_map[2][G] = 7;
  column_map[2][B] = 8;
  column_map[3][R] = 9;
  column_map[3][G] = 10;
  column_map[3][B] = 11;
  column_map[4][R] = 12;
  column_map[4][G] = 13;
  column_map[4][B] = 14;
  
  
  column_map[5][R] = 16;
  column_map[5][G] = 17;
  column_map[5][B] = 18;
  column_map[6][R] = 19;
  column_map[6][G] = 20;
  column_map[6][B] = 21;
  column_map[7][R] = 22;
  column_map[7][G] = 23;
  column_map[7][B] = 24;
  column_map[8][R] = 25;
  column_map[8][G] = 26;
  column_map[8][B] = 27;
  column_map[9][R] = 28;
  column_map[9][G] = 29;
  column_map[9][B] = 30;
}



void colour_map_init() {
  //defines the LED state to colour palette mapping [seq][state_val][RGB] -> pwm value
  
  //Seq#0 *************************************
  //state 0 = off
  colour_map[0][0][R] = 0;
  colour_map[0][0][G] = 0;
  colour_map[0][0][B] = 0;
  
  colour_map[0][1][R] = 100;
  colour_map[0][1][G] = 0;
  colour_map[0][1][B] = 0;
  
  colour_map[0][2][R] = 0;
  colour_map[0][2][G] = 100;
  colour_map[0][2][B] = 0;
  
  colour_map[0][3][R] = 0;
  colour_map[0][3][G] = 0;
  colour_map[0][3][B] = 100;
  
  colour_map[0][4][R] = 50;
  colour_map[0][4][G] = 0;
  colour_map[0][4][B] = 100;
  
  
  //Seq#1 *************************************
  colour_map[1][0][R] = 0;
  colour_map[1][0][G] = 0;
  colour_map[1][0][B] = 0;
  
  colour_map[1][1][R] = 10;
  colour_map[1][1][G] = 10;
  colour_map[1][1][B] = 10;
  colour_map[1][2][R] = 10;
  colour_map[1][2][G] = 10;
  colour_map[1][2][B] = 10;
  colour_map[1][3][R] = 10;
  colour_map[1][3][G] = 10;
  colour_map[1][3][B] = 10;
  
  
  //Seq#2 *************************************
  colour_map[2][0][R] = 0;
  colour_map[2][0][G] = 0;
  colour_map[2][0][B] = 0;
  
  colour_map[2][1][R] = 10;
  colour_map[2][1][G] = 0;
  colour_map[2][1][B] = 10;
  colour_map[2][2][R] = 10;
  colour_map[2][2][G] = 0;
  colour_map[2][2][B] = 10;
  colour_map[2][3][R] = 10;
  colour_map[2][3][G] = 0;
  colour_map[2][3][B] = 10;
}



void button_map_init() {
  //maps the serial stream of button states from the shift registers to the row/col index
  //read 0 is P7 from U30, P6 ... P0 U30, then P7 U32...
  
  //----- Columns 1-4
  //U30
  button_map[0][R] = 0;
  button_map[1][R] = 0;
  button_map[2][R] = 0;
  button_map[3][R] = 0;
  button_map[0][C] = 2;
  button_map[1][C] = 3;
  button_map[2][C] = 4;
  button_map[3][C] = 1;
  
  button_map[4][R] = 1;  
  button_map[5][R] = 1;  
  button_map[6][R] = 1;  
  button_map[7][R] = 1;  
  button_map[4][C] = 1;
  button_map[5][C] = 3;
  button_map[6][C] = 4;
  button_map[7][C] = 2;
  
  //U32
  button_map[8][R] = 2;
  button_map[9][R] = 2;
  button_map[10][R] = 2;
  button_map[11][R] = 2;
  button_map[8][C] = 2;
  button_map[9][C] = 3;
  button_map[10][C] = 4;
  button_map[11][C] = 1;
  
  button_map[12][R] = 3;  
  button_map[13][R] = 3;  
  button_map[14][R] = 3;  
  button_map[15][R] = 3;  
  button_map[12][C] = 1;
  button_map[13][C] = 3;
  button_map[14][C] = 4;
  button_map[15][C] = 2;
  
  
  //U34
  button_map[16][R] = 4;
  button_map[17][R] = 4;
  button_map[18][R] = 4;
  button_map[19][R] = 4;
  button_map[16][C] = 2;
  button_map[17][C] = 3;
  button_map[18][C] = 4;
  button_map[19][C] = 1;
  
  button_map[20][R] = 5;  
  button_map[21][R] = 5;  
  button_map[22][R] = 5;  
  button_map[23][R] = 5;  
  button_map[20][C] = 1;
  button_map[21][C] = 3;
  button_map[22][C] = 4;
  button_map[23][C] = 2;
  
  //U36
  button_map[24][R] = 6;
  button_map[25][R] = 6;
  button_map[26][R] = 6;
  button_map[27][R] = 6;
  button_map[24][C] = 2;
  button_map[25][C] = 3;
  button_map[26][C] = 4;
  button_map[27][C] = 1;
  
  button_map[28][R] = 7;  
  button_map[29][R] = 7;  
  button_map[30][R] = 7;  
  button_map[31][R] = 7;  
  button_map[28][C] = 1;
  button_map[29][C] = 3;
  button_map[30][C] = 4;
  button_map[31][C] = 2;
  
  
  //----- Columns 5-8 ------------------
  //U37
  button_map[32][R] = 6;
  button_map[33][R] = 6;
  button_map[34][R] = 6;
  button_map[35][R] = 6;
  button_map[32][C] = 8;
  button_map[33][C] = 5;
  button_map[34][C] = 6;
  button_map[35][C] = 7;
  
  button_map[36][R] = 7;  
  button_map[37][R] = 7;  
  button_map[38][R] = 7;  
  button_map[39][R] = 7;  
  button_map[36][C] = 7;
  button_map[37][C] = 5;
  button_map[38][C] = 6;
  button_map[39][C] = 8;
  
  //U35
  button_map[40][R] = 4;
  button_map[41][R] = 4;
  button_map[42][R] = 4;
  button_map[43][R] = 4;
  button_map[40][C] = 8;
  button_map[41][C] = 5;
  button_map[42][C] = 6;
  button_map[43][C] = 7;
  
  button_map[44][R] = 5;  
  button_map[45][R] = 5;  
  button_map[46][R] = 5;  
  button_map[47][R] = 5;  
  button_map[44][C] = 7;
  button_map[45][C] = 5;
  button_map[46][C] = 6;
  button_map[47][C] = 8;
  
  //U33
  button_map[48][R] = 2;
  button_map[49][R] = 2;
  button_map[50][R] = 2;
  button_map[51][R] = 2;
  button_map[48][C] = 8;
  button_map[49][C] = 5;
  button_map[50][C] = 6;
  button_map[51][C] = 7;
  
  button_map[52][R] = 3;  
  button_map[53][R] = 3;  
  button_map[54][R] = 3;  
  button_map[55][R] = 3;  
  button_map[52][C] = 7;
  button_map[53][C] = 5;
  button_map[54][C] = 6;
  button_map[55][C] = 8;
  
  //U31
  button_map[56][R] = 0;
  button_map[57][R] = 0;
  button_map[58][R] = 0;
  button_map[59][R] = 0;
  button_map[56][C] = 8;
  button_map[57][C] = 5;
  button_map[58][C] = 6;
  button_map[59][C] = 7;
  
  button_map[60][R] = 1;  
  button_map[61][R] = 1;  
  button_map[62][R] = 1;  
  button_map[63][R] = 1;  
  button_map[60][C] = 7;
  button_map[61][C] = 5;
  button_map[62][C] = 6;
  button_map[63][C] = 8;
  
  
  //------ Menus ------------------
  //U39
  button_map[64][R] = 0;
  button_map[65][R] = 1;
  button_map[66][R] = 2;
  button_map[67][R] = 3;
  button_map[64][C] = 9;
  button_map[65][C] = 9;
  button_map[66][C] = 9;
  button_map[67][C] = 9;
  
  button_map[68][R] = 4;  
  button_map[69][R] = 5;  
  button_map[70][R] = 6;  
  button_map[71][R] = 7;  
  button_map[68][C] = 9;
  button_map[69][C] = 9;
  button_map[70][C] = 9;
  button_map[71][C] = 9;
  
  //U38
  button_map[72][R] = 3;
  button_map[73][R] = 2;
  button_map[74][R] = 1;
  button_map[75][R] = 0;
  button_map[72][C] = 0;
  button_map[73][C] = 0;
  button_map[74][C] = 0;
  button_map[75][C] = 0;
  
  button_map[76][R] = 7;  
  button_map[77][R] = 6;  
  button_map[78][R] = 5;  
  button_map[79][R] = 4;  
  button_map[76][C] = 0;
  button_map[77][C] = 0;
  button_map[78][C] = 0;
  button_map[79][C] = 0;

}

