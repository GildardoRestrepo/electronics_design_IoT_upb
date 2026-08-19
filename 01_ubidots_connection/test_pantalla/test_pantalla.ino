/*
 * PASO 1 - Prueba básica del TTGO T-Display
 * Objetivo: verificar que TFT_eSPI está bien configurada y que
 * la pantalla ST7789V responde correctamente.
 *
 * Requisito previo (se hace una sola vez, fuera de este .ino):
 * En User_Setup_Select.h de la librería TFT_eSPI:
 *   1) Comentar:    #include <User_Setup.h>
 *   2) Descomentar: #include <User_Setups/Setup25_TTGO_T_Display.h>
 * Fuente: github.com/Bodmer/TFT_eSPI
 */

#include <TFT_eSPI.h>
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI(135, 240); // Resolución nativa del ST7789V en el TTGO T-Display

#define TFT_BL 4 // Pin de backlight (ya está definido en Setup25, lo dejamos explícito por claridad)

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH); // Enciende el backlight

  tft.init();
  tft.setRotation(1); // Si la ves al revés o cortada, prueba 0, 2 o 3
  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("TTGO T-Display");

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(10, 40);
  tft.println("Prueba de libreria OK");

  // Tres rectángulos de color para confirmar que el bus SPI mueve
  // datos correctamente a distintas regiones de memoria de video
  tft.fillRect(10, 60, 60, 20, TFT_RED);
  tft.fillRect(80, 60, 60, 20, TFT_GREEN);
  tft.fillRect(150, 60, 60, 20, TFT_BLUE);

  Serial.println("Setup completo. Si ves texto y 3 rectangulos de color, la pantalla funciona.");
}

unsigned long lastUpdate = 0;
int counter = 0;

void loop() {
  // Contador simple para confirmar refresco dinámico de texto:
  // esta es la misma lógica que usaremos en el Paso 3 para
  // actualizar temperatura/humedad en tiempo real.
  if (millis() - lastUpdate > 1000) {
    lastUpdate = millis();
    counter++;

    tft.fillRect(10, 90, 220, 20, TFT_BLACK); // borra la línea anterior
    tft.setCursor(10, 90);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.print("Contador: ");
    tft.println(counter);

    Serial.print("Loop activo, segundos: ");
    Serial.println(counter);
  }
}
