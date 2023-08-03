// Problemlose Funktion nur mit ** Arduino Nano / Uno ***
// ******************************************************
// ********** Änderungen NUR hier vornehmen *************
#define RING_LEDS_NUM 33   // Anzahl LEDs im "Run"      *
#define RING_LEDS_PIN 11   // DatenPin für den "Run"    *
#define SCORE_LEDS_NUM 20  // Anzahl LEDs aller "Score" *
#define SCORE_LEDS_PIN 12  // DatenPin für den "Score"  *
#define Button1Pin 2       // Pin für Taster 1 Rot      *
#define Button2Pin 3       // Pin für Taster 2 Gelb     *
#define Button3Pin 4       // Pin für Taster 3 Grün     *
#define Button4Pin 5       // Pin für Taster 4 Blau     *
// ******** Ab hier KEINE Änderungen vornehmen **********

// ************ Kurzanleitung (Stand 29.05.2020) ************
// Taste ROT  : Spiel einstellen ( 1 - 2 )                  *
// Taste GELB : Spiel 1 : Level einstellen ( 1 - 3 )        *
// Taste GELB : Spiel 2 : Gewinnspiele einstellen ( 1 - 5 ) *
// Taste GRÜN : Helligkeit einstellen (20-40-60-80-100%)    *
// Taste BLAU : Spiel starten                               *
// Jedes Spiel beginnt durch Tastendruck aller Mitspieler   *
// **********************************************************

#include <EEPROM.h>
#include <Bounce2.h>
#include "PinChangeInterrupt.h"
#include "FastLED.h"

Bounce btn1Bounce = Bounce(Button1Pin, 50);
Bounce btn2Bounce = Bounce(Button2Pin, 50);
Bounce btn3Bounce = Bounce(Button3Pin, 50);
Bounce btn4Bounce = Bounce(Button4Pin, 50);

byte FadeInterval = 100;                // *** Geschwindigkeit des Fade im Idle-Modus
byte PlayerBtns[] = { 0, 0, 0, 0, 0 };  // Index 4 ist "irgendeine Taste gedrückt"
byte PlayerActive[] = { 0, 0, 0, 0 };   // Welcher Spieler spielt mit ?
byte ButtonPin[] = { Button1Pin, Button2Pin, Button3Pin, Button4Pin };
byte PressedButton = 0;                 // Wer hat gedrückt ?
byte Score[] = { 0, 0, 0, 0 };          // Score der vier Spieler
byte Score1stLED[] = { 0, 5, 10, 15 };  // Pos. der ersten Score-LED jedes Spielers
byte Level = 0;                         // Spiel-Level -> Geschwindigkeit des Runners
byte KeyValue = 1;                      // Übergabe bei Einstellungen über Taster
boolean Interrupt = false;              // Zustand des Taster-Interrupts
byte CurrentPos = 0;                    // für "Lauflicht"
byte ValueLevel = 0;                    // Welcher der 3 Datensätze für "ChaosRun" wird verwendet
byte Brightness = 0;

// ********************* Zwischenspeicher für "Reaction"
// ********************* 00,01,02,03,04,05,06,07,08,09,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32
byte LEDRing[33] = { 15, 15, 12, 15, 15, 15, 15, 15, 15, 0, 0, 0, 15, 15, 15, 15, 15, 15, 10, 15, 15, 15, 15, 15, 15, 0, 0, 0, 0, 15, 15, 15, 15 };
byte UrRing[33] = { 15, 15, 12, 15, 15, 15, 15, 15, 15, 0, 0, 0, 15, 15, 15, 15, 15, 15, 10, 15, 15, 15, 15, 15, 15, 0, 0, 0, 0, 15, 15, 15, 15 };

// ****************** Farbvergabe für Score-Anzeigen ******************
byte R[14] = { 255, 255, 0, 0, 255, 255, 0, 0, 0, 0, 255, 255, 0, 0 };
byte G[14] = { 255, 255, 255, 0, 127, 0, 0, 0, 0, 0, 0, 255, 255, 0 };
byte B[14] = { 255, 0, 0, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255 };

// ******* ChaosRun *******
byte RunnerPosAct = 0;
byte RunnerPosOld = 0;
byte RunnerDirection = 0;
byte TargetPosAct = 0;
byte TargetPosOld = 0;
byte TargetWidth = 0;  // 1=EinzelPunkt 3=3-Punkt

// *** Level, an dem das Target von 3 Pixel auf 1 Pixel verkleinert wird
byte SwitchTargetPrecisionAtLevel = 9;

// *** Interne LED ***
byte LED = 13;

// *** Alle verwendbaren Zeiten ***
// ***  für  ALLE  Routinen !   ***
unsigned long AktZeit;
unsigned long StartZeit;
unsigned long DiffZeit;
unsigned long TimeOut;
// ********************************

// *** Variablen für ALLE Spiele ***
byte GameMode = 2;                            // Eingestellte Spielvariante
byte GameModeMax = 2;                         // Wieviele Spielvarianten gibt es = max. 5 !
byte GameLevel[] = { 1, 3, 5, 1, 1, 1 };      // Eingestelltes Spiellevel
byte GameLevelMax[] = { 1, 4, 5, 1, 1, 1 };   // Wieviele Spiellevel gibt es ? max. 5 !
byte GameBright[] = { 1, 1, 1, 1, 1, 1 };     // Eingestellte Helligkeit *20%
byte GameBrightMax[] = { 1, 5, 5, 5, 5, 5 };  // 5 Helligkeiten : 20% - 40% - 60% - 80% - 100%
byte GameState = 0;                           // Status des aktuellen Spiels
byte ConfigState = 0;                         // Status der Einstellungen / ConfigState=99 -> GameState=1 (Spiel-Initialisierung
boolean GameRunning = false;                  // Hilfs-Status für Loop-Funktion des aktuellen Spiels
boolean ContestRunning = false;               // Hilfs-Status für Loop-Funktion über mehrere Spiele
byte SavingNeeded = 0;                        // Merke, ob beim Spielstart die Parameter gespeichert werden sollen

// Gschwindigkeits-Daten für Spiel 1 "ChaosRun"
// ***********************************************************************
int MaxLevel = 30;  // Maximum ist aufgrund "LevelSpeed[30]" der Wert 30 *
// ***********************************************************************
const byte LevelSpeed1[] = { 200, 195, 190, 185, 180, 175, 170, 165, 160, 155, 150, 145, 140, 135, 130, 125, 120, 115, 110, 100, 90, 80, 70, 60, 50, 40, 35, 30, 25, 20 };
// Alt : const int LevelSpeed2[] = { 330,320,310,300,290,280,270,260,250,240,230,220,210,200,190,180,170,160,150,140,130,120,110,100, 90, 80, 70, 60, 50, 40};
const byte LevelSpeed2[] = { 165, 160, 155, 150, 145, 140, 135, 130, 125, 120, 115, 110, 105, 100, 95, 90, 85, 80, 75, 70, 65, 60, 55, 50, 45, 40, 35, 30, 25, 20 };
// Alt : const int LevelSpeed3[] = { 330,300,290,280,270,260,250,225,200,175,150,125,120,115,110,105,100, 95, 90, 85, 80, 75, 70, 65, 60, 55, 50, 40, 35, 30};
const byte LevelSpeed3[] = { 165, 150, 145, 140, 135, 130, 125, 112, 100, 87, 75, 62, 60, 57, 55, 52, 50, 47, 45, 42, 40, 37, 35, 32, 30, 27, 25, 20, 17, 15 };
// Alt : const int LevelSpeed4[] = { 330,300,290,280,270,260,250,225,200,175,150,125,100, 95, 90, 85, 80, 75, 70, 65, 60, 55, 50, 40, 35, 30, 25, 20, 15, 10};
const byte LevelSpeed4[] = { 165, 150, 145, 140, 135, 130, 125, 112, 100, 87, 75, 62, 50, 47, 45, 42, 40, 37, 35, 32, 30, 27, 25, 20, 17, 15, 12, 10, 7, 5 };
byte LevelSpeed[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
// Gschwindigkeits-Daten für Spiel 1 "ChaosReaction"
// Die Daten werden wir in "ChaosRun" generiert, es wird aber ausschließlich LevelSpeed1[] verwendet !
// ***************************************************************************************************

// ==========================================
CRGB ringLeds[RING_LEDS_NUM];
CRGB scoreLeds[SCORE_LEDS_NUM];

// Enable UART Debug
// ==========================================
#define DEBUG 0

void setup() {
  pinMode(Button1Pin, INPUT_PULLUP);
  pinMode(Button2Pin, INPUT_PULLUP);
  pinMode(Button3Pin, INPUT_PULLUP);
  pinMode(Button4Pin, INPUT_PULLUP);

  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);
#if DEBUG
  Serial.begin(9600);
#endif
  FastLED.addLeds<WS2812B, RING_LEDS_PIN, GRB>(ringLeds, RING_LEDS_NUM);
  FastLED.addLeds<WS2812B, SCORE_LEDS_PIN, GRB>(scoreLeds, SCORE_LEDS_NUM);

  readEEPROM();

  if (SavingNeeded == 111) {
    Brightness = GameBright[GameMode] * 20;
    SavingNeeded = 0;
  } else {
    Brightness = 5;
    SavingNeeded = 1;
  }
  FastLED.setBrightness(Brightness);  // 100% / 80% / 60% / 40% / 20%

  randomSeed(analogRead(A0));

  attachPCINT(digitalPinToPCINT(Button1Pin), ISRButton1, FALLING);
  attachPCINT(digitalPinToPCINT(Button2Pin), ISRButton2, FALLING);
  attachPCINT(digitalPinToPCINT(Button3Pin), ISRButton3, FALLING);
  attachPCINT(digitalPinToPCINT(Button4Pin), ISRButton4, FALLING);

  disablePCINT(digitalPinToPCINT(Button1Pin));
  disablePCINT(digitalPinToPCINT(Button2Pin));
  disablePCINT(digitalPinToPCINT(Button3Pin));
  disablePCINT(digitalPinToPCINT(Button4Pin));
  Interrupt = false;

  copyLevel(ValueLevel);
}

