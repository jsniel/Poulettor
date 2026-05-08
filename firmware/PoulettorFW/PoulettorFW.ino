/*
 * ============================================================================
 *  Poulettor
 *  Open-source automatic chicken coop door controller
 * ============================================================================
 *
 *  Autonomous ESP32-C3 controller for opening and closing a chicken coop door
 *  using a DC motor, relay outputs, limit switches and DS3231 RTC scheduling.
 *
 *  Project:
 *      https://github.com/jsniel/Poulettor
 *
 *  Author:
 *      Jean-Sébastien Niel
 *
 *  Firmware License:
 *      GNU General Public License v3.0 or later
 *      SPDX-License-Identifier: GPL-3.0-or-later
 *
 *  Hardware License:
 *      CERN Open Hardware Licence Version 2 - Strongly Reciprocal
 *      SPDX-License-Identifier: CERN-OHL-S-2.0
 *
 *  Copyright (C) 2026 Jean-Sébastien Niel
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * ============================================================================
 */

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Wire.h>
#include <RTClib.h>
#include <EEPROM.h>


#define BUTTON_PIN 10
#define BUTTON_CLOSE_PIN 21
#define LED_PIN 2
#define ENDSTOP_UP_PIN 3
#define MOTOR_UP_PIN 4
#define ENDSTOP_BOT_PIN 0
#define MOTOR_BOT_PIN 1
#define RTC_INT_PIN 5
#define SDAPIN 8
#define SCLPIN 9
#define EEPROM_SIZE 8
#define EEPROM_OFFSET_LEVER 0       // 2 octets pour offsetLever
#define EEPROM_OFFSET_COUCHER 2     // 2 octets pour offsetCoucher


const float LATITUDE = 49.4687; //Bois-Guillaume
const float LONGITUDE = 1.1178; //Bois-Guillaume


bool horairesCalcules = false;

unsigned long mouvementStart = 0; // à placer en global

RTC_DS3231 rtc;

const char* ssid = "poulator2000";
const char* password = "nounette";

int offsetLever = 0;
int offsetCoucher = 0;


WebServer server(80);
bool webServerEnabled = false;
unsigned long webServerStartTime = 0;
const unsigned long WEB_SERVER_DURATION = 1 * 600 * 1000UL;
DNSServer dnsServer;


enum EtatMouvement {
  AUCUN,
  OUVERTURE_EN_COURS,
  FERMETURE_EN_COURS
};
EtatMouvement mouvement = AUCUN;


IPAddress local_IP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

bool lastButtonState = HIGH;
bool buttonStateStable = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;
unsigned long lastCheckTime = 0;

bool rtcOk = false;
unsigned long lastBlinkTime = 0;
bool ledState = false;
unsigned long bootTime = 0;

struct AutoFeedConfig {
  bool enabled;
  uint8_t hour;
  uint8_t minute;
};

AutoFeedConfig autoFeed1, autoFeed2;
bool hasDistributed1 = false;
bool hasDistributed2 = false;

String getFormattedTime() {
  DateTime now = rtc.now();
  char buffer[6];
  sprintf(buffer, "%02d:%02d", now.hour(), now.minute());
  return String(buffer);
}



void arreterMoteurs() {
  digitalWrite(MOTOR_UP_PIN, LOW);
  digitalWrite(MOTOR_BOT_PIN, LOW);
  mouvement = AUCUN;
  Serial.println("⛔ Moteurs arrêtés !");
}



void saveOffsetsToEEPROM() {
  EEPROM.writeShort(EEPROM_OFFSET_LEVER, offsetLever);
  EEPROM.writeShort(EEPROM_OFFSET_COUCHER, offsetCoucher);
  EEPROM.commit();
  Serial.printf("💾 Offsets sauvegardés : lever=%d, coucher=%d\n", offsetLever, offsetCoucher);
}

