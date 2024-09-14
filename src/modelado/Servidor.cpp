#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <map>
#include <set>
#include <mutex>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <iostream>
#include <memory>
#include "json.hpp"

using namespace std;
using json = nlohmann::json;

// Mutex para asegurar acceso seguro a la consola
mutex coutMutex;

// Mutex para asegurar acceso seguro a la información de los clientes
mutex clientMutex;

// Mapa que asocia nombres de usuario con descriptores de socket
map<string, int> clientes;

// Mapa que asocia cada cliente con los cuartos en los que está
map<string, set<string>> cuartos;

// Mapa que asocia nombres de usuario con sus estados
map<string, string> estadosClientes;

// Función para validar si un puerto es válido (rango de 1 a 65535)
bool esPuertoValido(const string& puertoStr) {
    char* endptr;
    errno = 0;
    long puerto = strtol(puertoStr.c_str(), &endptr, 10);
    return !(errno != 0 || *endptr != '\0' || endptr == puertoStr.c_str() || puerto <= 0 || puerto > 65535);
}

// Función para obtener la dirección IP local de la interfaz de red activa
string obtenerIPLocal() {
    struct ifaddrs *interfaces, *ifa;
    string ipLocal = "";

    // Obtener las interfaces de red
    if (getifaddrs(&interfaces) == -1) {
        cerr << "Error al obtener las interfaces de red." << endl;
        return ipLocal;
    }

    // Iterar sobre las interfaces para encontrar una dirección IP válida
    for (ifa = interfaces; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr) continue;

        if (ifa->ifa_addr->sa_family == AF_INET) {
            char ip[INET_ADDRSTRLEN];
            void* addrPtr = &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
            inet_ntop(AF_INET, addrPtr, ip, sizeof(ip));

            // Evitar la interfaz de loopback "lo"
            if (strcmp(ifa->ifa_name, "lo") != 0) {
                ipLocal = ip;
                break;
            }
        }
    }

    freeifaddrs(interfaces);
    return ipLocal;
}

// Función para enviar un mensaje de texto a todos los clientes conectados
void broadcastMensaje(const string& mensaje, const string& remitente) {
    lock_guard<mutex> lock(clientMutex);

    // Crear el mensaje en formato JSON
    json mensajeJson = {
        {"type", "PUBLIC_TEXT_FROM"},
        {"username", remitente},
        {"text", mensaje}
    };

    string mensajeStr = mensajeJson.dump();

    // Enviar el mensaje a todos los clientes
    for (const auto& cliente : clientes) {
        send(cliente.second, mensajeStr.c_str(), mensajeStr.length(), 0);
    }
}

// Función para notificar a todos los clientes que un cliente se ha desconectado
void notificarDesconexion(const string& nombreCliente) {
    lock_guard<mutex> lock(clientMutex);

    json mensajeJson = {
        {"type", "DISCONNECTED"},
        {"username", nombreCliente}
    };
    string mensajeStr = mensajeJson.dump();

    for (const auto& cliente : clientes) {
        send(cliente.second, mensajeStr.c_str(), mensajeStr.length(), 0);
    }
}

// Función para notificar a todos los clientes que un cliente ha salido de todos los cuartos
void notificarSalidaCuartos(const string& nombreCliente) {
    lock_guard<mutex> lock(clientMutex);

    for (const auto& cuarto : cuartos[nombreCliente]) {
        json mensajeJson = {
            {"type", "LEFT_ROOM"},
            {"roomname", cuarto},
            {"username", nombreCliente}
        };
        string mensajeStr = mensajeJson.dump();

        for (const auto& cliente : clientes) {
            send(cliente.second, mensajeStr.c_str(), mensajeStr.length(), 0);
        }
    }
}

// Función para notificar a todos los clientes el cambio de estado de un cliente
void notificarCambioEstado(const string& nombreCliente, const string& nuevoEstado) {
    lock_guard<mutex> lock(clientMutex);

    json mensajeEstadoJson = {
        {"type", "NEW_STATUS"},
        {"username", nombreCliente},
        {"status", nuevoEstado}
    };
    string mensajeEstadoStr = mensajeEstadoJson.dump();

    for (const auto& cliente : clientes) {
        send(cliente.second, mensajeEstadoStr.c_str(), mensajeEstadoStr.length(), 0);
    }
}