void copyLevel(byte lvl) {  // Abhängig von "ValueLevel" die Werte in "LevelSpeed[] kopieren
  switch (lvl) {
    case 0:  // LevelSpeed1
      for (byte i = 0; i < MaxLevel; i++) { LevelSpeed[i] = LevelSpeed1[i]; }
      break;
    case 1:  // LevelSpeed2
      for (byte i = 0; i < MaxLevel; i++) { LevelSpeed[i] = LevelSpeed2[i]; }
      break;
    case 2:  // LevelSpeed2
      for (byte i = 0; i < MaxLevel; i++) { LevelSpeed[i] = LevelSpeed3[i]; }
      break;
    case 3:  // LevelSpeed2
      for (byte i = 0; i < MaxLevel; i++) { LevelSpeed[i] = LevelSpeed4[i]; }
      break;
      //  case 4: // LevelSpeed2
      //    for (byte i=0;i<MaxLevel;i++) { LevelSpeed[i] = LevelSpeed5[i]; }
      //    break;
  }
}

void writeEEPROM() {  // Inhalt der Einstellungen in das EEPROM schreiben
  // byte       GameMode        = 2;                 // Eingestellte Spielvariante
  // byte       GameModeMax     = 2;                 // Wieviele Spielvarianten gibt es = max. 5 !
  // byte       GameLevel[]     = {1,1,5,1,1,1};     // Eingestelltes Spiellevel
  // byte       GameLevelMax[]  = {1,4,5,1,1,1};     // Wieviele Spiellevel gibt es ? max. 5 !
  // byte       GameBright[]    = {1,1,1,1,1,1};     // Eingestellte Helligkeit *20%
  // byte       GameBrightMax[] = {1,5,5,5,5,5};     // 5 Helligkeiten : 20% - 40% - 60% - 80% - 100%
  // EEPROM.write(addr. val);
  byte address, data, index;
  address = 0;
  // StatusByte schreiben - 111 = gültige Daten vorhanden
  data = 111;
  EEPROM.write(address, data);
  address++;
  // GameMode
  data = GameMode;
  EEPROM.write(address, data);
  address++;
  data = GameModeMax;
  EEPROM.write(address, data);
  address++;
  // GameLevel
  for (index = 0; index < 6; index++) {
    data = GameLevel[index];
    EEPROM.write(address, data);
    address++;
  }
  for (index = 0; index < 6; index++) {
    data = GameLevelMax[index];
    EEPROM.write(address, data);
    address++;
  }
  // GameBright
  for (index = 0; index < 6; index++) {
    data = GameBright[index];
    EEPROM.write(address, data);
    address++;
  }
  for (index = 0; index < 6; index++) {
    data = GameBrightMax[index];
    EEPROM.write(address, data);
    address++;
  }
  // Save-Anforderung zurücksetzen
  SavingNeeded = 0;
}

void readEEPROM() {  // Inhalt der Einstellungen aus dem EEPROM lesen
  // val = EEPROM.read(addr)
  byte address, data, index;
  address = 0;
  // StatusByte lesen - 111 = gültige Daten vorhanden
  data = EEPROM.read(address);
  SavingNeeded = data;
  address++;
  if (SavingNeeded == 111) {  // restliche Daten einlesen
#if DEBUG
    Serial.println("Es sind gültige Daten im EEPROM vorhanden !");
#endif
    // GameMode
    data = EEPROM.read(address);
    GameMode = data;
    address++;
    data = EEPROM.read(address);
    GameModeMax = data;
    address++;
    // GameLevel
    for (index = 0; index < 6; index++) {
      data = EEPROM.read(address);
      GameLevel[index] = data;
      address++;
    }
    for (index = 0; index < 6; index++) {
      data = EEPROM.read(address);
      GameLevelMax[index] = data;
      address++;
    }
    // GameBright
    for (index = 0; index < 6; index++) {
      data = EEPROM.read(address);
      GameBright[index] = data;
      address++;
    }
    for (index = 0; index < 6; index++) {
      data = EEPROM.read(address);
      GameBrightMax[index] = data;
      address++;
    }
  }
}

void loop() {           // Einstellunmgen "Spiel-unabhängig"
  switch (GameState) {  // Den Status im Spiel auswerten
    case 0:
#if DEBUG
      Serial.println("Loop : SetupGame");
#endif
      setupGame();
      break;
    case 1:

#if DEBUG
      Serial.println("Loop : InitGame");
#endif
      initGame();
      break;
    case 2:
#if DEBUG
      Serial.println("Loop : RunGame");
#endif
      runGame();
      break;
    case 3:

#if DEBUG
      Serial.println("Loop : EndGame");
#endif
      endGame();
      break;
  }
}

void setupGame() {
#if DEBUG
  Serial.println("Sub : SetupGame");
  Serial.print("GameMode : ");
  Serial.print(GameMode);
  Serial.print(" ConfigState : ");
  Serial.println(ConfigState);
#endif
  switch (ConfigState) {
    case 0:  // Einstellungen abfragen
      DetectSettings();
      // Wenn Auswertung -> Button 1 -> GameMode einstellen
      // ConfigState = 1 -> setupOfGameMode();
      // Wenn Auswertung -> Button 2 -> GameLevel einstellen
      // ConfigState = 2 -> setupOfGameLevel();
      // Wenn Auswertung -> Button 3 ->
      // ConfigState = 3 ->
      // Wenn Auswertung -> Button 4 -> GamePlayer einstellen
      // ConfigState = 4 -> setupOfGamePlayer();  || DetectPlayers();
      // Wenn Auswertung -> kein Button -> Defaultwerte
      // Einstellungen für "TimeOut"
      // *** Default-Einstellungen ***
      break;
    case 1:  // ConfigState = 1 = GameMode einstellen
      setupUniversal(ConfigState);
      break;
    case 2:  // ConfigState = 2 = GameLevel einstellen
      setupUniversal(ConfigState);
      // setupOfGameLevel();
      break;
    case 3:  // ConfigState = 3 = Brightness einstellen
      setupUniversal(ConfigState);
      // SetupOfBrightness();
      break;
    case 4:  // ConfigState = 4 = GamePlayer eiinstellen
      setupUniversal(ConfigState);
      // SetupOfPlayers -> DetectPlayers;
      break;
  }
}