void loadOffsetsFromEEPROM() {
  offsetLever = EEPROM.readShort(EEPROM_OFFSET_LEVER);
  offsetCoucher = EEPROM.readShort(EEPROM_OFFSET_COUCHER);

  // Protection en cas de données corrompues
  if (offsetLever < -120 || offsetLever > 120) offsetLever = 0;
  if (offsetCoucher < -120 || offsetCoucher > 120) offsetCoucher = 0;

  Serial.printf("📥 Offsets chargés depuis EEPROM : lever=%d, coucher=%d\n", offsetLever, offsetCoucher);
}

void mettreAJourHorairesAuto() {
  DateTime now = rtc.now();
  bool ete = isHeureEte(now.day(), now.month(), now.year());

  int leverH, leverM, coucherH, coucherM;
  getSunriseSunset(now.day(), now.month(), now.year(), LATITUDE, LONGITUDE, ete, &leverH, &leverM, &coucherH, &coucherM);

  // Appliquer le décalage sur l'heure de lever
  int minutesLever = leverH * 60 + leverM + offsetLever;
  if (minutesLever < 0) minutesLever += 1440;
  if (minutesLever >= 1440) minutesLever -= 1440;
  autoFeed1.hour = minutesLever / 60;
  autoFeed1.minute = minutesLever % 60;
  autoFeed1.enabled = true;

  // Appliquer le décalage sur l'heure de coucher
  int minutesCoucher = coucherH * 60 + coucherM + offsetCoucher;
  if (minutesCoucher < 0) minutesCoucher += 1440;
  if (minutesCoucher >= 1440) minutesCoucher -= 1440;
  autoFeed2.hour = minutesCoucher / 60;
  autoFeed2.minute = minutesCoucher % 60;
  autoFeed2.enabled = true;

  hasDistributed1 = false;
  hasDistributed2 = false;

  horairesCalcules = true;

  Serial.printf("🕊️ Lever soleil : %02d:%02d | Coucher soleil : %02d:%02d\n", leverH, leverM, coucherH, coucherM);
  Serial.printf("⏰ Action ouverture à %02d:%02d (décalage %+d min)\n", autoFeed1.hour, autoFeed1.minute, offsetLever);
  Serial.printf("⏰ Action fermeture à %02d:%02d (décalage %+d min)\n", autoFeed2.hour, autoFeed2.minute, offsetCoucher);
}


bool isHeureEte(int jour, int mois, int annee) {
  if (mois < 3 || mois > 10) return false;
  if (mois > 3 && mois < 10) return true;

  int dernierDimanche = 31;
  DateTime date;
  if (mois == 3) {
    while (true) {
      date = DateTime(annee, 3, dernierDimanche, 2, 0, 0);
      if (date.dayOfTheWeek() == 0) break;
      dernierDimanche--;
    }
    return (jour >= dernierDimanche);
  } else if (mois == 10) {
    while (true) {
      date = DateTime(annee, 10, dernierDimanche, 3, 0, 0);
      if (date.dayOfTheWeek() == 0) break;
      dernierDimanche--;
    }
    return (jour < dernierDimanche);
  }
  return false;
}


