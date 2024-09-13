#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <unistd.h>
#include <arpa/inet.h>
#include <memory>
#include "json.hpp"

using namespace std;
using json = nlohmann::json;

string nombreCliente; // Mover nombreCliente aquí para acceder globalmente

void escucharServidor(int clientSocket) {
    char buffer[1024] = {0};
    while (true) {
        ssize_t bytesRecibidos = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesRecibidos <= 0) {
            cerr << "Conexión cerrada por el servidor o error al recibir." << endl;
            break;
        }
        string mensajeRecibido(buffer, bytesRecibidos);
        json mensajeJson;
        try {
            mensajeJson = json::parse(mensajeRecibido);
        } catch (const json::parse_error& e) {
            cerr << "Error al parsear el mensaje JSON: " << e.what() << endl;
            continue;
        }

        if (mensajeJson.contains("type")) {
            string tipo = mensajeJson["type"];
            if (tipo == "PUBLIC_TEXT_FROM") {
                string remitente = mensajeJson["username"];
                string mensaje = mensajeJson["text"];
                cout << remitente << ": " << mensaje << endl;
            } else if (tipo == "DISCONNECTED") {
                string usuario = mensajeJson["username"];
                cout << usuario << " se ha desconectado." << endl;
            } else if (tipo == "LEFT_ROOM") {
                string sala = mensajeJson["roomname"];
                string usuario = mensajeJson["username"];
                cout << usuario << " ha salido de la sala " << sala << "." << endl;
            } else if (tipo == "NEW_USER") {
                string usuario = mensajeJson["username"];
                cout << "Nuevo usuario se ha unido: " << usuario << endl;
            } else if (tipo == "RESPONSE") {
                string operacion = mensajeJson["operation"];
                string resultado = mensajeJson["result"];

                if (operacion == "IDENTIFY") {
                    if (resultado == "SUCCESS") {
                        cout << "Bienvenido, " << mensajeJson["extra"] << "!" << endl;
                        // Habilitar el envío de mensajes después de la identificación exitosa
                    } else if (resultado == "USER_ALREADY_EXISTS") {
                        cout << "El nombre de usuario '" << mensajeJson["extra"] << "' ya está en uso. Por favor, elige otro." << endl;
                    } else if (resultado == "INVALID_USERNAME") {
                        cout << "Error: " << mensajeJson["message"] << endl;
                    }
                } else if (operacion == "TEXT") {
                    if (resultado == "NO_SUCH_USER") {
                        cout << "El nombre de usuario '" << mensajeJson["extra"] << "' no existe." << endl;
                    }
                } else if (operacion == "NEW_ROOM") {
                    if (resultado == "SUCCESS") {
                        cout << "El cuarto " << mensajeJson["extra"] << " se creó exitosamente." << endl;
                    } else if (resultado == "ROOM_ALREADY_EXISTS") {
                        cout << "El nombre del cuarto " << mensajeJson["extra"] << " ya existe en el servidor." << endl;
                    }
                } else if (operacion == "INVITE") {
                    if (resultado == "NO_SUCH_USER") {
                        cout << "Invitación fallida. El nombre de usuario " << mensajeJson["extra"] << " no existe." << endl;
                    } else if (resultado == "NO_SUCH_ROOM") {
                        cout << "Invitación fallida. El cuarto " << mensajeJson["extra"] << " no existe." << endl;
                    }
                } else if (operacion == "JOIN_ROOM") {
                    if (resultado == "SUCCESS") {
                        cout << "Ingreso exitoso a la sala: " << mensajeJson["extra"] << endl;
                    } else if (resultado == "NO_SUCH_ROOM") {
                        cout << "Ingreso fallido. El cuarto " << mensajeJson["extra"] << " no existe." << endl;
                    } else if (resultado == "NOT_INVITED") {
                        cout << "Ingreso fallido. No estás invitado al cuarto: " << mensajeJson["extra"] << "." << endl;
                    }
                }
            } else if (tipo == "INVITATION") {
                string usuario = mensajeJson["username"];
                string sala = mensajeJson["roomname"];
                // Manejo de invitaciones
            } else if (tipo == "USER_LIST") {
                cout << "Usuarios conectados y sus estados:\n";
                for (auto it = mensajeJson["users"].begin(); it != mensajeJson["users"].end(); ++it) {
                    string username = it.key();  // Nombre del usuario
                    string status = it.value();  // Estado del usuario
                    cout << username << ": " << status << endl;
                }
            } else if (tipo == "NEW_STATUS") {
                string usuario = mensajeJson["username"];
                string estado = mensajeJson["status"];
                cout << usuario << " ha cambiado de estado: " << estado << endl;
            } else {
                cout << "Tipo de mensaje desconocido: " << mensajeJson.dump() << endl;
            }
        }
    }
}