void setupKeys(byte t, byte lvl, byte lvlMax) {  // t=typus (Welcher Knopf) l=lvl (Alter Wert)
  //unsigned long startZeit, aktZeit, diffZeit;
  TimeOut = 2000;
  byte taste;
  delay(100);
#if DEBUG
  Serial.print("SUB setupKeys : Relevanter Taster : ");
  Serial.print(taste);
  Serial.print(" Start-L : ");
  Serial.print(lvl);
  Serial.print(" Max : ");
  Serial.println(lvlMax);
#endif

  delay(100);
  Score[t] = lvl;
  setScore(t + 10);
  //taste = t+1; // 1 <= taste <= 4
  StartZeit = millis();
  AktZeit = millis();
  DiffZeit = AktZeit - StartZeit;
  while (DiffZeit < TimeOut) {  // Nach 2 Sekunden Inaktivität wird die Einstellung beendet
    digitalWrite(LED, HIGH);
    if (digitalRead(ButtonPin[t]) == LOW) {  //  Taste gedrückt
      digitalWrite(LED, LOW);
      delay(100);
      lvl++;
      // Grenzen abhängig von der Taste / Zweck einhalten
      if (lvl > lvlMax) { lvl = 1; }
      // Unabhängig von Grenzen max 5 LEDs
      if (lvl > 5) { lvl = 1; }
      Score[t] = lvl;
      setScore(t + 10);
      if (t == 2) {
        FastLED.setBrightness(lvl * 20);
        FastLED.show();
      }  // Brightness DIREKT anzeigen
      delay(100);
      StartZeit = millis();
    }
    delay(100);
    AktZeit = millis();
    DiffZeit = AktZeit - StartZeit;
  }
#if DEBUG
  Serial.print("SUB setupKeys - Taste : ");
  Serial.print(taste);
  Serial.print(" Aktueller L : ");
  Serial.println(lvl);
#endif
  delay(500);
  KeyValue = lvl;
  // Einstellung über TimeOut beendet
  // *** Rückkehr aus SUB
}

void setupUniversal(byte typus) {
#if DEBUG
  Serial.print("Sub : SetupUniversal Typus : ");
  Serial.println(typus);
#endif
  byte lvl;
  byte player;
  // Alle Score-LEDs löschen
  clearScoreLeds(4, 0);
  switch (typus) {
    case 1:  // GameMode
      // Aktuellen GameMode holen
      lvl = GameMode;
      player = typus - 1;
#if DEBUG
      Serial.print("SUB setupUniversal : Relevanter Taster : ");
      Serial.print(typus - 1);
      Serial.print(" Start-L : ");
      Serial.println(lvl);
#endif

      delay(100);
      // SUB aufrufen für Tastenhandling
      setupKeys(player, lvl, GameModeMax);
      // Tastenhandling zuende
      clearScoreLeds(4, 0);
      // Tastenhandling auswerten
      GameMode = KeyValue;
      if (lvl != KeyValue) { SavingNeeded = 1; }
      delay(500);
      setScore(player + 10);
      delay(500);
      ConfigState = 0;
      break;
    case 2:  // GameLevel
      // Aktuellen Level holen
      lvl = GameLevel[GameMode];
      player = typus - 1;
#if DEBUG
      Serial.print("SUB setupUniversal : Relevanter Taster : ");
      Serial.print(typus - 1);
      Serial.print(" Start-L : ");
      Serial.println(lvl);
#endif

      // SUB aufrufen für Tastenhandling
      setupKeys(player, lvl, GameLevelMax[GameMode]);
      // Tastenhandling zuende
      clearScoreLeds(4, 0);
      // Tastenhandling auswerten
      GameLevel[GameMode] = KeyValue;
      if (lvl != KeyValue) { SavingNeeded = 1; }
      delay(500);
      setScore(player + 10);
      delay(500);
      ConfigState = 0;
      break;
    case 3:  // Brightness
      // Aktuellen Level holen
      lvl = GameBright[GameMode];
      player = typus - 1;
#if DEBUG
      Serial.print("SUB setupUniversal : Relevanter Taster : ");
      Serial.print(typus - 1);
      Serial.print(" Start-L : ");
      Serial.println(lvl);
#endif

      // SUB aufrufen für Tastenhandling
      setupKeys(player, lvl, GameBrightMax[GameMode]);
      // Tastenhandling zuende
      clearScoreLeds(4, 0);
      // Tastenhandling auswerten
      if (lvl != KeyValue) { SavingNeeded = 1; }
      lvl = KeyValue;
      FastLED.setBrightness(lvl * 20);
      FastLED.show();
      GameBright[GameMode] = lvl;
      delay(500);
      setScore(player + 10);
      delay(500);
      ConfigState = 0;
      break;
    case 4:  // Starte Spiel
             // Daten in das EEPROM speciehrn, wenn sie geändert wurden
      if (SavingNeeded == 1) { writeEEPROM(); }
      GameState = 1;
      break;
  }
}

void initGame()  // GameState 1
{
#if DEBUG
  Serial.println("Sub : InitGame");
#endif
  fill_rainbow(ringLeds, RING_LEDS_NUM, 0, 7);    //2 = longer gradient strip
  fill_rainbow(scoreLeds, SCORE_LEDS_NUM, 0, 7);  //2 = longer gradient strip
  delay(1000);
  switch (GameMode) {
    case 1:  // ChaosRun
#if DEBUG
      Serial.println(" ChaosRun ");
#endif
      memset(PlayerBtns, 0, sizeof(PlayerBtns));
      memset(PlayerActive, 0, sizeof(PlayerActive));
      // Scores zurücksetzen
      memset(Score, 0, sizeof(PlayerActive));
      // Erste Runde vorbereiten
      RunnerPosAct = 0;
      TargetPosAct = 16;
      // Auf Startbedingung (alle selektierten Knöpfe) warten
      // ****************************************************
      // Ring- und Score-LEDs löschen
      clearRingLeds(1);      // Den Ring nacheinander löschen
      clearScoreLeds(4, 1);  // Alle Scores nacheinander löschen
      DetectPlayers();
      //DetectSettings();
      delay(1000);
      break;
    case 2:  // Reaktion
      Serial.println(" Reaction ");
      // Spieler ROT und GRÜN
      memset(PlayerBtns, 0, sizeof(PlayerBtns));
      memset(PlayerActive, 0, sizeof(PlayerActive));
      // Scores zurücksetzen
      memset(Score, 0, sizeof(PlayerActive));
      // Auf Startbedingung (alle selektierten Knöpfe) warten
      // ****************************************************
      // Ring- und Score-LEDs löschen
      clearRingLeds(1);      // Den Ring nacheinander löschen
      clearScoreLeds(4, 1);  // Alle Scores nacheinander löschen
      //DetectPlayers();
      //DetectSettings();
      delay(1000);
      break;
  }
  GameState = 2;
}

void runGame()  // GameState 2
{
#if DEBUG
  Serial.print("Sub : RunGame - GameMode ");
  Serial.println(GameMode);
#endif
  GameRunning = true;
  // Ab hier Spiel unterscheiden
  switch (GameMode) {
    case 1:  // ChaosRun
#if DEBUG
      Serial.println(" ChaosRun ");
#endif

      runGameChaosRun();
      break;
    case 2:  // Reaktion

#if DEBUG
      Serial.println(" Reaction ");
#endif
      runGameReaction();
      break;
  }
  // Ab hier wieder alle Games zusammen
}

