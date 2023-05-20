#include "MedidorGas.h"

#include <ArduinoHA.h>
#include <ArduinoOTA.h>
#include <BMx280I2C.h>
#include <ESPTelnet.h>
#include <FIR.h>
#include <HTTPClient.h>
#include <HX711.h>
#include <SPI.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_wifi.h>

#include "lcd_ui.h"
#include "sound.h"
#include "time.h"
#include "web-server.h"

/*
        Definiciones para el guardado de la configuración
*/
EEPROMClass SettingsData("settings");
Conf_t Settings;
// ESPTelnet telnet;

FIR<float, 13> fir_lp;
float coef_lp[13] = { 660, 470, -1980, -3830, 504, 10027, 15214, 10027, 504, -3830, -1980, 470, 660 };
SlowFilter TestFiltro(3, 2, 15000, 120000, 0);
SlowFilter TestFiltro2(2, 1, 15000, 120000, 0);

BMx280I2C bmx280(0x76);
HX711 hx711;

float LoadCell, Temperature, Pressure, LoadCell_Raw;
float OTA_Progress;

LiquidCrystal lcd(25, 26, 27, 32, 33, 16); // Lcd 2 x 16

TactSwitch BtnEnter, BtnUp, BtnDown, BtnEsc; // Interfase de 4 botones
keyboard Keyboard;                           // Interfase de teclado
lcd_ui ui(16, 2);                            // Interfaz de usuario con una pantalla de 2 x 16

WiFiClient client;
HADevice device;
HAMqtt mqtt(client, device);
HASensor HA_Wifi("mg_wifi");
HASensor HA_Pres("mg_pres");
HASensor HA_Temp("mg_temp");
HASensor HA_Load("mg_loadcell");
HASensor HA_KG_Cons("mg_kg_cons");
HASensor HA_KG_Disp("mg_kg_disp");
HASensor HA_Porc_Cons("mg_porc_cons");
HASensor HA_Porc_Disp("mg_porc_disp");
HASensor HA_Tiempo_Online("mg_time_online");
HASensor HA_Tiempo_Disp("mg_time_disp");
HASensor HA_FechaCambio("mg_date_cambio");
HASensor HA_TestFiltro("mg_filtro");
HASensor HA_TestFiltro2("mg_filtro2");
HASensor HA_Consumo("mg_consumo");

// Variables de medición:
bool LoadCell_Ready = false, Vars_Ready = false; // Indica que ya se puede leer el peso, Indica que ya están disponibles los calculos
VarGarrafa_t Garrafa_Estado;
InfoGarrafa_t Garrafa_Info;

void printLocalTime();
void setupTelnet();

void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info);

/*
        Listado de los caracteres creados con la función Create_WiFi_Chars()
*/
enum WiFiChar : char
    {
    WiFiChar_Off = 0x1,  // Símbolo que representa el Wifi apagado
    WiFiChar_Lock,       // Símbolo que representa un candado
    WiFiChar_RSSI_1 = 4, // Símbolo que representa la intensidad mínima de recepción de Wifi (RSSI < -80)
    WiFiChar_RSSI_2,     // Símbolo que representa la intensidad media de recepción de Wifi (RSSI < -70)
    WiFiChar_RSSI_3,     // Símbolo que representa la intensidad media alta de recepción de Wifi (RSSI < -60)
    WiFiChar_RSSI_4,     // Símbolo que representa la intensidad máxima de recepción de Wifi (RSSI  > -60)
    };

bool UI_Main(lcd_ui* ui, UI_Action action);
bool UI_MainMenu(lcd_ui* ui, UI_Action action);
bool UI_Calibrate(lcd_ui* ui, UI_Action action);
bool UI_Ota(lcd_ui* ui, UI_Action action);
bool UI_CambiarGarrafa(lcd_ui* ui, UI_Action action);

const char* responsePath = "MedidorGas/cmd/response";

void onMqttMessage(const char* _topic, const uint8_t* payload, uint16_t length)
    {
        // This callback is called when message from MQTT broker is received.
        // Please note that you should always verify if the message's topic is the one you expect.
        // For example: if (memcmp(topic, "myCustomTopic") == 0) { ... }

    Serial.print("New message on topic: ");
    Serial.println(_topic);
    Serial.print("Data: ");
    Serial.println((const char*)payload);
    String topic = String(_topic);
    String msg = String(payload, length);

    if (topic == "MedidorGas/cmd") {
        if (msg == "detener") {
            DetenerMedicion();
            mqtt.publish(responsePath, "Medicion detenedida");
            }
        else if (msg == "reanudar") {
            ReanudarMedicion();
            mqtt.publish(responsePath, "Medicion iniciada");
            }
        else if (msg == "cal_info") {
            char str[128];
            sprintf(str, "{\"Zero\":%.0f, \"Conversion\":%.0f, \"Weight\":%.3f, \"CalTemp\": %.2f, \"Date\": %d}",
                    Settings.LoadCell_Calibration.Zero,
                    Settings.LoadCell_Calibration.Conversion,
                    Settings.LoadCell_Calibration.Weight,
                    Settings.LoadCell_Calibration.TempCompensation.CalTemp,
                    Settings.LoadCell_Calibration.Date);
            mqtt.publish(responsePath, str);
            }
        else if (msg == "state") {
            mqtt.publish(responsePath, Garrafa_Estado.getJSON().c_str());
            }
        else if (msg == "info") {
            mqtt.publish(responsePath, Garrafa_Info.getJSON().c_str());
            }
        }
    }

void onMqttConnected()
    {
    Serial.println("Connected to the broker!");
    // You can subscribe to custom topic if you need
    mqtt.subscribe("MedidorGas/cmd");
    }