int main() {
    string host;
    int puerto;
    nombreCliente = ""; // Inicializar globalmente para evitar problemas

    cout << "Ingresa la dirección del servidor: ";
    cin >> host;
    cout << "Ingresa el puerto: ";
    cin >> puerto;

    // Crear el socket del cliente
    auto clientSocket = make_unique<int>(socket(AF_INET, SOCK_STREAM, 0));
    if (*clientSocket == -1) {
        cerr << "Error al crear el socket del cliente" << endl;
        return -1;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(puerto);
    inet_pton(AF_INET, host.c_str(), &serverAddress.sin_addr);

    if (connect(*clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        cerr << "Error al conectar con el servidor. Asegúrate de que el servidor está encendido." << endl;
        return -1;
    }

    // Bucle para obtener y validar el nombre de usuario
    while (true) {
        cout << "Ingresa tu nombre/ID (máximo 8 caracteres): ";
        cin.ignore();
        getline(cin, nombreCliente);

        // Validar longitud del nombre de usuario
        if (nombreCliente.length() > 8) {
            cout << "El nombre de usuario debe tener 8 caracteres o menos. Por favor, inténtalo de nuevo." << endl;
            continue;
        }

        // Crear el mensaje de identificación en formato JSON
        json mensajeIdentificacion = {
            {"type", "IDENTIFY"},
            {"username", nombreCliente}
        };

        string mensajeStr = mensajeIdentificacion.dump();
        ssize_t bytesEnviados = send(*clientSocket, mensajeStr.c_str(), mensajeStr.length(), 0);
        if (bytesEnviados <= 0) {
            cerr << "Error al enviar el mensaje de identificación." << endl;
            return -1;
        }

        // Recibir la respuesta del servidor
        char buffer[1024] = {0};
        ssize_t bytesRecibidos = recv(*clientSocket, buffer, sizeof(buffer), 0);
        if (bytesRecibidos <= 0) {
            cerr << "Error al recibir respuesta del servidor." << endl;
            return -1;
        }

        string respuesta(buffer, bytesRecibidos);
        json respuestaJson;
        try {
            respuestaJson = json::parse(respuesta);
        } catch (const json::parse_error& e) {
            cerr << "Error al parsear la respuesta JSON: " << e.what() << endl;
            return -1;
        }

        if (respuestaJson.contains("result")) {
            if (respuestaJson["result"] == "SUCCESS") {
                cout << "Nombre/ID aceptado. Puedes comenzar a enviar mensajes." << endl;
                break; // Salir del bucle si la identificación es exitosa
            } else if (respuestaJson["result"] == "USER_ALREADY_EXISTS") {
                cout << "El nombre de usuario '" << nombreCliente << "' ya está en uso. Por favor, elige otro." << endl;
            } else if (respuestaJson["result"] == "INVALID_USERNAME") {
                cout << "Error: " << respuestaJson["message"] << endl;
            }
        } else {
            cerr << "Respuesta inesperada del servidor: " << respuesta << endl;
        }
    }

    // Lanzar un hilo para escuchar los mensajes del servidor
    auto hiloEscuchar = make_shared<thread>(escucharServidor, *clientSocket);
    hiloEscuchar->detach();

    // Loop para enviar mensajes al servidor
    string mensaje;
    while (true) {
        getline(cin, mensaje);

        if (mensaje == "/exit") {
            // Enviar el mensaje de desconexión
            json mensajeJson = {{"type", "DISCONNECT"}};
            string mensajeStr = mensajeJson.dump();
            send(*clientSocket, mensajeStr.c_str(), mensajeStr.length(), 0);
            break;
        } else if (mensaje == "/user_list") {
            // Solicitar la lista de usuarios
            json mensajeJson = {{"type", "USER_LIST"}};
            string mensajeStr = mensajeJson.dump();
            send(*clientSocket, mensajeStr.c_str(), mensajeStr.length(), 0);
        } else if (mensaje == "/status_ACTIVE" || mensaje == "/status_AWAY" || mensaje == "/status_BUSY") {
            // Enviar el cambio de estado
            string estado;
            if (mensaje == "/status_ACTIVE") estado = "ACTIVE";
            else if (mensaje == "/status_AWAY") estado = "AWAY";
            else if (mensaje == "/status_BUSY") estado = "BUSY";

            json mensajeJson = {
                {"type", "STATUS"},
                {"status", estado}
            };
            string mensajeStr = mensajeJson.dump();
            send(*clientSocket, mensajeStr.c_str(), mensajeStr.length(), 0);
        } else {
            // Enviar mensaje de texto
            json mensajeJson = {
                {"type", "PUBLIC_TEXT"},
                {"text", mensaje}
            };

            string mensajeStr = mensajeJson.dump();
            send(*clientSocket, mensajeStr.c_str(), mensajeStr.length(), 0);
        }
    }

    return 0;
}