void runGameChaosRun()  // Game State 2
{
#if DEBUG
  Serial.println("GameState 2 -> runGame Spiel ChaosRun");
#endif
  long DiffZeit = (LevelSpeed[Level - 1]) ;  // byte -> integer (long)
  long randdir = 0;
  // Target zum ersten Mal setzen
  showTarget(0);
  copyLevel(ValueLevel);
  AktZeit = millis();
  // ===
  while (GameRunning) {
    DiffZeit = (LevelSpeed[Level - 1]) ;  // byte -> integer (long)
    if (millis() > (AktZeit + DiffZeit)) {
      AktZeit = millis();
#if DEBUG
      Serial.print("Level : ");
      Serial.print(Level);
      Serial.print(" DiffZeit : ");
      Serial.println(DiffZeit);
#endif

      // Target wiederholen
      showTarget(1);
      // Runner setzen
      showRunner(0);
    }
    // Tasteninterrupt freigeben
    if (Interrupt == false) enableInterrupts();
    // Wenn Tastendruck ...
    if (PlayerBtns[4] == true)  // es wurde eine Taste gedrückt
    {                           // Der Spieler, der gedrückt hat, steht in "PressedButton" (0-3)
      // *** Auswerten, ob der Spieler (PressedButton) drücken durfte, weil er in "PlayerActive[PressedButton]" eine "1" stehen hat ***
      // ******************************************************************************************************************************
      // Merker für Tastendruck löschen
      PlayerBtns[4] = false;
#if DEBUG
      Serial.print("Spieler ");
      Serial.print(PressedButton + 1);
      Serial.print(" hat gedrückt und ");
#endif

      if (PlayerActive[PressedButton] == 1) {
#if DEBUG
        Serial.println("wird gewertet");
        Serial.print("TargetPos : ");
        Serial.print(TargetPosAct);
        Serial.print(" RunnerPos : ");
        Serial.print(RunnerPosAct);
        Serial.print("RunnerDir : ");
        Serial.println(RunnerDirection);
#endif
        //   Ist Position WINNER oder LOSER ?
        //   CALL Winner() oder CALL Loser()
        if (RunnerDirection == 0) {
          if (TargetWidth == 3) {
            if (RunnerPosAct >= TargetPosAct - 1 && RunnerPosAct <= TargetPosAct + 1) {
              winner(PressedButton);
            } else {
              loser(PressedButton);
            }
          } else {
            if (RunnerPosAct == TargetPosAct || RunnerPosAct == TargetPosAct) {
              winner(PressedButton);
            } else {
              loser(PressedButton);
            }
          }
        } else {  // RunnerDirection==1
          if (TargetWidth == 3) {
            if (RunnerPosAct <= TargetPosAct + 1 && RunnerPosAct >= TargetPosAct - 1) {
              winner(PressedButton);
            } else {
              loser(PressedButton);
            }
          } else {
            if (RunnerPosAct == TargetPosAct || RunnerPosAct == TargetPosAct) {
              winner(PressedButton);
            } else {
              loser(PressedButton);
            }
          }
        }
        // Nächstes Level
        // Alle Player-Variablen löschen
      } else {
#if DEBUG
        Serial.println("wird nicht gewertet");
#endif
      }
      memset(PlayerBtns, 0, sizeof(PlayerBtns));
      //   Spielende erreicht ?
      //   NEIN -> in "runGame" bleiben
      //   JA   -> zu "endGame" wechseln
      if (Level > MaxLevel) {
        GameRunning = false;
        GameState = 3;
      }
    }
  }
}

