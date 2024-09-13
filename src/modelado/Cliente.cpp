#include <iostream>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <thread>
#include "json.hpp"

using namespace std;
using json = nlohmann::json;

// Enumeración para los tipos de mensajes
enum class TipoMensaje {
    PUBLIC_TEXT_FROM,
    DISCONNECTED,
    LEFT_ROOM,
    NEW_USER,
    RESPONSE,
    INVITATION,
    USER_LIST,
    NEW_STATUS,
    UNKNOWN
};

// Función para convertir el tipo de mensaje de string a enum
TipoMensaje convertirAEnum(const string& tipo) {
    if (tipo == "PUBLIC_TEXT_FROM") return TipoMensaje::PUBLIC_TEXT_FROM;
    if (tipo == "DISCONNECTED") return TipoMensaje::DISCONNECTED;
    if (tipo == "LEFT_ROOM") return TipoMensaje::LEFT_ROOM;
    if (tipo == "NEW_USER") return TipoMensaje::NEW_USER;
    if (tipo == "RESPONSE") return TipoMensaje::RESPONSE;
    if (tipo == "INVITATION") return TipoMensaje::INVITATION;
    if (tipo == "USER_LIST") return TipoMensaje::USER_LIST;
    if (tipo == "NEW_STATUS") return TipoMensaje::NEW_STATUS;
    return TipoMensaje::UNKNOWN;
}

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
            TipoMensaje tipoMensaje = convertirAEnum(tipo);

            switch (tipoMensaje) {
                case TipoMensaje::PUBLIC_TEXT_FROM: {
                    string remitente = mensajeJson["username"];
                    string mensaje = mensajeJson["text"];
                    cout << remitente << ": " << mensaje << endl;
                    break;
                }
                case TipoMensaje::DISCONNECTED: {
                    string usuario = mensajeJson["username"];
                    cout << usuario << " se ha desconectado." << endl;
                    break;
                }
                case TipoMensaje::LEFT_ROOM: {
                    string sala = mensajeJson["roomname"];
                    string usuario = mensajeJson["username"];
                    cout << usuario << " ha salido de la sala " << sala << "." << endl;
                    break;
                }
                case TipoMensaje::NEW_USER: {
                    string usuario = mensajeJson["username"];
                    cout << "Nuevo usuario se ha unido: " << usuario << endl;
                    break;
                }
                case TipoMensaje::RESPONSE: {
                    string operacion = mensajeJson["operation"];
                    string resultado = mensajeJson["result"];

                    if (operacion == "IDENTIFY") {
                        if (resultado == "SUCCESS") {
                            cout << "Bienvenido, " << mensajeJson["extra"] << "!" << endl;
                        } else if (resultado == "USER_ALREADY_EXISTS") {
                            cout << "El nombre de usuario '" << mensajeJson["extra"] << "' ya está en uso." << endl;
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
                    break;
                }
                case TipoMensaje::INVITATION: {
                    string usuario = mensajeJson["username"];
                    string sala = mensajeJson["roomname"];
                    cout << "Invitación a sala -> " << usuario << ": " << sala << endl;
                    break;
                }
                case TipoMensaje::USER_LIST: {
                    cout << "Usuarios conectados y sus estados:\n";
                    for (auto it = mensajeJson["users"].begin(); it != mensajeJson["users"].end(); ++it) {
                        string username = it.key();  // Nombre del usuario
                        string status = it.value();  // Estado del usuario
                        cout << username << ": " << status << endl;
                    }
                    break;
                }
                case TipoMensaje::NEW_STATUS: {
                    string usuario = mensajeJson["username"];
                    string estado = mensajeJson["status"];
                    cout << usuario << " ha cambiado de estado: " << estado << endl;
                    break;
                }
                case TipoMensaje::UNKNOWN:
                default: {
                    cout << "Tipo de mensaje desconocido: " << mensajeJson.dump() << endl;
                    break;
                }
            }
        }
        memset(buffer, 0, sizeof(buffer));
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
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        cerr << "Error al crear el socket del cliente" << endl;
        return -1;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(puerto);
    inet_pton(AF_INET, host.c_str(), &serverAddress.sin_addr);

    if (connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        cerr << "Error al conectar con el servidor. Asegúrate de que el servidor está encendido." << endl;
        close(clientSocket);
        return -1;
    }

    cout << "Ingresa tu nombre/ID: ";
    cin.ignore();
    getline(cin, nombreCliente);

    // Crear el mensaje de identificación en formato JSON
    json mensajeIdentificacion = {
        {"type", "IDENTIFY"},
        {"username", nombreCliente}
    };

    string mensajeStr = mensajeIdentificacion.dump();
    send(clientSocket, mensajeStr.c_str(), mensajeStr.length(), 0);

    // Recibir la respuesta del servidor
    char buffer[1024] = {0};
    ssize_t bytesRecibidos = recv(clientSocket, buffer, sizeof(buffer), 0);
    if (bytesRecibidos <= 0) {
        cerr << "Error al recibir respuesta del servidor." << endl;
        close(clientSocket);
        return -1;
    }

    // Procesar la respuesta JSON del servidor
    string respuesta(buffer, bytesRecibidos);
    json respuestaJson;
    try {
        respuestaJson = json::parse(respuesta);
    } catch (const json::parse_error& e) {
        cerr << "Error al parsear la respuesta JSON: " << e.what() << endl;
        close(clientSocket);
        return -1;
    }

    if (respuestaJson.contains("result") && respuestaJson["result"] == "SUCCESS") {
        cout << "Nombre/ID aceptado. Puedes comenzar a enviar mensajes." << endl;
    } else {
        cerr << "Identificación fallida: " << respuestaJson["message"] << endl;
        close(clientSocket);
        return -1;
    }

    // Lanzar un hilo para escuchar los mensajes del servidor
    thread hiloEscuchar(escucharServidor, clientSocket);
    hiloEscuchar.detach();

    // Loop para enviar mensajes al servidor
    string mensaje;
    while (true) {
        getline(cin, mensaje);

        if (mensaje == "/exit") {
            // Enviar el mensaje de desconexión
            json mensajeJson = {{"type", "DISCONNECT"}};
            string mensajeStr = mensajeJson.dump();
            send(clientSocket, mensajeStr.c_str(), mensajeStr.length(), 0);
            break;
        }

        json mensajeJson = {
            {"type", "PUBLIC_TEXT"},
            {"text", mensaje}
        };

        string mensajeStr = mensajeJson.dump();
        send(clientSocket, mensajeStr.c_str(), mensajeStr.length(), 0);
    }

    close(clientSocket);
    return 0;
}