// Función que maneja la comunicación con un cliente específico
void manejarCliente(int clientSocket) {
    char buffer[1024] = {0};
    string mensajeRecibido;

    // Recibir el mensaje del cliente
    ssize_t bytesRecibidos = recv(clientSocket, buffer, sizeof(buffer), 0);
    if (bytesRecibidos <= 0) {
        cerr << "Error al recibir el mensaje del cliente o conexión cerrada." << endl;
        close(clientSocket);
        return;
    }

    mensajeRecibido = string(buffer, bytesRecibidos);

    json mensajeJson;
    try {
        mensajeJson = json::parse(mensajeRecibido);
    } catch (const json::parse_error& e) {
        cerr << "Error al parsear el mensaje JSON: " << e.what() << endl;
        close(clientSocket);
        return;
    }

    // Procesar el mensaje recibido según su tipo
    if (mensajeJson.contains("type") && mensajeJson["type"] == "IDENTIFY" && mensajeJson.contains("username")) {
        string nombreCliente = mensajeJson["username"];

        // Validar longitud del nombre de usuario
        if (nombreCliente.length() > 8) {
            json respuestaJson = {
                {"type", "RESPONSE"},
                {"operation", "IDENTIFY"},
                {"result", "INVALID_USERNAME"},
                {"message", "El nombre de usuario debe tener 8 caracteres o menos"}
            };
            string respuestaStr = respuestaJson.dump();
            send(clientSocket, respuestaStr.c_str(), respuestaStr.length(), 0);
            close(clientSocket);
            return;
        }

        {
            lock_guard<mutex> lock(clientMutex);
            // Verificar si el nombre de usuario ya está en uso
            if (clientes.find(nombreCliente) == clientes.end()) {
                clientes[nombreCliente] = clientSocket;
                estadosClientes[nombreCliente] = "ACTIVE";  // Estado inicial

                // Utilizar std::shared_ptr para manejar el socket del cliente
                auto clientSocketPtr = make_shared<int>(clientSocket);

                // Crear un nuevo hilo para manejar la comunicación con el cliente
                thread([clientSocketPtr, nombreCliente]() {
                    char buffer[1024] = {0};
                    {
                        lock_guard<mutex> lock(coutMutex);
                        cout << "Cliente " << nombreCliente << " conectado." << endl;
                    }

                    while (true) {
                        ssize_t bytesRecibidos = recv(*clientSocketPtr, buffer, sizeof(buffer), 0);
                        if (bytesRecibidos <= 0) {
                            lock_guard<mutex> lock(coutMutex);
                            cout << "Cliente " << nombreCliente << " desconectado." << endl;
                            break;
                        }

                        string mensajeRecibido = string(buffer, bytesRecibidos);

                        json mensajeJson;
                        try {
                            mensajeJson = json::parse(mensajeRecibido);
                        } catch (const json::parse_error& e) {
                            cerr << "Error al parsear el mensaje JSON: " << e.what() << endl;
                            continue;
                        }

                        // Procesar el mensaje según su tipo
                        if (mensajeJson.contains("type") && mensajeJson["type"] == "PUBLIC_TEXT") {
                            string textoMensaje = mensajeJson["text"];
                            broadcastMensaje(textoMensaje, nombreCliente);
                        } else if (mensajeJson.contains("type") && mensajeJson["type"] == "DISCONNECT") {
                            {
                                lock_guard<mutex> lock(clientMutex);
                                clientes.erase(nombreCliente);
                                notificarDesconexion(nombreCliente);
                                notificarSalidaCuartos(nombreCliente);
                            }
                            break;
                        } else if (mensajeJson.contains("type") && mensajeJson["type"] == "STATUS") {
                            string nuevoEstado = mensajeJson["status"];
                            if (nuevoEstado == "ACTIVE" || nuevoEstado == "AWAY" || nuevoEstado == "BUSY") {
                                estadosClientes[nombreCliente] = nuevoEstado;
                                notificarCambioEstado(nombreCliente, nuevoEstado);
                            } else {
                                json mensajeErrorJson = {
                                    {"type", "RESPONSE"},
                                    {"operation", "STATUS"},
                                    {"result", "INVALID_STATUS"}
                                };
                                string mensajeErrorStr = mensajeErrorJson.dump();
                                send(*clientSocketPtr, mensajeErrorStr.c_str(), mensajeErrorStr.length(), 0);
                            }
                        } else if (mensajeJson.contains("type") && mensajeJson["type"] == "USER_LIST") {
                            json listaUsuariosJson = {
                                {"type", "USER_LIST"},
                                {"users", json::object()}
                            };
                            for (const auto& [usuario, estado] : estadosClientes) {
                                listaUsuariosJson["users"][usuario] = estado;
                            }
                            string listaUsuariosStr = listaUsuariosJson.dump();
                            send(*clientSocketPtr, listaUsuariosStr.c_str(), listaUsuariosStr.length(), 0);
                        }
                    }

                    close(*clientSocketPtr);
                }).detach();
            } else {
                json respuestaJson = {
                    {"type", "RESPONSE"},
                    {"operation", "IDENTIFY"},
                    {"result", "USERNAME_TAKEN"},
                    {"message", "El nombre de usuario ya está en uso"}
                };
                string respuestaStr = respuestaJson.dump();
                send(clientSocket, respuestaStr.c_str(), respuestaStr.length(), 0);
                close(clientSocket);
            }
        }
    } else {
        json respuestaJson = {
            {"type", "RESPONSE"},
            {"operation", "UNKNOWN"},
            {"result", "INVALID_OPERATION"}
        };
        string respuestaStr = respuestaJson.dump();
        send(clientSocket, respuestaStr.c_str(), respuestaStr.length(), 0);
    }

    close(clientSocket);
}

// Función principal que inicia el servidor
int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "Uso: " << argv[0] << " <puerto>" << endl;
        return 1;
    }

    string puertoStr = argv[1];
    if (!esPuertoValido(puertoStr)) {
        cerr << "Puerto inválido. Debe estar en el rango 1-65535." << endl;
        return 1;
    }

    int puerto = stoi(puertoStr);
    int serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd == -1) {
        cerr << "Error al crear el socket del servidor." << endl;
        return 1;
    }

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(puerto);

    if (bind(serverFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        cerr << "Error al vincular el socket al puerto." << endl;
        close(serverFd);
        return 1;
    }

    if (listen(serverFd, 3) < 0) {
        cerr << "Error al poner el socket en modo de escucha." << endl;
        close(serverFd);
        return 1;
    }

    string ipLocal = obtenerIPLocal();
    if (ipLocal.empty()) {
        cerr << "No se pudo obtener la IP local." << endl;
        close(serverFd);
        return 1;
    }

    cout << "Servidor escuchando en " << ipLocal << ":" << puerto << endl;

    while (true) {
        sockaddr_in clientAddr = {};
        socklen_t clientAddrLen = sizeof(clientAddr);
        int clientSocket = accept(serverFd, (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket < 0) {
            cerr << "Error al aceptar la conexión del cliente." << endl;
            continue;
        }

        thread(manejarCliente, clientSocket).detach();
    }

    close(serverFd);
    return 0;
}
