
#include "Arduino.h"
#include <SPI.h>
#include <WiFi.h>
 #include "WiFiClient.h"
#include "HTTPClient.h"
#include "Audio.h"
#include <SD.h>
#include <FS.h>
#include "bb_spi_lcd.h"
#include "7segment60pt7b.h"
#include <FileConfig.h>

#define CYD_22C
#ifdef CYD_22C
//#define TOUCH_CAPACITIVE
#define TOUCH_SDA -1
#define TOUCH_SCL -1
#define TOUCH_INT -1
#define TOUCH_RST -1
#define LCD DISPLAY_CYD_22C
#endif

#define SD_CS 5
#define SPI_MOSI 23
#define SPI_MISO 19
#define SPI_SCK  18

#define PINL 3 // change to -1 if not connected or use PinTouch=.... in config file

BB_SPI_LCD tft;
unsigned int disoff=0;

Audio audio (true, I2S_DAC_CHANNEL_LEFT_EN);

String ssid = "****"; //complete or use SSID=.... in config file
String password = "****"; //complete or use Password=.... in config file
bool no_wif = false;
short PinL = PINL;
#define  MAXMP3 100
String mp3tab[MAXMP3];
char menbut[20] = "PS+-<>";
int mp3idx = 0;
String exPand="";
char disnam[25] = "123456789012345678901234";
String stations[] = {
  "http://stream.srg-ssr.ch/m/rsj/mp3_128",
  "http://icecast.vrtcdn.be/stubru_tijdloze-high.mp3",
  "http://icecast.vrtcdn.be/klara-high.mp3",
  "http://icecast.vrtcdn.be/mnm_hits-high.mp3",
  "http://progressive-audio.vrtcdn.be/content/fixed/11_11niws-snip_hi.mp3",
  ""
};

File dir;

void playmp3 (const char *fnam);

#include <time.h>
#include <ESP32Time.h>
void
settim ()
{
  configTzTime ("CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00",
		"pool.ntp.org");
  Serial.print ("Waiting for NTP time sync: ");
  time_t now = time (nullptr);
  while (now < 8 * 3600 * 2)
    {
      delay (100);
      Serial.print (".");
      now = time (nullptr);
    }
  Serial.println ();

  struct tm timeinfo;
  gmtime_r (&now, &timeinfo);
  Serial.printf ("%s %s", tzname[0], asctime (&timeinfo));
}
 
bool
touch (int pin)
 {if (pin > 0)
    return digitalRead (pin) == 1;
  static unsigned long psta;
  bool r;
  r = (millis () - psta) > 8000;
  if (r)
    psta = millis ();
  return r;
}

int 
gettim ( )
{
  tm rtcTime;
  char buf[80];
  unsigned long dif=0;
  getLocalTime (&rtcTime, 20);
  strftime (buf, sizeof (buf), "%H:%M", &rtcTime);	 
  tft.fillScreen (0x0000);
  tft.setFreeFont (&DSEG7_Classic_Regular_90); 
  tft.setTextColor (TFT_ORANGE, TFT_BLACK);
  int h, s, m, ph = 99, pm = 99;
  h=rtcTime.tm_hour;m=rtcTime.tm_min;
  for (s = 0; s < 3;)
    {
      if (ph != h || pm != m)
	{
	  sprintf (buf, "%02d:%02d", 0 % 24, 0 % 60);
	  tft.fillRect (5, 65, 235, 170, TFT_BLACK);
	  sprintf (buf, "%02.2d:%02.2d", h % 24, m % 60);
	  tft.drawString (buf, 5, 165, TFT_ORANGE);
	  ph = h;
	  pm = m;
	}
      if (touch (PinL))
	{
	  delay (500);
	  if (touch (PinL))
	    {
	      s == 0 ? h++ : s == 1 ? m++ : m;
	    }
	  else
	    {
	      if (s < 1)
		s++;
	      else
		break;
	    }
	}
      delay (50);
    }
  return((h%24)*60+(m%60));
}
void settim(int houmin)
  {ESP32Time rtc (0);		 
   rtc.setTime (0, houmin%60, houmin/60, 1, 1, 2025);
  }

