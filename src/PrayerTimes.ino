#include <ArduinoJson.h>
#include <DFRobotDFPlayerMini.h>
#include <LiquidCrystal_I2C.h>
#include <NTPClient.h>
#include <RTC.h>
#include <SoftwareSerial.h>
#include <time.h>
#include <Timezone.h>
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

    // Speaker pins and settings.
    constexpr int RX_PIN = 0;
    constexpr int TX_PIN = 1;
    constexpr int VOLUME = 10;
    constexpr int ADHAN_DURATION_MS = 18000;

    // JSON configuration.
    constexpr int JSON_DOCUMENT_SIZE = 1500;
    constexpr int HTTP_STATUS_SIZE = 100;
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
    char maghrib_endtime[6] = "--:--";
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
    bool duhr = false;
    bool asr = false;
    bool maghrib = false;
    bool isha = false;
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

TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120}; // UTC +2
TimeChangeRule CET = {"CET", Last, Sun, Oct, 3, 60};    // UTC +1
Timezone CE(CEST, CET);

void calculateMaghribEndTime(const char maghrib[6], int minutesToAdd, char outBuffer[6])
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
    lcd.setCursor(0, 1);
    lcd.print(secondLine);
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

StaticJsonDocument<Config::JSON_DOCUMENT_SIZE> getPrayerTimes(const RTCTime currentTime)
{
    displayMessage("Updating prayer", "times");

    if (!wifiClient.connect(Config::HOST_NAME, Config::HTTPS_PORT))
    {
        displayMessage("Client connect", "failed");
        return jsonDoc;
    }

    String path = String(Config::PATH_NAME) + currentTime.getYear() + "/" + Month2int(currentTime.getMonth()) + "/" + currentTime.getDayOfMonth() + "/";

    wifiClient.print(String(Config::HTTP_METHOD) + " " + path + " HTTP/1.1\r\n");
    wifiClient.print("Host: " + String(Config::HOST_NAME) + "\r\n");
    wifiClient.print("Api-Token: " + String(Config::API_TOKEN) + "\r\n");
    wifiClient.print("Accept: " + String(Config::CONTENT_TYPE) + "\r\n");
    wifiClient.print("Connection: close\r\n\r\n");

    while (!wifiClient.available())
    {
        delay(3000);
    }

    String statusLine = wifiClient.readStringUntil('\n');
    if (!statusLine.startsWith("HTTP/1.1 200 OK"))
    {
        displayMessage("HTTP error:", statusLine);
        return jsonDoc;
    }

    while (wifiClient.available() && wifiClient.connected())
    {
        String headerLine = wifiClient.readStringUntil('\n');
        if (headerLine == "\r")
        {
            break;
        }
    }

    String responseBody = "";
    while (wifiClient.available())
    {
        char responseBytes = wifiClient.read();
        responseBody += responseBytes;
    }

    DeserializationError error = deserializeJson(jsonDoc, responseBody);
    if (error)
    {
        displayMessage("Deserialize", "failed");
        return jsonDoc;
    }

    displayMessage("Updated prayer", "times");
    wifiClient.stop();

    return jsonDoc;
}

