#pragma once
#include "EEPROM.h"
#include "arduino.h"
#include "keyboard.h"
#include "lcd_ui.h"
#include <CRC32.h>
#include <IPAddress.h>
#include <WiFi.h>

int32_t getWiFiRSSI();
char *getWiFiSSID();
char getWiFiRSSICode();

char getWiFiRSSICode(int32_t RSSI);

char getWiFiRSSICode();

void Create_WiFi_Chars(lcd_ui *ui);

void WifiStart();

void OTAStart();

void OTA_onStart();

void OTA_onEnd();

void OTA_onProgress(uint32_t progress, uint32_t total);

// void OTA_onError(ota_error_t error);

void TimeStart();

void TimeSetServer();

void SetTimeTo(uint32_t Secs);

time_t getTime(time_t *_timer);

String getElapsedTime(time_t time, bool format = true);

class SlowFilter
{
public:
    SlowFilter()
    {
        Init(2, 1, 30000, 180000, 0); //Valores de inicio por defecto
    };
    SlowFilter(int32_t Decimales, int32_t Hysteresis, uint32_t MinTime, uint32_t TimeOut, uint32_t MaxOutStep)
    {
        Init(Decimales, Hysteresis, MinTime, TimeOut, MaxOutStep); //Valores de inicio por defecto
    };

    /*
    @param {uint32_t} Decimales - Cantidad de dígitos para redondear la medicion
    @param {uint32_t} MinTime - Tiempo mínimo que tiene que tener la lectura para tomar el valor actual

    */
    void Init(int32_t Decimales, int32_t Hysteresis, uint32_t MinTime, uint32_t TimeOut, uint32_t MaxOutStep)
    {
        this->Decimales = Digitos[constrain(Decimales, 0, 5)];
        this->MinTime = MinTime;
        this->Hysteresis = Hysteresis;
        this->MaxOutStep = MaxOutStep;

        Input = Output = 0;
        LastTime = 0;
        FirstRun = true;
    };

    float Run(float Value)
    {
        Input = Value * (float)Decimales;
        if (FirstRun) {
            FirstRun = false;
            Output = Input;
            LastTime = millis();
        }

        Diff = Input - Output;
        if (abs(Diff) < Hysteresis) {
            LastTime = millis();
        }

        if (millis() - LastTime > MinTime) { // Tiempo mínimo (en ms) que se debe mantener el valor para hacer el cambio
            LastTime = millis();
            if (MaxOutStep) {
                Diff = constrain(Diff, -MaxOutStep, MaxOutStep);
            }
            Output += Diff;
        }
        return (float)Output / (float)Decimales;
    };

    void getJSONState(char *str)
    {
        sprintf(str, "{\"Input\":%d, \"Output\":%d, \"Diff\":%d, \"LastTime\":%u}",
                Input,
                Output,
                Diff,
                millis() - LastTime);
    };

private:
    const int32_t Digitos[6] = {1, 10, 100, 1000, 10000, 100000};
    bool FirstRun;
    int32_t Decimales, Hysteresis, MaxOutStep; //Parámetos
    uint32_t LastTime, MinTime;
    int32_t Input, Output, Diff; //Copias de los valores de entrada y salida
};

struct VarGarrafa_t
{
    float Porc_Disponible;
    float Porc_Consumido;
    float Kg_Disponible;
    float Kg_Consumido;
    float Kg_Bruto;
    time_t TiempoEnLinea;
    time_t TiempoDisponible;
    float ConsumoHora;
    float ConsumoDia;
    float ConsumoMes;
    bool OnLine;

    void Print()
    {
        Serial.printf("Peso -> disponible: %.2f Kg, consumido: %.2f Kg, bruto: %.3f Kg\n", Kg_Disponible, Kg_Consumido, Kg_Bruto);
        Serial.printf("Porcentaje -> disponible: %.2f %%, consumido: %.2f %%\n", Porc_Disponible, Porc_Consumido);
        Serial.printf("Consumo -> hora: %.3f Kg, dia: %.3f Kg, mes: %.3f Kg\n", ConsumoHora, ConsumoDia, ConsumoMes);
        Serial.printf("Tiempo en linea: ");
        Serial.println(getElapsedTime(TiempoEnLinea, false));
        Serial.printf("Tiempo restante estimado: ");
        Serial.println(getElapsedTime(TiempoDisponible, false));
    }