void
conwif ()
{
  tft.println ("Connecting Wifi\n");
  int i;
  for (i = 0; i < 150; i++)
    {
      if ((i % 10) == 0)
          	WiFi.begin (ssid.c_str (), password.c_str ());
      if (WiFi.status () == WL_CONNECTED)
	        break;
      delay (200);
      Serial.print (".");
      tft.print ("+");
    }
  if (i == 150)
    {
      no_wif = true;
      settim(gettim ());
    }
  else
    {
      tft.println ("\nConnected, setting time\n");
      settim ();
    }
}

void
inidis ()
{
  tft.begin (LCD);
  tft.fillScreen (TFT_BLACK);
  tft.setTextSize (1.0);
  tft.setRotation (1);
  tft.setBrightness (255);
  tft.setFont (FONT_12x16);	
  tft.setTextColor (0xFFFF);	
  tft.drawString ("Radio-mp3 player", 20, 20);
  tft.setCursor (20, 40);
}

void
inisdc ()
{
  pinMode (SD_CS, OUTPUT);
  digitalWrite (SD_CS, HIGH);
  SPI.begin (SPI_SCK, SPI_MISO, SPI_MOSI, SD_CS);
  SD.begin (SD_CS, SPI);
  dir = SD.open ("/");
}

void
get_SD ()
{
  Serial.println ("Building list");
  tft.setFont (FONT_12x16);	// LovyanGFX Fonts
  tft.setTextColor (0xFFFF);
  if (mp3idx == 0)
    for (; stations[mp3idx] != ""; mp3idx++)
      mp3tab[mp3idx] = stations[mp3idx];
  tft.println ("scanning mem card");
  if (!dir && (!(dir = SD.open ("/"))))
    {
      Serial.println ("failed to openSD");
      tft.println ("failed to openSD");
      return;
    }
  for (; mp3idx < MAXMP3;)
    {
      File file = dir.openNextFile ();
      if (file)
	{
	  char fnam[256];
	  strcpy (fnam, "/");
	  strcat (fnam, file.name ());
	  //  Serial.printf("%d %s\n",mp3idx,fnam);
	  if (String (file.name ()).endsWith (".mp3"))
	    {
	      mp3tab[mp3idx] = String (fnam);
	      mp3idx++;
	      if ((mp3idx % 10) == 0)
		{
		  Serial.printf ("%d %s\n", mp3idx, fnam);
		  tft.drawString (fnam, 10, 200);
		}
	    }
	}
      else
	{
	  dir.close ();
	  break;
	}
    }
  if (mp3idx == MAXMP3)
    dir.close ();
}

FileConfig cfg;
void
inicon ()
{
  int maxLineLength = 130;
  int maxSectionLength = 20;
  bool ignoreCase = true;
  bool ignoreError = true;
  if (cfg.begin
      (SD, "/radioweb.cfg", maxLineLength, maxSectionLength, ignoreCase,
       ignoreError))
    {
      while (cfg.readNextSetting ())
	{
	  if (cfg.nameIs ("Station") ||
	      cfg.nameIs ("Station1") ||
	      cfg.nameIs ("Station2") ||
	      cfg.nameIs ("Station3") ||
	      cfg.nameIs ("Station4") ||
	      cfg.nameIs ("Station5") ||
	      cfg.nameIs ("Station6") ||
	      cfg.nameIs ("Station7") ||
	      cfg.nameIs ("Station8") || cfg.nameIs ("Station9"))
	    {
	      mp3tab[mp3idx++] = cfg.copyValue ();
	    }
	  else if (cfg.nameIs ("Menu"))
	    {
	      String s;
        s=cfg.copyValue();
        strncpy(menbut,s.c_str(),15);
	    }
	  else if (cfg.nameIs ("SSID"))
	    {
	      ssid = cfg.copyValue ();
	    }
	  else if (cfg.nameIs ("Password"))
	    {
	      password = cfg.copyValue ();
	    }
	  else if (cfg.nameIs ("PinTouch"))
	    {
	      PinL = cfg.getIntValue ();
	    }
	  else if (cfg.nameIs ("Expand"))
	    {
	      exPand = cfg.copyValue ();
	    }
	  else
	    Serial.println (cfg.getName ());
	}
    }
}
void expand()
{if (exPand=="" || no_wif) return;
 HTTPClient http;
 http.begin(exPand.c_str());
 int httpResponseCode = http.GET();
 if (httpResponseCode>0) 
 {String payload = http.getString();
  int a;
  while((a=payload.indexOf("\n"))>=0)
  {mp3tab[mp3idx] =payload.substring(0,a);
   mp3tab[mp3idx].replace(" ","%20");
   payload=payload.substring(a+1);
   mp3idx++;
  }	    
}
 else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }
 http.end(); 
}