void updatePrayerTimes(StaticJsonDocument<Config::JSON_DOCUMENT_SIZE> jsonDoc)
{
    prayerTimes.location = jsonDoc["location"];
    prayerTimes.date = jsonDoc["date"];
    prayerTimes.district_code = jsonDoc["district_code"];
    prayerTimes.kommune = jsonDoc["kommune"];
    prayerTimes.hijri_date = jsonDoc["hijri_date"];
    prayerTimes.istiwa_noon = jsonDoc["istiwa_noon"];
    prayerTimes.duhr = jsonDoc["duhr"];
    prayerTimes.asr = jsonDoc["asr"];
    prayerTimes.shadow_1x = jsonDoc["shadow_1x"];
    prayerTimes.shadow_2x = jsonDoc["shadow_2x"];
    prayerTimes.wusta_noon_sunset = jsonDoc["wusta_noon_sunset"];
    prayerTimes.asr_endtime = jsonDoc["asr_endtime"];
    prayerTimes.ghrub_sunset = jsonDoc["ghrub_sunset"];
    prayerTimes.maghrib = jsonDoc["maghrib"];
    calculateMaghribEndTime(prayerTimes.maghrib, 45, prayerTimes.maghrib_endtime);
    prayerTimes.isha = jsonDoc["isha"];
    prayerTimes.shafaqal_ahmar_end_redlight = jsonDoc["shafaqal_ahmar_end_redlight"];
    prayerTimes.shafaqal_abyadh_end_whitelight = jsonDoc["shafaqal_abyadh_end_whitelight"];
    prayerTimes.muntasafallayl_midnight = jsonDoc["muntasafallayl_midnight"];
    prayerTimes.fajr_sadiq = jsonDoc["fajr_sadiq"];
    prayerTimes.fajr = jsonDoc["fajr"];
    prayerTimes.fajr_endtime = jsonDoc["fajr_endtime"];
    prayerTimes.shuruq_sunrise = jsonDoc["shuruq_sunrise"];
    prayerTimes.laylat_falakia_one3rd = jsonDoc["laylat_falakia_one3rd"];
    prayerTimes.laylat_falakia_two3rd = jsonDoc["laylat_falakia_two3rd"];
    prayerTimes.laylatal_shariea_one3rd = jsonDoc["laylatal_shariea_one3rd"];
    prayerTimes.laylatal_shariea_two3rd = jsonDoc["laylatal_shariea_two3rd"];
    prayerTimes.prayer_after_sunrise = jsonDoc["prayer_after_sunrise"];
    prayerTimes.altitude_noon = jsonDoc["altitude_noon"];
    prayerTimes.altitude_midnight = jsonDoc["altitude_midnight"];
    prayerTimes.asr_1x_shadow = jsonDoc["asr_1x_shadow"];
    prayerTimes.asr_2x_shadow = jsonDoc["asr_2x_shadow"];
    prayerTimes.asr_wusta_noon_sunset = jsonDoc["asr_wusta_noon_sunset"];
    prayerTimes.sun_shadow_noon = jsonDoc["sun_shadow_noon"];
    prayerTimes.sun_shadow_wusta = jsonDoc["sun_shadow_wusta"];
    prayerTimes.elevation_noon = jsonDoc["elevation_noon"];
    prayerTimes.elevation_asrwusta = jsonDoc["elevation_asrwusta"];
    prayerTimes.elevation_shadow_1x = jsonDoc["elevation_shadow_1x"];
    prayerTimes.elevation_shadow_2x = jsonDoc["elevation_shadow_2x"];
    prayerTimes.elevation_midnight = jsonDoc["elevation_midnight"];
}

String formatTimeWithLeadingZero(int value)
{
    return (value < 10 ? "0" : "") + String(value);
}

int convertStringTimeToMinutes(const String timeAsString)
{
    if (timeAsString.length() < 5)
    {
        return 0;
    }
    int hours = timeAsString.substring(0, 2).toInt();
    int minutes = timeAsString.substring(3, 5).toInt();

    return hours * 60 + minutes;
}

bool isBetween(int start, int end, int currentTimeInMinutes)
{
    if (start <= end)
    {
        return (currentTimeInMinutes >= start && currentTimeInMinutes < end);
    }
    else
    {
        return (currentTimeInMinutes >= start || currentTimeInMinutes < end);
    }
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
    lcd.init();
    lcd.backlight();
    lcd.clear();

    Serial.begin(19200);
    fxSerial.begin(9600);
    while (!Serial)
        ;

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

    timeClient.begin();
    RTC.begin();
}

