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
#include "json.hpp"

using namespace std;

mutex coutMutex;
mutex clientMutex;
map<string, int> clientes;
map<string, set<string>> cuartos;  // Cuartos asociados a cada cliente
map<string, string> estadosClientes;  // Estados de cada cliente

bool esPuertoValido(const string& puertoStr) {
    char* endptr;
    errno = 0;
    long puerto = strtol(puertoStr.c_str(), &endptr, 10);
    return !(errno != 0 || *endptr != '\0' || endptr == puertoStr.c_str() || puerto <= 0 || puerto > 65535);
}

string obtenerIPLocal() {
    struct ifaddrs *interfaces, *ifa;
    string ipLocal = "";

    if (getifaddrs(&interfaces) == -1) {
        cerr << "Error al obtener las interfaces de red." << endl;
        return ipLocal;
    }

    for (ifa = interfaces; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr) continue;

        if (ifa->ifa_addr->sa_family == AF_INET) {
            char ip[INET_ADDRSTRLEN];
            void* addrPtr = &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
            inet_ntop(AF_INET, addrPtr, ip, sizeof(ip));

            if (strcmp(ifa->ifa_name, "lo") != 0) {
                ipLocal = ip;
                break;
            }
        }
    }

    freeifaddrs(interfaces);
    return ipLocal;
}

void broadcastMensaje(const string& mensaje, const string& remitente) {
    lock_guard<mutex> lock(clientMutex);

    nlohmann::json mensajeJson = {
        {"type", "PUBLIC_TEXT_FROM"},
        {"username", remitente},
        {"text", mensaje}
    };

    string mensajeStr = mensajeJson.dump();

    for (const auto& cliente : clientes) {
        send(cliente.second, mensajeStr.c_str(), mensajeStr.length(), 0);
    }
}

void notificarDesconexion(const string& nombreCliente) {
    lock_guard<mutex> lock(clientMutex);

    nlohmann::json mensajeJson = {
        {"type", "DISCONNECTED"},
        {"username", nombreCliente}
    };
    string mensajeStr = mensajeJson.dump();

    for (const auto& cliente : clientes) {
        send(cliente.second, mensajeStr.c_str(), mensajeStr.length(), 0);
    }
}

void notificarSalidaCuartos(const string& nombreCliente) {
    lock_guard<mutex> lock(clientMutex);

    for (const auto& cuarto : cuartos[nombreCliente]) {
        nlohmann::json mensajeJson = {
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

void notificarCambioEstado(const string& nombreCliente, const string& nuevoEstado) {
    lock_guard<mutex> lock(clientMutex);

    nlohmann::json mensajeEstadoJson = {
        {"type", "NEW_STATUS"},
        {"username", nombreCliente},
        {"status", nuevoEstado}
    };
    string mensajeEstadoStr = mensajeEstadoJson.dump();

    for (const auto& cliente : clientes) {
        send(cliente.second, mensajeEstadoStr.c_str(), mensajeEstadoStr.length(), 0);
    }
}

void manejarCliente(int clientSocket) {
    char buffer[1024] = {0};
    string mensajeRecibido;

    ssize_t bytesRecibidos = recv(clientSocket, buffer, sizeof(buffer), 0);
    if (bytesRecibidos <= 0) {
        cerr << "Error al recibir el mensaje del cliente o conexión cerrada." << endl;
        close(clientSocket);
        return;
    }

    mensajeRecibido = string(buffer, bytesRecibidos);

    nlohmann::json mensajeJson;
    try {
        mensajeJson = nlohmann::json::parse(mensajeRecibido);
    } catch (const nlohmann::json::parse_error& e) {
        cerr << "Error al parsear el mensaje JSON: " << e.what() << endl;
        close(clientSocket);
        return;
    }

    if (mensajeJson.contains("type") && mensajeJson["type"] == "IDENTIFY" && mensajeJson.contains("username")) {
        string nombreCliente = mensajeJson["username"];

        {
            lock_guard<mutex> lock(clientMutex);
            if (clientes.find(nombreCliente) == clientes.end()) {
                clientes[nombreCliente] = clientSocket;
                estadosClientes[nombreCliente] = "ACTIVE";  // Estado inicial

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

                        nlohmann::json mensajeJson;
                        try {
                            mensajeJson = nlohmann::json::parse(mensajeRecibido);
                        } catch (const nlohmann::json::parse_error& e) {
                            cerr << "Error al parsear el mensaje JSON: " << e.what() << endl;
                            continue;
                        }

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
                                nlohmann::json mensajeErrorJson = {
                                    {"type", "RESPONSE"},
                                    {"operation", "STATUS"},
                                    {"result", "INVALID_STATUS"}
                                };
                                string mensajeErrorStr = mensajeErrorJson.dump();
                                send(clientSocket, mensajeErrorStr.c_str(), mensajeErrorStr.length(), 0);
                            }
                        } else if (mensajeJson.contains("type") && mensajeJson["type"] == "USER_LIST") {
                            nlohmann::json listaUsuariosJson = {
                                {"type", "USER_LIST"},
                                {"users", nlohmann::json::object()}
                            };
                            for (const auto& [usuario, estado] : estadosClientes) {
                                listaUsuariosJson["users"][usuario] = estado;
                            }
                            string listaUsuariosStr = listaUsuariosJson.dump();
                            send(clientSocket, listaUsuariosStr.c_str(), listaUsuariosStr.length(), 0);
                        }

                        memset(buffer, 0, sizeof(buffer));
                    }

                    close(clientSocket);
                }).detach();

                nlohmann::json respuestaJson = {
                    {"type", "RESPONSE"},
                    {"operation", "IDENTIFY"},
                    {"result", "SUCCESS"},
                    {"username", nombreCliente}
                };
                string respuestaStr = respuestaJson.dump();
                send(clientSocket, respuestaStr.c_str(), respuestaStr.length(), 0);

                // Eliminar la lista de usuarios que se enviaba automáticamente
            } else {
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

        thread(manejarCliente, clientSocket).detach();
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2 || !esPuertoValido(argv[1])) {
        cerr << "Uso: " << argv[0] << " <puerto>" << endl;
        return 1;
    }

    int puerto = stoi(argv[1]);

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == 0) {
        cerr << "Error al crear el socket: " << strerror(errno) << endl;
        return 1;
    }

    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        cerr << "Error al configurar el socket: " << strerror(errno) << endl;
        return 1;
    }

    struct sockaddr_in direccionServidor;
    memset(&direccionServidor, 0, sizeof(direccionServidor));
    direccionServidor.sin_family = AF_INET;
    direccionServidor.sin_addr.s_addr = INADDR_ANY;
    direccionServidor.sin_port = htons(puerto);

    if (bind(serverSocket, (struct sockaddr*)&direccionServidor, sizeof(direccionServidor)) < 0) {
        cerr << "Error al enlazar el socket: " << strerror(errno) << endl;
        return 1;
    }

    if (listen(serverSocket, 3) < 0) {
        cerr << "Error al escuchar en el socket: " << strerror(errno) << endl;
        return 1;
    }

    cout << "Servidor en ejecución en IP local: " << obtenerIPLocal() << " y puerto: " << puerto << endl;
    cout << "Esperando conexiones..." << endl;

    aceptarConexiones(serverSocket);

    return 0;
}