void runGameReaction()  // GameState 2
{
#if DEBUG
  Serial.println("GameState 2 -> runGame Spiel Reaction");
#endif
  byte oldPosR, actPosR, oldPosG, actPosG;
  byte lp;
  byte dirR, dirG;
  byte maxVal = 0;
  byte targetG1, targetG2, targetR1, targetR2, newTarget;
  byte x;
  boolean start = false;
  byte targetsR, targetsG;
  byte gameLevel = 0;
  byte gameCounter = 0;
  byte runDelayR, runDelayG;
  byte debounceR, debounceG;
  byte gamesToWin = GameLevel[2];
  boolean ContestRunning = true;
  // LED Pos 2 = Grün-Center
  // LED Pos 18 = Rot Center
  copyLevel(1);
  // Der Contest geht einstellbar bis 1 / 2 / 3 / 4 / 5 Punkte ("Best-Of")
  while (ContestRunning == true) {  //Start des Contest
    //                          0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32
    //byte UrRing[33]       = {15,15,12,15,15,15,15,15,15, 0, 0, 0,15,15,15,15,15,15,10,15,15,15,15,15,15, 0, 0, 0, 0,15,15,15,15};
    gameLevel++;
    gameCounter++;
    runDelayR = 0;
    runDelayG = 0;
    for (x = 0; x < 33; x++) { LEDRing[x] = UrRing[x]; }
    // An x=8=GRÜN
    for (x = 0; x < 9; x++) { ringLeds[x].setRGB(64, 64, 64); }
    actPosG = 2;
    for (x = 29; x < 33; x++) { ringLeds[x].setRGB(64, 64, 64); }
    ringLeds[actPosG].setRGB(0, 255, 0);
    LEDRing[actPosG] = 12;
    targetG1 = 1;
    oldPosG = 2;
    targetG2 = 3;
    LEDRing[targetG1] = 22;
    LEDRing[targetG2] = 22;
    ringLeds[targetG1].setRGB(0, 0, 32);
    ringLeds[targetG2].setRGB(0, 0, 32);
    targetsG = 1;
    // An x=24=ROT
    for (x = 12; x < 25; x++) { ringLeds[x].setRGB(64, 64, 64); }
    actPosR = 18;
    ringLeds[actPosR].setRGB(255, 0, 0);
    LEDRing[actPosR] = 10;
    targetR1 = 17;
    oldPosR = 24;
    targetR2 = 19;
    LEDRing[targetR1] = 20;
    LEDRing[targetR2] = 20;
    ringLeds[targetR1].setRGB(0, 0, 32);
    ringLeds[targetR2].setRGB(0, 0, 32);
    targetsR = 1;
    FastLED.show();
#if DEBUG
    Serial.println("Vorbereitung fertig");
#endif

    // Vorbereitung fertig
    // ROT und GRÜN gleichzeitig drücken
    while (((digitalRead(Button1Pin) == LOW) && (digitalRead(Button3Pin) == LOW)) == false) { delay(100); }
    delay(500);
    // CountDown einfügen ?
    // oder Abfrage, dass beide tasten losgelassen sind
    while (((digitalRead(Button1Pin) == HIGH) && (digitalRead(Button3Pin) == HIGH)) == false) { delay(100); }
    delay(500);
    // Zum Start identische Richtung
    dirR = 0;
    dirG = 0;
    // StartLevel setzen
    Level = Level + 2;
    // Vorbereituzng fertig
    // let the Runners run ...
#if DEBUG
    Serial.print("Game-Level : ");
    Serial.println(gameLevel);
#endif

    GameRunning = true;
    while (GameRunning) {                          // Start des Games
      DiffZeit = (LevelSpeed[gameLevel - 1]) * 2;  // byte * 2 = int / long
#if DEBUG
//Serial.print("Period : ");
//Serial.println(period);
#endif

      if (millis() > (AktZeit + DiffZeit)) {
#if DEBUG
        // Serial.println("Spiel läuft : Neue Position aufbauen");
#endif
        AktZeit = millis();
        if ((runDelayG == 0)) {  // Alte GRÜN-Position löschen
          if (LEDRing[oldPosG] != 0) {
            if (LEDRing[oldPosG] == 10) {
              ringLeds[oldPosG].setRGB(255, 0, 0);
            } else if (LEDRing[oldPosG] == 12) {
              ringLeds[oldPosG].setRGB(0, 255, 0);
            } else if (LEDRing[oldPosG] == 15) {
              ringLeds[oldPosG].setRGB(64, 64, 64);
            } else if (LEDRing[oldPosG] == 20) {
              ringLeds[oldPosG].setRGB(0, 0, 32);
            } else if (LEDRing[oldPosG] == 22) {
              ringLeds[oldPosG].setRGB(0, 0, 32);
            }
          } else {
            ringLeds[oldPosG].setRGB(0, 0, 0);
          }
          // Neue GRÜN-Position ermitteln und setzen
#if DEBUG
          // Serial.print("actPosG = "); Serial.println(actPosG);
#endif
          if (dirG == 0) {
            actPosG++;
            if ((actPosG > 8) && (actPosG < 28)) {
              dirG = 1;
              actPosG = 7;
            } else if (actPosG > 32) {
              dirG = 0;
              actPosG = 0;
            }
          } else {
            if (actPosG > 0) {
              actPosG--;
              if ((actPosG < 29) && (actPosG > 10)) {
                dirG = 0;
                actPosG = 30;
              }
            } else {
              dirG = 1;
              actPosG = 32;
            }
          }
          ringLeds[actPosG].setRGB(0, 32, 0);
          oldPosG = actPosG;
        } else {
          if ((runDelayG == 5) || (runDelayG == 3) || (runDelayG == 1)) {
            ringLeds[actPosG].setRGB(0, 0, 0);
          } else {
            ringLeds[actPosG].setRGB(0, 255, 0);
          }
#if DEBUG
          Serial.print("Pause für Grün : ");
          Serial.println(runDelayG);
#endif
          if (runDelayG > 0) runDelayG--;
        }
        // --------------------------------------
        if ((runDelayR == 0)) {  // Alte ROT-Position löschen
          if (LEDRing[oldPosR] != 0) {
            if (LEDRing[oldPosR] == 10) {
              ringLeds[oldPosR].setRGB(255, 0, 0);
            } else if (LEDRing[oldPosR] == 12) {
              ringLeds[oldPosR].setRGB(0, 255, 0);
            } else if (LEDRing[oldPosR] == 15) {
              ringLeds[oldPosR].setRGB(64, 64, 64);
            } else if (LEDRing[oldPosR] == 20) {
              ringLeds[oldPosR].setRGB(0, 0, 32);
            } else if (LEDRing[oldPosR] == 22) {
              ringLeds[oldPosR].setRGB(0, 0, 32);
            }
          } else {
            ringLeds[oldPosR].setRGB(0, 0, 0);
          }
          // Neue ROT-Position ermitteln und setzen
          if (dirR == 0) {
            actPosR++;
            if (actPosR > 24) {
              dirR = 1;
              actPosR = 23;
            }
          } else {
            actPosR--;
            if (actPosR < 12) {
              dirR = 0;
              actPosR = 13;
            }
          }
          ringLeds[actPosR].setRGB(32, 0, 0);
          oldPosR = actPosR;
          if (runDelayR > 0) runDelayR--;
        } else {
          if ((runDelayR == 5) || (runDelayR == 3) || (runDelayR == 1)) {
            ringLeds[actPosR].setRGB(0, 0, 0);
          } else {
            ringLeds[actPosR].setRGB(255, 0, 0);
          }

#if DEBUG
          Serial.print("Pause für Rot : ");
          Serial.println(runDelayR);
#endif
          if (runDelayR > 0) runDelayR--;
        }
        FastLED.show();
        debounceR = 0;
        debounceG = 0;
      } else  // Tasten-Handling
      {

#if DEBUG
        // Serial.println("Spiel läuft : Tasten abfragen");
#endif
        btn1Bounce.update();
        if (btn1Bounce.fell()) {  // ROT : Beide Spieler gleichberechtigt abfragen
#if DEBUG
          Serial.print("ROT hat gedrückt an Position ");
          Serial.print(actPosR);
          Serial.print(" LED: ");
          Serial.print(LEDRing[actPosR]);
#endif
          if (LEDRing[actPosR] == 20) {  // Target getroffen

#if DEBUG
            Serial.println(" Winner !");
#endif
            LEDRing[actPosR] = 10;  // Target erobert -> ROT
            if (actPosR > 18) {
              newTarget = actPosR + 1;
              if (dirR == 0) actPosR++;
            } else {
              newTarget = actPosR - 1;
              if (dirR == 1) actPosR--;
            }
            if (LEDRing[newTarget] != 0) {
              LEDRing[newTarget] = 20;
              ringLeds[newTarget].setRGB(0, 0, 32);
            }
            targetsR++;
            //runDelayR = 1;
          } else {  // Target verfehlt
#if DEBUG
            Serial.println(" Loser!");
#endif
            runDelayR = 5;  // Der rote Runner wird fünf Zyklen lang nicht bewegt, wegen "Loser"
          }
          PlayerBtns[0] = false;
          debounceR = 1;
        }
        btn3Bounce.update();
        if (btn3Bounce.fell()) {  // GRÜN : Beide Spieler gleichberechtigt abfragen

#if DEBUG
          Serial.print("GRÜN hat gedrückt an Position ");
          Serial.print(actPosG);
          Serial.print(" LED: ");
          Serial.print(LEDRing[actPosG]);
#endif
          if (LEDRing[actPosG] == 22) {  // Target getroffen
#if DEBUG
            Serial.println(" Winner !");
#endif
            LEDRing[actPosG] = 12;  // Target erobert -> GRÜN
            if ((actPosG > 2) && (actPosG < 28)) {
              newTarget = actPosG + 1;
              if (dirG == 0) actPosG++;
            }  // Target PLUS
            else if ((actPosG < 2) && (actPosG > 0)) {
              newTarget = actPosG - 1;
              if (dirG == 1) actPosG--;
            }  // Target MINUS
            else if ((actPosG >= 28) && (actPosG <= 32)) {
              newTarget = actPosG - 1;
              if (dirG == 1) actPosG--;
            }  // Target MINUS
            else if (actPosG == 0) {
              newTarget = 32;
              if (dirG == 1) actPosG = 31;
            }  // Target SET

#if DEBUG
            Serial.print("New Target = ");
            Serial.println(newTarget);
#endif

            if (LEDRing[newTarget] != 0) {
              LEDRing[newTarget] = 22;
              ringLeds[newTarget].setRGB(0, 0, 32);
            }
            targetsG++;
            //runDelayG = 1; // Der grüne Runner wird einen Zyklus lang nicht bewegt, wegen "Doppelschlag"
          } else {  // Target verfehlt

#if DEBUG
            Serial.println(" Loser!");
#endif
            runDelayG = 5;  // Der grüne Runner wird fünf Zyklen lang nicht bewegt, wegen "Loser"
          }
          PlayerBtns[2] = false;
          debounceG = 1;
        }
      }  // Ende Tastenauswertung
      PlayerBtns[4] = false;
      PlayerBtns[2] = false;
      PlayerBtns[0] = false;
      if (targetsR == 13) { winner(0); }
      if (targetsG == 13) { winner(2); }
      if ((targetsR == 13) || (targetsG == 13)) {
        GameRunning = false;
        memset(LEDRing, 0, sizeof(LEDRing));
        targetsR = 1;
        targetsG = 1;
      }
      //} // end of Tasten-Handling
    }  // end of GameRunning
    if ((Score[0] == gamesToWin) || (Score[2] == gamesToWin)) {
      ContestRunning = false;
      GameState = 3;
    }
  }  // end-of ContestRunning
}  // end-of SUB

void pollReactionButtons() {
  // poll Buttons for "reaction"
  if (digitalRead(Button1Pin) == LOW) {
    setPlayerButtonsState(0, true);
    setPlayerButtonsState(4, true);
  }
  if (digitalRead(Button3Pin) == LOW) {
    setPlayerButtonsState(2, true);
    setPlayerButtonsState(4, true);
  }
}

