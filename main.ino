/*
Basic Menu Navigation,
Background Service[2]: display elapsed time , cycle Drives when time overflows.[MAIN FUNCTIONALITY]
Setting - Modify saved Cooling Time, Fan Time.
Sound Effect when Drive Mode Changes.


*/


/*
Pin Layout
2 - IR LED
3 - Buzzer
4 - Button A
5 - Button B

6,7,8,9,10,11 - lcd 4,6,11,12,13,14

13 - DEBUG Mode (at Power on)
*/




#include <LiquidCrystal.h>
#include <EEPROM.h>
#include <math.h>

#define PIN_BUZZER 3

// IR PART
#define PIN_IR_LED 2


#define PIN_DEBUG_MODE 13
bool IS_DEBUG_MODE = false;

#define DEBUG_USE_SEC true

uint8_t ir_raw_sample_wind_1[4*17];
char ir_code[] = "b210040521000000b";

void hex_to_bin(char hex[],int length, uint8_t code[]) {

  uint8_t bdigit;
  int hdigit;
  for (int i = 0;i < length;i++) {

    // ascii => hex
    hdigit = (int)hex[i];
    if (hdigit < 58) {
      hdigit -= 48;
    }
    else {
      hdigit -= 87;
    }
    
    // Perform Single hex digit -> 4 bin digits.
    for (int digit = 3;digit >=0;digit--) {
      //1101
      //Printing order: 1101
      //assign to codes order: 1101 to 4th,3rd,2nd,1st.(3,2,1,0 index)

      bdigit = (hdigit&(1 << digit)) > 0 ? 1 : 0;
      code[4*i+digit] = bdigit;
    }
  }
}

void pulseIR(long microsecs){
  // we'll count down from the number of microseconds we are told to wait

  cli();  // this turns off any background interrupts
  while (microsecs > 0)
  {
    // 38 kHz is about 13 microseconds high and 13 microseconds low
    digitalWrite(PIN_IR_LED, HIGH);  // this takes about 3 microseconds to happen
    delayMicroseconds(10);         // hang out for 10 microseconds, you can also change this to 9 if its not working
    digitalWrite(PIN_IR_LED, LOW);   // this also takes about 3 microseconds
    delayMicroseconds(10);         // hang out for 10 microseconds, you can also change this to 9 if its not working

                     // so 26 microseconds altogether
    microsecs -= 26;
  }

  sei();  // this turns them back on
}
void change_mode(uint8_t code[], uint8_t mode){
  //mode 0: cool, mode 1: fan
  // ind: 1,2,3
  // COOL: 1,0,0 (1), FAN: 1,1,0 (3)
  mode = 2*mode+1;
  for(int i=0;i<3;i++){
    code[1-1+i] = mode>>i&1;
  }
}

void change_wd(uint8_t code[], uint8_t value){
  /*
  Wind Direction (order as remote ctrl) = binary =  value
  1,2,3,4,5 = 2,3,4,5,6 = 1,2,3,4,5
  345,234,123 = 7,9,11 = 6,7,8
  -> 
  */

  // convert value(1~8) to dec signal value.
  if(value<7){
    value+=1;
  }else{
    if(value==7){
      value=9;
    }else{
      value=11;
    }
  }


  for(int i=0;i<4;i++){
    code[35+i] = value>>i&1;
  } 
}
void change_wind_power(uint8_t code[], uint8_t value){
  // value: 1,2,3, index:6,7
  // 1,0  0,1  1,1
  for(int i=0;i<2;i++){
    code[6-1+i] = value>>i&i;  
  }

}

void change_light(uint8_t code[], bool on) {
  code[22-1] = on ? 1 : 0;
}

void change_temp(uint8_t code[], uint8_t temp) {
  /*
  temp: 16~30. 
  signal: 9,10,11,12 : 0~14
  signal: 65,66,67,68: (reversed) 5~19
  */
  temp -= 16;
  for (uint8_t digit = 0;digit < 4;digit++) {
    code[9-1 + digit] = temp >>digit & 1;
  }
  temp += 5;
  for (uint8_t digit = 0;digit < 4;digit++) {
    code[65-2 + digit] = temp >> digit & 1;
  }

}

