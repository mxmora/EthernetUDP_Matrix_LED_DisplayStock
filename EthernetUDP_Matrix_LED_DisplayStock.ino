

/*
  UDPSendReceiveString:
  This sketch receives UDP message strings, prints them to the serial port
  and sends an "acknowledge" string back to the sender

  A Processing sketch is included at the end of file that can be used to send
  and received messages for testing with a computer.

  created 21 Aug 2010
  by Michael Margolis

  This code is in the public domain.
*/

#define SPLASH_MESSAGE "Network LED Display vers 1.0.2"
#define kTimeout (3000)

// These define's must be placed at the beginning before #include "TimerInterrupt.h"
// _TIMERINTERRUPT_LOGLEVEL_ from 0 to 4
// Don't define _TIMERINTERRUPT_LOGLEVEL_ > 0. Only for special ISR debugging only. Can hang the system.
#define TIMER_INTERRUPT_DEBUG         0
#define _TIMERINTERRUPT_LOGLEVEL_     0

#define USE_TIMER_1     true

#if ( defined(__AVR_ATmega644__) || defined(__AVR_ATmega644A__) || defined(__AVR_ATmega644P__) || defined(__AVR_ATmega644PA__)  || \
        defined(ARDUINO_AVR_UNO) || defined(ARDUINO_AVR_NANO) || defined(ARDUINO_AVR_MINI) ||    defined(ARDUINO_AVR_ETHERNET) || \
        defined(ARDUINO_AVR_FIO) || defined(ARDUINO_AVR_BT)   || defined(ARDUINO_AVR_LILYPAD) || defined(ARDUINO_AVR_PRO)      || \
        defined(ARDUINO_AVR_NG) || defined(ARDUINO_AVR_UNO_WIFI_DEV_ED) || defined(ARDUINO_AVR_DUEMILANOVE) || defined(ARDUINO_AVR_FEATHER328P) || \
        defined(ARDUINO_AVR_METRO) || defined(ARDUINO_AVR_PROTRINKET5) || defined(ARDUINO_AVR_PROTRINKET3) || defined(ARDUINO_AVR_PROTRINKET5FTDI) || \
        defined(ARDUINO_AVR_PROTRINKET3FTDI) )
  #define USE_TIMER_2     true
  #warning Using Timer1, Timer2
#else          
  #define USE_TIMER_3     true
  #warning Using Timer1, Timer3
#endif

#include "TimerInterrupt.h"

#define TIMER1_INTERVAL_MS    1
#define TIMER_INTERVAL_MS     2

#define USE_ENET 1
#define USE_BONJOUR 0

#include <MD_Parola.h> 
#include <MD_MAX72xx.h>
//#include <SPI.h>               // needed for Arduino versions later than 0018

#if USE_ENET
#include <Ethernet.h>
//#include <EthernetUdp2.h>     // UDP library from: bjoern@cs.stanford.edu 12/30/2008
#include <EthernetBonjour.h>
#endif



// Turn on debug statements to the serial output
#define  DEBUG  1

#if  DEBUG
#define PRINT(s, x) { Serial.print(F(s)); Serial.print(x); }
#define PRINTS(x) Serial.print(F(x))
#define PRINTX(x) Serial.println(x, HEX)
#else
#define PRINT(s, x)
#define PRINTS(x)
#define PRINTX(x)
#endif

// Define the number of devices we have in the chain and the hardware interface
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW //PAROLA_HW
#define MAX_DEVICES 16
#define USE_SW_DISPLAY 1

#if USE_SW_DISPLAY
#define   CLK_PIN   13
#define   DATA_PIN  11
#define   CS_PIN    10
// SOFTWARE SPI
MD_Parola P = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);
#else
#define CLK_PIN   52
#define DATA_PIN  51
#define CS_PIN    53
// HARDWARE SPI
MD_Parola P = MD_Parola(CS_PIN, MAX_DEVICES);
#endif


int watch_dog = 0;
int handle_ethernet_calls = 0;

int tim = 150; //the value of delay time
int INTERVAL = 100;
int countDown;

uint8_t frameDelay = 1;  // default frame delay value
textEffect_t  scrollEffect = PA_SCROLL_LEFT;

// Global message buffers shared by Serial and Scrolling functions
#define BUF_SIZE  1024
char curMessage[BUF_SIZE] = SPLASH_MESSAGE;
char temp[24]= {};
char newMessage[BUF_SIZE] = "This is a test";
bool newMessageAvailable = false;

#if USE_ENET
// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = {
  0x90, 0xA2, 0xDA, 0x10, 0x6C, 0x50
};

IPAddress ip(192, 168, 1, 55);
IPAddress myDns(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 0, 0);

unsigned int localPort = 8888;              // local port to listen on

