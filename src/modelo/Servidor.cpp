#include "Servidor.h"
#include <iostream>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>

using namespace std;

Servidor::Servidor(int puerto) : puerto_(puerto), serverSocket_(-1) {}

Servidor::~Servidor() {
    if (serverSocket_ != -1) {
        close(serverSocket_);
    }
}

void Servidor::iniciar() {
    serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket_ == -1) {
        cerr << "Error al crear el socket del servidor: " << strerror(errno) << endl;
        exit(-1);
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(puerto_);
    inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr);

    if (bind(serverSocket_, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        cerr << "Error al vincular el socket: " << strerror(errno) << endl;
        close(serverSocket_);
        exit(-1);
    }

    if (listen(serverSocket_, 5) < 0) {
        cerr << "Error al escuchar en el socket: " << strerror(errno) << endl;
        close(serverSocket_);
        exit(-1);
    }

    cout << "Servidor iniciado en el puerto: " << puerto_ << endl;
}

void Servidor::aceptarConexiones() {
    while (true) {
        int clientSocket = accept(serverSocket_, nullptr, nullptr);
        if (clientSocket < 0) {
            cerr << "Error al aceptar la conexión: " << strerror(errno) << endl;
            continue;
        }

        cout << "Cliente conectado. Solicitando identificación..." << endl;
        thread(manejarCliente, clientSocket).detach();
    }
}

void Servidor::manejarCliente(int clientSocket) {
    char buffer[1024] = {0};
    string nombreCliente;

    ssize_t bytesRecibidos = recv(clientSocket, buffer, sizeof(buffer), 0);
    if (bytesRecibidos <= 0) {
        cerr << "Error al recibir el nombre del cliente o conexión cerrada." << endl;
        close(clientSocket);
        return;
    }

    nombreCliente = string(buffer, bytesRecibidos);

    {
        lock_guard<mutex> lock(clientMutex_);
        if (clientes_.find(nombreCliente) == clientes_.end()) {
            clientes_[nombreCliente] = thread([clientSocket, nombreCliente]() {
                char buffer[1024] = {0};
                {
                    lock_guard<mutex> lock(coutMutex_);
                    cout << "Cliente " << nombreCliente << " conectado." << endl;
                }

                while (true) {
                    ssize_t bytesRecibidos = recv(clientSocket, buffer, sizeof(buffer), 0);
                    if (bytesRecibidos <= 0) {
                        lock_guard<mutex> lock(coutMutex_);
                        cout << "Cliente " << nombreCliente << " desconectado." << endl;
                        break;
                    }

                    string mensajeRecibido = nombreCliente + ": " + string(buffer, bytesRecibidos);

                    {
                        lock_guard<mutex> lock(clientMutex_);
                        for (const auto& cliente : clientes_) {
                            if (cliente.first != nombreCliente) {
                                send(clientSocket, mensajeRecibido.c_str(), mensajeRecibido.length(), 0);
                            }
                        }
                    }

                    memset(buffer, 0, sizeof(buffer));
                }

                close(clientSocket);
                {
                    lock_guard<mutex> lock(clientMutex_);
                    clientes_.erase(nombreCliente);
                }
            });

            clientes_[nombreCliente].detach();

            string successMsg = "IDENTIFY_SUCCESS";
            send(clientSocket, successMsg.c_str(), successMsg.length(), 0);
        } else {
            string errorMsg = "USER_ALREADY_EXISTS";
            send(clientSocket, errorMsg.c_str(), errorMsg.length(), 0);
            close(clientSocket);
        }
    }
}

bool Servidor::esPuertoValido(const std::string& puertoStr) {
    char* endptr;
    errno = 0;
    long puerto = strtol(puertoStr.c_str(), &endptr, 10);

    if (errno != 0 || *endptr != '\0' || endptr == puertoStr.c_str() || puerto <= 0 || puerto > 65535) {
        return false;
    }
    return true;
}