void setup()
    {
    Serial.begin(115200);
    Serial.println("Iniciando placa....");

    // Iniciar LCD
    lcd.begin(16, 2);

    // Inicializar las teclas
    BtnEsc.begin(Keys::Esc, Keys::Left, 17);
    BtnDown.begin(Keys::Down, 5);
    BtnUp.begin(Keys::Up, 18);
    BtnEnter.begin(Keys::Enter, Keys::Next, 23);

    // Agregar teclas al teclado
    Keyboard.AddButton(&BtnEnter);
    Keyboard.AddButton(&BtnUp);
    Keyboard.AddButton(&BtnDown);
    Keyboard.AddButton(&BtnEsc);

    Settings.Load();

    ui.begin(&lcd, &Keyboard); // Inicia el stack UI

    ui.Add_UI(0x10, "main", UI_Main);                      // Pantalla Principal
    ui.Add_UI(0x11, "calibrate", UI_Calibrate);            // Muestra el progreso de actualización OTA
    ui.Add_UI(0x12, "wifi_ota", UI_Ota);                   // Muestra el progreso de actualización OTA
    ui.Add_UI(0x15, "menu", UI_MainMenu);                  // Menu principal
    ui.Add_UI(0x16, "cambiar_garrafa", UI_CambiarGarrafa); // Muestra el progreso de actualización OTA

    ui.setMainScreen("main");
    ui.Show("main");

    hx711.begin(4, 22, 128);
    fir_lp.setFilterCoeffs(coef_lp);
    Serial.print("Low Pass Filter Gain: ");
    Serial.println(fir_lp.getGain());

    Wire.begin(21, 19);
    if (!bmx280.begin()) {
        Serial.println("begin() failed. check your BMx280 Interface and I2C Address.");
        }
    bmx280.resetToDefaults();

    // by default sensing is disabled and must be enabled by setting a non-zero
    // oversampling setting.
    // set an oversampling setting for pressure and temperature measurements.
    bmx280.writeOversamplingPressure(BMx280MI::OSRS_P_x16);
    bmx280.writeOversamplingTemperature(BMx280MI::OSRS_T_x16);

    if (!bmx280.measure())
        Serial.println("could not start measurement, is a measurement already running?");

    WifiStart();

    byte mac[6];
    WiFi.macAddress(mac);
    mqtt.onMessage(onMqttMessage);
    mqtt.onConnected(onMqttConnected);

    device.setUniqueId(mac, sizeof(mac));
    device.setName("MedidorGas");
    device.setSoftwareVersion("1.1.0");
    device.enableSharedAvailability();
    device.enableLastWill();

    HA_Wifi.setName("Intensidad señal wifi");
    HA_Wifi.setUnitOfMeasurement("dBm");
    HA_Wifi.setIcon("mdi:wifi");

    HA_Temp.setName("Temperatura MedidorGas");
    HA_Temp.setUnitOfMeasurement("°C");
    HA_Temp.setIcon("mdi:thermometer");

    HA_Pres.setName("Presion MedidorGas");
    HA_Pres.setUnitOfMeasurement("hPa");
    HA_Pres.setIcon("mdi:gauge");

    HA_Load.setName("Celda de carga MedidorGas");
    HA_Load.setUnitOfMeasurement("Kg");
    HA_Load.setIcon("mdi:scale");

    HA_KG_Cons.setName("Kilos de gas consumidos");
    HA_KG_Cons.setUnitOfMeasurement("Kg");
    HA_KG_Cons.setIcon("mdi:scale");

    HA_KG_Disp.setName("Kilos de gas disponibles");
    HA_KG_Disp.setUnitOfMeasurement("Kg");
    HA_KG_Disp.setIcon("mdi:scale");

    HA_Porc_Cons.setName("Porcentaje de gas consumido");
    HA_Porc_Cons.setUnitOfMeasurement("%");
    HA_Porc_Cons.setIcon("mdi:sack-percent");

    HA_Porc_Disp.setName("Porcentaje de gas disponible");
    HA_Porc_Disp.setUnitOfMeasurement("%");
    HA_Porc_Disp.setIcon("mdi:sack-percent");

    HA_Tiempo_Online.setName("Tiempo en linea de garrafa");
    HA_Tiempo_Online.setUnitOfMeasurement("");
    HA_Tiempo_Online.setIcon("mdi:calendar-start");

    HA_Tiempo_Disp.setName("Tiempo restante estimado");
    HA_Tiempo_Disp.setUnitOfMeasurement("");
    HA_Tiempo_Disp.setIcon("mdi:calendar-end");

    HA_FechaCambio.setName("Fecha y hora de cambio de garrafa");
    HA_FechaCambio.setUnitOfMeasurement("");
    HA_FechaCambio.setIcon("mdi:calendar-clock");

    HA_TestFiltro.setName("Test filtro");
    HA_TestFiltro.setUnitOfMeasurement("Kg");
    HA_TestFiltro.setIcon("mdi:scale");

    HA_TestFiltro2.setName("Kilos de gas consumidos 2");
    HA_TestFiltro2.setUnitOfMeasurement("Kg");
    HA_TestFiltro2.setIcon("mdi:scale");

    HA_Consumo.setName("Consumo gas por hora");
    HA_Consumo.setUnitOfMeasurement("g/h");
    HA_Consumo.setIcon("mdi:scale");

    Serial.println("Cargando datos de garrafa...");
    Garrafa_Info = Settings.Garrafas.Datos[0];

    if (Settings.Garrafas.Contador == 0) {
        Serial.println("No hay ninguna garrafa cargada!!");
        }
    Garrafa_Info.Print(false);
    }