void getSunriseSunset(int day, int month, int year, float latitude, float longitude, bool ete, int* riseH, int* riseM, int* setH, int* setM) {
  float zenith = 90.833;  // Angle officiel

  int N1 = floor(275 * month / 9);
  int N2 = floor((month + 9) / 12);
  int K = 1 + floor((year - 4 * floor(year / 4) + 2) / 3);
  int N = N1 - (K * N2) + day - 30;

  float lngHour = longitude / 15.0;

  auto calcTime = [&](bool sunrise) -> float {
    float t = sunrise ? N + ((6 - lngHour) / 24) : N + ((18 - lngHour) / 24);
    float M = (0.9856 * t) - 3.289;
    float L = fmod(M + (1.916 * sin(M * DEG_TO_RAD)) + (0.020 * sin(2 * M * DEG_TO_RAD)) + 282.634, 360.0);
    float RA = fmod(atan(0.91764 * tan(L * DEG_TO_RAD)) * RAD_TO_DEG, 360.0);
    float Lquadrant = floor(L / 90.0) * 90.0;
    float RAquadrant = floor(RA / 90.0) * 90.0;
    RA = RA + (Lquadrant - RAquadrant);
    RA /= 15.0;

    float sinDec = 0.39782 * sin(L * DEG_TO_RAD);
    float cosDec = cos(asin(sinDec));
    float cosH = (cos(zenith * DEG_TO_RAD) - (sinDec * sin(latitude * DEG_TO_RAD))) / (cosDec * cos(latitude * DEG_TO_RAD));

    if (cosH > 1 || cosH < -1) return -1;  // Soleil ne se lève/couche pas ce jour-là

    float H = sunrise ? 360 - acos(cosH) * RAD_TO_DEG : acos(cosH) * RAD_TO_DEG;
    H = H / 15.0;

    float T = H + RA - (0.06571 * t) - 6.622;
    float UT = fmod(T - lngHour, 24.0);
    if (UT < 0) UT += 24;

    int timezone = ete ? 2 : 1;
    float localT = UT + timezone;
    if (localT >= 24) localT -= 24;

    return localT;
  };

  float riseTime = calcTime(true);
  float setTime = calcTime(false);

  *riseH = int(riseTime);
  *riseM = int((riseTime - *riseH) * 60);
  *setH = int(setTime);
  *setM = int((setTime - *setH) * 60);
}

void ouvrirPorte() {
  if (mouvement != AUCUN) {
    Serial.println("🛑 Interruption en cours, passage à l'ouverture !");
    arreterMoteurs();
    delay(200); // petit délai de sécurité
  }

  if (digitalRead(ENDSTOP_UP_PIN) == LOW) {
    Serial.println("⚠️ Porte déjà ouverte");
    return;
  }

  Serial.println("🔼 Ouverture en cours...");
  digitalWrite(MOTOR_BOT_PIN, LOW); // sécurité
  digitalWrite(MOTOR_UP_PIN, HIGH);
  mouvement = OUVERTURE_EN_COURS;
  mouvementStart = millis(); // pour le timeout
}



void fermerPorte() {
  
  if (mouvement != AUCUN) {
    Serial.println("🛑 Interruption en cours, passage à la fermeture !");
    arreterMoteurs();
    delay(200); // petit délai de sécurité
  }

  if (digitalRead(ENDSTOP_BOT_PIN) == LOW) {
    Serial.println("⚠️ Porte déjà fermée");
    return;
  }

  Serial.println("🔽 Fermeture en cours...");
  digitalWrite(MOTOR_UP_PIN, LOW); // sécurité
  digitalWrite(MOTOR_BOT_PIN, HIGH);
  mouvement = FERMETURE_EN_COURS;
  mouvementStart = millis(); // pour le timeout
}



String generateOptions(int max, int selected) {
  String options;
  for (int i = 0; i <= max; i++) {
    options += "<option value='" + String(i) + "'" + (i == selected ? " selected" : "") + ">" + String(i) + "</option>";
  }
  return options;
}

String generateOffsetOptions(int selected) {
  String options = "";
  for (int i = -120; i <= 120; i += 5) {
    options += "<option value='" + String(i) + "'";
    if (i == selected) options += " selected";
    options += ">" + String(i) + " min</option>";
  }
  return options;
}

