#include <ArduinoJson.h>
#include <DFRobotDFPlayerMini.h>
#include <LiquidCrystal_I2C.h>
#include <NTPClient.h>
#include <RTC.h>
#include <SoftwareSerial.h>
#include <time.h>
#include <WiFiS3.h>
#include <WiFiSSLClient.h>

// Configuration Constants.
namespace Config
{
    // Request settings.
    constexpr int HTTPS_PORT = 443;
    constexpr char HTTP_METHOD[] = "GET";
    constexpr char HOST_NAME[] = "";
    constexpr char PATH_NAME[] = "";
    constexpr char API_TOKEN[] = "";
    constexpr char CONTENT_TYPE[] = "application/json";

    // WiFi connection settings.
    constexpr char SECRET_SSID[] = "";
    constexpr char SECRET_PASSWORD[] = "";
    constexpr int WIFI_CONNECTION_ATTEMPTS = 5;

    // Speaker pins and settings.
    constexpr int RX_PIN = 0;
    constexpr int TX_PIN = 1;
    constexpr int VOLUME = 20;
    constexpr int ADHAN_DURATION_MS = 18000;

    // JSON configuration.
    constexpr int JSON_DOCUMENT_SIZE = 1500;
    constexpr int HTTP_STATUS_SIZE = 100;

    // Timezone and DST.
    constexpr int TIMEZONE_OFFSET_HOURS = 1 * 3600;
    constexpr int DST_OFFSET = 3600;
};

// Prayer Times Structure.
struct PrayerTimes
{
    const char *location = "--:--";
    const char *date = "--:--";
    const char *district_code = "--:--";
    const char *kommune = "--:--";
    const char *hijri_date = "--:--";
    const char *istiwa_noon = "--:--";
    const char *duhr = "--:--";
    const char *asr = "--:--";
    const char *shadow_1x = "--:--";
    const char *shadow_2x = "--:--";
    const char *wusta_noon_sunset = "--:--";
    const char *asr_endtime = "--:--";
    const char *ghrub_sunset = "--:--";
    const char *maghrib = "--:--";
    char *maghrib_endtime = nullptr;
    const char *isha = "--:--";
    const char *shafaqal_ahmar_end_redlight = "--:--";
    const char *shafaqal_abyadh_end_whitelight = "--:--";
    const char *muntasafallayl_midnight = "--:--";
    const char *fajr_sadiq = "--:--";
    const char *fajr = "--:--";
    const char *fajr_endtime = "--:--";
    const char *shuruq_sunrise = "--:--";
    const char *laylat_falakia_one3rd = "--:--";
    const char *laylat_falakia_two3rd = "--:--";
    const char *laylatal_shariea_one3rd = "--:--";
    const char *laylatal_shariea_two3rd = "--:--";
    const char *prayer_after_sunrise = "--:--";
    const char *altitude_noon = "--:--";
    const char *altitude_midnight = "--:--";
    const char *asr_1x_shadow = "--:--";
    const char *asr_2x_shadow = "--:--";
    const char *asr_wusta_noon_sunset = "--:--";
    const char *sun_shadow_noon = "--:--";
    const char *sun_shadow_wusta = "--:--";
    const char *elevation_noon = "--:--";
    const char *elevation_asrwusta = "--:--";
    const char *elevation_shadow_1x = "--:--";
    const char *elevation_shadow_2x = "--:--";
    const char *elevation_midnight = "--:--";
};

// State management.
struct PrayerSoundState
{
    bool duhrSoundPlayed = false;
    bool asrSoundPlayed = false;
    bool maghribSoundPlayed = false;
    bool ishaSoundPlayed = false;
};

// Global Objects.
WiFiUDP udp;
NTPClient timeClient(udp);

WiFiSSLClient wifiClient;

LiquidCrystal_I2C lcd(0x27, 16, 2);

DFRobotDFPlayerMini fxPlayer;
SoftwareSerial fxSerial(Config::RX_PIN, Config::TX_PIN);

StaticJsonDocument<Config::JSON_DOCUMENT_SIZE> jsonDoc;
PrayerTimes prayerTimes;
PrayerSoundState soundState;

void calculateMaghribEndTime(const char *maghrib, int minutesToAdd, char outBuffer[6])
{
    int hours = 0;
    int minutes = 0;
    if (sscanf(maghrib, "%d:%d", &hours, &minutes) != 2)
    {
        strncpy(outBuffer, "--:--", 6);
        return;
    }

    minutes += minutesToAdd;
    hours += minutes / 60;
    minutes %= 60;
    hours %= 24;

    snprintf(outBuffer, 6, "%02d:%02d", hours, minutes);
}