void loop()
    {
    static uint32_t last = 0, Publish_Time = 0, Update_Time = 0, MsgTime = 0, Consumo_Time = 0;
    /*
            Procesar la interfase de usuario

            Esta función se encarga de actualizar las pulsaciones de las teclas
            y de llamar a la función de visualización que corresponde.
            En screen.cpp están las funciones de cada pantalla.
    */
    ui.Run();

    ArduinoOTA.handle();

    // Leer el BMP280
    if (millis() - last > 1999) {
        last = millis();

        if (bmx280.hasValue()) {
            Pressure = bmx280.getPressure() / 100.0;
            Temperature = bmx280.getTemperature();
            }
        bmx280.measure();
        }

        // Leer el HX711
    if (hx711.is_ready()) {
        static int i = 0, acc = 0;
        acc += fir_lp.processReading(hx711.read());

        if (i++ == 9) {
            LoadCell_Raw = acc / 10.0;
            if (millis() > 2500) { // Dejar que se estabilice el filtro
                LoadCell = (LoadCell_Raw - Settings.LoadCell_Calibration.Zero) / Settings.LoadCell_Calibration.Conversion;
                LoadCell_Ready = true;
                if (Settings.LoadCell_Calibration.TempCompensation.Compensate) {
                    float CalTemp = Settings.LoadCell_Calibration.TempCompensation.CalTemp;
                    float Coeff = Settings.LoadCell_Calibration.TempCompensation.Coeff;
                    // LoadCellComp = LoadCell * 1000 + (CalTemp - Temperature) * 6;
                    } // else
                    // LoadCellComp = LoadCell;
                }
                // Serial.printf("Acc= %f, Zero: %f, Conv: %f\n", acc,
                // Settings.LoadCell_Calibration.Zero,
                // Settings.LoadCell_Calibration.Conversion); LoadCell.Print();
            i = 0;
            acc = 0.0;
            }
        }

    if (millis() - Update_Time > 4999) {
        Update_Time = millis();
        CalcularVariables();
        // Garrafa_Estado.Print();
        }

        // Publicar cada 5 segundos en home assistant, una vez que estè todo inicializado
    if (!Vars_Ready)
        Publish_Time = millis();

    if (millis() - Consumo_Time > 2499) {
        Consumo_Time = millis();
        if (!Vars_Ready)
            return;

        static uint64_t LastTime = 0, Time;
        static float LastVal, Diff, Consumo;
        float Val = TestFiltro.Run(LoadCell);

        if (LastTime == 0) {
            LastTime = millis();
            LastVal = Val;
            }

        if (Val < LastVal) {
            Time = millis() - LastTime; //Tiempo en mili segundos
            Diff = LastVal - Val;       //Gramos
            LastVal = Val;
            LastTime = millis();
            if (Time > 10 * 60 * 1000) //Si el tiempo es más de 10 minutos, tomarlo como timeout e ignorar la medida
                Consumo = 0.0;
            else
                Consumo = (Consumo + Diff * 1000.0 * (3600000 / Time)) / 2;
            if (Time > 10 * 60 * 1000)
                Time = Time > 10 * 60 * 1000;
            }
        else if (Val > LastVal) {
            LastVal = Val;
            }
        else if (millis() - LastTime > Time * 2) { //Una vez pasado el doble del último tiempo de cambio, poner consumo en 0
            Consumo = 0.0;
            }

        HA_TestFiltro.setValue(Val, 3);
        HA_Consumo.setValue(Consumo);

        char str[128];
        sprintf(str, "{\"LastTime\":%ul, \"Val\":%.3f, \"LastVal\":%.3f, \"Consumo\": %.2f, \"Time\": %ul}",
                LastTime,
                Val,
                LastVal,
                Consumo,
                Time);
        mqtt.publish("MedidorGas/consumo", str);
        }

    if (millis() - Publish_Time > 4999) {
        Publish_Time = millis();

        if (Garrafa_Info.OnLine)
            Garrafa_Estado.Print();

        char fecha_cambio[128];
        struct tm tstruct = *localtime(&Garrafa_Info.TiempoInicio);
        strftime(fecha_cambio, sizeof(fecha_cambio), "%d/%m/%Y %T", &tstruct);

        HA_Wifi.setValue((float)getWiFiRSSI());
        HA_Temp.setValue(Temperature);
        HA_Pres.setValue(Pressure);
        HA_Load.setValue(LoadCell, 3);
        HA_KG_Cons.setValue(Garrafa_Estado.Kg_Consumido, 2);
        HA_KG_Disp.setValue(Garrafa_Estado.Kg_Disponible, 2);
        HA_Porc_Cons.setValue(Garrafa_Estado.Porc_Consumido, 1);
        HA_Porc_Disp.setValue(Garrafa_Estado.Porc_Disponible, 1);
        HA_Tiempo_Online.setValue(getElapsedTime(Garrafa_Estado.TiempoEnLinea, false).c_str());
        HA_Tiempo_Disp.setValue(getElapsedTime(Garrafa_Estado.TiempoDisponible, false).c_str());
        HA_FechaCambio.setValue(fecha_cambio);

        char str[256];
        TestFiltro.getJSONState(str);
        mqtt.publish("MedidorGas/test_filtro", str);
        }

    mqtt.loop();

    static uint32_t WifiTimeOut = 0;
    if (!WiFi.isConnected() && Settings.Wifi.Mode == Wifi_Mode::Station) {
        if (WifiTimeOut == 0)
            WifiTimeOut = millis();
        if (millis() - WifiTimeOut > 5 * 1000) {
            lcd.clear();
            lcd.println("Wifi desconectado");
            lcd.setCursor(1, 1);
            lcd.println("Reiniciando...");
            delay(5000);
            ESP.restart();
            }
        }
    else
        WifiTimeOut = 0;

    static uint32_t MQTTTimeOut = 0;
    if (!mqtt.isConnected() && Settings.Mqtt.Enabled) {
        if (MQTTTimeOut == 0)
            MQTTTimeOut = millis();
        if (millis() - MQTTTimeOut > 5 * 1000) {
            lcd.clear();
            lcd.println("MQTT desconectado");
            lcd.setCursor(1, 1);
            lcd.println("Reiniciando...");
            delay(5000);
            ESP.restart();
            }
        }
    else
        MQTTTimeOut = 0;
    }

    /*
            Calcular el peso de garrafa y determinar el resto de las mediciones.
    */
