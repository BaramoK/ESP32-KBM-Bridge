// Bibliothèques pour la gestion USB Host
#include <usbhub.h>
#include "USBHost_t36.h"  // Version adaptée pour ESP32-S3

// Bibliothèques pour le mode périphérique USB
#include <Adafruit_TinyUSB.h>
#include <Adafruit_USBD_HID.h>

// Bibliothèques pour BLE HID
#include <BleKeyboard.h>
#include <BleMouse.h>

// Bibliothèque pour la gestion des tâches
#include <TaskScheduler.h>

// Définition des broches et constantes
#define USB_MODE_LED 2   // LED pour indiquer le mode USB
#define BLE_MODE_LED 3   // LED pour indiquer le mode Bluetooth

// Configuration de la combinaison de touches pour basculer
#define MOD_KEY KEY_LEFT_ALT
#define TOGGLE_KEY KEY_ESC

// Constantes pour les reports HID
#define HID_KEYBOARD_REPORT_ID 1
#define HID_MOUSE_REPORT_ID 2

// Variables globales
enum OperationMode { MODE_USB, MODE_BLE };
OperationMode currentMode = MODE_USB;
uint8_t modifierKeys = 0;
bool toggleKeyPressed = false;

// Objets pour les modes USB et Bluetooth
USBHost myusb;
USBHub hub1(myusb);
KeyboardController keyboard1(myusb);
MouseController mouse1(myusb);

// Configuration du périphérique USB
Adafruit_USBD_HID usb_hid;

// Configuration du BLE HID
BleKeyboard bleKeyboard("ESP32-S3 Keyboard", "Espressif", 100);
BleMouse bleMouse("ESP32-S3 Mouse", "Espressif", 100);

// Gestionnaire de tâches
Scheduler ts;

// Prototypes des fonctions
void updateLEDs();
void toggleMode();
void onKeyPress(int key);
void onKeyRelease(int key);
void onMouseMove(uint8_t x, uint8_t y);
void onMouseButtons(uint8_t buttons);

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32-S3 USB/BLE HID Gateway");
  
  // Configuration des LEDs
  pinMode(USB_MODE_LED, OUTPUT);
  pinMode(BLE_MODE_LED, OUTPUT);
  updateLEDs();
  
  // Initialisation de l'USB Host
  myusb.begin();
  keyboard1.attachPress(onKeyPress);
  keyboard1.attachRelease(onKeyRelease);
  mouse1.attachMove(onMouseMove);
  mouse1.attachButtons(onMouseButtons);
  
  // Initialisation du périphérique USB HID
  usb_hid.setPollInterval(2);
  usb_hid.setReportDescriptor(
    HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(HID_KEYBOARD_REPORT_ID)) | 
    HID_REPORT_DESC_MOUSE(HID_REPORT_ID(HID_MOUSE_REPORT_ID)));
  usb_hid.begin();
  
  // Initialisation du BLE HID
  bleKeyboard.begin();
  bleMouse.begin();
  
  Serial.println("USB et BLE HID initialisés");
}

void loop() {
  // Traiter les événements USB
  myusb.Task();
  
  // Exécuter les tâches planifiées
  ts.execute();
}

// Met à jour les LEDs en fonction du mode actif
void updateLEDs() {
  digitalWrite(USB_MODE_LED, currentMode == MODE_USB ? HIGH : LOW);
  digitalWrite(BLE_MODE_LED, currentMode == MODE_BLE ? HIGH : LOW);
}

// Change le mode et met à jour les LEDs
void toggleMode() {
  currentMode = (currentMode == MODE_USB) ? MODE_BLE : MODE_USB;
  updateLEDs();
  Serial.print("Mode changé: ");
  Serial.println(currentMode == MODE_USB ? "USB" : "Bluetooth");
}

// Gestion des événements clavier
void onKeyPress(int key) {
  // Détection de la combinaison pour basculer de mode
  if (key == MOD_KEY) {
    modifierKeys |= (1 << 0); // Utilise un bitflag pour le modifier
  } else if (key == TOGGLE_KEY && (modifierKeys & (1 << 0))) {
    toggleKeyPressed = true;
    toggleMode();
    return; // Ne pas transmettre cette combinaison
  }
  
  // Redirection des touches vers USB ou BLE
  if (currentMode == MODE_USB) {
    uint8_t keycode[6] = {0};
    keycode[0] = key;
    usb_hid.keyboardReport(HID_KEYBOARD_REPORT_ID, modifierKeys, keycode);
  } else {
    bleKeyboard.press(key);
  }
}

void onKeyRelease(int key) {
  if (key == MOD_KEY) {
    modifierKeys &= ~(1 << 0); // Enlève le bit du modifier
  } else if (key == TOGGLE_KEY) {
    toggleKeyPressed = false;
  }
  
  // Redirection des touches vers USB ou BLE
  if (currentMode == MODE_USB) {
    uint8_t keycode[6] = {0}; // Libère toutes les touches
    usb_hid.keyboardReport(HID_KEYBOARD_REPORT_ID, modifierKeys, keycode);
  } else {
    bleKeyboard.release(key);
  }
}

// Gestion des événements souris
void onMouseMove(uint8_t x, uint8_t y) {
  if (currentMode == MODE_USB) {
    usb_hid.mouseReport(HID_MOUSE_REPORT_ID, 0, x, y, 0);
  } else {
    bleMouse.move(x, y);
  }
}

void onMouseButtons(uint8_t buttons) {
  if (currentMode == MODE_USB) {
    usb_hid.mouseReport(HID_MOUSE_REPORT_ID, buttons, 0, 0, 0);
  } else {
    // Utilisation d'un masque de bits pour traiter les boutons de souris de manière plus efficace
    (buttons & 1) ? bleMouse.press(MOUSE_LEFT) : bleMouse.release(MOUSE_LEFT);
    (buttons & 2) ? bleMouse.press(MOUSE_RIGHT) : bleMouse.release(MOUSE_RIGHT);
    (buttons & 4) ? bleMouse.press(MOUSE_MIDDLE) : bleMouse.release(MOUSE_MIDDLE);
  }
}