void
setup ()
{
  Serial.begin (115200);
  inisdc ();
  inicon ();
  delay (100);
  inidis ();
  conwif ();
  expand ();
  get_SD ();
  //audio.setBalance(-16);
  //audio.forceMono(true);
  audio.setVolume (19);		// 0...21
  audio.setConnectionTimeout(2000,6000);
  if (PinL > -1)
    pinMode (PinL, INPUT);
  //WiFi.disconnect();
  playmp3 ("");
}

char *
shonam (char *s)
{
  if (strncmp (s, "http", 4))
    return s;
  return strrchr (s, '/');
}

static int pmen = 888;
void
dismen (int men)
{
  if (pmen != men)
    {
      for (int i = 0; i < strlen (menbut); i++)
	{
	  tft.setCursor (((i + 1) * (280 / strlen (menbut))), 210);
	  tft.setTextColor (men == i ? TFT_RED : TFT_BLUE, TFT_BLACK);
	  tft.print (menbut[i]);
	}
      pmen = men;
    }
}

static int psta = 8888;
static int pidx = 888;
void
play_SD (int sta, int idx, int men)
{
  if ((psta != sta) || (pidx != idx) || (pmen != men))
    {
      if (psta != sta)
	{
	  tft.fillScreen (0x0000);
	}
       tft.setFont (FONT_12x16);	//FreeMonoBold12pt7b); // LovyanGFX Fonts
       tft.setTextColor (TFT_BLUE);
      tft.setCursor (10, 10);
      for (int j = 0; j < 12 && (j + sta * 12 < mp3idx); j++)
	{
	  if (j == idx)
	    tft.setTextColor (TFT_WHITE);
	  if ((psta != sta) || (j == pidx) || (j == idx))
	    {
	      char buf[120];
	      strcpy (buf, mp3tab[j + sta * 12].c_str ());
	      tft.drawString (shonam (buf), 15, 15 + 15 * j);
	    }
	  tft.setTextColor (TFT_BLUE);
	}
      psta = sta;
      pidx = idx;
    }
}

void
panelt (bool force = false)
{
  tm rtcTime;
  char buf[80];
  static char pbuf[12] = "";
  static char pbis[25] = "";
  bool allgon=false;
  getLocalTime (&rtcTime, 20);
  strftime (buf, sizeof (buf), "%H:%M", &rtcTime);	// hh:mm:ss
   if (strcmp (buf, pbuf) || force)
    {
      strcpy (pbuf, buf);
      psta = 888;
      tft.fillScreen (0x0000);allgon=true;	// BLACK
      tft.setFreeFont (&DSEG7_Classic_Regular_90);	//FreeMonoBold12pt7b); // LovyanGFX Fonts
       tft.setTextSize (1.0);	// text H pos
      tft.setTextColor (TFT_ORANGE);
      tft.drawString (buf, 5, 165);
    }
    if(strcmp(disnam,pbis)|| allgon)
    {tft.setFont (1);
      tft.setTextSize (0.2);
      tft.setTextColor (TFT_BLUE);
      sprintf (buf, "%d.%d.%d.%d", WiFi.localIP ()[0], WiFi.localIP ()[1],
	       WiFi.localIP ()[2], WiFi.localIP ()[3]);
      tft.drawString (buf, 20, 220);
      tft.setFont (FONT_12x16);
      tft.drawString ("                       ", 10, 30);
      tft.drawString (disnam, 10, 30);
      strcpy(pbis,disnam);
    }

}