void CalcularVariables()
    {
    static float LastHora = NAN, LastDia = NAN;
    static bool HoraOk = false, DiaOk = false, MesOk = false;
    static int32_t LastVal = 0, LastVal2;
    static uint32_t Last_Time = 0, TimeOut = 0;
    char str[128];

    if (!Garrafa_Info.OnLine || !LoadCell_Ready) {
        Last_Time = 0;
        return;
        }

    Garrafa_Estado.Kg_Bruto = LoadCell;
    if (Garrafa_Info.RefPesoIncial) // Calcular lo consumido según el método seleccionado
        Garrafa_Estado.Kg_Consumido = Garrafa_Info.Kg_Inicial - LoadCell;
    else
        Garrafa_Estado.Kg_Consumido = Garrafa_Info.Capacidad - (LoadCell - Garrafa_Info.Tara);
    Garrafa_Estado.Kg_Consumido = constrain(Garrafa_Estado.Kg_Consumido, 0.0, Garrafa_Info.Capacidad);

    HA_TestFiltro2.setValue(TestFiltro2.Run(Garrafa_Info.Capacidad - Garrafa_Estado.Kg_Consumido), 3);
    TestFiltro2.getJSONState(str);
    mqtt.publish("MedidorGas/test_filtro2", str);

    // Limitar la precisión a solo 2 dígitos y hacer filtrado por tiempo
    int32_t val = Garrafa_Estado.Kg_Consumido * 100.0;
    int32_t max_step = 1; // Máxima variacion de salida permitida en un paso
    if (Last_Time == 0) { // Solo al inicio
        LastVal = LastVal2 = val;
        Last_Time = millis();
        }

    if (abs(val - LastVal2) > 0) {
        Last_Time = millis();
        LastVal2 = val;
        TimeOut++;
        }

    if (millis() - Last_Time > 30000 || TimeOut > 24) { // Tiempo mínimo (en ms) que se debe mantener el valor para hacer el cambio
        Last_Time = millis();
        int32_t diff = val - LastVal;
        if (diff < -max_step)
            diff = -max_step;
        else if (diff > max_step)
            diff = max_step;
        LastVal += diff;
        TimeOut = 0;
        }

    sprintf(str, "{\"Last_Time\":%u, \"LastVal\":%d, \"LastVal2\":%d,  \"val\":%d, \"LoadCell\": %.2f,  \"timeout\":%d}",
            millis() - Last_Time,
            LastVal,
            LastVal2,
            val,
            LoadCell, TimeOut);
    mqtt.publish(responsePath, str);

    Garrafa_Estado.Kg_Consumido = (float)LastVal / 100.0;
    Garrafa_Estado.Kg_Disponible = Garrafa_Info.Capacidad - Garrafa_Estado.Kg_Consumido;
    Garrafa_Estado.Kg_Disponible = constrain(Garrafa_Estado.Kg_Disponible, 0.0, Garrafa_Info.Capacidad);
    Garrafa_Estado.Porc_Consumido = Garrafa_Estado.Kg_Consumido / Garrafa_Info.Capacidad * 100.0;
    Garrafa_Estado.Porc_Consumido = constrain(Garrafa_Estado.Porc_Consumido, 0.0, 100.0);
    Garrafa_Estado.Porc_Disponible = 100.0 - Garrafa_Estado.Porc_Consumido;
    time_t secs = getTime(nullptr);
    Garrafa_Estado.TiempoEnLinea = difftime(secs, Garrafa_Info.TiempoInicio);

    // Calcular consumos y estimar duración restante
    if (isnan(LastHora))
        LastHora = Garrafa_Estado.Kg_Consumido;
    if (isnan(LastDia))
        LastDia = Garrafa_Estado.Kg_Consumido;

    secs = getTime(nullptr);
    if (secs % 3600 < 5 && !HoraOk) // Consumo por hora
        {
        Garrafa_Estado.TiempoDisponible = Garrafa_Estado.Kg_Disponible / Garrafa_Estado.Kg_Consumido * Garrafa_Estado.TiempoEnLinea; // Estimar tiempo restante
        Garrafa_Estado.ConsumoHora = LastHora - Garrafa_Estado.Kg_Consumido;
        LastHora = Garrafa_Estado.Kg_Consumido;
        HoraOk = true;
        }
    else
        HoraOk = false;

    if (secs % (3600 * 24) < 5 && !DiaOk) // Consumo por día
        {
        Garrafa_Estado.ConsumoDia = LastDia - Garrafa_Estado.Kg_Consumido;
        LastDia = Garrafa_Estado.Kg_Consumido;
        DiaOk = true;
        }
    else
        DiaOk = false;

    Garrafa_Estado.ConsumoMes = 0.0;
    Vars_Ready = true;
    }

    /*
            Detener el calculo de mediciones de peso y de consumo
    */
void DetenerMedicion(bool save)
    {
    Garrafa_Info.OnLine = false;
    Garrafa_Info.Kg_Final = LoadCell;
    Garrafa_Info.TiempoFinal = getTime(nullptr);


    Serial.printf("Deteniendo medicion de garrafa... \n");

    Settings.Garrafas.Datos[0] = Garrafa_Info;
    if (save)
        Settings.Save();
    }

    /*
            Detener el calculo de mediciones de peso y de consumo
    */
void ReanudarMedicion(bool save)
    {
    Garrafa_Info.OnLine = true;
    Serial.printf("Reanudando medicion de garrafa... \n");

    Settings.Garrafas.Datos[0] = Garrafa_Info;
    if (save)
        Settings.Save();
    }

    /*

    */
void CambiarGarrafa(InfoGarrafa_t Info)
    {
    if (Settings.Garrafas.Contador > 0) {
        Serial.printf("Iniciando cambio de garrafa...\n");
        DetenerMedicion(false);
        Serial.printf("Caracteristicas de garrafa actual:\n");
        Garrafa_Info.Print();
        }
    else {
        Serial.printf("Iniciando instalacion de primer garrafa...\n");
        DetenerMedicion(false);
        }

    Serial.printf("\nCaracteristicas de garrafa nueva:\n");
    constrain(Info.Capacidad, 5.0, 100.0);
    constrain(Info.Kg_Inicial, 5.0, 100.0);
    constrain(Info.Tara, 5.0, 100.0);
    Info.Kg_Final = 0.0;
    Info.TiempoFinal = 0.0;
    Info.Print(false);

    if (Settings.Garrafas.Contador > 0) {
        Serial.printf("Actualizando lista de cambios...\n");
        for (int i = 1; i < sizeof(Settings.Garrafas.Datos) / sizeof(Settings.Garrafas.Datos[0]); i++) {
            Settings.Garrafas.Datos[i] = Settings.Garrafas.Datos[i - 1];
            }
        }
    Settings.Garrafas.Contador++;
    Settings.Garrafas.Datos[0] = Info;
    Garrafa_Info = Info;
    ReanudarMedicion();
    if (Settings.Garrafas.Contador > 1)
        Serial.printf("Se han realizado %d cambios de garras\n", Settings.Garrafas.Contador);
    Serial.printf("Listo!\n");
    }

void PrintListaGarrafas()
    {
    char str[128];
    struct tm tstruct;

    Serial.printf("Imprimiendo lista de cambios de garrafas\n");
    Serial.printf("%-30s | %-10s | %-15s\n", "Fecha", "Capacidad", "Duracion");
    Serial.printf("%-30s | %-10.s | %-15.s\n", "Fecha", "Peso", "Duracion");



    // for (int i = 0; i < sizeof(Settings.Readings.List) / sizeof(Settings.Readings.List[0]); i++) {
    //     tstruct = *localtime(&Settings.Readings.List[i].Fecha);
    //     strftime(str, sizeof(str), "%c", &tstruct);

    //     Serial.printf("%-30s | %-10.2f | %-15.s\n", str,
    //                   Settings.Readings.List[i].Capacidad, "-");
    // }
    }

//Devolver un String() en formato JSON con el estado del medidor de gas
String getStatusJSON() {
    char str[512];

    sprintf(str, "{\"disponible\":%.3f, \"consumido\":%.3f, \"bruto\":%.3f, "
                 "\"disp_porc\": %.2f, \"cons_porc\": %.2f, "
                 "\"capacidad\": %.3f, \"tara\": %.3f, "
                 "\"estado\": \"%s\", "
                 "\"online\": \"%s\", "
                 "\"fin\": \"%s\", "
                 "\"temp\": %.2f, \"presion\": %.2f, "
                 "\"ssid\": \"%s\", \"rssi\": %d"
                 "}",
            Garrafa_Estado.Kg_Disponible, Garrafa_Estado.Kg_Consumido, Garrafa_Estado.Kg_Bruto,
            Garrafa_Estado.Porc_Disponible, Garrafa_Estado.Porc_Consumido,
            Garrafa_Info.Capacidad, Garrafa_Info.Tara,
            Garrafa_Info.OnLine ? "online" : "detenido",
            getElapsedTime(Garrafa_Estado.TiempoEnLinea, false).c_str(),
            getElapsedTime(Garrafa_Estado.TiempoDisponible, false).c_str(),
            Temperature, Pressure,
            getWiFiSSID(), getWiFiRSSI()
    );
    return String(str);
    }