void endGame()  // GameState 3
{               // Siegerehrung
#if DEBUG
  Serial.println("Sub : EndGame");
#endif
  fill_rainbow(ringLeds, RING_LEDS_NUM, 0, 7);  //2 = longer gradient strip
  FastLED.show();
  // **********************************************
  // Sieger ist der Spieler mit den meisten Punkten
  // **********************************************
  byte maxVal = 0;
  byte maxIndex = 0;
  byte maxVal2 = 0;
  byte maxIndex2 = 0;
  for (int i = 0; i < sizeof(Score); i++) {
    Serial.print(" #");
    Serial.print(i);
    Serial.print(" : ");
    Serial.print(Score[i]);
    delay(50);
    if (Score[i] > maxVal) {
      maxVal = Score[i];
      maxIndex = i;
      maxVal2 = 0;
      maxIndex2 = 0;
    } else if (Score[i] == maxVal) {
      maxVal2 = Score[i];
      maxIndex2 = i;
    }
  }

#if DEBUG
  //Serial.println(" # ");
  //Serial.print(F("max Value:")); Serial.print(maxVal); Serial.print(F(" at Index:")); Serial.print(maxIndex); Serial.println();
  //Serial.print(F("max Value2:")); Serial.print(maxVal2); Serial.print(F(" at Index2:")); Serial.print(maxIndex2); Serial.println();
  // Der Gewinnder-Index steht in "maxIndex"
  // Dessen Score-Anzeige 5-mal flashen
#endif
  byte maxflash = 5;

  byte start = 0;
  byte anzahl = 5;
  byte level = 0;
  for (byte flash = 0; flash < maxflash; flash++) {
    clearScoreLeds(maxIndex, 0);
    if (maxIndex2 > 0) { clearScoreLeds(maxIndex2, 0); }
    delay(500);
    setScore(maxIndex);
    if (maxIndex2 > 0) { setScore(maxIndex2); }
    delay(500);
    clearScoreLeds(maxIndex, 0);
    if (maxIndex2 > 0) { clearScoreLeds(maxIndex2, 0); }
    delay(500);
    start = Score1stLED[maxIndex];
    level = maxIndex + 10;
    for (byte i = 0; i < anzahl; i++) { scoreLeds[start + i].setRGB(R[level], G[level], B[level]); }
    FastLED.show();
    start = Score1stLED[maxIndex2];
    level = maxIndex2 + 10;
    if (maxIndex2 > 0) {
      for (byte i = 0; i < anzahl; i++) { scoreLeds[start + i].setRGB(R[level], G[level], B[level]); }
    }
    FastLED.show();
    delay(500);
  }
  // Neues Spiel, neues Glück ...
  GameState = 0;
  ConfigState = 0;
}

void DetectSettings() {  // Hier wird auf Tastendrücke gewartet, die die Einstellungen (Settings) beeinflussen
  // Nach Zeit x ohne Tastendruck werden Defaultwerte geladen und das Spiel wird gestartet
  // Grundanzeige zum Ermitteln des ConfigState
  // 5 x Rot  -> Anzeige für Einstellbarkeit GameMode
  // 5 x Gelb -> Anzeige für Einstellbarkeit GameLevel
  // 5 x Grün ->
  // 5 x Blau -> Anzeige für Einstellbarkeit GamePlayer
  //unsigned long StartZeit, AktZeit;
  // Zuerst alle Score-LEDs löschen

#if DEBUG
  Serial.println("DetectSettings");
#endif
  byte buttonPressed = 0;
  for (byte i = 0; i < SCORE_LEDS_NUM; i++) {
    scoreLeds[i].setRGB(0, 0, 0);
    delay(50);
    FastLED.show();
  }
  // Alle LEDs pro Taste leuchten lassen
  for (byte i = 0; i < 5; i++) {
    scoreLeds[i].setRGB(255, 0, 0);
    delay(50);
  }
  for (byte i = 5; i < 10; i++) {
    scoreLeds[i].setRGB(255, 255, 0);
    delay(50);
  }
  for (byte i = 10; i < 15; i++) {
    scoreLeds[i].setRGB(0, 255, 0);
    delay(50);
  }
  for (byte i = 15; i < 20; i++) {
    scoreLeds[i].setRGB(0, 0, 255);
    delay(50);
  }
  FastLED.show();
  // Auf einen ersten Tastendruck warten
  while ((digitalRead(Button1Pin) == HIGH) && (digitalRead(Button2Pin) == HIGH) && (digitalRead(Button3Pin) == HIGH) && (digitalRead(Button4Pin) == HIGH)) {
    idleAnimation(FadeInterval);
    delay(50);
  }
  // Startzeit merken für zeitlichen TimeOut
  StartZeit = millis();
  AktZeit = millis();

  while (AktZeit - StartZeit < 2000) {
    StartZeit = millis();

#if DEBUG
    //Serial.println("WHILE Zeit ...()");
#endif
    idleAnimation(FadeInterval);

    if (digitalRead(Button1Pin) == LOW) {  // Rot : Taster für GameMode
      AktZeit = millis();
      ConfigState = 1;
      buttonPressed = 1;
    } else if (digitalRead(Button2Pin) == LOW) {  // Gelb : Taster für GameLevel
      AktZeit = millis();
      ConfigState = 2;
      buttonPressed = 1;
    } else if (digitalRead(Button3Pin) == LOW) {  // Grün : Taster für ???
      AktZeit = millis();
      ConfigState = 3;
      buttonPressed = 1;
    } else if (digitalRead(Button4Pin) == LOW) {  // Blau : Taster für GamePlayer
      AktZeit = millis();
      ConfigState = 4;
      buttonPressed = 1;
    }
  }
  for (byte i = 0; i < SCORE_LEDS_NUM; i++) {
    scoreLeds[i].setRGB(0, 0, 0);
    delay(50);
    FastLED.show();
  }
}

void DetectPlayers() {  // Alte Programmierung zum Testen angepasst an neue Struktur und neue Variablen
  // Hier wird in einer Zeitspanne auf Tastendrücke gewartet
  // Die gedrückten Tasten sind anschließend aktiv für das Spiel
  // Das Spiel wird durch GLEICHZEITIGEN Tastendruck der Spieler gestartet
  // *********************************************************************
  //unsigned long StartZeit, AktZeit;
  // Zuerst alle Score-LEDs löschen
  for (byte i = 0; i < SCORE_LEDS_NUM; i++) {
    scoreLeds[i].setRGB(0, 0, 0);
    delay(50);
    FastLED.show();
  }
#if DEBUG
  Serial.println("Detecting Players startet");
#endif
  // Je eine LED pro Taste leuchten lassen
  scoreLeds[2].setRGB(255, 0, 0);
  delay(50);
  scoreLeds[7].setRGB(255, 255, 0);
  delay(50);
  scoreLeds[12].setRGB(0, 255, 0);
  delay(50);
  scoreLeds[17].setRGB(0, 0, 255);
  delay(50);
  FastLED.show();
  // Auf einen ersten Tastendruck warten
  AktZeit = millis();
  StartZeit = millis();
  while (((AktZeit - StartZeit) < 3000) || ((PlayerActive[0] == 0) && (PlayerActive[1] == 0) && (PlayerActive[2] == 0) && (PlayerActive[3] == 0))) {
    idleAnimation(FadeInterval);
    if (((digitalRead(Button1Pin) == LOW)) && (PlayerActive[0] == 0)) {
      PlayerActive[0] = 1;
      for (byte k = 1; k < 4; k++) {
        scoreLeds[k].setRGB(255, 0, 0);
        delay(50);
      }
      FastLED.show();
      StartZeit = millis();
    }
    if (((digitalRead(Button2Pin) == LOW)) && (PlayerActive[1] == 0)) {
      PlayerActive[1] = 1;
      for (byte k = 6; k < 9; k++) {
        scoreLeds[k].setRGB(255, 255, 0);
        delay(50);
      }
      FastLED.show();
      StartZeit = millis();
    }
    if (((digitalRead(Button3Pin) == LOW)) && (PlayerActive[2] == 0)) {
      PlayerActive[2] = 1;
      for (byte k = 11; k < 14; k++) {
        scoreLeds[k].setRGB(0, 255, 0);
        delay(50);
      }
      FastLED.show();
      StartZeit = millis();
    }
    if (((digitalRead(Button4Pin) == LOW)) && (PlayerActive[3] == 0)) {
      PlayerActive[3] = 1;
      for (byte k = 16; k < 19; k++) {
        scoreLeds[k].setRGB(0, 0, 255);
        delay(50);
      }
      FastLED.show();
      StartZeit = millis();
    }
    if ((PlayerActive[0] == 1) || (PlayerActive[1] == 1) || (PlayerActive[2] == 1) || (PlayerActive[3] == 1)) { AktZeit = millis(); }
  }
  // Scores löschen als Zeichen für "Detecting beendet"
  for (byte k = 0; k < 20; k++) { scoreLeds[k].setRGB(0, 0, 0); }
  FastLED.show();
  delay(1000);
  if (PlayerActive[0] == 1) {
    for (byte k = 0; k < 5; k++) {
      scoreLeds[k].setRGB(255, 0, 0);
      delay(50);
      FastLED.show();
    }
  }
  if (PlayerActive[1] == 1) {
    for (byte k = 5; k < 10; k++) {
      scoreLeds[k].setRGB(255, 255, 0);
      delay(50);
      FastLED.show();
    }
  }
  if (PlayerActive[2] == 1) {
    for (byte k = 10; k < 15; k++) {
      scoreLeds[k].setRGB(0, 255, 0);
      delay(50);
      FastLED.show();
    }
  }
  if (PlayerActive[3] == 1) {
    for (byte k = 15; k < 20; k++) {
      scoreLeds[k].setRGB(0, 0, 255);
      delay(50);
      FastLED.show();
    }
  }
#if DEBUG
  Serial.println("Detecting Players beendet");
#endif
  // ===
  fill_rainbow(ringLeds, RING_LEDS_NUM, 0, 7);    //2 = longer gradient strip
  fill_rainbow(scoreLeds, SCORE_LEDS_NUM, 0, 7);  //2 = longer gradient strip
  // Solange nicht alle selektierten Tasten gleichzeitig gedrückt wurden, bleibt der Regenbogen stehen
  while (((((PlayerActive[0] == 1) && (digitalRead(Button1Pin) == LOW)) || (PlayerActive[0] == 0)) && (((PlayerActive[1] == 1) && (digitalRead(Button2Pin) == LOW)) || (PlayerActive[1] == 0)) && (((PlayerActive[2] == 1) && (digitalRead(Button3Pin) == LOW)) || (PlayerActive[2] == 0)) && (((PlayerActive[3] == 1) && (digitalRead(Button4Pin) == LOW)) || (PlayerActive[3] == 0))) == false) { delay(50); }
  // Wenn eine Taste gedrückt wird, dann erlöschen nacheinander alle LEDs und das Spiel startet
  delay(50);
  for (byte i = 0; i < RING_LEDS_NUM; i++) {
    ringLeds[i].setRGB(0, 0, 0);
    delay(20);
    FastLED.show();
  }
  for (byte i = 0; i < SCORE_LEDS_NUM; i++) {
    scoreLeds[i].setRGB(0, 0, 0);
    delay(50);
    FastLED.show();
  }
  Level = 1;
  // "Alte Routine" Ende
  GameState = 1;  // Spielstart
}