String generateHTML() {
  String page = R"rawliteral(
  <!DOCTYPE html>
  <html lang='fr'>
  <head>
    <meta charset='UTF-8'>
    <meta name='viewport' content='width=device-width, initial-scale=1.0'>
    <title>Porte Poulailler</title>
    <style>
      body { font-family: Arial, sans-serif; text-align: center; background-color: #f8f3e3; padding: 20px; }
      h1 { color: #6a4c93; }
      button {
        background-color: #ffcb77;
        border: none;
        padding: 15px 30px;
        font-size: 24px;
        color: white;
        cursor: pointer;
        border-radius: 10px;
        margin: 10px;
        box-shadow: 2px 2px 10px rgba(0,0,0,0.2);
      }
      button:active { background-color: #ff9f1c; }
      .form-container {
        margin-top: 40px;
        background-color: #fff;
        padding: 20px;
        border-radius: 12px;
        display: inline-block;
        box-shadow: 2px 2px 15px rgba(0,0,0,0.1);
      }
      select { padding: 10px; font-size: 16px; margin: 5px; }
    </style>
  </head>
  <body>
    <div style='position: absolute; top: 10px; left: 10px; font-size: 18px; color: #333;'>Temps restant : <span id='countdown'>--</span>s</div>
    <br>
    <div id='datetime' style='font-size: 24px; margin-bottom: 10px;'>--/--/---- --:--</div>
    <h1>Porte de Poulailler Automatique</h1>
    <button onclick="fetch('/ouvrir')">Ouvrir la porte</button>
    <button onclick="fetch('/fermer')">Fermer la porte</button>

    <div class='form-container'>
      <h2>Ouverture automatique (matin)</h2>
      <p>Lever du soleil : %LEVER%</p>
      <p>Décalage : %OFFSET1% min</p>
      <p>Action prévue à : %AUTO1%</p>
    </div>

    <div class='form-container'>
      <h2>Fermeture automatique (soir)</h2>
      <p>Coucher du soleil : %COUCHER%</p>
      <p>Décalage : %OFFSET2% min</p>
      <p>Action prévue à : %AUTO2%</p>
    </div>

    <div class='form-container'>
      <h2>Définir les décalages</h2>
      <form method='GET' action='/setoffset'>
        Lever : <select name='offsetLever'>%OPTIONS1%</select><br>
        Coucher : <select name='offsetCoucher'>%OPTIONS2%</select><br>
        <button type='submit'>Valider</button>
      </form>
    </div>

    <div class='form-container'>
      <h2>Mettre à jour l'heure</h2>
      <form method='GET' action='/settime'>
        Heure: <select name='h'>%HOURS%</select>
        Minute: <select name='m'>%MINUTES%</select>
        <button type='submit'>Définir l'heure</button>
      </form>
    </div>

    <script>
      async function updateClock() {
        try {
          const res = await fetch('/datetime');
          const text = await res.text();
          document.getElementById('datetime').textContent = text;
        } catch (e) {
          document.getElementById('datetime').textContent = '--/--/---- --:--';
        }
      }

      let remaining = parseInt('%SERVER_REMAINING%');
      if (isNaN(remaining)) remaining = 0;

      function updateCountdown() {
        const countdownEl = document.getElementById('countdown');
        if (remaining > 0) {
          countdownEl.textContent = remaining;
          countdownEl.style.color = (remaining <= 10) ? 'red' : '#333';
          remaining--;
        } else {
          countdownEl.textContent = '--';
          countdownEl.style.color = '#333';
        }
      }

      setInterval(updateClock, 10000);
      setInterval(updateCountdown, 1000);
      updateClock();
      updateCountdown();
    </script>
  </body>
  </html>
  )rawliteral";

  DateTime now = rtc.now();
  bool ete = isHeureEte(now.day(), now.month(), now.year());

  int leverH, leverM, coucherH, coucherM;
  getSunriseSunset(now.day(), now.month(), now.year(), LATITUDE, LONGITUDE, ete, &leverH, &leverM, &coucherH, &coucherM);

  page.replace("%LEVER%", String(leverH) + ":" + (leverM < 10 ? "0" : "") + String(leverM));
  page.replace("%COUCHER%", String(coucherH) + ":" + (coucherM < 10 ? "0" : "") + String(coucherM));
  page.replace("%OFFSET1%", String(offsetLever));
  page.replace("%OFFSET2%", String(offsetCoucher));
  page.replace("%AUTO1%", String(autoFeed1.hour) + ":" + (autoFeed1.minute < 10 ? "0" : "") + String(autoFeed1.minute));
  page.replace("%AUTO2%", String(autoFeed2.hour) + ":" + (autoFeed2.minute < 10 ? "0" : "") + String(autoFeed2.minute));
  page.replace("%OPTIONS1%", generateOffsetOptions(offsetLever));
  page.replace("%OPTIONS2%", generateOffsetOptions(offsetCoucher));
  page.replace("%HOURS%", generateOptions(23, now.hour()));
  page.replace("%MINUTES%", generateOptions(59, now.minute()));

  unsigned long remainingTime = WEB_SERVER_DURATION;
  if (webServerEnabled && millis() - webServerStartTime < WEB_SERVER_DURATION) {
    remainingTime = WEB_SERVER_DURATION - (millis() - webServerStartTime);
  }
  page.replace("%SERVER_REMAINING%", String(remainingTime / 1000));

  return page;
}


void setup() {
  Serial.begin(115200);
  delay(2000);

autoFeed1.enabled = true;
autoFeed2.enabled = true;

EEPROM.begin(EEPROM_SIZE);
loadOffsetsFromEEPROM();  // Charger les valeurs à l’allumage


  pinMode(MOTOR_UP_PIN, OUTPUT);
  pinMode(MOTOR_BOT_PIN, OUTPUT);
  pinMode(ENDSTOP_UP_PIN, INPUT_PULLUP);
  pinMode(ENDSTOP_BOT_PIN, INPUT_PULLUP);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUTTON_CLOSE_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(MOTOR_UP_PIN, LOW);
  digitalWrite(MOTOR_BOT_PIN, LOW);

  Wire.begin(SDAPIN, SCLPIN);
  rtcOk = rtc.begin();
  if (!rtcOk) {
  Serial.println("Erreur : RTC non détectée !");
} else {
  if (rtc.lostPower()) rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  mettreAJourHorairesAuto();  // <<< APPEL ICI
}
//rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));//////////////////////////////////////////////A supprimer
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", generateHTML());
  });

server.on("/setoffset", HTTP_GET, []() {
  if (server.hasArg("offsetLever") && server.hasArg("offsetCoucher")) {
    offsetLever = server.arg("offsetLever").toInt();
    offsetCoucher = server.arg("offsetCoucher").toInt();
    saveOffsetsToEEPROM(); // <<< Sauvegarde ici
    mettreAJourHorairesAuto();
    server.sendHeader("Location", "/", true);
    server.send(302, "Décalages mis à jour", "");
  } else {
    server.send(400, "text/plain", "Paramètres manquants");
  }
});


  server.on("/ouvrir", HTTP_GET, []() {
    ouvrirPorte();
    server.send(200, "text/plain", "Porte ouverte");
  });

  server.on("/fermer", HTTP_GET, []() {
    fermerPorte();
    server.send(200, "text/plain", "Porte fermée");
  });

  server.on("/time", HTTP_GET, []() {
    if (rtcOk) {
      server.send(200, "text/plain", getFormattedTime());
    } else {
      server.send(503, "text/plain", "RTC non disponible");
    }
  });

  server.on("/datetime", HTTP_GET, []() {
  if (rtcOk) {
    DateTime now = rtc.now();
    char buffer[30];
    snprintf(buffer, sizeof(buffer), "%02d/%02d/%04d %02d:%02d",
             now.day(), now.month(), now.year(), now.hour(), now.minute());
    server.send(200, "text/plain", buffer);
  } else {
    server.send(503, "text/plain", "RTC non disponible");
  }
});


  server.on("/settime", HTTP_GET, []() {
    if (server.hasArg("h") && server.hasArg("m")) {
      int h = server.arg("h").toInt();
      int m = server.arg("m").toInt();
      DateTime now = rtc.now();
      rtc.adjust(DateTime(now.year(), now.month(), now.day(), h, m, 0));
      server.sendHeader("Location", "/", true);
      server.send(302, "Heure mise à jour", "");
    } else {
      server.send(400, "text/plain", "Paramètres manquants");
    }
  });

  server.onNotFound([]() {
    server.sendHeader("Location", "http://" + WiFi.softAPIP().toString(), true);
    server.send(302, "text/plain", "Redirecting...");
  });

  bootTime = millis();
  // Serveur lancé seulement sur demande via bouton
}

