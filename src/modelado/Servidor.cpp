#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <map>
#include <mutex>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <iostream>
#include "json.hpp"

using namespace std;

mutex coutMutex;
mutex clientMutex;
map<string, int> clientes;

bool esPuertoValido(const string& puertoStr) {
    char* endptr;
    errno = 0;
    long puerto = strtol(puertoStr.c_str(), &endptr, 10);
    return !(errno != 0 || *endptr != '\0' || endptr == puertoStr.c_str() || puerto <= 0 || puerto > 65535);
}

// Función para obtener la IP local de la red
string obtenerIPLocal() {
    struct ifaddrs *interfaces, *ifa;
    string ipLocal = "";

    if (getifaddrs(&interfaces) == -1) {
        cerr << "Error al obtener las interfaces de red." << endl;
        return ipLocal;
    }

    for (ifa = interfaces; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr) continue;

        if (ifa->ifa_addr->sa_family == AF_INET) { // Solo IPv4
            char ip[INET_ADDRSTRLEN];
            void* addrPtr = &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
            inet_ntop(AF_INET, addrPtr, ip, sizeof(ip));

            // Evitar interfaces locales como lo (127.0.0.1)
            if (strcmp(ifa->ifa_name, "lo") != 0) {
                ipLocal = ip;
                break;
            }
        }
    }

    freeifaddrs(interfaces);
    return ipLocal;
}

// Función para retransmitir mensajes a todos los clientes conectados
void broadcastMensaje(const string& mensaje, const string& remitente) {
    lock_guard<mutex> lock(clientMutex);
    
    // Crear el mensaje JSON con el protocolo PUBLIC_TEXT_FROM
    nlohmann::json mensajeJson = {
        {"type", "PUBLIC_TEXT_FROM"},
        {"username", remitente},
        {"text", mensaje}
    };
    
    string mensajeStr = mensajeJson.dump();

    // Enviar el mensaje a todos los clientes conectados
    for (const auto& cliente : clientes) {
        send(cliente.second, mensajeStr.c_str(), mensajeStr.length(), 0);
    }
}