void change_temp_and_mode(uint8_t code[],uint8_t temp,uint8_t mode){
  /*
  temp: 16~30. 
  signal: 9,10,11,12 : 0~14

  mode 0: cool, mode 1: fan
  ind: 1,2,3
  COOL: 1,0,0 (1), FAN: 1,1,0 (3)

  68,67,66,65 0010 + temp(0~14) + mode(1|3) = cool|fan.
  */
  

  temp -= 16;
  for (uint8_t digit = 0;digit < 4;digit++) {
    code[9-1 + digit] = temp >>digit & 1;
  }

  mode = 2*mode+1;
  for(int i=0;i<3;i++){
    code[1-1+i] = mode>>i&1;
  }

  uint8_t starting_hex=2;
  starting_hex+= temp +mode;

  for (uint8_t digit = 0;digit < 4;digit++) {
    code[65-2 + digit] = starting_hex >> digit & 1;
  }

}

void signal_with_code(uint8_t code[]) {
  int time = 0;
  //Header
  pulseIR(8680);
  delayMicroseconds(4500);

  // Content. Just send code in order. @Center, send Center too.
  for (int i = 0;i < 4*17;i++) {

    // Center Point. -1 for adjust index for header.
    if (i == 35 ) {
      pulseIR(600);
      delay(18);
      //delayMicroseconds(19300);
    }

    //on, 60
    pulseIR(600);

    //rest
    if (code[i] == (uint8_t) 0) {
      delayMicroseconds(600);
    }
    else {
      delayMicroseconds(1600);
    }
  }
}
#define COOL_TEMP 20 // Same as Fan(No Effect though)
void send_Cool(){
  change_light(ir_raw_sample_wind_1,true);
  change_wd(ir_raw_sample_wind_1,6);
  change_wind_power(ir_raw_sample_wind_1,3);


  change_temp_and_mode(ir_raw_sample_wind_1,COOL_TEMP,0);

  signal_with_code(ir_raw_sample_wind_1);
  
}
void send_Fan(){
  change_light(ir_raw_sample_wind_1, DEBUG_USE_SEC && IS_DEBUG_MODE);
  change_wd(ir_raw_sample_wind_1,6);
  change_wind_power(ir_raw_sample_wind_1,3);


  change_temp_and_mode(ir_raw_sample_wind_1,COOL_TEMP,1);

  signal_with_code(ir_raw_sample_wind_1);
}

// IR PART

uint8_t input;
uint8_t flag;
bool init_bit;
volatile unsigned long t_last_event=0;
unsigned int last_updated_time = -1;


#define BUTTON_AFTER_DELAY 100
#define BUTTON_LONG_CLICK 500
bool button_delay=true;
bool prevent_button_down=false;

void (*func_init)();
void (*func_loop)(int);


volatile int n_flag=0;

// LiquidCrystal lcd(13,12,11,10,9,8);
LiquidCrystal lcd(6, 7, 8, 9, 10, 11);

uint8_t buzzer_up[] = {80,10,99,6,120,10}; // *10. [herz,duration]
//uint8_t buzzer_down[]
uint8_t drive_state = 0;//0: Cooling 1: Fan.
uint8_t setting_state=0;

struct Drive_Mode{
  uint8_t time;
  uint8_t digit;
  uint8_t max;
  uint8_t min;
};
unsigned long drive_started_millis=0;

struct Drive_Mode drive_modes[2] = {
  { // Cooling Mode
    .time=20,
    .digit=2,
    .max=120,
    .min=10
  },{
    // Fan Mode
    .time=40,
    .digit=2,
    .max=120,
    .min=10
  }
};

uint8_t get_int_length(int num){
  return num==0? 1:floor(log10(num))+1;
}

void play_sound(uint8_t sound){
  if(sound==0){
    for(int i=0;i<3;i++){
      tone(PIN_BUZZER,10*buzzer_up[2*i],10*buzzer_up[2*i+1]);
      delay(10*buzzer_up[2*i+1]);
    }
  }else{
    for(int i=2;i>=0;i--){
      tone(PIN_BUZZER,10*buzzer_up[2*i],10*buzzer_up[2*i+1]);
      delay(10*buzzer_up[2*i+1]);
    }
  }
}