void loop() {
  dnsServer.processNextRequest();
  if (webServerEnabled) server.handleClient();

  handleButton();

  // 🧲 Gestion du bouton fermeture
  static bool lastCloseBtn = HIGH;
  bool currentCloseBtn = digitalRead(BUTTON_CLOSE_PIN);
  if (lastCloseBtn == HIGH && currentCloseBtn == LOW) {
    delay(50); // debounce
    if (digitalRead(BUTTON_CLOSE_PIN) == LOW) {
            hasDistributed1 = false;  // <-- désactive la protection
            hasDistributed2 = false;  // <-- désactive la protection
      if (mouvement != AUCUN) {
        Serial.println("🛑 Interruption en cours par bouton fermeture");
        arreterMoteurs();
        delay(200);
        fermerPorte();
      } else {
        Serial.println("👆 Bouton fermeture manuel appuyé !");
        fermerPorte();
      }
    }
  }
  lastCloseBtn = currentCloseBtn;

  // ✅ Fin de course ou timeout
  static bool erreurTimeout = false;

  if (mouvement == OUVERTURE_EN_COURS) {
    if (digitalRead(ENDSTOP_UP_PIN) == LOW) {
    delay(100); // Anti-rebond
      if (digitalRead(ENDSTOP_UP_PIN) == LOW) {
        Serial.println("✅ Porte ouverte (fin de course haut détecté)");
        arreterMoteurs();
      }
    }
else if (millis() - mouvementStart > 15000) {
      Serial.println("❌ Timeout ouverture ! Moteur arrêté.");
      arreterMoteurs();
      erreurTimeout = true;
    }
  }

  if (mouvement == FERMETURE_EN_COURS) {
    if (digitalRead(ENDSTOP_BOT_PIN) == LOW) {
      Serial.println("✅ Porte fermée (fin de course bas détecté)");
      arreterMoteurs();
    } else if (millis() - mouvementStart > 15000) {
      Serial.println("❌ Timeout fermeture ! Moteur arrêté.");
      arreterMoteurs();
      erreurTimeout = true;
    }
  }

  // 🕒 Tâches périodiques toutes les 10 secondes
  unsigned long nowMillis = millis();
  if (nowMillis - lastCheckTime >= 10000) {
    lastCheckTime = nowMillis;

    DateTime now = rtc.now();
    rtcOk = now.isValid() && now.hour() < 24 && now.minute() < 60;

    // 📴 Extinction automatique du serveur web
    if (webServerEnabled && (nowMillis - webServerStartTime > WEB_SERVER_DURATION)) {
      Serial.println("Extinction automatique du serveur web");
      server.stop();
      WiFi.softAPdisconnect(true);
      webServerEnabled = false;
    }

    // 🕛 Mise à jour des horaires à 00:10
    if (rtcOk && now.hour() == 0 && now.minute() == 10 && !horairesCalcules) {
      mettreAJourHorairesAuto();
    }
    if (rtcOk && now.hour() == 0 && now.minute() == 11) {
      horairesCalcules = false;
    }

    // 🚪 Ouverture/Fermeture automatique
    if (!erreurTimeout && rtcOk && nowMillis - bootTime > 5000) {
      // if (autoFeed1.enabled && now.hour() == autoFeed1.hour && now.minute() == autoFeed1.minute && !hasDistributed1) {
      //   ouvrirPorte();
      //   hasDistributed1 = true;
      // }
      // if (autoFeed2.enabled && now.hour() == autoFeed2.hour && now.minute() == autoFeed2.minute && !hasDistributed2) {
      //   fermerPorte();
      //   hasDistributed2 = true;
      // }
      // 🔄 Conversion de l'heure actuelle et des heures cibles en minutes depuis minuit
      int currentMinutes = now.hour() * 60 + now.minute();
      int targetMinutes1 = autoFeed1.hour * 60 + autoFeed1.minute;
      int targetMinutes2 = autoFeed2.hour * 60 + autoFeed2.minute;

      // 🕐 Tolérance : action autorisée pendant la minute prévue (±1 minute)
      if (autoFeed1.enabled && !hasDistributed1 &&
          abs(currentMinutes - targetMinutes1) <= 1) {
        Serial.println("⏰ Déclenchement ouverture automatique (fenêtre ±1 min)");
        ouvrirPorte();
        hasDistributed1 = true;
      }

      if (autoFeed2.enabled && !hasDistributed2 &&
          abs(currentMinutes - targetMinutes2) <= 1) {
        Serial.println("🌙 Déclenchement fermeture automatique (fenêtre ±1 min)");
        fermerPorte();
        hasDistributed2 = true;
      }

    }
  }

  // 💡 LED état système
  if (erreurTimeout) {
    // ⚠️ Clignotement rapide 250 ms
    if (millis() - lastBlinkTime >= 250) {
      lastBlinkTime = millis();
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
    }
  }
  else if (!rtcOk || !rtc.begin()) {
    // 🔴 Clignotement lent
    if (millis() - lastBlinkTime >= 500) {
      lastBlinkTime = millis();
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
    }
  } else {
    // 🟢 LED fixe ou clignotement serveur
    if (webServerEnabled) {
      unsigned long patternTime = millis() % 1200;
      digitalWrite(LED_PIN, (patternTime < 200 || (patternTime >= 400 && patternTime < 600)) ? HIGH : LOW);
    } else {
      digitalWrite(LED_PIN, (autoFeed1.enabled || autoFeed2.enabled) ? HIGH : LOW);
    }
  }
}



