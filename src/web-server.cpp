#include "web-server.h"
#include "MedidorGas.h"
#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include <ArduinoJson.h>
#include "SPIFFS.h"

AsyncWebServer server(80);


extern VarGarrafa_t Garrafa_Estado;
extern InfoGarrafa_t Garrafa_Info;
extern Conf_t Settings;
void SetTimeTo(uint32_t Secs);


void onRequest(AsyncWebServerRequest* request) {
	// dummy callback function for handling params, etc.
	}


void onFileUpload(AsyncWebServerRequest* request, const String& filename, size_t index, uint8_t* data, size_t len, bool final) {
	// dummy callback function signature, not in used in our code
	}


void onBody(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
	DynamicJsonDocument doc(512);
	DeserializationError error = deserializeJson(doc, (char*)data);
	InfoGarrafa_t info;
	if (!error) {

		String online = doc["online"].as<String>();
		String metodo = doc["metodo"].as<String>();

		info.OnLine = online == "online" ? true : false;
		info.RefPesoIncial = metodo == "inicial" ? true : false;
		info.Capacidad = doc["capacidad"];
		info.Tara = doc["tara"];
		info.Kg_Inicial = doc["kg_inicio"];
		info.Kg_Final = doc["kg_fin"];
		info.TiempoInicio = doc["tm_inicio"];
		info.TiempoFinal = doc["tm_fin"];
		request->send(200, "text/plain", info.getJSON());

		}
	request->send(200, "text/plain", "ok-error");
	}




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

		server.on("/time", HTTP_POST, [](AsyncWebServerRequest* request) {
			Serial.printf("params: %i\n", request->params());
			for (int x = 0; x < request->params(); x++) {
				Serial.printf(" p%i-> %i '%s'='%s'\n", x,
				request->getParam(x)->isPost(),
				request->getParam(x)->name().c_str(),
				request->getParam(x)->value().c_str());
				}
			String error = "";
			if (request->hasParam("body", true)) {
				AsyncWebParameter* body = request->getParam("body", true);
				Serial.printf(" '%s' = '%s'\n", body->name().c_str(), body->value().c_str());
				int32_t time = body->value().toInt();
				SetTimeTo(time);
				}
			else
				error = "Parametro BODY faltante";

			if (error != "")
				request->send(404, "text/plain", error);
			else
				request->send(200, "text/plain", "ok");
		});


		server.on("/edit", HTTP_POST, [](AsyncWebServerRequest* request) {
			int32_t id = 0;
			InfoGarrafa_t info;
			String error = "";

			Serial.printf("params: %i\n", request->params());
			for (int x = 0; x < request->params(); x++) {
				Serial.printf(" p%i-> %i '%s'='%s'\n", x,
				request->getParam(x)->isPost(),
				request->getParam(x)->name().c_str(),
				request->getParam(x)->value().c_str());
				}

			if (request->hasParam("id")) {
				AsyncWebParameter* pid = request->getParam("id");
				Serial.printf(" '%s' = '%s'\n", pid->name().c_str(), pid->value().c_str());

				id = request->getParam("id")->value().toInt();
				if (id < 0 || id > 31)
					error = "ID fuera de rango [0-31]";
				}

			if (request->hasParam("body", true)) {
				AsyncWebParameter* body = request->getParam("body", true);
				Serial.printf(" '%s' = '%s'\n", body->name().c_str(), body->value().c_str());

				DynamicJsonDocument doc(512);
				DeserializationError err = deserializeJson(doc, body->value());
				if (!err) {
					String online = doc["online"].as<String>();
					String metodo = doc["metodo"].as<String>();

					info.OnLine = online == "online" ? true : false;
					info.RefPesoIncial = metodo == "inicial" ? true : false;
					info.Capacidad = doc["capacidad"];
					info.Tara = doc["tara"];
					info.Kg_Inicial = doc["kg_inicio"];
					info.Kg_Final = doc["kg_fin"];
					info.TiempoInicio = doc["tm_inicio"];
					info.TiempoFinal = doc["tm_fin"];
					}
				else
					error = "JSON deserialize error";
				}
			else
				error = "Parametro BODY faltante";

			if (error != "")
				request->send(404, "text/plain", error);
			else
				request->send(200, "text/plain", info.getJSON());

		// Serial.printf("args: %i\n", request->args());
		// request->send(200, "text/plain", "ok-error");

		});


		server.on("/detener", HTTP_GET, [](AsyncWebServerRequest* request) {
			DetenerMedicion(true);
			request->send(200, "application/json", Garrafa_Info.getJSON());
		});

		server.on("/iniciar", HTTP_GET, [](AsyncWebServerRequest* request) {
			ReanudarMedicion(true);
			request->send(200, "application/json", Garrafa_Info.getJSON());
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

		server.onNotFound([](AsyncWebServerRequest* request) {
			request->send(404, "text/plain", "Page Not Found");
		});

		// Start server
		server.begin();
		}

	}


void loopWebServer() {



	}