// buffers for receiving and sending data
//char packetBuffer[UDP_TX_PACKET_MAX_SIZE];  //buffer to hold incoming packet,
char packetBuffer[BUF_SIZE];  //buffer to hold incoming packet,

char  ReplyBuffer[] = "acknowledged";       // a string to send back

// An EthernetUDP instance to let us send and receive packets over UDP
EthernetUDP Udp;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~``
void ipAddressToString(IPAddress inIP, char* inStr, int port)
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~``
{
  char tempStr[10] = {0};
  if (inStr) {
    for (int i = 0; i < 4; i++) {
      itoa(inIP[i], tempStr, 10);
      strcat(inStr, tempStr);
      if (i < 3) {
        strcat(inStr, ".");
      }
    }
    if (port) {
      strcat(inStr, ":");
      itoa(port, tempStr, 10);
      strcat(inStr, tempStr);
    }
  }
}
#endif

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~``
void UpdateMessages(char * msg)
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~``
{
    Serial.println("UpdateMessages");
    P.displayClear();
    P.displayText(msg, PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~``
void InitEthernet(void)
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~``
{
    int result = 0;
#if USE_ENET
  Serial.println("starting Ethernet...");
  result = Ethernet.begin(mac);
  if (0 == result)
  {
    Serial.println("DHCP failed");
  }
  else if (1 == result)
  {
    Serial.println("DHCP success");
  }
  else
  {
    Serial.print(" Ethernet.begin result=");
    Serial.println(result);
  }
  //Ethernet.begin(mac,ip);
  //Ethernet.begin(mac,ip,myDns, gateway, subnet);

#if USE_BONJOUR  
    EthernetBonjour.begin("arduino");
    EthernetBonjour.addServiceRecord("LEDSegDisp._StockDisplay",
                                   localPort,
                                   MDNSServiceUDP);
                                   
#endif

  Udp.begin(localPort);
  Serial.print("Udp.begin(");
  Serial.print(localPort);
  Serial.println(")");
  IPAddress addr = Ethernet.localIP();

  sprintf(newMessage, "Chat server address: ");

  ipAddressToString(Ethernet.localIP(), newMessage, localPort);

  Serial.print("Chat server address:");
  Serial.print(Ethernet.localIP());
  Serial.print(":");
  Serial.println(localPort);
  Serial.println(newMessage);
  Serial.print("UDP_TX_PACKET_MAX_SIZE:");
  Serial.println(UDP_TX_PACKET_MAX_SIZE);
  newMessageAvailable = true;
  UpdateMessages("Ethernet Setup done.");
  P.displayScroll(newMessage, PA_LEFT, scrollEffect, frameDelay);
     
   
#endif
}


void ManageDisplay(void);
void TimerHandler1(void)
{

  ManageDisplay();

}

void TimerHandler(void)
{
  ManageDisplay();
}

void SetUpTimer()
{

  Serial.print(F("\nStarting Argument_None on "));
  Serial.println(BOARD_TYPE);
  Serial.println(TIMER_INTERRUPT_VERSION);
  Serial.print(F("CPU Frequency = ")); Serial.print(F_CPU / 1000000); Serial.println(F(" MHz"));

  // Timer0 is used for micros(), millis(), delay(), etc and can't be used
  // Select Timer 1-2 for UNO, 1-5 for MEGA, 1,3,4 for 16u4/32u4
  // Timer 2 is 8-bit timer, only for higher frequency
  // Timer 4 of 16u4 and 32u4 is 8/10-bit timer, only for higher frequency
  
  ITimer1.init();

  // Using ATmega328 used in UNO => 16MHz CPU clock ,
  // For 16-bit timer 1, 3, 4 and 5, set frequency from 0.2385 to some KHz
  // For 8-bit timer 2 (prescaler up to 1024, set frequency from 61.5Hz to some KHz

  if (ITimer1.attachInterruptInterval(TIMER1_INTERVAL_MS, TimerHandler1))
  {
    Serial.print(F("Starting  ITimer1 OK, millis() = ")); Serial.println(millis());
  }
  else
    Serial.println(F("Can't set ITimer1. Select another freq. or timer"));

#if USE_TIMER_2
 
  // Select Timer 1-2 for UNO, 0-5 for MEGA, 1,3,4 for 32u4
  // Timer 2 is 8-bit timer, only for higher frequency
  ITimer2.init();

  if (ITimer2.attachInterruptInterval(TIMER_INTERVAL_MS, TimerHandler))
  {
    Serial.print(F("Starting  ITimer2 OK, millis() = ")); Serial.println(millis());
  }
  else
    Serial.println(F("Can't set ITimer2. Select another freq. or timer"));

#elif USE_TIMER_3

  // Select Timer 1-2 for UNO, 0-5 for MEGA, 1,3,4 for 32u4
  // Timer 3 is 16-bit timer
  ITimer3.init();

  if (ITimer3.attachInterruptInterval(TIMER_INTERVAL_MS, TimerHandler))
  {
    Serial.print(F("Starting  ITimer3 OK, millis() = ")); Serial.println(millis());
  }
  else
    Serial.println(F("Can't set ITimer3. Select another freq. or timer"));  
#endif
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~``
void setup() 
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~``
{

  countDown = INTERVAL;

  Serial.begin(115200);

  P.begin();
  P.displayClear();
  P.displaySuspend(false);
  
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  Serial.println("serial started");
  // start the Ethernet and UDP:

  InitEthernet();
  UpdateMessages("Ethernet Setup done.");
    
  Serial.println("end setup");
  P.displayScroll(curMessage, PA_LEFT, scrollEffect, frameDelay);
  SetUpTimer();

}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~``
void ScheduleMessage(void)
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~``
{
    P.displayClear();
    delay(100);
    //P.displaySuspend(true);
    P.setTextBuffer(curMessage);
    P.setTextEffect(scrollEffect,PA_SCROLL_UP);
    P.setTextAlignment( PA_LEFT);
    P.setSpeed( frameDelay);
    P.setPause( 3000);
    //P.displaySuspend(false);
    P.displayReset();
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~``
void kickWatchDog()
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~``
{
  static int last_count = 0;
  
  watch_dog +=1;
  if (0 == (watch_dog % kTimeout))
  {
     Serial.print("kick:");
     Serial.println(handle_ethernet_calls);
     if (last_count == handle_ethernet_calls)
     {
         Serial.println("## Is it stuck? ## ");
     }
     last_count = handle_ethernet_calls;
     
     //watch_dog = 0;
  }
}


// non blocking test display
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~``
void ManageDisplay()
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~``
{
  if (P.displayAnimate())
  {
    if (newMessageAvailable)
    {
          //Serial.println("ManageDisplay newMessageAvailable cur:");
          //Serial.println(curMessage);
          //Serial.println("ManageDisplay newMessageAvailable new:");
          //Serial.println(newMessage);
          strcpy(curMessage, newMessage);
          memset(newMessage,0,BUF_SIZE);
          //P.setTextBuffer(curMessage);
          ScheduleMessage();
          newMessageAvailable = false;
    }
    else
    {
      //P.displayReset();
    }
  }
  kickWatchDog();
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppendMessage(char *dst, char *src, int len)
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
{
   Serial.println("AppendMessage:");
    Serial.println(src);
    Serial.println(dst);
    
    int curlen = strlen(dst);
    if (curlen + len > BUF_SIZE)
    {
      len = BUF_SIZE-2;
    }
    
    // clear out any 0 bytes
    for (int i = 0 ; i < len ; i++)
    {
      if (src[i] == 0)
          src[i] = ' '; 
    }

   
    strcat(dst,src);
    Serial.println("AppendMessage: after");
    Serial.println(dst);
     
}

#if USE_ENET
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void HandleEthernet()
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
{
  // if there's data available, read a packet
  int packetSize = Udp.parsePacket();
  if (packetSize && packetSize != -1 ) 
  {

    Serial.print("Received packet of size ");
    Serial.println(packetSize);
    if (packetSize > BUF_SIZE )
    {
      packetSize = BUF_SIZE - 4;
      Serial.print("resized packetSize now: ");
      Serial.println(packetSize);
    }

    Serial.print("From ");
    IPAddress remote = Udp.remoteIP();
    for (int i = 0; i < 4; i++) {
      Serial.print(remote[i], DEC);
      if (i < 3) {
        Serial.print(".");
      }
    }
    Serial.print(", port ");
    Serial.println(Udp.remotePort());

    // read the packet into packetBufffer
    Udp.read(packetBuffer, BUF_SIZE);
    
    Serial.println("Contents:");
    Serial.println(packetBuffer);
    //memset (newMessage, 0, BUF_SIZE);

    AppendMessage(newMessage,packetBuffer,packetSize);
    
    newMessageAvailable = true;
    Serial.println(newMessage);
    memset (packetBuffer, 0, BUF_SIZE);
    //P.displayScroll(newMessage, PA_LEFT, scrollEffect, frameDelay);
    //P.displayClear();
    //ScheduleMessage();

    // send a reply to the IP address and port that sent us the packet we received
    Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
    Udp.write(ReplyBuffer);
    Udp.endPacket();
  }
  // delay(10);
  if ( Ethernet.maintain() == 2 )
  {
    Serial.print("New Chat server address:");
    Serial.println(Ethernet.localIP());
  }
  handle_ethernet_calls += 1;
}
#endif

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~``
void loop() 
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~``
{

#if USE_ENET
#if USE_BONJOUR
  EthernetBonjour.run();
#endif

  HandleEthernet();
#endif

 // ManageDisplay();
}