bool UI_Main(lcd_ui* ui, UI_Action action)
    {
    static ValBlinker Blink(5000);
    static int8_t Pos = 0, Level = 0;
    char str[32] = { 0 };

    if (action == UI_Action::Init || action == UI_Action::Restore) {
        Serial.printf("UI_Main-> action == UI_Action::Init\n");
        ui->lcd->clear();
        Create_WiFi_Chars(ui);
        Level = 0;
        return true;
        }
    else if (action != UI_Action::Run)
        return false;

    time_t secs = time(nullptr);
    struct tm now = *localtime(&secs);

    ui->lcd->setCursor(0, 0);
    if (WiFi.getMode() == WIFI_MODE_NULL)
        ui->lcd->write(WiFiChar_Off);
    else {
        wl_status_t status = WiFi.status();
        if (status == WL_CONNECTED) {
            ui->lcd->write(getWiFiRSSICode());
            }
        else {
            if (ui->Blinker.Update())
                ui->lcd->write(WiFiChar_RSSI_4);
            else
                ui->lcd->write(' ');
            }
        }

    if (ui->Blinker.Update())
        ui->lcd->printf(" %0.2i:%0.2i", now.tm_hour, now.tm_min, now.tm_sec);
    else
        ui->lcd->printf(" %0.2i %0.2i", now.tm_hour, now.tm_min, now.tm_sec);

    if (Garrafa_Info.OnLine == true)
        ui->lcd->printf("  %0.2i/%0.2i", now.tm_mday, now.tm_mon + 1);
    else
        ui->lcd->printf("  PAUSADO");

    ui->lcd->setCursor(0, 1);
    if (Level == 0)
        ui->lcd->printf("%2.2fC %7.3fKg  ", Temperature, LoadCell);
    else if (Level == 1)
        ui->lcd->printf("Cons:%2.2fKg%3.0f%%   ", Garrafa_Estado.Kg_Consumido, Garrafa_Estado.Porc_Consumido);
    else if (Level == 2)
        ui->lcd->printf("Disp:%2.2fKg%3.0f%%   ", Garrafa_Estado.Kg_Disponible, Garrafa_Estado.Porc_Disponible);
    else if (Level == 3) {
        time_t time = Garrafa_Info.TiempoInicio;
        now = *localtime(&time);
        ui->lcd->printf("Inicio: %02d/%02d/%02d  ", now.tm_mday, now.tm_mon + 1, now.tm_year - 100);
        }
    else if (Level == 4) {
        String res = getElapsedTime(Garrafa_Estado.TiempoEnLinea, false);
        res.replace("dias", "d");
        res.replace("dia", "d");
        if (res.length() > 12)
            res = res.substring(0, res.length() - 3);
        // Dur:xxx d xx:xx
        ui->lcd->printf("Dur:%12s", res.c_str());
        }
    else
        Level = 0;

    switch (ui->GetKeys()) {
            case Keys::Enter:
                ui->Show("menu");
                break;

            case Keys::Esc:
                ui->Close(UI_DialogResult::Cancel);
                break;

            case Keys::Up:
                Blink.Reset();
                Level++;
                if (Level > 4)
                    Level = 0;
                break;

            case Keys::Down:
                Blink.Reset();
                Level--;
                if (Level < 0)
                    Level = 4;
                break;
        }
    return true;
    }

bool UI_MainMenu(lcd_ui* ui, UI_Action action)
    {
    static const char* Main_List[] = { "Detener/Reanudar", "Cambiar garrafa", "Calibracion", "Valores defecto" };
    static MenuHelper Menu("Menu principal", Main_List, sizeof_menu(Main_List));
    Main_List[0] = Garrafa_Info.OnLine ? "Detener medicion" : "Retomar medicion";

    Menu.Run(ui, action);

    if (action == UI_Action::Init) {
        }
    else if (action == UI_Action::Closing) {
        }

    switch (Menu.getItem()) {
            case 1:
                ui->Question.ShowQuestion("Desea continuar?", "", Screen_Question::Options::OkCancel, [](UI_DialogResult res) {
                    if (res == UI_DialogResult::Ok) {
                        if (Garrafa_Info.OnLine)
                            DetenerMedicion();
                        else
                            ReanudarMedicion();
                        } });
                break;
            case 2:
                ui->Show("cambiar_garrafa");
                break;
            case 3:
                ui->Show("calibrate");
                break;
            case 4:
                ui->Question.ShowQuestion("Borrar todo?", "", Screen_Question::Options::OkCancel, [](UI_DialogResult res) {
                    if (res == UI_DialogResult::Ok) {
                        Settings.Default();
                        Settings.Save();
                        } });
                break;
                break;
            case 5:
                break;
        }
    return true;
    }

