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

  // Route for root / web page
		server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
			request->send(SPIFFS, "/index.html", "text/html");
		});

		// Route to load style.css file
		server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest* request) {
			request->send(SPIFFS, "/style.css", "text/css");
		});

		// Route to load style.css file
		server.on("/app.js", HTTP_GET, [](AsyncWebServerRequest* request) {
			request->send(SPIFFS, "/app.js", "text/javascript");
		});

		// Route to load style.css file
		server.on("/info", HTTP_GET, [](AsyncWebServerRequest* request) {
			request->send(200, "application/json", Garrafa_Info.getJSON());
		});

		// Route to load style.css file
		server.on("/status", HTTP_GET, [](AsyncWebServerRequest* request) {
			request->send(200, "application/json", Garrafa_Estado.getJSON());
		});

		// Route to load style.css file
		server.on("/garrafa", HTTP_GET, [](AsyncWebServerRequest* request) {
			int params = request->params();
			if (params > 0) {
				for (int i = 0;i < params; i++) {

					String res = "[";
					res += Settings.Garrafas.Datos[0].getJSON() + ",";
					res += Settings.Garrafas.Datos[1].getJSON() + ",";
					res += Settings.Garrafas.Datos[2].getJSON() + ",";
					res += Settings.Garrafas.Datos[3].getJSON() + ",";
					res += Settings.Garrafas.Datos[4].getJSON() + ",";
					res += Settings.Garrafas.Datos[5].getJSON() + ",";
					res += Settings.Garrafas.Datos[6].getJSON() + ",";
					res += Settings.Garrafas.Datos[7].getJSON() + ",";
					res += Settings.Garrafas.Datos[8].getJSON() + ",";
					res += Settings.Garrafas.Datos[9].getJSON() + ",";
					res += Settings.Garrafas.Datos[10].getJSON();
					res += "]";

					request->send(200, "application/json", res);

					// AsyncWebParameter* p = request->getParam(i);
					// if (p->name() == "id") {
					// 	uint32_t id = p->value().toInt();
					// 	if (id < Settings.Garrafas.Contador) {
					// 		request->send(200, "application/json", Settings.Garrafas.Datos[id].getJSON());
					// 		}
					// 	}
					// else
					// 	request->send(404);
					}
				}
			else {
				char response[32];	//Si no hay parámetros, pasar la cantidad de garrasfas que hay
				sprintf(response, "{\"cantidad\":%d}", Settings.Garrafas.Contador);
				request->send(200, "application/json", String(response));

				}
		});

		DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
		DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT");
		DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");

		// Start server
		server.begin();
		}

	}

void loopWebServer() {



	}