void handleButton() {
  if (millis() - bootTime < 3000) return;

  static bool lastButtonState = HIGH;
  static unsigned long lastDebounceTime = 0;
  static bool buttonStateStable = HIGH;
  static unsigned long buttonPressStart = 0;
  static bool longPressHandled = false;

  bool reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonStateStable) {
      buttonStateStable = reading;

      if (buttonStateStable == LOW) {
        buttonPressStart = millis();
        longPressHandled = false;
      } else {
        unsigned long pressDuration = millis() - buttonPressStart;
        if (pressDuration < 3000 && !longPressHandled) {
             hasDistributed1 = false;  // <-- désactive la protection
            hasDistributed2 = false;  // <-- désactive la protection
          if (mouvement != AUCUN) {
            Serial.println("🛑 Interruption en cours par bouton ouverture");
            arreterMoteurs();
            delay(200); // petit délai de sécurité
            ouvrirPorte();
          }

          else {
            Serial.println("Appui bouton court détecté - ouverture manuelle");
            ouvrirPorte();
          }
}

        buttonPressStart = 0;
        longPressHandled = false;
      }
    } else if (buttonStateStable == LOW && !longPressHandled) {
      if (millis() - buttonPressStart >= 3000) {
        Serial.println("Appui long détecté - activation du serveur web");
        startWebServer();
        longPressHandled = true;
      }
    }
  }

  lastButtonState = reading;
}

void startWebServer() {
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(ssid, password, 11);
  dnsServer.start(53, "", WiFi.softAPIP());
  server.begin();
  webServerEnabled = true;
  webServerStartTime = millis();
  Serial.println("Serveur web démarré.");
}