bool UI_Calibrate(lcd_ui* ui, UI_Action action)
    {
    static int8_t Step = 0;
    static float UI_Peso, UI_Offset, UI_Gain;

    if (action == UI_Action::Init) {
        UI_Peso = Settings.LoadCell_Calibration.Weight;
        UI_Offset = Settings.LoadCell_Calibration.Zero;
        UI_Gain = Settings.LoadCell_Calibration.Conversion;
        Step = 0;
        }
    else if (action != UI_Action::Run)
        return false;

    /*
     *	Calibración:
     *		1: Poner en 0
     *		2: Poner peso conocido
     *		3: Compensación de temperatura
     */
    switch (Step) {
            case 0:
                ui->lcd->setCursor(0, 0);
                ui->PrintText("Calibrar balanza", TextPos::Left);
                ui->lcd->setCursor(0, 1);
                ui->PrintText("Continuar?", TextPos::Center);
                break;

            case 1:
                ui->lcd->setCursor(0, 0);
                ui->PrintText("Vacie balanza...", TextPos::Left);
                ui->lcd->setCursor(0, 1);
                ui->PrintText("Listo?", TextPos::Center);
                break;

            case 2:
                ui->lcd->setCursor(0, 0);
                ui->PrintText("Cargue peso", TextPos::Left);
                ui->lcd->setCursor(0, 1);
                ui->PrintText("patron... Listo?", TextPos::Center);
                break;

            case 3:
                if (ui->getDialogResult() == UI_DialogResult::Ok) {
                    UI_Gain = (LoadCell_Raw - UI_Offset) / UI_Peso;
                    ui->Question.ShowQuestion("Guardar datos?", "");
                    Step++;
                    }
                else {
                    Step--;
                    }
                break;

            case 4:
                if (ui->getDialogResult() == UI_DialogResult::Ok) {
                    Settings.LoadCell_Calibration.Zero = UI_Offset;
                    Settings.LoadCell_Calibration.Conversion = UI_Gain;
                    Settings.LoadCell_Calibration.Weight = UI_Peso;
                    Settings.LoadCell_Calibration.Date = 0;
                    Settings.LoadCell_Calibration.TempCompensation.CalTemp = Temperature;
                    Settings.LoadCell_Calibration.TempCompensation.Coeff = 6.0;
                    Settings.LoadCell_Calibration.TempCompensation.Compensate = false;
                    Serial.printf("Calibracion -> Offset: %.0f, Gain: %.1f, Peso: %.3f Kg, Temp %.2t\n", UI_Offset, UI_Gain, UI_Peso, Settings.LoadCell_Calibration.TempCompensation.CalTemp);
                    Settings.Save();
                    }
                ui->Close(ui->getDialogResult());
                break;
        };

    switch (ui->GetKeys()) {
            case Keys::Enter:
                if (Step == 0) {
                    Step++;
                    }
                else if (Step == 1) {
                    UI_Offset = LoadCell_Raw;
                    Step++;
                    }
                else if (Step == 2) {
                    ValFormat_t format(" Kg", 3);
                    ui->Show_SetVal("Peso patron:", &UI_Peso, 0, 50.0, format);
                    Step++;
                    }
                break;

            case Keys::Esc:
                if (Step == 0)
                    ui->Close(UI_DialogResult::Cancel);
                else
                    Step--;
                break;
        }
    return true;
    }

const Option Opt_Medicion[] = { Option(0, "Por tara"),
                               Option(1, "Por peso inicial"),
                               Option(2, "Automatico") };
ValFormat_t format(" Kg", 2);

bool UI_CambiarGarrafa(lcd_ui* ui, UI_Action action)
    {
    static InfoGarrafa_t Info;
    static uint32_t Time;
    static int32_t Step = 0;

    if (action == UI_Action::Init) {
        Info = Garrafa_Info;
        Info.RefPesoIncial = 2;
        Step = 0;
        Time = 0;
        }
    else if (action != UI_Action::Run)
        return false;

    /*
     *	Cambio de garrafa:
     *		1: Sacar garrafa vacía
     *		2: Poner garrafa nueva
     *		3: Pedir capacidad en kilos de la misma
     *		4: Pedir tara de la garrafa
     *		5: Fecha y hora de cambio
     *		6: Confirmar
     */
    switch (Step) {
            case 0:
                ui->lcd->setCursor(0, 0);
                if (Settings.Garrafas.Contador == 0)
                    ui->PrintText("Instalar garrafa", TextPos::Left);
                else
                    ui->PrintText("Cambiar garrafa", TextPos::Left);
                ui->lcd->setCursor(0, 1);
                ui->PrintText("Continuar?", TextPos::Center);
                break;

            case 1:
                ui->lcd->setCursor(0, 0);
                ui->PrintText("Sacar garrafa", TextPos::Left);
                ui->lcd->setCursor(0, 1);
                ui->PrintText("usada... Listo?", TextPos::Center);
                break;

            case 2:
                ui->lcd->setCursor(0, 0);
                ui->PrintText("Cargue garrafa", TextPos::Left);
                ui->lcd->setCursor(0, 1);
                ui->PrintText("nueva...", TextPos::Center);

                // Una vez cargada la garrafa nueva, se pasa al siguiente paso automáticamente
                if (LoadCell > 5.0)
                    if (millis() - Time > 3500) {
                        Step++;
                        }
                    else
                        Time = millis();
                break;

            case 3:
                ui->Show_SetVal("Capacidad:", &Info.Capacidad, 5.0, 50.0, format);
                Step++;
                break;

            case 4:
                ui->Show_SetVal("Tara:", &Info.Tara, 5.0, 50.0, format);
                Step++;
                break;

            case 5:
                ui->OptionBox.ShowList("Metodo medicion", (int32_t*)&Info.RefPesoIncial, Opt_Medicion, 3);
                Step++;
                break;

            case 6:
                Info.Kg_Inicial = LoadCell;
                Info.TiempoInicio = getTime(nullptr);
                Info.OnLine = true;
                Info.TiempoFinal = 0;
                if (Info.RefPesoIncial == 2) {
                    if (LoadCell > (Info.Capacidad + Info.Tara) * 1.02)
                        Info.RefPesoIncial = true;
                    else
                        Info.RefPesoIncial = false;
                    }
                CambiarGarrafa(Info);
                // ui->Msg.ShowMessage("Info:", "Garrafa lista!");
                ui->Close(UI_DialogResult::Ok);
                break;
        };

    switch (ui->GetKeys()) {
            case Keys::Enter:
                if (Step == 0) {
                    DetenerMedicion(); // Detener actualización de consumo de balanza
                    Step++;
                    if (Settings.Garrafas.Contador == 0)
                        Step++;
                    }
                else if (Step == 1)
                    Step++;
                else if (Step == 2)
                    Step++;
                break;

            case Keys::Esc:
                if (Step == 0)
                    ui->Close(UI_DialogResult::Cancel);
                else
                    Step--;
                break;
        }
    return true;
    }

    // Estado de actualización OTA
bool UI_Ota(lcd_ui* ui, UI_Action action)
    {
    if (action == UI_Action::Init) {
        ui->lcd->clear();
        return true;
        }
    else if (action == UI_Action::Closing) {
        return true;
        }
    else if (action != UI_Action::Run)
        return false;

    ui->lcd->setCursor(0, 0);
    ui->PrintText("OTA Update", TextPos::Center);

    char val[20];
    ui->lcd->setCursor(0, 1);
    sprintf(val, "Descarga: %.0f %%", OTA_Progress);
    ui->PrintText(val, TextPos::Center);
    return true;
    }

void printLocalTime()
    {
    unsigned long Time;

    struct tm timeinfo;
    Time = micros();
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        return;
        }
    Time = micros() - Time;
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    Serial.printf("Time: %u us\n", Time);
    }