void manejarCliente(int clientSocket) {
    char buffer[1024] = {0};
    string mensajeRecibido;

    // Recibir el primer mensaje del cliente (para identificarlo)
    ssize_t bytesRecibidos = recv(clientSocket, buffer, sizeof(buffer), 0);
    if (bytesRecibidos <= 0) {
        cerr << "Error al recibir el mensaje del cliente o conexión cerrada." << endl;
        close(clientSocket);
        return;
    }

    // Convertimos el mensaje recibido en un string
    mensajeRecibido = string(buffer, bytesRecibidos);

    // Parseamos el mensaje JSON
    nlohmann::json mensajeJson;
    try {
        mensajeJson = nlohmann::json::parse(mensajeRecibido);
    } catch (const nlohmann::json::parse_error& e) {
        cerr << "Error al parsear el mensaje JSON: " << e.what() << endl;
        close(clientSocket);
        return;
    }

    // Verificamos que el mensaje sea de tipo IDENTIFY
    if (mensajeJson.contains("type") && mensajeJson["type"] == "IDENTIFY" && mensajeJson.contains("username")) {
        string nombreCliente = mensajeJson["username"];
        
        {
            lock_guard<mutex> lock(clientMutex);
            if (clientes.find(nombreCliente) == clientes.end()) {
                // Guardar el nombre del cliente y el socket
                clientes[nombreCliente] = clientSocket;

                // Crear un nuevo hilo para manejar al cliente (solo para recibir mensajes del cliente)
                thread([clientSocket, nombreCliente]() {
                    char buffer[1024] = {0};
                    {
                        lock_guard<mutex> lock(coutMutex);
                        cout << "Cliente " << nombreCliente << " conectado." << endl;
                    }

                    while (true) {
                        ssize_t bytesRecibidos = recv(clientSocket, buffer, sizeof(buffer), 0);
                        if (bytesRecibidos <= 0) {
                            lock_guard<mutex> lock(coutMutex);
                            cout << "Cliente " << nombreCliente << " desconectado." << endl;
                            break;
                        }

                        string mensajeRecibido = string(buffer, bytesRecibidos);

                        // Parsear el mensaje recibido
                        nlohmann::json mensajeJson;
                        try {
                            mensajeJson = nlohmann::json::parse(mensajeRecibido);
                        } catch (const nlohmann::json::parse_error& e) {
                            cerr << "Error al parsear el mensaje JSON: " << e.what() << endl;
                            continue;
                        }

                        // Verificar si el mensaje es de tipo "PUBLIC_TEXT"
                        if (mensajeJson.contains("type") && mensajeJson["type"] == "PUBLIC_TEXT") {
                            string textoMensaje = mensajeJson["text"];
                            broadcastMensaje(textoMensaje, nombreCliente);
                        }

                        memset(buffer, 0, sizeof(buffer));
                    }

                    close(clientSocket);
                    {
                        lock_guard<mutex> lock(clientMutex);
                        clientes.erase(nombreCliente);  // Eliminar el cliente cuando se desconecta
                    }
                }).detach();

                // Enviar mensaje de éxito en formato JSON
                nlohmann::json respuestaJson = {
                    {"type", "RESPONSE"},
                    {"operation", "IDENTIFY"},
                    {"result", "SUCCESS"},
                    {"username", nombreCliente}
                };
                string respuestaStr = respuestaJson.dump();
                send(clientSocket, respuestaStr.c_str(), respuestaStr.length(), 0);
            } else {
                // El nombre de usuario ya existe
                nlohmann::json respuestaJson = {
                    {"type", "RESPONSE"},
                    {"operation", "IDENTIFY"},
                    {"result", "USER_ALREADY_EXISTS"},
                    {"username", nombreCliente}
                };
                string respuestaStr = respuestaJson.dump();
                send(clientSocket, respuestaStr.c_str(), respuestaStr.length(), 0);
                close(clientSocket);
            }
        }
    } else {
        // Mensaje inválido, enviamos un error al cliente
        nlohmann::json respuestaJson = {
            {"type", "RESPONSE"},
            {"operation", "INVALID"},
            {"result", "ERROR"},
            {"message", "Mensaje no reconocido o malformado"}
        };
        string respuestaStr = respuestaJson.dump();
        send(clientSocket, respuestaStr.c_str(), respuestaStr.length(), 0);
        close(clientSocket);
    }
}

void aceptarConexiones(int serverSocket) {
    while (true) {
        int clientSocket = accept(serverSocket, nullptr, nullptr);
        if (clientSocket < 0) {
            cerr << "Error al aceptar la conexión: " << strerror(errno) << endl;
            continue;
        }

        cout << "Cliente conectado. Solicitando identificación..." << endl;
        thread(manejarCliente, clientSocket).detach();
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "Uso: " << argv[0] << " <puerto>" << endl;
        return -1;
    }

    string puertoStr = argv[1];
    if (!esPuertoValido(puertoStr)) {
        cerr << "Número de puerto inválido. Debe ser un número entero positivo entre 1 y 65535." << endl;
        return -1;
    }

    int puerto = stoi(puertoStr);

    string ipLocal = obtenerIPLocal();
    if (ipLocal.empty()) {
        cerr << "No se pudo obtener la IP local." << endl;
        return -1;
    }

    cout << "Iniciando servidor en IP local: " << ipLocal << " y puerto: " << puerto << endl;

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        cerr << "Error al crear el socket del servidor: " << strerror(errno) << endl;
        return -1;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(puerto);

    if (inet_pton(AF_INET, ipLocal.c_str(), &serverAddress.sin_addr) <= 0) {
        cerr << "Error al convertir la dirección IP." << endl;
        close(serverSocket);
        return -1;
    }

    if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        cerr << "Error al vincular el socket: " << strerror(errno) << endl;
        close(serverSocket);
        return -1;
    }

    if (listen(serverSocket, 5) < 0) {
        cerr << "Error al escuchar en el socket: " << strerror(errno) << endl;
        close(serverSocket);
        return -1;
    }

    cout << "Esperando conexiones..." << endl;

    thread hiloAceptarConexiones(aceptarConexiones, serverSocket);
    hiloAceptarConexiones.join();

    close(serverSocket);

    return 0;
}