bool revers = true;
unsigned long slened(int houmin)
{tm rtcTime;
  unsigned long dif=0,sta=0;
  getLocalTime (&rtcTime, 20);
  sta=rtcTime.tm_hour*60+rtcTime.tm_min;
  if (sta>houmin) houmin+=24*60;
   dif=houmin-sta;
return dif;
  }
void
playmp3 (const char *fnam)
{
  static int i = 0;
  static int pi=-1;
  static int sta = 0;
  static int sel = 0;
  static int men = 0;
  static int n = 0;
  if (PinL < 0 || !strcmp (fnam, "nxt"))
    i = random (0, mp3idx);
  else
  {for (bool obr = false; !obr;)
   {play_SD (sta, sel, -1);
    delay(300);
    while(touch(PinL))
	  {men = (men + 1) % strlen (menbut);
     dismen(men);
     delay(men?650:1000);
    }
    delay (300);
    for (bool ibr = false; !touch(PinL)&& !ibr;)
	  {dismen (men);
	   delay (200);
	   if (menbut[men] == '-'||menbut[men] == '+')
		 {int v=audio.getVolume () +(menbut[men]=='+'?+2:- 2);
      v=min(v,22);v=max(0,v);
      audio.setVolume(v);
		  int mid;
		  mid = map (audio.getVolume (), 0, 22, 0, 320);
		  tft.fillRect (0, 235,mid,5,TFT_YELLOW);
		  tft.fillRect ( mid, 235, 320 - mid, 5,TFT_BLACK);
		 }
		 else if (menbut[men] == '>' || menbut[men] == '<')
		 {i=(i+ (menbut[men]=='<'?-1:1));
      if(i<0) i=mp3idx-1;if(i>=mp3idx) i=0;sta = i / 12;sel = i % 12;
	    play_SD (sta, sel, men);
      dismen(men);	      
	   }
     else if (menbut[men] == 'S')
		 {unsigned long    a=gettim();
      unsigned long  b=slened(a);
     Serial.printf("%d %d %d\n",a,a/60,a%60); 
	   Serial.printf("%d %d %d\n",b,b/60,b%60); 
	   tft.setFont (2); 
      tft.print("going to sleep for ");
      delay(10000);
      tft.backlight(false);
     delay(1000);
     esp_sleep_enable_timer_wakeup(b*60 * 1000000ULL);
     esp_deep_sleep_start();
     }
     else if (menbut[men]=='P')
     {ibr=obr=true;
      break;
     }
     delay(400);
    }
   }
 }
  Serial.printf ("Start playing :%s\n", mp3tab[i].c_str ());
  strncpy (disnam, mp3tab[i].c_str (), 24);
  if(i==pi)
     audio.pauseResume();
  else  if (mp3tab[i].startsWith ("http"))
    audio.connecttohost (mp3tab[i].c_str ());
  else
    audio.connecttoFS (SD, mp3tab[i].c_str ());
  pi=i;
  panelt (true);
}

void
loop ()
{
  static unsigned long sta;
  static unsigned long sta2;
  static unsigned long sta3;
  static bool bl=true;
  if (!sta) sta=sta2=sta3=millis();
  if (!audio.isRunning ())
    {
      playmp3 ("nxt");
      return;
    }
  audio.loop ();
  vTaskDelay (10);
  if (millis () - sta > 5000)
    {
      if (bl) panelt (false);
      sta = millis ();
    } 
    if ((disoff>0)&&(millis () - sta3 > (disoff*1000)))
    {
      tft.backlight(bl=false);

    }
  if ((millis () - sta2) > 500)
    if (PinL >= 0 && touch (PinL))
      {if(!bl)
       {tft.backlight(bl=true);
       }
       else
	     {audio.pauseResume ();
        tft.backlight(bl=true);
	      playmp3 ("");
        sta3 = millis ();
       }
      }
    else
      sta2 = millis ();
}

void
audio_info (const char *info)
{
  Serial.print ("info ");
  Serial.println (info);
  char *a;
  if(a=strstr(info,"=\'"))
    {strncpy(disnam,a+2,24);
     panelt();
    }
}

 