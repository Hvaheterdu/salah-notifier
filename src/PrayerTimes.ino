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
    constexpr int HTTPS_PORT = 443;
    constexpr char HTTP_METHOD[] = "GET";
    constexpr char HOST_NAME[] = "";
    constexpr char PATH_NAME[] = "";
    constexpr char API_TOKEN[] = "";
    constexpr char CONTENT_TYPE[] = "application/json";
    constexpr char SECRET_SSID[] = "";
    constexpr char SECRET_PASSWORD[] = "";
    constexpr int WIFI_STATUS = WL_IDLE_STATUS;

    // Speaker pins and settings.
    constexpr int RX_PIN = 0;
    constexpr int TX_PIN = 1;
    constexpr int VOLUME = 20;
    constexpr int ADHAN_DURATION_MS = 18000;

    // JSON configuration.
    constexpr int JSON_DOCUMENT_SIZE = 1024;
    constexpr int HTTP_STATUS_SIZE = 32;

    // Timezone and DST
    constexpr int TIMEZONE_OFFSET_HOURS = 1 * 3600;
    constexpr int DST_OFFSET = 3600;
}

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
    bool maghribEndtimeComputed = false;
    bool duhrSoundPlayed = false;
    bool asrSoundPlayed = false;
    bool maghribSoundPlayed = false;
    bool ishaSoundPlayed = false;
};