    String getJSON()
    {
        char str[256];

        sprintf(str, "{\"disponible\":%.3f, \"consumido\":%.3f, \"bruto\":%.3f, "
                     "\"disp_porc\": %.2f, \"cons_porc\": %.2f, "
                     "\"cons_hora\": %.3f, \"cons_dia\": %.3f,\"cons_mes\": %.3f, "
                     "\"online\": \"%s\", "
                     "\"estimado\": \"%s\"}",
                Kg_Disponible, Kg_Consumido, Kg_Bruto,
                Porc_Disponible, Porc_Consumido,
                ConsumoHora, ConsumoDia, ConsumoMes,
                getElapsedTime(TiempoEnLinea, false).c_str(),
                getElapsedTime(TiempoDisponible, false).c_str());
        String res = str;
        return res;
    }
};

struct InfoGarrafa_t
{
    time_t TiempoInicio;    // Fecha y hora en la que se instala la garrafa
    time_t TiempoFinal;     // Fecha y hora en la que se la cambia
    float Kg_Inicial;       // Kg al momento de la instalación
    float Kg_Final;         // Kg al momento del cambio
    float Capacidad;        // Capacidad en Kg de la garrafa (nominal)
    float Tara;             // Tara en Kg de la garrafa
    bool OnLine;            // Indica si actuamente está conectada (es decir, si se la está midiendo)
    uint32_t RefPesoIncial; // Indica el modo de cálculo de consumo: true= usa el peso incial y la capacidad. false: usa la tara y la capacidad.

    void Print(bool final = true)
    {
        char str[64];
        struct tm tstruct = *localtime(&TiempoInicio);
        strftime(str, sizeof(str), "%c", &tstruct);

        Serial.printf("Estado: %s, metodo de calculo: %s\n", OnLine ? "en uso" : "fuera de uso", RefPesoIncial ? "por peso inicial" : "por tara");
        Serial.printf("Capacidad: %.2f Kg, Tara: %.2f Kg\n", Capacidad, Tara);
        Serial.printf("Estado inicial -> peso en bruto: %.2f Kg, neto: %.2f Kg, fecha y hora: %s\n", Kg_Inicial, Kg_Inicial - Tara, str);
        if (final) {
            tstruct = *localtime(&TiempoInicio);
            strftime(str, sizeof(str), "%c", &tstruct);
            Serial.printf("Estado final   -> peso en bruto: %.2f Kg, neto: %.2f Kg, fecha y hora: %s\n", Kg_Final, Kg_Final - Tara, str);
            Serial.printf("Resultado      -> gas usado: %.2f Kg en %s -> %.2f %% de desperdicio\n", Kg_Final - Kg_Inicial, getElapsedTime(TiempoFinal - TiempoInicio).c_str(), (Kg_Final - Kg_Inicial) / Capacidad * 100.0);
        }
    }

    /*
            Imprimir en una linea la info de la estructura
            |Fecha Inicio| Fecha final| Capacidad | Tara | Peso Inicial | Peso Final |
    */
    void PrintLn()
    {
    }

    String getJSON()
    {
        char str[512];
        char date1[64], date2[64];
        struct tm tstruct = *localtime(&TiempoInicio);
        strftime(date1, sizeof(str), "%c", &tstruct);
        tstruct = *localtime(&TiempoFinal);
        strftime(date2, sizeof(str), "%c", &tstruct);
        float porc = (Kg_Inicial - Kg_Final) / Capacidad * 100.0;
        porc = constrain(porc, 0.0, 100.0);

        sprintf(str, "{\"estado\":\"%s\", \"metodo\":\"%s\", "
                     "\"capacidad\": %.3f, \"tara\": %.3f, "
                     "\"inicio\":{\"bruto\": %.3f, \"neto\": %.3f, \"date\": \"%s\"}, "
                     "\"fin\":{\"bruto\": %.3f, \"neto\": %.3f, \"date\": \"%s\"}, "
                     "\"resultado\":{\"total_gas\": %.3f, \"duracion\": \"%s\", \"eficiencia_porc\": %.2f}}",
                OnLine ? "en uso" : "fuera de uso", RefPesoIncial ? "por peso inicial" : "por tara",
                Capacidad, Tara,
                Kg_Inicial, Kg_Inicial - Tara, date1,
                Kg_Final, Kg_Final - Tara, date2,
                Kg_Final - Kg_Inicial, getElapsedTime(TiempoFinal - TiempoInicio, false).c_str(), porc);
        String res = str;
        return res;
    }
};

void DetenerMedicion(bool save = true);
void ReanudarMedicion(bool save = true);
void CalcularVariables();
void PrintListaGarrafas();
String getStatusJSON();

extern EEPROMClass SettingsData;
/*
        Modo del Wifi.
*/
enum class Wifi_Mode : int32_t
{
    Off = WIFI_MODE_NULL,
    Station = WIFI_MODE_STA,
    SoftAP = WIFI_MODE_AP,
    Station_AP = WIFI_MODE_APSTA,
};