void _lcd_clear(uint8_t line){
  lcd.setCursor(0,line);
  lcd.print("                ");
}

#define INPUT_A bit(1)
#define INPUT_A_LONG bit(2)
#define INPUT_B bit(3)
#define INPUT_B_LONG bit(4)

uint8_t get_input(){
  // 0: no input. 1,2 single,dbl click A. 3,4 : B.
  cli();
  uint8_t temp = 0;
  if( flag &  bit(0)){
    if(flag & bit(2)){
      temp |= bit(1);
    }
    flag &=~bit(0);
  }else if( flag & bit(1)){
    if(millis()-t_last_event>BUTTON_LONG_CLICK){
      temp |= bit(2);
      //flag |= bit(2);
      flag &= ~(bit(1)|bit(0));

      prevent_button_down=true;
    }
  }
  else if(flag & bit(3) ){

    if(flag & bit(5)){
      temp |= bit(3);
    }
    flag &=~bit(3);

  }else if (flag & bit(4)){
    if(millis()-t_last_event>BUTTON_LONG_CLICK){
      temp |= bit(4);
      //flag |= bit(5);
      flag &= ~(bit(4)|bit(3));
      prevent_button_down=true;
    }
  }else{
    sei();
    return 0;
  }
  sei();
  return temp;

}


ISR(PCINT2_vect){
  if(!(flag&bit(1)) && PIND & bit(4) && (millis()-t_last_event>30)){
    if(prevent_button_down){
      flag &=~bit(2);
      return;
    }

    flag |= (bit(1) | bit(0));
    flag &= ~bit(2);
    t_last_event=millis();
  }else if( !(flag&bit(2)) && !(PIND &bit(4)) && (millis()-t_last_event>30)){
    if(prevent_button_down){
      prevent_button_down=false;
      t_last_event=millis();
      flag |= bit(2);
      return;
    }
    flag |= (bit(2) | bit(0));
    flag &= ~bit(1);
    t_last_event=millis();

  }

  else if( !(flag&bit(4)) && PIND & bit(5) && (millis()-t_last_event>30) ){
    if(prevent_button_down){
      flag &=~bit(5);
      return;
    }

    flag |= (bit(4) | bit(3));
    flag &= ~bit(5);
    t_last_event=millis();
  }else if( !(flag&bit(5)) && !(PIND &bit(5)) && (millis()-t_last_event>30)){
    if(prevent_button_down){
      prevent_button_down=false;
      t_last_event=millis();
      flag |= bit(5);
      return;
    }
    flag |= (bit(5) | bit(3));
    flag &= ~bit(4);
    t_last_event=millis();
  }
}

void setup_ISR(){
  PCMSK2 |= (bit(PCINT20)|bit(PCINT21));
  PCIFR |= bit(PCIF2);
  PCICR |= bit(PCIE2);

  flag |= bit(2); // 2 1 0
  flag |= bit(5); // 5 4 3

}

void Main_init();
void Main_loop();
void Setting_init();
void Setting_loop();

void Mode_Change_Now(){
  _lcd_clear(0);
  lcd.setCursor(0,0);
  if(drive_state==0){
    drive_state=1;
    send_Fan();
    lcd.print("Now Fan");
    if(IS_DEBUG_MODE){
      lcd.print(" DEBUG");
    }
    play_sound(drive_state);
    
    
  }else{
    drive_state=0;
    send_Cool();
    lcd.print("Now Cooling");
    if(IS_DEBUG_MODE){
      lcd.print(" DEBUG");
    }
    play_sound(drive_state);
  }

  drive_started_millis=millis();
  last_updated_time = -1;
  _lcd_clear(1);
}