void loop()
{
    timeClient.update();

    time_t epochTime = timeClient.getEpochTime();
    time_t localTime = CE.toLocal(epochTime);
    RTCTime currentTime(localTime);
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

    String timeForUpdate = formatTimeWithLeadingZero(currentTime.getHour()) + ":" + formatTimeWithLeadingZero(currentTime.getMinutes()) + ":" + formatTimeWithLeadingZero(currentTime.getSeconds());
    if (timeForUpdate == "00:01:00" || strcmp(prayerTimes.fajr, "--:--") == 0)
    {
        jsonDoc = getPrayerTimes(currentTime);
        updatePrayerTimes(jsonDoc);
    }
    if (timeForUpdate == "23:59:50")
    {
        soundState = {};
    }

    int fajr = convertStringTimeToMinutes(prayerTimes.fajr);
    int fajrEnd = convertStringTimeToMinutes(prayerTimes.fajr_endtime);
    int duhr = convertStringTimeToMinutes(prayerTimes.duhr);
    int asr = convertStringTimeToMinutes(prayerTimes.asr_2x_shadow);
    int asrEnd = convertStringTimeToMinutes(prayerTimes.asr_endtime);
    int maghrib = convertStringTimeToMinutes(prayerTimes.maghrib);
    int maghribEnd = convertStringTimeToMinutes(prayerTimes.maghrib_endtime);
    int isha = convertStringTimeToMinutes(prayerTimes.isha);
    int midnight = convertStringTimeToMinutes(prayerTimes.muntasafallayl_midnight);
    int ishaAzaanToggle = convertStringTimeToMinutes("22:00");
    int currentTimeInMinutes = currentTime.getHour() * 60 + currentTime.getMinutes();

    lcd.setCursor(0, 1);
    if (isBetween(midnight, fajr, currentTimeInMinutes))
    {
        lcd.print("FAJR: " + String(prayerTimes.fajr) + "                ");
    }
    else if (isBetween(fajr, duhr, currentTimeInMinutes))
    {
        if (currentTimeInMinutes < fajrEnd && currentTime.getSeconds() % 3 == 0)
        {
            lcd.print("FAJR END: " + String(prayerTimes.fajr_endtime) + "                ");
        }
        else
        {
            lcd.print("DUHR: " + String(prayerTimes.duhr) + "                ");
        }
    }
    else if (isBetween(duhr, asr, currentTimeInMinutes))
    {
        lcd.print("ASR: " + String(prayerTimes.asr_2x_shadow) + "                ");
        if (!soundState.duhr && currentTimeInMinutes == duhr)
        {
            soundState.duhr = true;
            playAdhan();
        }
    }
    else if (isBetween(asr, maghrib, currentTimeInMinutes))
    {
        if (currentTimeInMinutes < asrEnd && currentTime.getSeconds() % 3 == 0)
        {
            lcd.print("ASR END: " + String(prayerTimes.asr_endtime) + "                ");
        }
        else
        {
            lcd.print("MAGHRIB: " + String(prayerTimes.maghrib) + "                ");
        }

        if (!soundState.asr && currentTimeInMinutes == asr)
        {
            soundState.asr = true;
            playAdhan();
        }
    }
    else if (isBetween(maghrib, isha, currentTimeInMinutes))
    {
        if (currentTimeInMinutes < maghribEnd && currentTime.getSeconds() % 3 == 0)
        {
            lcd.print("MAGRB END: " + String(prayerTimes.maghrib_endtime) + "                ");
        }
        else
        {
            lcd.print("ISHA: " + String(prayerTimes.isha) + "                ");
        }

        if (!soundState.maghrib && currentTimeInMinutes == maghrib)
        {
            soundState.maghrib = true;
            playAdhan();
        }
    }
    else if (isBetween(isha, midnight, currentTimeInMinutes))
    {
        lcd.print("MIDNATT: " + String(prayerTimes.muntasafallayl_midnight) + "                ");
        if (!soundState.isha && currentTimeInMinutes < ishaAzaanToggle && currentTimeInMinutes == isha)
        {
            soundState.isha = true;
            playAdhan();
        }
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        displayMessage("WiFi lost!", "Reconnecting...");
        connectToWifi();
    }
}