/*
 *	Esturctura que contiene la informaci�n del cambio de la garrafa
 */
struct Cambios_t
{
    time_t Fecha;    // Fecha de carga de la garrafa
    float PesoNeto;  // Peso de la garrafa
    float Capacidad; // Capacidad de la garrafa
};

/*
        Estructura que contiene toda la configuraci�n a guardar en FLASH.

        Cualquier cosas que se necesite guardar de manera permanente debe estar aqu�.
*/
struct Conf_t
{
private:
    uint32_t CRC32; // C�digo de chequeo de errores de la configuraci�n. DEBE SER EL PRIMER ELEMENTO DE LA ESTRUCTURA.
    bool FirstRun;

public:
    struct
    {
        Wifi_Mode Mode;
        struct
        {
            char SSID[48];
            char Password[48];
            int32_t StaticIP;
            uint32_t IP;
            uint32_t Mask;
            uint32_t Gateway;
            uint32_t DNS1;
            uint32_t DNS2;
        } Station;

        struct
        {
            char SSID[48];
            char Password[48];
            int32_t UseUserIP;
            int32_t Hide_SSID;
            uint32_t IP;
            uint32_t Mask;
            uint32_t Gateway;
        } SoftAP;

        struct
        {
            char Name[16];
            char Password[16];
            int32_t Enable;
        } OTA;
    } Wifi;

    struct
    {
        int32_t UpdateFromInternet;
        char Server[48];
        time_t StartFrom;
        struct timezone TimeZone;
        bool Simulate; // Aumenta la velocidad del tiempo, avanza una hora por segundo
    } Time;

    struct
    {
        char MainTopic[32];
        char Broker[48];
        char User[32];
        char Password[32];
        uint32_t Port;
        uint32_t UpdateTime; // Velocidad de actualizaci�n de datos
        uint32_t Enabled;
    } Mqtt;

    struct
    {
        float Zero;       // Cantidad de cuentas para marcar 0.0Kg
        float Conversion; // Cantidad de cuentas que equivalen a 1.0Kg
        float Weight;     // Peso de calibraci�n utilizado
        time_t Date;      // Fecha de calibraci�n
        struct
        {
            bool Compensate; // Activa compensaci�n por temperatura
            float CalTemp;   // Temperatura a la que se hizo la calibraci�n
            float Coeff;     // Coeficiente de ajuste
            uint32_t Lapse;  // Tiempo que dur� la compensaci�n
        } TempCompensation;  // Parametros de ajuste por temperatura
    } LoadCell_Calibration;  // Par�metros de calibraci�n de la celda de carga

    struct
    {
        int32_t Contador;
        InfoGarrafa_t Datos[32];
    } Garrafas;

    struct
    {

    } Hardware;

    /*
            Cargar la configuraci�n desde la memoria FLASH
    */
    void Load()
    {
        ESP_LOGI("Load", "Cargando configuracion... ");
        if (SettingsData.begin(sizeof(Conf_t)) == false) {
            ESP_LOGI("Load", "Se produjo un error al leer los datos");
            Default();
            Save();
        } else {
            SettingsData.get(0x0000, *this);
            ESP_LOGI("Load", "Los datos se cargaron correctamente");

            if (Check() == false) {
                Default();
                Save();
            }
        }
    };

    /*
            Guardar la configuraci�n en la memoria FLASH
    */
    void Save()
    {
        class CRC32 crc;
        ESP_LOGI("Save", "Guardando configuracion... ");
        uint8_t *p = ((uint8_t *)this) + 4; // Apuntar al siguiente elemento de CRC32
        uint32_t aux = crc.calculate(p, sizeof(Conf_t) - 4);

        ESP_LOGI("Save", "CRC old: 0x%.8X, new: 0x%.8X", CRC32, aux);
        ESP_LOGI("Save", "CRC: %p, this+4:%p", &CRC32, p);

        this->CRC32 = aux;

        if (memcmp(this, SettingsData.getDataPtr(), sizeof(Conf_t))) {
            SettingsData.put(0x0000, *this);
            if (SettingsData.commit() == false) {
                ESP_LOGI("Save", "ocurrio un error al guardar la configuracion!");
            } else {
                ESP_LOGI("Save", "CRC32= 0x%.8X Ok!", CRC32);
            }
        } else {
            ESP_LOGI("Save", "no hay cambios que guardar.");
        }
    };