String getElapsedTime(time_t time, bool format)
    {
    int dias, horas, minutos, segundos;
    dias = time / (24 * 60 * 60);
    if (dias)
        time %= dias * (24 * 60 * 60);

    horas = time / (60 * 60);
    if (horas)
        time %= horas * (60 * 60);

    minutos = time / 60;
    if (minutos)
        time %= minutos * 60;
    segundos = time;

    String res = "";
    if (dias == 1)
        res += String(dias) + " dia ";
    else if (dias > 1)
        res += String(dias) + " dias ";
    if (format) {
        if (horas > 1)
            res += String(horas) + " horas ";
        else if (horas > 0)
            res += String(horas) + " hora ";
        if (minutos > 1)
            res += String(minutos) + " minutos ";
        else if (minutos > 0)
            res += String(minutos) + " minuto ";
        if (segundos > 1 || segundos == 0)
            res += String(segundos) + " segundos ";
        else if (segundos > 0)
            res += String(segundos) + " segundo ";
        }
    else {
        char str[16];
        sprintf(str, "%02d:%02d:%02d", horas, minutos, segundos);
        res += String(str);
        }
    res.trim();
    return res;
    }

int32_t getWiFiRSSI()
    {
    wifi_ap_record_t wifidata;
    // uint64_t time = micros();
    if (esp_wifi_sta_get_ap_info(&wifidata) == 0) {
        // time = micros() - time;
        // printf("rssi:%d in %u us\n", wifidata.rssi, time);
        return wifidata.rssi;
        }
    return 0;
    }

char* getWiFiSSID()
    {
    static wifi_ap_record_t wifidata;
    if (esp_wifi_sta_get_ap_info(&wifidata) == 0) {
        return (char*)wifidata.ssid;
        }
    return nullptr;
    }

char getWiFiRSSICode(int32_t RSSI)
    {
    char c = ' ';
    if (RSSI < -80)
        c = WiFiChar_RSSI_1;
    else if (RSSI < -70)
        c = WiFiChar_RSSI_2;
    else if (RSSI < -60)
        c = WiFiChar_RSSI_3;
    else // if (RSSI >= -60)
        c = WiFiChar_RSSI_4;
    return c;
    }

char getWiFiRSSICode() { return getWiFiRSSICode(getWiFiRSSI()); }

void Create_WiFi_Chars(lcd_ui* ui)
    {
    const uint8_t wifi_Off[] = { 0x0E, 0x11, 0x00, 0x0A,
                                0x04, 0x0A, 0x00, 0x0E }; // Wifi off
    const uint8_t wifi_Lock[] = { 0x0E, 0x11, 0x11, 0x11, 0x1F, 0x1B, 0x1B, 0x1F };
    const uint8_t wifi_1[] = { 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x0E }; // Wifi min
    const uint8_t wifi_2[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x0A, 0x00 };
    const uint8_t wifi_3[] = { 0x00, 0x00, 0x00, 0x0E, 0x11, 0x04, 0x0A, 0x00 };
    const uint8_t wifi_4[] = { 0x0E, 0x11, 0x00, 0x0E,
                              0x11, 0x04, 0x0A, 0x00 }; // Wifi max

    ui->lcd->createChar(0x1, (uint8_t*)wifi_Off);
    ui->lcd->createChar(0x2, (uint8_t*)wifi_Lock);
    ui->lcd->createChar(0x4, (uint8_t*)wifi_1);
    ui->lcd->createChar(0x5, (uint8_t*)wifi_2);
    ui->lcd->createChar(0x6, (uint8_t*)wifi_3);
    ui->lcd->createChar(0x7, (uint8_t*)wifi_4);

    Serial.printf("Create_WiFi_Chars() ok\n");
    }

    /*
            Iniciar el Wifi según la configuración que se lee
    */
void WifiStart()
    {
    WiFi.disconnect(true, true);

    Serial.printf("Iniciando Wifi...\n");
    WiFi.mode(static_cast<wifi_mode_t>(Settings.Wifi.Mode));

    if (Settings.Wifi.Mode == Wifi_Mode::Station)
        Serial.printf("Wifi modo estacion\n");
    else if (Settings.Wifi.Mode == Wifi_Mode::SoftAP)
        Serial.printf("Wifi modo soft AP\n");
    else if (Settings.Wifi.Mode == Wifi_Mode::Station_AP)
        Serial.printf("Wifi modo soft AP y estacion\n");
    else {
        Serial.printf("Wifi modo off\n");
        return;
        }

    WiFi.onEvent(WiFiStationConnected);
    WiFi.setSleep(false);

    if (Settings.Wifi.Station.StaticIP) {
        Serial.printf("Seteando dirección IP estatica... \n");
        Serial.printf("Dirección IP:    ");
        Serial.println(IPAddress(Settings.Wifi.Station.IP));
        Serial.printf("Mascara de red:  ");
        Serial.println(IPAddress(Settings.Wifi.Station.Mask));
        Serial.printf("Gateway:         ");
        Serial.println(IPAddress(Settings.Wifi.Station.Gateway));
        Serial.printf("DNS preferiado:  ");
        Serial.println(IPAddress(Settings.Wifi.Station.DNS1));
        Serial.printf("DNS alternativo: ");
        Serial.println(IPAddress(Settings.Wifi.Station.DNS2));

        if (WiFi.config(Settings.Wifi.Station.IP, Settings.Wifi.Station.Gateway,
            Settings.Wifi.Station.Mask, Settings.Wifi.Station.DNS1,
            Settings.Wifi.Station.DNS2))
            Serial.printf("IP iniciada correctamente!\n");
        else
            Serial.printf("Error al setear IP!\n");
        }

    Serial.printf("Iniciando wifi...\nConectando a: %s...",
                  Settings.Wifi.Station.SSID);
    WiFi.setAutoReconnect(true);

    if (WiFi.begin(Settings.Wifi.Station.SSID, Settings.Wifi.Station.Password))
        Serial.printf("Ok!\n");
    else
        Serial.printf("Error!\n");

    if (Settings.Wifi.Mode == Wifi_Mode::SoftAP ||
        Settings.Wifi.Mode == Wifi_Mode::Station_AP) {
        // Eventos de modo softAP
        //  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        //  	Serial.printf("Info: WiFi AP iniciado\n"); },
        //  SYSTEM_EVENT_AP_START);

        // WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        // 	Serial.printf("Info: WiFi AP terminado\n"); }, SYSTEM_EVENT_AP_STOP);

        // WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        // 	Serial.printf("Info: WiFi AP direccion IP aignada: ",
        // Serial.println(WiFi.softAPIP())); }, SYSTEM_EVENT_AP_STOP);

        // Configuracion soft ap
        Serial.printf("Configurando el punto de acceso (AP)...");
        if (WiFi.softAP(Settings.Wifi.SoftAP.SSID, Settings.Wifi.SoftAP.Password, 1,
            Settings.Wifi.SoftAP.Hide_SSID))
            Serial.printf("Ok!\n");
        else
            Serial.printf("Error!\n");

        if (Settings.Wifi.SoftAP.UseUserIP) {
            Serial.printf("Usar direccion IP definida por usuario para AP\n");
            Serial.printf("Dirección IP:    ");
            Serial.println(Settings.Wifi.SoftAP.IP);
            Serial.printf("Mascara de red:  ");
            Serial.println(Settings.Wifi.SoftAP.Mask);
            Serial.printf("Gateway:         ");
            Serial.println(Settings.Wifi.SoftAP.Gateway);

            if (WiFi.softAPConfig(Settings.Wifi.Station.IP,
                Settings.Wifi.Station.Mask,
                Settings.Wifi.Station.Gateway))
                Serial.printf("IP iniciada correctamente!\n");

            else
                Serial.printf("Error al setear IP!\n");
            }
        WiFi.softAPsetHostname("Medidor de Gas AP");
        WiFi.enableAP(true);
        }
    Serial.printf("Configuracion Wifi terminada correctamente\n\n");

    OTAStart();

    // TimeSetServer();
    }

    /*
            Métodos de actualización OTA (over the air)
    */