void winner(byte player) {  // Punkte hinzufügen und neuen Score anzeigen
  byte Punkte;
  byte r, g, b;
  r = R[player + 10];
  g = G[player + 10];
  b = B[player + 10];
  Punkte = Score[player];
  Punkte++;
  Score[player] = Punkte;
  // "La Ola"
  for (byte i = 0; i < 3; i++) {
    for (byte j = 0; j < RING_LEDS_NUM; j++) {
      ringLeds[j].setRGB(r, g, b);
      delay(20);
      FastLED.show();
    }
    delay(100);
    clearRingLeds(0);
    delay(100);
  }
  // Ring-LEDs löschen
  for (byte j = 0; j < RING_LEDS_NUM; j++) {
    ringLeds[j].setRGB(0, 0, 0);
  }
  FastLED.show();
  delay(100);
  // Score-LEDs des Gewinners anpassen
  setScore(player);
  // Target neu Setzen
  Level++;
  if (GameMode == 1) {
    if (Level > MaxLevel) {
      delay(100);
    } else {
      showTarget(0);
    }
    delay(1000);
    // Verzögerung, damit alle das neue Target sehen können
  }
}

void loser(byte player) {  // Punkte abziehen und neuen Score anzeigen
  byte Punkte, Sieger, SiegA, SiegB;
  byte r, g, b;
  r = R[player + 10];
  g = G[player + 10];
  b = B[player + 10];
  Punkte = Score[player];
  if (Punkte > 2) {
    Punkte = Punkte - 2;
  } else {
    Punkte = 0;
  }
  Score[player] = Punkte;
  // TARGET und POSITION anzeigen
  for (byte i = 0; i < 10; i++) {
    ringLeds[TargetPosAct].setRGB(0, 255, 0);
    ringLeds[RunnerPosAct].setRGB(255, 0, 0);
    FastLED.show();
    delay(100);
    clearRingLeds(0);
    delay(100);
    for (byte j = 0; j < RING_LEDS_NUM; j++) {
      ringLeds[j].setRGB(r, g, b);
    }
    FastLED.show();
    delay(100);
  }
  // Ring-LEDs löschen
  for (byte j = 0; j < RING_LEDS_NUM; j++) {
    ringLeds[j].setRGB(0, 0, 0);
  }
  FastLED.show();
  delay(100);
  // Reduzierten SCORE anzeigen
  setScore(player);
  // Target neu Setzen
  Level++;
  if (GameMode == 1) {
    if (Level > MaxLevel) {
      delay(100);
    } else {
      showTarget(0);
    }
  }
  // Verzögerung, damit alle das neue Target sehen können
  delay(1000);
}

void setScore(byte player) {  // Routine, um die Scores jedes Spieler anhand der Punkte komplett zu setzen
  byte Punkte;
  byte Start, Level, Anzahl;
  if (player > 9) {  // Setzen des kompletten Scores in Spielerfarbe
    Start = Score1stLED[player - 10];
    Punkte = Score[player - 10];

#if DEBUG
    Serial.print("SetScore Spieler ");
    Serial.print(player);
    Serial.print(" Punkte ");
    Serial.println(Punkte);
#endif
    delay(10);
    // "Alte Punkte" löschen
    for (byte i = 0; i < 5; i++) { scoreLeds[Start + i].setRGB(0, 0, 0); }
    FastLED.show();
    delay(10);
    for (byte i = 0; i < Punkte; i++) { scoreLeds[Start + i].setRGB(R[player], G[player], B[player]); }
    FastLED.show();
  } else {  // "Normale" Punkteanzeige
    Punkte = Score[player];
    Start = Score1stLED[player];
    Level = abs((Punkte - 1) / 5);
    Anzahl = ((Punkte - 1) % 5) + 1;
    // "Alte Punkte" löschen
    for (byte i = 0; i < 5; i++) { scoreLeds[Start + i].setRGB(0, 0, 0); }
    FastLED.show();
    delay(10);
    // "Neue Punkte" setzen
    if (Punkte > 0) {
      for (byte i = 0; i < Anzahl; i++) { scoreLeds[Start + i].setRGB(R[Level], G[Level], B[Level]); }
      FastLED.show();
    }
    delay(10);
    GameState = 0;
  }
}

void pollPlayerButtons() {
  // reset last poll
  memset(PlayerBtns, 0, sizeof(PlayerBtns));
  // poll Buttons
  if (digitalRead(Button1Pin) == LOW) {
    setPlayerButtonsState(0, true);
    setPlayerButtonsState(4, true);
    PressedButton = 1;
  } else setPlayerButtonsState(0, false);
  if (digitalRead(Button2Pin) == LOW) {
    setPlayerButtonsState(1, true);
    setPlayerButtonsState(4, true);
    PressedButton = 2;
  } else setPlayerButtonsState(1, false);
  if (digitalRead(Button3Pin) == LOW) {
    setPlayerButtonsState(2, true);
    setPlayerButtonsState(4, true);
    PressedButton = 3;
  } else setPlayerButtonsState(2, false);
  if (digitalRead(Button4Pin) == LOW) {
    setPlayerButtonsState(3, true);
    setPlayerButtonsState(4, true);
    PressedButton = 4;
  } else setPlayerButtonsState(3, false);
  // print Debug
  //	Serial.print(F("playerBtns:"));
  //	printByteArray(playerBtns, sizeof(playerBtns));
  //	Serial.println();
}

void setPlayerButtonsState(byte index, boolean value) {
  //BtnMillis[index] = millis();
  PlayerBtns[index] = value;
}

void idleAnimation(unsigned int interval) {
  if (millis() - AktZeit >= interval) {
    fill_rainbow(ringLeds, RING_LEDS_NUM, CurrentPos * 2, RING_LEDS_NUM / 2);
    FastLED.show();
    CurrentPos++;
    if (CurrentPos == RING_LEDS_NUM * 4) {
      CurrentPos = 0;
    }
    AktZeit = millis();
  }
}