// Prayer time display configuration.
struct PrayerDisplay
{
    const char *name;
    const char *time;
    const char *startTime;
    const char *endTime;
    bool playSound;
    bool *soundPlayedFlag;
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

// Prayer schedule.
const PrayerDisplay prayerSchedule[] =
    {
        {"FAJR", nullptr, prayerTimes.muntasafallayl_midnight, prayerTimes.fajr, false, nullptr},
        {"FAJR END", prayerTimes.fajr_endtime, prayerTimes.fajr, prayerTimes.fajr_endtime, false, nullptr},
        {"DUHR", prayerTimes.duhr, prayerTimes.fajr_endtime, prayerTimes.duhr, true, &soundState.duhrSoundPlayed},
        {"ASR", prayerTimes.asr_2x_shadow, prayerTimes.duhr, prayerTimes.asr_2x_shadow, true, &soundState.asrSoundPlayed},
        {"ASR END", prayerTimes.asr_endtime, prayerTimes.asr_2x_shadow, prayerTimes.asr_endtime, false, nullptr},
        {"MAGHRIB", prayerTimes.maghrib, prayerTimes.asr_2x_shadow, prayerTimes.maghrib, true, &soundState.maghribSoundPlayed},
        {"MAGHR END", prayerTimes.maghrib_endtime, prayerTimes.maghrib, prayerTimes.maghrib_endtime, false, nullptr},
        {"ISHA", prayerTimes.isha, prayerTimes.maghrib, prayerTimes.isha, true, &soundState.ishaSoundPlayed},
        {"MIDNATT", prayerTimes.muntasafallayl_midnight, prayerTimes.isha, "00:00", false, nullptr},
        {"MIDNATT", prayerTimes.muntasafallayl_midnight, prayerTimes.muntasafallayl_midnight, nullptr, false, nullptr}};

// Display message on LCD.
void displayMessage(const String &firstLine, const String &secondLine = "", int duration = 3000)
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

// Connect to WiFi.
bool connectToWifi()
{
    if (WiFi.status() == Config::WIFI_STATUS)
    {
        displayMessage("Connect to SSID:", Config::SECRET_SSID);
        WiFi.begin(Config::SECRET_SSID, Config::SECRET_PASSWORD);

        return false;
    }
    displayMessage("Connected to", Config::SECRET_SSID);

    return true;
}

// JSON with prayer times.
StaticJsonDocument<Config::JSON_DOCUMENT_SIZE> getPrayerTimes(const RTCTime &currentTime)
{
    StaticJsonDocument<Config::JSON_DOCUMENT_SIZE> doc;
    displayMessage("Updating prayer", "times");

    if (!wifiClient.connect(Config::HOST_NAME, Config::HTTPS_PORT))
    {
        displayMessage("Connection to", "client failed");
        return doc;
    }

    String path = String(Config::PATH_NAME) + currentTime.getYear() + "/" + Month2int(currentTime.getMonth()) + "/" + currentTime.getDayOfMonth() + "/";

    wifiClient.print(String(Config::HTTP_METHOD) + " " + path + " HTTP/1.1\r\n");
    wifiClient.print("Host: " + String(Config::HOST_NAME) + "\r\n");
    wifiClient.print("Api-Token: " + String(Config::API_TOKEN) + "\r\n");
    wifiClient.print("Accept: " + String(Config::CONTENT_TYPE) + "\r\n");
    wifiClient.println("Connection: close\r\n");

    if (wifiClient.println() == 0)
    {
        displayMessage("Failed to send", "request");
        return doc;
    }

    char status[Config::HTTP_STATUS_SIZE] = {0};
    wifiClient.readBytesUntil('\r', status, sizeof(status));

    if (strcmp(status, "HTTP/1.1 200 OK") != 0)
    {
        displayMessage("HTTP response: ", status);
        return doc;
    }

    if (!wifiClient.find("\r\n\r\n"))
    {
        displayMessage("Invalid", "response");
        return doc;
    }

    String responseBody = "";
    while (wifiClient.available())
    {
        char c = wifiClient.read();
        responseBody += c;
    }

    DeserializationError error = deserializeJson(doc, responseBody);
    if (error)
    {
        displayMessage("Deserialize", "failed");
        return doc;
    }

    displayMessage("Updated prayer", "times");

    return doc;
}

// Update JSON with prayer times.
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

// Add leading zero to time.
String formatTimeWithLeadingZero(int value)
{
    return (value < 10 ? "0" : "") + String(value);
}

char *caculateMaghribEndTime(const char *maghrib, int minutesToAdd)
{
    int hours, minutes;
    char *result = (char *)malloc(6);
    sscanf(maghrib, "%d:%d", &hours, &minutes);

    minutes += minutesToAdd;
    hours += minutes / 60;
    minutes %= 60;
    hours %= 24;

    snprintf(result, 6, "%02d:%02d", hours, minutes);

    return result;
}

bool isDaylightSavingTime(int day, int month, int weekday, int hour)
{
    if (month < 3 || month > 10)
        return false;
    if (month > 3 && month < 10)
        return true;

    int lastSunday = 31 - ((5 + day) % 7);
    if (month == 3)
        return (day > lastSunday || (day == lastSunday && hour >= 2));

    if (month == 10)
        return (day > lastSunday || (day == lastSunday && hour >= 3));

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

    if (WiFi.firmwareVersion() < WIFI_FIRMWARE_LATEST_VERSION)
    {
        displayMessage("Upgrade WiFi", "firmware");
    }

    if (WiFi.status() == WL_NO_MODULE)
    {
        displayMessage("No WiFi", "module found");
        while (true)
            ;
    }

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

    String timeForComparison = formatTimeWithLeadingZero(currentTime.getHour()) + ":" + formatTimeWithLeadingZero(currentTime.getMinutes()) + ":" + formatTimeWithLeadingZero(currentTime.getSeconds());

    // Update prayer time daily.
    if (timeForComparison == "00:01:00" || strcmp(prayerTimes.fajr, "--:--"))
    {
        jsonDoc = getPrayerTimes(currentTime);
        updatePrayerTimes(jsonDoc);

        if (!soundState.maghribEndtimeComputed)
        {
            if (prayerTimes.maghrib_endtime)
            {
                free(prayerTimes.maghrib_endtime);
            }
            prayerTimes.maghrib_endtime = caculateMaghribEndTime(prayerTimes.maghrib, 45);
            soundState.maghribEndtimeComputed = true;
        }
    }

    // Reset flags
    if (timeForComparison == "23:59:50")
    {
        soundState.maghribEndtimeComputed = false;
        soundState.duhrSoundPlayed = false;
        soundState.asrSoundPlayed = false;
        soundState.maghribSoundPlayed = false;
        soundState.ishaSoundPlayed = false;
    }

    static String last_displayed_time = "";
    String current_time_display = formatTimeWithLeadingZero(currentTime.getHour()) + ":" + formatTimeWithLeadingZero(currentTime.getMinutes());

    String current_date_display = formatTimeWithLeadingZero(currentTime.getDayOfMonth()) + "/" + formatTimeWithLeadingZero(Month2int(currentTime.getMonth())) + "/" + formatTimeWithLeadingZero(currentTime.getYear());

    if (current_time_display != last_displayed_time)
    {
        last_displayed_time = current_time_display;
        lcd.setCursor(0, 0);
        lcd.print(current_date_display + " " + current_time_display + "          ");
    }

    String prayerTimeToCompare = formatTimeWithLeadingZero(currentTime.getHour()) + ":" + formatTimeWithLeadingZero(currentTime.getMinutes());

    for (const auto &prayer : prayerSchedule)
    {
        if (prayer.startTime && strcmp(prayer.startTime, "--:--") != 0 && strcmp(prayer.startTime, prayerTimeToCompare.c_str()) <= 0 && (!prayer.endTime || strcmp(prayer.endTime, prayerTimeToCompare.c_str()) > 0))
        {
            if (strcmp(prayer.name, "ASR END") == 0 && currentTime.getSeconds() % 3 != 0)
            {
                continue;
            }

            lcd.setCursor(0, 1);
            lcd.print(String(prayer.name) + ": " + (prayer.time ? String(prayer.time) : String(prayerTimes.muntasafallayl_midnight)) + "          ");

            if (prayer.playSound && prayer.time && strcmp(prayer.time, prayerTimeToCompare.c_str()) == 0 && prayer.soundPlayedFlag && !(*prayer.soundPlayedFlag))
            {
                if (strcmp(prayer.name, "ISHA") == 0 && strcmp(prayer.time, "22:00") >= 0)
                {
                    continue;
                }
                *prayer.soundPlayedFlag = true;
                playAdhan();
            }
            break;
        }
    }

    while (!connectToWifi())
    {
        connectToWifi();
    }
}