void displayMessage(const String firstLine, const String secondLine = "", int duration = 3000)
{
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(firstLine);
    if (!secondLine.isEmpty())
    {
        lcd.setCursor(0, 1);
        lcd.print(secondLine);
    }

    delay(duration);
    lcd.clear();
}

void connectToWifi()
{
    if (WiFi.status() == WL_NO_MODULE)
    {
        displayMessage("No WiFi", "module found");
        while (true)
            ;
    }

    String fv = WiFi.firmwareVersion();
    if (fv < WIFI_FIRMWARE_LATEST_VERSION)
    {
        displayMessage("Upgrade WiFi", "firmware");
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        WiFi.begin(Config::SECRET_SSID, Config::SECRET_PASSWORD);
        displayMessage("Connect to SSID:", Config::SECRET_SSID);
    }

    displayMessage("Connected to", Config::SECRET_SSID);
}

StaticJsonDocument<Config::JSON_DOCUMENT_SIZE> getPrayerTimes(const RTCTime &currentTime)
{
    StaticJsonDocument<Config::JSON_DOCUMENT_SIZE> doc;
    displayMessage("Updating prayer", "times");

    if (!wifiClient.connect(Config::HOST_NAME, Config::HTTPS_PORT))
    {
        displayMessage("Client connect", "failed");
        return doc;
    }

    String path = String(Config::PATH_NAME) + currentTime.getYear() + "/" + Month2int(currentTime.getMonth()) + "/" + currentTime.getDayOfMonth() + "/";

    wifiClient.print(String(Config::HTTP_METHOD) + " " + path + " HTTP/1.1\r\n");
    wifiClient.print("Host: " + String(Config::HOST_NAME) + "\r\n");
    wifiClient.print("Api-Token: " + String(Config::API_TOKEN) + "\r\n");
    wifiClient.print("Accept: " + String(Config::CONTENT_TYPE) + "\r\n");
    wifiClient.print("Connection: close\r\n");
    wifiClient.println();

    if (wifiClient.println() == 0)
    {
        displayMessage("Request send", "failed");
        return doc;
    }

    String statusLine = wifiClient.readStringUntil('\n');
    statusLine.trim();
    if (!statusLine.startsWith("HTTP/1.1 200 OK"))
    {
        displayMessage("HTTP error:", statusLine.c_str());
        wifiClient.stop();
        return doc;
    }

    while (wifiClient.available())
    {
        String headerLine = wifiClient.readStringUntil('\n');
        headerLine.trim();
        if (headerLine.length() == 0)
        {
            break;
        }
    }

    if (!wifiClient.available())
    {
        displayMessage("No body", "after headers");
        wifiClient.stop();
        return doc;
    }

    displayMessage("Getting", "response JSON");
    String responseBody = "";
    while (wifiClient.available())
    {
        responseBody += wifiClient.readString();
    }

    wifiClient.stop();

    DeserializationError error = deserializeJson(doc, responseBody);
    if (error)
    {
        displayMessage("Deserialize", "failed");
        return doc;
    }

    displayMessage("Updated prayer", "times");

    return doc;
}

void updatePrayerTimes(StaticJsonDocument<Config::JSON_DOCUMENT_SIZE> &doc)
{
    prayerTimes.location = doc["location"];
    prayerTimes.date = doc["date"];
    prayerTimes.district_code = doc["district_code"];
    prayerTimes.kommune = doc["kommune"];
    prayerTimes.hijri_date = doc["hijri_date"];
    prayerTimes.istiwa_noon = doc["istiwa_noon"];
    prayerTimes.duhr = doc["duhr"];
    prayerTimes.asr = doc["asr"];
    prayerTimes.shadow_1x = doc["shadow_1x"];
    prayerTimes.shadow_2x = doc["shadow_2x"];
    prayerTimes.wusta_noon_sunset = doc["wusta_noon_sunset"];
    prayerTimes.asr_endtime = doc["asr_endtime"];
    prayerTimes.ghrub_sunset = doc["ghrub_sunset"];
    prayerTimes.maghrib = doc["maghrib"];
    calculateMaghribEndTime(prayerTimes.maghrib, 45, prayerTimes.maghrib_endtime);
    prayerTimes.isha = doc["isha"];
    prayerTimes.shafaqal_ahmar_end_redlight = doc["shafaqal_ahmar_end_redlight"];
    prayerTimes.shafaqal_abyadh_end_whitelight = doc["shafaqal_abyadh_end_whitelight"];
    prayerTimes.muntasafallayl_midnight = doc["muntasafallayl_midnight"];
    prayerTimes.fajr_sadiq = doc["fajr_sadiq"];
    prayerTimes.fajr = doc["fajr"];
    prayerTimes.fajr_endtime = doc["fajr_endtime"];
    prayerTimes.shuruq_sunrise = doc["shuruq_sunrise"];
    prayerTimes.laylat_falakia_one3rd = doc["laylat_falakia_one3rd"];
    prayerTimes.laylat_falakia_two3rd = doc["laylat_falakia_two3rd"];
    prayerTimes.laylatal_shariea_one3rd = doc["laylatal_shariea_one3rd"];
    prayerTimes.laylatal_shariea_two3rd = doc["laylatal_shariea_two3rd"];
    prayerTimes.prayer_after_sunrise = doc["prayer_after_sunrise"];
    prayerTimes.altitude_noon = doc["altitude_noon"];
    prayerTimes.altitude_midnight = doc["altitude_midnight"];
    prayerTimes.asr_1x_shadow = doc["asr_1x_shadow"];
    prayerTimes.asr_2x_shadow = doc["asr_2x_shadow"];
    prayerTimes.asr_wusta_noon_sunset = doc["asr_wusta_noon_sunset"];
    prayerTimes.sun_shadow_noon = doc["sun_shadow_noon"];
    prayerTimes.sun_shadow_wusta = doc["sun_shadow_wusta"];
    prayerTimes.elevation_noon = doc["elevation_noon"];
    prayerTimes.elevation_asrwusta = doc["elevation_asrwusta"];
    prayerTimes.elevation_shadow_1x = doc["elevation_shadow_1x"];
    prayerTimes.elevation_shadow_2x = doc["elevation_shadow_2x"];
    prayerTimes.elevation_midnight = doc["elevation_midnight"];
}