void OTAStart()
    {
        /*
                Iniciar el servicio de actualización por OTA
        */
    if (Settings.Wifi.OTA.Enable) {
        Serial.printf("Iniciando servicio de actualización de software OTA\n");

        if (strlen(Settings.Wifi.OTA.Name) > 0)
            ArduinoOTA.setHostname(Settings.Wifi.OTA.Name);
        else
            ArduinoOTA.setHostname("MedidorGas");

        if (strlen(Settings.Wifi.OTA.Password) > 0)
            ArduinoOTA.setPassword(Settings.Wifi.OTA.Password);

        ArduinoOTA.onStart(OTA_onStart);
        ArduinoOTA.onEnd(OTA_onEnd);
        ArduinoOTA.onProgress(OTA_onProgress);
        // ArduinoOTA.onError(OTA_onError);

        ArduinoOTA.begin();
        }
    else {
        ArduinoOTA.end();
        }
    }

void OTA_onStart()
    {
    if (ArduinoOTA.getCommand() == U_FLASH)
        Serial.printf("Iniciando actualización de firware via OTA...\n");
    else // U_SPIFFS
        Serial.printf(
            "Iniciando actualización de sistema de archivos via OTA...\n");

    ui.Show("wifi_ota");
    }

void OTA_onEnd()
    {
    Serial.printf("Actualización terminada\n");
    ui.Close(UI_DialogResult::Ok);
    }

void OTA_onProgress(uint32_t progress, uint32_t total)
    {
    OTA_Progress = (float)progress / (float)total * 100;
    // UI_Ota(&ui, UI_Action::Run);
    ui.Run();
    }

char OTA_ErrorMsg[32] = { 0 };
void OTA_onError(ota_error_t error)
    {
    if (error == OTA_AUTH_ERROR)
        strcpy(OTA_ErrorMsg, "Error de autenticacion");
    else if (error == OTA_BEGIN_ERROR)
        strcpy(OTA_ErrorMsg, "Error al iniciar");
    else if (error == OTA_CONNECT_ERROR)
        strcpy(OTA_ErrorMsg, "Error de conexion");
    else if (error == OTA_RECEIVE_ERROR)
        strcpy(OTA_ErrorMsg, "Error de recepcion");
    else if (error == OTA_END_ERROR)
        strcpy(OTA_ErrorMsg, "Error al terminar");

    Serial.printf("OTA Error[%u]: %s\n", error, OTA_ErrorMsg);

    ui.Msg.ShowMessage("Fallo de actualizacion", OTA_ErrorMsg, 2500);
    }

void TimeStart()
    {
    Serial.printf("Iniciando reloj... ");
    time_t now = Settings.Time.StartFrom;
    SetTimeTo(now); // 1/1/2020 00:00:00

    struct tm time = *localtime(&now);
    Serial.print(&time);
    Serial.println(" Ok!");
    }

void SetTimeTo(uint32_t Secs)
    {
    struct timeval now = { Secs, 0 };
    Screen_Date::setTimeZone(&Settings.Time.TimeZone);
    settimeofday(&now, nullptr);
    }

void TimeSetServer()
    {
    Serial.printf("Iniciando servidor de hora %s...\n", Settings.Time.Server);
    if (Settings.Time.UpdateFromInternet == false) {
        Serial.printf(
            "La actualizacion de hora desde internet esta desactivada.\n");
        return;
        }
        // Setear el servidor de tiempo
    char* tz =
        Screen_Date::setTimeZone(Settings.Time.TimeZone.tz_minuteswest * 60,
                                 Settings.Time.TimeZone.tz_dsttime * 60);
    configTzTime(tz, Settings.Time.Server);

    Serial.printf("getenv: %s\n", getenv("TZ"));

    /*
            Una vez seteado el servidor sntp la hora se sincroniza automáticamente
            cada una hora.

            https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/system_time.html
    */
    }

time_t getTime(time_t* _timer)
    {
    time_t now = time(_timer);
    return now + (Settings.Time.TimeZone.tz_minuteswest * 60);
    // return difftime(now, Settings.Time.TimeZone.tz_minuteswest * 60);
    }

    /*
            Evento de conexión de Wifi
    */
void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info)
    {
        // Serial.printf("----------------Event: %d, %d\n", event,
        // SYSTEM_EVENT_STA_CONNECTED);
    if (event == SYSTEM_EVENT_STA_CONNECTED || event == SYSTEM_EVENT_STA_GOT_IP) {
        static char SSID[32];
        if (info.wifi_sta_connected.ssid_len > 31)
            info.wifi_sta_connected.ssid_len = 31;

        memcpy(SSID, info.wifi_sta_connected.ssid,
               info.wifi_sta_connected.ssid_len);
        SSID[info.wifi_sta_connected.ssid_len] = 0;
        Serial.printf("Connected to AP!\n");
        Serial.printf("SSID: %s", SSID);
        Serial.printf("SSID: %s", getWiFiSSID());
        Serial.printf("Direccion IP: ");
        Serial.println(WiFi.localIP());

        ui.Msg.ShowMessage("Conectado!", getWiFiSSID());

        TimeSetServer();

        mqtt.begin(Settings.Mqtt.Broker, Settings.Mqtt.User,
                   Settings.Mqtt.Password);


        initWebServer();
        }
    }