void Main_init(){
  _lcd_clear(0);
  lcd.setCursor(0,0);
  lcd.print("Main");
  if(IS_DEBUG_MODE){
    lcd.print(" DEBUG");
  }

}
void Main_loop(int inp){
  switch(inp){
    case INPUT_A:
      // CHANGE MODE NOW
        Mode_Change_Now();

      break;
    case INPUT_A_LONG:
      _lcd_clear(0);
      lcd.setCursor(0,0);
      lcd.print("Setting");
      delay(700);
      _lcd_clear(0);
      delay(300);
      func_init = &Setting_init;
      func_loop = &Setting_loop;
      init_bit=true;

      return;

  }

}


void Setting_init(){
  switch(setting_state){
    case 0: // Setting CT
      _lcd_clear(0);
      _lcd_clear(1);
      lcd.setCursor(0,0);
      lcd.print("CT : ");
      break;
    case 1: // Setting FT
      _lcd_clear(0);
      _lcd_clear(1);
      lcd.setCursor(0,0);
      lcd.print("FT : ");
      break;
  }
  lcd.setCursor(5,0);
  lcd.print(drive_modes[setting_state].time);
}


void Setting_loop(int inp){
  struct Drive_Mode *mode;
  mode = &drive_modes[setting_state];
  switch(inp){
    case INPUT_A:
      if(mode->time+10<=mode->max){
        mode->time+=10;
        mode->digit = get_int_length(mode->time);
        init_bit=true;
      }
      break;
    case INPUT_B:
      if(mode->time-10 >= mode->min){
        mode->time-=10;
        mode->digit = get_int_length(mode->time);
        init_bit=true;
      }
      init_bit=true;
      break;
    case INPUT_A_LONG:
      if(setting_state==0){
        setting_state=1;
        init_bit=true;
        return;
      }else{
        // SAVE CHANGES.
        EEPROM.update(0,1);
        EEPROM.update(1,drive_modes[0].time);
        EEPROM.update(2,drive_modes[1].time);


        setting_state=0;
        func_init = &Main_init;
        func_loop = &Main_loop;
        init_bit=true;
        return;

      }
  }
}

unsigned int convert_millis_to_elapsed(unsigned long current_millis){
  // in Minutes. Seconds in Debug mode.
  return DEBUG_USE_SEC && IS_DEBUG_MODE ? (millis()-current_millis)/1000 : (millis()-current_millis)/60000;

}

void background_cycle_drive_modes(){
  if(convert_millis_to_elapsed(drive_started_millis)>=drive_modes[drive_state].time){
    Mode_Change_Now();
  }
}

void background_display_time(){ 
  unsigned int now = convert_millis_to_elapsed(drive_started_millis);
  if(last_updated_time!=now){
    last_updated_time = now;
    uint8_t cursor = drive_modes[drive_state].digit -get_int_length(now);
    lcd.setCursor(cursor,1);
    lcd.print(now);
    lcd.print("/");
    lcd.print(drive_modes[drive_state].time);
  }
}
void empty(){}

void (*func_background[2])() = {&background_display_time,&background_cycle_drive_modes};

bool isDefaultSetting(){
  return (uint8_t) EEPROM.read(0)==1? false:true;
}


void setup(){
  setup_ISR();
  lcd.begin(16,2);

  func_init = &Main_init;
  func_loop = &Main_loop;

  init_bit=true;

  // Serial.begin(115200);
  // Serial.println("Setup");

  if(!isDefaultSetting()){
    drive_modes[0].time = (uint8_t) EEPROM.read(1);
    drive_modes[0].digit = get_int_length(drive_modes[0].time);
    drive_modes[1].time = (uint8_t) EEPROM.read(2);
    drive_modes[1].digit = get_int_length(drive_modes[1].time);
  }

  pinMode(PIN_IR_LED,OUTPUT);

  hex_to_bin(ir_code,17,ir_raw_sample_wind_1);

  // Start with Cool: Fan + ModeChangeNow

  IS_DEBUG_MODE = (PIND >> PIN_DEBUG_MODE & 1) == 1;
  drive_state=1;
  Mode_Change_Now();
}



void loop(){

  input = get_input();

  for(int f_i=0;f_i<2;f_i++){
    func_background[f_i]();
  }

  if(init_bit){
    func_init();
    init_bit=false;
  }

  func_loop(input);
}