String formatTimeWithLeadingZero(int value)
{
    return (value < 10 ? "0" : "") + String(value);
}

bool isDaylightSavingTime(int day, int month, int weekday, int hour)
{
    if (month < 3 || month > 10)
    {
        return false;
    }

    if (month > 3 && month < 10)
    {
        return true;
    }

    int weekdayOf31 = (weekday + (31 - day)) % 7;
    int lastSunday = 31 - weekdayOf31;

    if (month == 3)
    {
        return (day > lastSunday || (day == lastSunday && hour >= 2));
    ]

    if (month == 10)
    {
        return !(day > lastSunday || (day == lastSunday && hour >= 3));
    }

    return false;
    }

    void extractDateComponent(time_t epoch, int &day, int &month, int &weekday, int &hour)
    {
        struct tm *timeInfo;
        timeInfo = gmtime(&epoch);

        day = timeInfo->tm_mday;
        month = timeInfo->tm_mon + 1;
        weekday = timeInfo->tm_wday;
        hour = timeInfo->tm_hour;
    }

    void playAdhan()
    {
        fxPlayer.start();
        fxPlayer.play(1);
        delay(Config::ADHAN_DURATION_MS);
        fxPlayer.stop();
    }

    void setup()
    {
        Serial.begin(19200);
        fxSerial.begin(9600);
        while (!Serial)
            ;

        lcd.init();
        lcd.backlight();
        lcd.clear();

        connectToWifi();

        if (!fxPlayer.begin(fxSerial, true, true))
        {
            displayMessage("DFMiniPlayer", "not connected");
        }
        else
        {
            displayMessage("DFMiniPlayer", "connected");
        }
        fxPlayer.volume(Config::VOLUME);

        RTC.begin();
        timeClient.begin();
    }

    void loop()
    {
        timeClient.update();

        int timezoneOffsetHour = Config::TIMEZONE_OFFSET_HOURS;
        int day, month, weekday, hour;
        extractDateComponent(timeClient.getEpochTime(), day, month, weekday, hour);

        if (isDaylightSavingTime(day, month, weekday, hour))
        {
            timezoneOffsetHour += Config::DST_OFFSET;
        }

        RTCTime currentTime(timeClient.getEpochTime() + timezoneOffsetHour);
        RTC.setTime(currentTime);

        String currentTimeDisplay = formatTimeWithLeadingZero(currentTime.getHour()) + ":" + formatTimeWithLeadingZero(currentTime.getMinutes());
        String currentDateDisplay = formatTimeWithLeadingZero(currentTime.getDayOfMonth()) + "/" + formatTimeWithLeadingZero(Month2int(currentTime.getMonth())) + "/" + formatTimeWithLeadingZero(currentTime.getYear());

        static String lastDisplayedTime = "";
        if (currentTimeDisplay != lastDisplayedTime)
        {
            lastDisplayedTime = currentTimeDisplay;
            lcd.setCursor(0, 0);
            lcd.print(currentDateDisplay + " " + currentTimeDisplay + "          ");
        }

        String timeForReset = formatTimeWithLeadingZero(currentTime.getHour()) + ":" + formatTimeWithLeadingZero(currentTime.getMinutes()) + ":" + formatTimeWithLeadingZero(currentTime.getSeconds());

        // Update prayer time daily.
        if (timeForReset == "00:01:00" || strcmp(prayerTimes.fajr, "--:--") == 0)
        {
            jsonDoc = getPrayerTimes(currentTime);
            updatePrayerTimes(jsonDoc);
        }

        // Reset sound flags.
        if (timeForReset == "23:59:50")
        {
            soundState = {};
        }

        String prayerTimeToCompare = formatTimeWithLeadingZero(currentTime.getHour()) + ":" + formatTimeWithLeadingZero(currentTime.getMinutes());

        if (strcmp(prayerTimeToCompare.c_str(), prayerTimes.muntasafallayl_midnight) > 0 && strcmp(prayerTimeToCompare.c_str(), prayerTimes.fajr) < 0)
        {
            lcd.setCursor(0, 1);
            lcd.print("FAJR:      " + String(prayerTimes.fajr) + "   ");
        }
        else if (strcmp(prayerTimeToCompare.c_str(), prayerTimes.fajr) >= 0 && strcmp(prayerTimeToCompare.c_str(), prayerTimes.fajr_endtime) < 0)
        {
            lcd.setCursor(0, 1);
            lcd.print("FAJR END:   " + String(prayerTimes.fajr_endtime) + "   ");
            if (formatTimeWithLeadingZero(currentTime.getMinutes()).toInt() % 3 == 0)
            {
                lcd.setCursor(0, 1);
                lcd.print("DUHR:       " + String(prayerTimes.duhr) + "   ");
            }
        }
        else if (strcmp(prayerTimeToCompare.c_str(), prayerTimes.fajr_endtime) >= 0 && strcmp(prayerTimeToCompare.c_str(), prayerTimes.duhr) < 0)
        {
            lcd.setCursor(0, 1);
            lcd.print("DUHR:       " + String(prayerTimes.duhr) + "   ");
            if (!soundState.duhrSoundPlayed && strcmp(prayerTimeToCompare.c_str(), prayerTimes.duhr) == 0)
            {
                soundState.duhrSoundPlayed = true;
                playAdhan();
            }
        }
        else if (strcmp(prayerTimeToCompare.c_str(), prayerTimes.duhr) >= 0 && strcmp(prayerTimeToCompare.c_str(), prayerTimes.asr) < 0)
        {
            lcd.setCursor(0, 1);
            lcd.print("ASR:        " + String(prayerTimes.asr) + "   ");
            if (!soundState.asrSoundPlayed && strcmp(prayerTimeToCompare.c_str(), prayerTimes.asr) == 0)
            {
                soundState.asrSoundPlayed = true;
                playAdhan();
            }
        }
        else if (strcmp(prayerTimeToCompare.c_str(), prayerTimes.asr) >= 0 && strcmp(prayerTimeToCompare.c_str(), prayerTimes.maghrib) < 0)
        {
            lcd.setCursor(0, 1);
            lcd.print("MAGHRIB:   " + String(prayerTimes.maghrib) + "   ");
            if (!soundState.maghribSoundPlayed && strcmp(prayerTimeToCompare.c_str(), prayerTimes.maghrib) == 0)
            {
                soundState.maghribSoundPlayed = true;
                playAdhan();
            }
        }
        else if (strcmp(prayerTimeToCompare.c_str(), prayerTimes.maghrib) >= 0 && strcmp(prayerTimeToCompare.c_str(), prayerTimes.maghrib_endtime) < 0)
        {
            lcd.setCursor(0, 1);
            lcd.print("MAGHR END: " + String(prayerTimes.maghrib_endtime) + "   ");
        }
        else if (strcmp(prayerTimeToCompare.c_str(), prayerTimes.maghrib_endtime) >= 0 && strcmp(prayerTimeToCompare.c_str(), prayerTimes.isha) < 0)
        {
            lcd.setCursor(0, 1);
            lcd.print("ISHA:      " + String(prayerTimes.isha) + "   ");
            if (!soundState.ishaSoundPlayed && strcmp(prayerTimeToCompare.c_str(), prayerTimes.isha) == 0)
            {
                soundState.ishaSoundPlayed = true;
                playAdhan();
            }
        }
        else if (strcmp(prayerTimeToCompare.c_str(), prayerTimes.isha) >= 0 && strcmp(prayerTimeToCompare.c_str(), prayerTimes.muntasafallayl_midnight) < 0)
        {
            lcd.setCursor(0, 1);
            lcd.print("MIDNATT:   " + String(prayerTimes.muntasafallayl_midnight) + "   ");
        }
    }
