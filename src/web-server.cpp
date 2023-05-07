#include "web-server.h"
#include "MedidorGas.h"
#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include "SPIFFS.h"

AsyncWebServer server(80);


extern VarGarrafa_t Garrafa_Estado;
extern InfoGarrafa_t Garrafa_Info;
extern Conf_t Settings;


void initWebServer() {
// Initialize SPIFFS
	if (!SPIFFS.begin(true)) {
		Serial.println("An Error has occurred while mounting SPIFFS");
		return;
		}

	if (WiFi.isConnected()) {

		//Archivos de la página web
		server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
			request->send(SPIFFS, "/index.html", "text/html");
		});

		server.on("/styles.css", HTTP_GET, [](AsyncWebServerRequest* request) {
			request->send(SPIFFS, "/styles.css", "text/css");
		});

		server.on("/app.js", HTTP_GET, [](AsyncWebServerRequest* request) {
			request->send(SPIFFS, "/app.js", "text/javascript");
		});

		//Archivos de librerias
		server.on("/src/bootstrap.bundle.min.js", HTTP_GET, [](AsyncWebServerRequest* request) {
			request->send(SPIFFS, "/src/bootstrap.bundle.min.js", "text/javascript");
		});

		server.on("/src/jquery-3.6.4.min.js", HTTP_GET, [](AsyncWebServerRequest* request) {
			request->send(SPIFFS, "/src/jquery-3.6.4.min.js", "text/javascript");
		});

		server.on("/src/bootstrap.min.css", HTTP_GET, [](AsyncWebServerRequest* request) {
			request->send(SPIFFS, "/src/bootstrap.min.css", "text/css");
		});


		//Consultas API
		server.on("/info", HTTP_GET, [](AsyncWebServerRequest* request) {
			request->send(200, "application/json", Garrafa_Info.getJSON());
		});

		server.on("/status", HTTP_GET, [](AsyncWebServerRequest* request) {
			request->send(200, "application/json", getStatusJSON());
		});

		/*
			Devolver información del historial de garrafas

			Si no se pasan argumentos devuelve un objeto JSON con la cantidad de cambios realizados.
			Si se pasa id, devuelve un objeto JSON con información de la garrafa indicada
			Si se pasa page, devuelve un array JSON con información de 8 garrafas

			Ej:
			/historial?id=2
			/historial?page=0
		*/
		server.on("/historial", HTTP_GET, [](AsyncWebServerRequest* request) {
			int params = request->params();
			if (params > 0) {
				AsyncWebParameter* p = request->getParam(0);
				if (p->name() == "id") {
					uint32_t id = p->value().toInt();
					if (id < Settings.Garrafas.Contador) {
						request->send(200, "application/json", Settings.Garrafas.Datos[id].getJSON());
						}
					}
				else if (p->name() == "page") {		//Devolver páginas de 8 garrafas
					int32_t page = p->value().toInt(), cant = 8, max;
					page = constrain(page, 0, 3);
					max = (page + 1) * cant;
					String res = "[";
					for (int x = page * cant; x < max; x++) {
						res += Settings.Garrafas.Datos[x].getJSON();
						if (x < (max - 1))
							res += ",";
						}
					res += "]";

					request->send(200, "application/json", res);
					}
				else
					request->send(404, "text/plain", "no encontrado");
				}
			else {
				char response[32];	//Si no hay parámetros, pasar la cantidad de garrasfas que hay
				sprintf(response, "{\"cantidad\":%d}", Settings.Garrafas.Contador);
				request->send(200, "application/json", String(response));
				}
		});

		// Para habilitar CORS. Al simular desde un servidor local no se puede llamar a la API. Con esto se soluciona.
		DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
		DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT");
		DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");

		// Start server
		server.begin();
		}

	}

void loopWebServer() {



	}