bool isAnyButtonPressed() {
  for (byte i = 0; i < sizeof(PlayerBtns); i++) {
    if (PlayerBtns[i] == 1) {
      return true;
    }
  }
  return false;
}

void setGameModeLeds() {
  switch (GameMode) {
    case 0:
      fill_solid(ringLeds, RING_LEDS_NUM, CRGB::Blue);
      FastLED.show();
      break;
    case 1:
      fill_solid(ringLeds, RING_LEDS_NUM, CRGB::Green);
      FastLED.show();
      break;
  }
}

void showTarget(byte mode) {
  byte ledDiff;
  switch (mode) {
    case 0:  // Neue Position ermitteln und anzeigen
      // Alte Position sichern
      TargetPosOld = TargetPosAct;
      // Neue Position und neue Laufrichtung suchen
      if (Level < SwitchTargetPrecisionAtLevel) {
        TargetWidth = 3;
        ledDiff = 7;
      } else {
        TargetWidth = 1;
        ledDiff = 5;
      }
      // Target soll nicht näher als 5 LEDs an RunnerPosAct gebaut werden
      TargetPosAct = random(2, RING_LEDS_NUM - 2);
      while ((TargetPosAct - RunnerPosAct < ledDiff) && (RunnerPosAct - TargetPosAct < ledDiff)) { TargetPosAct = random(2, RING_LEDS_NUM - 2); }
      RunnerDirection = random(0, 2);

#if DEBUG
      Serial.print("Neues Target ! TargetPos : ");
      Serial.print(TargetPosAct);
      Serial.print(" RunnerPos : ");
      Serial.print(RunnerPosAct);
      Serial.print("  RunnerDir : ");
      Serial.println(RunnerDirection);
#endif

      // Target anzeigen
      if (TargetWidth == 3) {
        ringLeds[TargetPosAct - 1].setRGB(255, 140, 0);
        ringLeds[TargetPosAct].setRGB(0, 255, 0);
        ringLeds[TargetPosAct + 1].setRGB(255, 140, 0);
      } else {
        ringLeds[TargetPosAct].setRGB(0, 255, 0);
      }
      break;
    case 1:  // An alter Postion erneut anzeigen
      if (TargetWidth == 3) {
        ringLeds[TargetPosAct - 1].setRGB(255, 140, 0);
        ringLeds[TargetPosAct].setRGB(0, 255, 0);
        ringLeds[TargetPosAct + 1].setRGB(255, 140, 0);
      } else {
        ringLeds[TargetPosAct].setRGB(0, 255, 0);
      }
      break;
  }
  FastLED.show();
}

void showRunner(byte mode) {
  RunnerPosOld = RunnerPosAct;
  switch (mode) {
    case 0:
      // Runner an alter Position löschen
      ringLeds[RunnerPosOld].setRGB(0, 0, 0);
      // Runner an neuer Position setzen
      switch (RunnerDirection) {
        case 0:  // Runner läuft aufwärts
          RunnerPosAct++;
          if (RunnerPosAct >= RING_LEDS_NUM) {
            RunnerPosAct = 0;
          }
          break;
        case 1:  // Runner läuft abwärts
          if (RunnerPosOld == 0) {
            RunnerPosAct = RING_LEDS_NUM - 1;
          } else {
            RunnerPosAct--;
          }
          break;
      }
  }
  ringLeds[RunnerPosAct].setRGB(255, 0, 0);

#if DEBUG
//Serial.print(" # RunnerPosAct = ");Serial.println(RunnerPosAct);
#endif
  FastLED.show();
}

void clearScoreLeds(byte player, byte modus) {  // Score-LEDs löschen in unterschiedlichen Modi

#if DEBUG
  Serial.print("ClearScore bei Spieler ");
  Serial.println(player);
#endif
  delay(100);
  byte firstLED;
  byte range;
  switch (player) {
    case 0:
      // Score von Player 1 nacheinander ausschalten
      firstLED = Score1stLED[0];
      range = 5;
      break;
    case 1:
      // Score von Player 2 nacheinander ausschalten
      firstLED = Score1stLED[1];
      range = 5;
      break;
    case 2:
      // Score von Player 3 nacheinander ausschalten
      firstLED = Score1stLED[2];
      range = 5;
      break;
    case 3:
      // Score von Player 4 nacheinander ausschalten
      firstLED = Score1stLED[3];
      range = 5;
      break;
    case 4:
      // Alle Score-LEDs nacheinander ausschalten
      firstLED = 0;
      range = 20;
      break;
  }

#if DEBUG
  Serial.print("Lösche Score von ");
  Serial.print(firstLED);
  Serial.print(" bis ");
  Serial.println(firstLED + range);
#endif
  delay(100);
  if (modus == 0) {  // Schlagartig löschen
    for (byte i = firstLED; i < (firstLED + range); i++) {
      scoreLeds[i].setRGB(0, 0, 0);
    }
    FastLED.show();
  } else if (modus == 1) {  // Nacheinander löschen
    for (byte i = firstLED; i < (firstLED + range); i++) {
      scoreLeds[i].setRGB(0, 0, 0);
      delay(50);
      FastLED.show();
    }
  }
}

void clearRingLeds(byte mode) {  // Ring-LEDs löschen in unterschiedlichen Modi
  byte firstLED = 0;
  byte range = RING_LEDS_NUM;
  switch (mode) {
    case 0:
      // Alle Ring-LED auf einmal löschen
      for (byte i = firstLED; i < range; i++) {  // Alle LED "im dunklen" löschen
        ringLeds[i].setRGB(0, 0, 0);
      }
      FastLED.show();
      break;
    case 1:
      // Alle Ring-LED nacheinander löschen
      for (byte i = firstLED; i < range; i++) {  // Jede LED einzeln löschen
        ringLeds[i].setRGB(0, 0, 0);
        FastLED.show();
        delay(50);
      }
      break;
  }
}

/*
void printByteArray(byte arr[], byte siz)
{
	for(byte i = 0; i < siz; i++)
	{
		
		#if DEBUG
			Serial.print(arr[i]);
		#endif 
		if(i < siz-1)
		{
			
			#if DEBUG
				Serial.print(F(","));
			#endif 
		}
	}
	
	#if DEBUG
		Serial.println();
	#endif 


}
*/

// ****************************************
// * Interrupts - absolut nichts ändern ! *
// ****************************************

void disableInterrupts() {
  disablePCINT(digitalPinToPCINT(Button1Pin));
  disablePCINT(digitalPinToPCINT(Button2Pin));
  disablePCINT(digitalPinToPCINT(Button3Pin));
  disablePCINT(digitalPinToPCINT(Button4Pin));

#if DEBUG
//Serial.println("=Disable Interrupts=");
#endif
}

void enableInterrupts() {
  enablePCINT(digitalPinToPCINT(Button1Pin));
  enablePCINT(digitalPinToPCINT(Button2Pin));
  enablePCINT(digitalPinToPCINT(Button3Pin));
  enablePCINT(digitalPinToPCINT(Button4Pin));
#if DEBUG
//Serial.println("=Enable Interrupts=");
#endif
}

void ISRButton1() {
  // Wenn Button 1 (Rot) gedrückt wird ...
  disableInterrupts();
  setPlayerButtonsState(0, true);
  PressedButton = 0;
  setPlayerButtonsState(4, true);
  // Ende ISR
}

void ISRButton2() {
  // Wenn Button 2 (Gelb) gedrückt wird ...
  if (GameMode == 1) { disableInterrupts(); }
  setPlayerButtonsState(1, true);
  PressedButton = 1;
  setPlayerButtonsState(4, true);
  // Ende ISR
}

void ISRButton3() {
  // Wenn Button 3 (Grün) gedrückt wird ...
  if (GameMode == 1) { disableInterrupts(); }
  setPlayerButtonsState(2, true);
  PressedButton = 2;
  setPlayerButtonsState(4, true);
  // Ende ISR
}

void ISRButton4() {
  // Wenn Button 4 (Blau) gedrückt wird ...
  if (GameMode == 1) { disableInterrupts(); }
  setPlayerButtonsState(3, true);
  PressedButton = 3;
  setPlayerButtonsState(4, true);
  // Ende ISR
}
