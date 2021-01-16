#include <WiFiUdp.h>

WiFiUDP UDP;                     // Create an instance of the WiFiUDP class to send and receive

enum ntp_state_t
{
  NTP_STATE_IDLE,
  NTP_STATE_RESOLVE,
  NTP_STATE_SEND,
  NTP_STATE_WAIT_RECV,
  NTP_STATE_WAIT_NEXT_SECOND,
  NUM_NTP_STATES
};

static enum ntp_state_t ntpState;
static uint32_t lastNTPUpdate;
static char ntpServer[128];
static uint32_t ntpInterval;

static const int NTP_PACKET_SIZE = 48;  // NTP time stamp is in the first 48 bytes of the message
static byte ntpBuffer[NTP_PACKET_SIZE]; // buffer to hold incoming and outgoing packets

static time_t nextNTPTime;
static uint32_t nextNTPMillis;
static uint32_t lastNTPSendMillis;
static uint32_t networkDelayMillis;

IPAddress ntpIP;

static const uint16_t ntpDelayLUT[] = {1000, 997, 993, 989, 985, 981, 977, 973, 969, 965, 961, 957, 953, 950, 946, 942,
                                      938, 934, 930, 926, 922, 918, 914, 910, 906, 902, 899, 895, 891, 887, 883, 879,
                                      875, 871, 867, 863, 859, 855, 851, 848, 844, 840, 836, 832, 828, 824, 820, 816,
                                      812, 808, 804, 800, 797, 793, 789, 785, 781, 777, 773, 769, 765, 761, 757, 753,
                                      750, 746, 742, 738, 734, 730, 726, 722, 718, 714, 710, 706, 702, 699, 695, 691,
                                      687, 683, 679, 675, 671, 667, 663, 659, 655, 651, 648, 644, 640, 636, 632, 628,
                                      624, 620, 616, 612, 608, 604, 600, 597, 593, 589, 585, 581, 577, 573, 569, 565,
                                      561, 557, 553, 550, 546, 542, 538, 534, 530, 526, 522, 518, 514, 510, 506, 502,
                                      499, 495, 491, 487, 483, 479, 475, 471, 467, 463, 459, 455, 451, 448, 444, 440,
                                      436, 432, 428, 424, 420, 416, 412, 408, 404, 400, 397, 393, 389, 385, 381, 377,
                                      373, 369, 365, 361, 357, 353, 350, 346, 342, 338, 334, 330, 326, 322, 318, 314,
                                      310, 306, 302, 299, 295, 291, 287, 283, 279, 275, 271, 267, 263, 259, 255, 251,
                                      248, 244, 240, 236, 232, 228, 224, 220, 216, 212, 208, 204, 200, 197, 193, 189,
                                      185, 181, 177, 173, 169, 165, 161, 157, 153, 150, 146, 142, 138, 134, 130, 126,
                                      122, 118, 114, 110, 106, 102,  99,  95,  91,  87,  83,  79,  75,  71,  67,  63,
                                       59,  55,  51,  48,  44,  40,  36,  32,  28,  24,  20,  16,  12,   8,   4,   0};

void ntp_init()
{
  ntpState = NTP_STATE_IDLE;
  lastNTPUpdate = UINT32_MAX / 2;
}

bool ntp_handle(time_t *timeUNIX)
{
  bool ret = false;
  
  switch (ntpState)
  {
    case NTP_STATE_IDLE:
    {
      if ((millis() - lastNTPUpdate) > ntpInterval)
      {
        ntpState = NTP_STATE_RESOLVE;
      }
      break;
    }

    case NTP_STATE_RESOLVE:
    {
      if (!WiFi.hostByName(ntpServer, ntpIP))
      {
        break;
      }
      //Serial.print("Found server IP: ");
      //Serial.println(ntpIP);
      ntpState = NTP_STATE_SEND;
      /* FALLTHROUGH TO NTP_STATE_SEND */
    }

    case NTP_STATE_SEND:
    {
      //Serial.println("Sending NTP packet");
      UDP.begin(123);
      send_ntp_packet();
      lastNTPSendMillis = millis();
      ntpState = NTP_STATE_WAIT_RECV;
      /* FALLTHROUGH TO NTP_STATE_WAIT_RECV */
    }

    case NTP_STATE_WAIT_RECV:
    {
      if (millis() - lastNTPSendMillis > 60000ul)
      {
        ntpState = NTP_STATE_SEND;
        break;
      }
      else if (UDP.parsePacket() == 0)
      {
        break;
      }
      //Serial.println("Parsing NTP packet");
      networkDelayMillis = (millis() - lastNTPSendMillis) / 2;
      process_ntp_packet();

      // Handle network delay
      nextNTPTime += 1;
      nextNTPMillis += (1000 - networkDelayMillis);
      
      ntpState = NTP_STATE_WAIT_NEXT_SECOND;
      /* FALLTHROUGH TO NTP_STATE_WAIT_NEXT_SECOND */
    }

    case NTP_STATE_WAIT_NEXT_SECOND:
    {
      if (millis() < nextNTPMillis)
      {
        break;
      }
      //Serial.println("Sending back NTP result");
      lastNTPUpdate = millis();
      *timeUNIX = nextNTPTime;
      ntpState = NTP_STATE_IDLE;
      ret = true;
    }

    default:
    {
      break;
    }
  }
  return ret;
}

bool ntp_config(char* server, uint8_t interval)
{
  strncpy(ntpServer, server, 128);
  ntpInterval = interval * (60ul * 60ul * 1000ul);
}

static void send_ntp_packet()
{
  memset(ntpBuffer, 0, NTP_PACKET_SIZE);  // set all bytes in the buffer to 0
  // Initialize values needed to form NTP request
  ntpBuffer[0] = 0b11100011;   // LI, Version, Mode
  // send a packet requesting a timestamp:
  UDP.beginPacket(ntpIP, 123); // NTP requests are to port 123
  UDP.write(ntpBuffer, NTP_PACKET_SIZE);
  UDP.endPacket();
}

static void process_ntp_packet()
{
  UDP.read(ntpBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
  // Combine the 4 timestamp bytes into one 32-bit number
  uint32_t NTPTime = (ntpBuffer[40] << 24) | (ntpBuffer[41] << 16) | (ntpBuffer[42] << 8) | ntpBuffer[43];
  // Convert NTP time to a UNIX timestamp:
  // Unix time starts on Jan 1 1970. That's 2208988800 seconds in NTP time:
  const uint32_t seventyYears = 2208988800UL;
  // subtract seventy years:
  uint32_t UNIXTime = NTPTime - seventyYears;
  nextNTPTime = UNIXTime + 1;
  nextNTPMillis = millis() + ntpDelayLUT[ntpBuffer[44]];
}