    /*
            Chequea si el CRC calculado es igual al que est� en la estructura.
            Devuelve true si son iguales y false si no lo son
    */
    bool Check(bool Silent = false)
    {
        class CRC32 crc;

        if (!Silent) {
            ESP_LOGI("Check", "Chequeando configuracion...");
        }
        uint8_t *p = ((uint8_t *)this) + 4; // Apuntar al siguiente elemento de CRC32
        uint32_t aux = crc.calculate(p, sizeof(Conf_t) - 4);
        if (!Silent) {
            ESP_LOGI("Check", "CRC: %p, this+4:%p\n", &CRC32, p);
        }

        aux = crc.calculate(p, sizeof(Conf_t) - 4);

        if (!Silent) {
            ESP_LOGI("Check", "Settings CRC32=0x%.8X vs Data=0x%.8X -> %s", aux, CRC32, aux == CRC32 ? "Ok!" : "mal");
        }

        if (aux != CRC32)
            return false;
        return true;
    };

    /*
            Inicializa la estructura con los valores por defecto.

            Restaura la configuraci�n a f�brica.
    */
    void Default()
    {

        Serial.printf("Cargando configuracion por defecto...\n");

        // Wifi modo
        Wifi.Mode = Wifi_Mode::Station;
        // Modo estaci�n
        Wifi.Station.StaticIP = true;
        Wifi.Station.IP = IPAddress(192, 168, 1, 57);
        Wifi.Station.Mask = IPAddress(255, 255, 255, 0);
        Wifi.Station.Gateway = IPAddress(192, 168, 1, 1);
        Wifi.Station.DNS1 = IPAddress(8, 8, 8, 8);
        Wifi.Station.DNS2 = IPAddress(8, 8, 4, 4);
        strcpy(Wifi.Station.SSID, "Wifi-Luis");
        strcpy(Wifi.Station.Password, "");

        // Modo punto de acceso
        Wifi.SoftAP.Hide_SSID = false;
        Wifi.SoftAP.IP = IPAddress(192, 168, 1, 1);
        Wifi.SoftAP.Mask = IPAddress(255, 255, 255, 0);
        Wifi.SoftAP.Gateway = IPAddress(192, 168, 1, 1);
        strcpy(Wifi.SoftAP.SSID, "Medidor Gas AP");
        strcpy(Wifi.SoftAP.Password, "123456789");

        // OTA Update
        Wifi.OTA.Enable = true;
        strcpy(Wifi.OTA.Name, "MedidorGas");
        strcpy(Wifi.OTA.Password, "");

        // Configuraci�n de hora
        strcpy(Time.Server, "ar.pool.ntp.org");
        Time.UpdateFromInternet = true;
        Time.StartFrom = 1577836800;         // 1/1/2020 00:00:00
        Time.TimeZone.tz_minuteswest = -180; // Timezone -3Hs, no daylight savings
        Time.TimeZone.tz_dsttime = 0;        // Timezone -3Hs, no daylight savings
        Time.Simulate = false;

        // MQTT
        strcpy(Mqtt.Broker, "192.168.1.30");  // Broker
        Mqtt.Port = 1883;                     // Puerto
        strcpy(Mqtt.User, "luis");            // Usuario
        strcpy(Mqtt.Password, "electro");     // Password
        strcpy(Mqtt.MainTopic, "MedidorGas"); // ID
        Mqtt.Enabled = 1;
        Mqtt.UpdateTime = 2000; // Velocidad de actualizaci�n de datos

        LoadCell_Calibration.Zero = 129000;
        LoadCell_Calibration.Conversion = 44450;
        LoadCell_Calibration.Date = 0;
        LoadCell_Calibration.Weight = 10.0;
        LoadCell_Calibration.TempCompensation.Compensate = true;
        LoadCell_Calibration.TempCompensation.CalTemp = 10.5;
        LoadCell_Calibration.TempCompensation.Coeff = 0.0025;
        LoadCell_Calibration.TempCompensation.Lapse = 0;

        Garrafas.Contador = 0;
        memset(Garrafas.Datos, sizeof(Garrafas.Datos), 0);
    };

    /*
            Determina si la estructura tuvo alg�n cambio en los datos que contiene

            Lo hace comparando el CRC de los datos con el CRC calculado al inicio.
    */
    bool HasChanged()
    {
        return !Check(true);
    };

    /*
            Operador de copia
    */
    Conf_t &operator=(const Conf_t &data)
    {
        if (this != &data)
            memcpy(this, &data, sizeof(data));
        return *this;
    };

    /*
            Operador de comparaci�n. Compara
    */

    /*
            Constructor por defecto. Inicializa todo a 0
    */
    Conf_t()
    {
        memset(this, 0, sizeof(*this));
    };
};
